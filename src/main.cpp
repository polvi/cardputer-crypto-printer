// Cardputer -> C330 "hello world" embosser
// ----------------------------------------
// Type a message on the M5Cardputer keyboard, press ENTER, and it is sent over
// USB to a Matica Fintec C330 metal-plate embosser to be embossed on a plate.
//
// Wire-up: the Cardputer's USB-C port runs in ESP32-S3 USB HOST mode and talks
// to the C330's built-in FTDI USB-serial chip. You need a USB-C OTG/host cable
// from the Cardputer to the C330's USB-B port. The C330 is self-powered, so it
// does not draw bus power from the Cardputer.
//
// Protocol (from the C330 Operator Manual, rev 1.0):
//   - Serial line: baud per the printer's power-on display (here 9600), 8 data
//     bits, no parity, 1 stop bit, Xon/Xoff.
//   - Everything BETWEEN '<' and '>' is embossed; anything outside is discarded.
//   - Layout comes from "format 0" already stored in the machine, so a plain
//     line of text is enough: we send  <YOUR TEXT>\r\n
//   - (To (re)define the layout you would send a line starting with "<]F0 ..." ;
//     see chapter 7 of the manual. Not needed for hello-world.)

#include <Arduino.h>
#include <M5Cardputer.h>

#include "ui.h"

#include <memory>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "usb/usb_host.h"
#include "usb/vcp.hpp"
#include "usb/vcp_ftdi.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ch34x.hpp"

using namespace esp_usb;

// ---- C330 serial parameters --------------------------------------------------
// Must match the baud the printer shows at power-on ("Prot:Xon-Xoff Baud:NNNN").
// The C330 menu default is 57600, but units can be configured lower (e.g. 9600).
static constexpr uint32_t C330_BAUD = 9600;
// cdc_acm_line_coding_t: bCharFormat 0 = 1 stop bit, bParityType 0 = none.
static constexpr uint8_t  C330_STOP_BITS = 0;
static constexpr uint8_t  C330_PARITY    = 0;
static constexpr uint8_t  C330_DATA_BITS = 8;

static const char *TAG = "c330";

// Note: we never pre-load a format here. Every plate we stream is self-contained
// (its own `<]Fn SY540SX860 ...>` layout block immediately followed by `<text>`),
// so the Cardputer defines and overwrites the format it uses on each card — just
// like docs/keyprint.go. See src/c330_format.cpp.

// ---- shared state between the USB task and the UI loop ----------------------
static std::unique_ptr<CdcAcmDevice> g_vcp;
static SemaphoreHandle_t g_vcp_mutex = nullptr;        // guards g_vcp
static SemaphoreHandle_t g_disconnected_sem = nullptr; // signalled on unplug
static volatile bool g_ready = false;                  // device open + configured

// Low-level write to the C330 (locks g_vcp). Defined after the USB task.
static esp_err_t c330_write_raw(const char *data, size_t len);

// =============================================================================
// USB host plumbing
// =============================================================================

// Flow control is handled in hardware by the FTDI chip: we enable its Xon/Xoff at
// connect, so it pauses its own TX the instant the C330 asserts XOFF (no E64/E37
// overrun) and consumes the flow bytes itself -- so there's nothing to do with
// received data here.
static bool handle_rx(const uint8_t *data, size_t len, void *arg) {
    return true;
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx) {
    switch (event->type) {
        case CDC_ACM_HOST_ERROR:
            ESP_LOGE(TAG, "CDC-ACM error %d", event->data.error);
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            ESP_LOGW(TAG, "device disconnected");
            g_ready = false;
            xSemaphoreGive(g_disconnected_sem);
            break;
        case CDC_ACM_HOST_SERIAL_STATE:
            break;
        case CDC_ACM_HOST_NETWORK_CONNECTION:
        default:
            break;
    }
}

