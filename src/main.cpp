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
static volatile bool g_cts   = true;                   // clear-to-send (Xon/Xoff)

// Low-level write to the C330 (locks g_vcp). Defined after the USB task.
static esp_err_t c330_write_raw(const char *data, size_t len);

// C330 software flow control.
static constexpr uint8_t XON  = 0x11;
static constexpr uint8_t XOFF = 0x13;

// =============================================================================
// USB host plumbing
// =============================================================================

// The C330 paces us with Xon/Xoff: it sends XOFF (0x13) when its input buffer is
// filling and XON (0x11) when it can take more. We honor that in c330_write_raw
// so streaming several plates at once doesn't overflow it (manual error E64).
// debug counters: how much the printer sends back, and how many XON/XOFF we see.
static volatile uint32_t g_rx_total = 0;
static volatile uint32_t g_xoff_total = 0;
static volatile uint32_t g_xon_total = 0;
static volatile uint32_t g_card_idx = 0; // which card in the current job (1-based)
static bool handle_rx(const uint8_t *data, size_t len, void *arg) {
    g_rx_total += len;
    for (size_t i = 0; i < len; ++i) {
        if (data[i] == XOFF) { g_cts = false; g_xoff_total++; }
        else if (data[i] == XON) { g_cts = true; g_xon_total++; }
    }
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

        // Enable the FTDI chip's HARDWARE Xon/Xoff flow control, exactly as the
        // Linux ftdi_sio driver does for CUPS's flow=soft (which is why `lp`
        // streams every card with no overrun). FTDI "Set Flow Control" is the REAL
        // command 0x02 (the esp FTDI component mislabels 0x02 as "SET_MHS"); value
        // = XOFF<<8 | XON = 0x1311, index = XON_XOFF_HS(0x0400) | channel 0. Now the
        // chip halts its own TX the instant the C330 asserts XOFF -- byte-accurate,
        // no software overshoot of the printer's field buffer (the E37 cause).
        //   bmRequestType 0x40 = vendor | host-to-device | device.
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

// Low-level write to the C330. Flow control is now done by the FTDI CHIP itself
// (hardware Xon/Xoff enabled at connect), exactly like CUPS's flow=soft path: the
// chip halts its own TX the instant the printer asserts XOFF, so we cannot overrun
// the printer's field buffer no matter how fast we feed it. Back-pressure surfaces
// naturally -- when the chip is paused (XOFF) its TX FIFO fills and tx_blocking
// blocks until the chip drains -- so we give it a long timeout to ride out a full
// card's emboss. The software g_cts gate is kept only as an inert fallback (if the
// chip flow control were not active, the printer's XOFF would still reach us).
// Returns ESP_OK on success.
static esp_err_t c330_write_raw(const char *data, size_t len) {
    static constexpr size_t kChunk = 16;
    xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
    esp_err_t err = g_vcp ? ESP_OK : ESP_ERR_INVALID_STATE;
    for (size_t off = 0; off < len && err == ESP_OK; off += kChunk) {
        for (int spins = 0; !g_cts && g_ready && spins < 24000; ++spins)
            vTaskDelay(pdMS_TO_TICKS(5)); // inert unless chip flow control is off
        size_t n = (len - off < kChunk) ? (len - off) : kChunk;
        // 60s timeout: the chip may hold this write while the printer XOFFs us for a
        // whole card's emboss (~55s) with its FIFO full.
        err = g_vcp->tx_blocking(
            reinterpret_cast<uint8_t *>(const_cast<char *>(data + off)), n, 60000);

        // (debug) live readout. With chip Xon/Xoff working, XF/XN stay 0 (the chip
        // consumes the printer's flow bytes) and the print still completes -- that is
        // the success signature. If XF climbs, the chip is NOT doing flow control and
        // the software gate above is carrying it instead.
        auto &d = M5Cardputer.Display;
        d.fillRect(0, 0, 240, 9, TFT_BLACK);
        d.setTextSize(1);
        d.setTextColor(g_cts ? TFT_GREEN : TFT_RED, TFT_BLACK);
        d.setCursor(2, 1);
        char b[64];
        snprintf(b, sizeof(b), "#%u TX%u/%u RX%u XF%u XN%u C%d", (unsigned)g_card_idx,
                 (unsigned)(off + n), (unsigned)len, (unsigned)g_rx_total,
                 (unsigned)g_xoff_total, (unsigned)g_xon_total, (int)g_cts);
        d.print(b);
    }
    xSemaphoreGive(g_vcp_mutex);
    return err;
}

// Sink the UI hands framed payload bytes to. Writes one card; the FTDI flow
// control paces it against the printer's emboss speed.
static bool device_send(const char *data, unsigned len) {
    if (!g_ready) return false;
    ++g_card_idx; // one device_send per plate/card
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

        for (auto c : st.word) {
            // The Cardputer has no dedicated arrow keys: Fn + ; , . / is the
            // arrow cluster. While Fn is held, map those to arrows instead of
            // typing them.
            if (st.fn) {
                InputKey k = InputKey::None;
                switch (c) {
                    case ';': k = InputKey::Up;    break;
                    case '.': k = InputKey::Down;  break;
                    case ',': k = InputKey::Left;  break;
                    case '/': k = InputKey::Right; break;
                    default:  break;
                }
                if (k != InputKey::None) {
                    if (ui_handle_input(g_ui, {k, 0})) dirty = true;
                }
                continue; // don't type characters while Fn is held
            }
            if (ui_handle_input(g_ui, {InputKey::Char, c})) dirty = true;
        }
        if (st.space && ui_handle_input(g_ui, {InputKey::Char, ' '})) dirty = true;
        if (st.del   && ui_handle_input(g_ui, {InputKey::Backspace, 0})) dirty = true;
        if (st.enter && ui_handle_input(g_ui, {InputKey::Enter, 0}))     dirty = true;
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
        g_card_idx = 0; // restart the per-job card counter (debug overlay)
        ui_run_print(g_ui);
        ui_render(M5Cardputer.Display, g_ui);
    }
    delay(10);
}