// Services the USB host library event loop.
static void usb_lib_task(void *arg) {
    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

// Owns the USB host: installs the stack, then repeatedly waits for a USB-serial
// adapter to appear, configures it for the C330, and waits for it to be unplugged.
static void usb_host_task(void *arg) {
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskCreate(usb_lib_task, "usb_lib", 4096, nullptr, 10, nullptr);

    // Teach VCP::open() about the bridges we might encounter. FTDI is the C330.
    VCP::register_driver<FT23x>();
    VCP::register_driver<CP210x>();
    VCP::register_driver<CH34x>();

    while (true) {
        const cdc_acm_host_device_config_t dev_config = {
            .connection_timeout_ms = 5000,
            .out_buffer_size = 512,
            .in_buffer_size = 512,
            .event_cb = handle_event,
            .data_cb = handle_rx,
            .user_arg = nullptr,
        };

        CdcAcmDevice *dev = VCP::open(&dev_config);
        if (dev == nullptr) {
            // No device yet; loop and wait again.
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        cdc_acm_line_coding_t line_coding = {
            .dwDTERate   = C330_BAUD,
            .bCharFormat = C330_STOP_BITS,
            .bParityType = C330_PARITY,
            .bDataBits   = C330_DATA_BITS,
        };
        if (dev->line_coding_set(&line_coding) != ESP_OK) {
            ESP_LOGE(TAG, "failed to set line coding");
            delete dev;
            continue;
        }
        dev->set_control_line_state(true, true); // DTR, RTS

        // Enable the FTDI chip's HARDWARE Xon/Xoff flow control, exactly as the Linux
        // ftdi_sio driver does for CUPS's flow=soft. The chip then halts its own TX
        // the instant the C330 asserts XOFF -- byte-accurate, with no software
        // overshoot of the printer's ~1.1KB receive buffer (manual error E64) when a
        // multi-card wallet is streamed in one shot. FTDI "Set Flow Control" is the
        // REAL command 0x02 (the esp FTDI component mislabels 0x02 as "SET_MHS");
        // wValue = XOFF<<8 | XON = 0x1311, wIndex = XON_XOFF_HS(0x0400) | channel 0,
        // bmRequestType 0x40 = vendor | host-to-device | device.
        esp_err_t fc = dev->send_custom_request(0x40, 0x02, 0x1311, 0x0400, 0, nullptr);
        ESP_LOGI(TAG, "FTDI hw Xon/Xoff: %s", esp_err_to_name(fc));

        xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
        g_vcp.reset(dev);
        xSemaphoreGive(g_vcp_mutex);

        g_ready = true;
        ESP_LOGI(TAG, "C330 ready @ %u baud", (unsigned)C330_BAUD);

        // Block until the device is unplugged.
        xSemaphoreTake(g_disconnected_sem, portMAX_DELAY);

        xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
        g_vcp.reset();
        xSemaphoreGive(g_vcp_mutex);
    }
}

// Low-level write to the C330, in small chunks. The FTDI chip's hardware Xon/Xoff
// (enabled at connect) pauses the chip's TX while the printer embosses, so tx_blocking
// simply blocks on the full FIFO -- a long timeout rides that out. CRITICAL: do NOT
// retry a timed-out chunk: cdc_acm's timeout resets the endpoint, which *completes*
// the in-flight transfer (bytes delivered), so resending would duplicate bytes and
// corrupt the stream (the C330 then parses a malformed field -> E37). Returns ESP_OK.
static esp_err_t c330_write_raw(const char *data, size_t len) {
    static constexpr size_t kChunk = 16;
    xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
    esp_err_t err = g_vcp ? ESP_OK : ESP_ERR_INVALID_STATE;
    for (size_t off = 0; off < len && err == ESP_OK; off += kChunk) {
        size_t n = (len - off < kChunk) ? (len - off) : kChunk;
        err = g_vcp->tx_blocking(
            reinterpret_cast<uint8_t *>(const_cast<char *>(data + off)), n, 300000);
    }
    xSemaphoreGive(g_vcp_mutex);
    return err;
}

// Sink the UI hands framed payload bytes to. Writes one card; the FTDI flow
// control paces it against the printer's emboss speed.
static bool device_send(const char *data, unsigned len) {
    if (!g_ready) return false;
    return c330_write_raw(data, len) == ESP_OK;
}

// =============================================================================
// Cardputer UI (shared UiState; see ui.h)
// =============================================================================

static UiState g_ui;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextWrap(true);

    g_vcp_mutex = xSemaphoreCreateMutex();
    g_disconnected_sem = xSemaphoreCreateBinary();

    // Run USB host on core 0 so the Arduino UI keeps core 1 to itself.
    xTaskCreatePinnedToCore(usb_host_task, "usb_host", 8192, nullptr, 5, nullptr, 0);

    ui_init(g_ui, device_send, nullptr);
    ui_render(M5Cardputer.Display, g_ui);
}

void loop() {
    M5Cardputer.update();

    bool dirty = ui_set_connected(g_ui, g_ready);

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();

        // Printable characters. Two keys are NOT plain text: the wide space bar
        // spans several key-matrix cells (so it lands in st.word repeatedly -- we
        // handle it once via st.space below), and the esc-labelled key types '`'
        // when pressed without Fn (we use it as Back/Esc).
        for (auto c : st.word) {
            if (c == ' ') continue;
            if (c == '`') { if (ui_handle_input(g_ui, {InputKey::Esc, 0})) dirty = true; continue; }
            if (ui_handle_input(g_ui, {InputKey::Char, c})) dirty = true;
        }

        // Fn layer: while Fn is held the library leaves st.word empty and reports
        // the Fn+;,./ arrow cluster and Fn+esc as state flags instead.
        if (st.up    && ui_handle_input(g_ui, {InputKey::Up,    0})) dirty = true;
        if (st.down  && ui_handle_input(g_ui, {InputKey::Down,  0})) dirty = true;
        if (st.left  && ui_handle_input(g_ui, {InputKey::Left,  0})) dirty = true;
        if (st.right && ui_handle_input(g_ui, {InputKey::Right, 0})) dirty = true;
        if (st.esc   && ui_handle_input(g_ui, {InputKey::Esc,   0})) dirty = true;

        if (st.space && ui_handle_input(g_ui, {InputKey::Char, ' '})) dirty = true;
        // Backspace: the bare key sets .backspace; Fn+backspace sets .del.
        if ((st.backspace || st.del) &&
            ui_handle_input(g_ui, {InputKey::Backspace, 0})) dirty = true;
        if (st.enter && ui_handle_input(g_ui, {InputKey::Enter, 0})) dirty = true;
    }

    // Print: a single press of the physical top button (G0). M5Cardputer.update()
    // above refreshed BtnA; wasPressed() fires once per press edge.
    if (M5Cardputer.BtnA.wasPressed() && ui_handle_input(g_ui, {InputKey::Print, 0})) {
        dirty = true;
    }

    if (dirty) ui_render(M5Cardputer.Display, g_ui);

    // Run a requested print after the "Printing…" frame is on screen. This blocks
    // (streaming all plates, FTDI-flow-controlled) until the wallet is sent.
    if (ui_pending_print(g_ui)) {
        ui_run_print(g_ui);
        ui_render(M5Cardputer.Display, g_ui);
    }
    delay(10);
}
