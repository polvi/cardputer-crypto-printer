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
//   - Serial line: 57600 baud, 8 data bits, no parity, 1 stop bit, Xon/Xoff.
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
static constexpr uint32_t C330_BAUD = 57600;
// cdc_acm_line_coding_t: bCharFormat 0 = 1 stop bit, bParityType 0 = none.
static constexpr uint8_t  C330_STOP_BITS = 0;
static constexpr uint8_t  C330_PARITY    = 0;
static constexpr uint8_t  C330_DATA_BITS = 8;

static const char *TAG = "c330";

// ---- C330 embossing layout (format 0) ---------------------------------------
// Pushed on every connect so we never rely on a format stored in the machine.
// Syntax + values follow the manual (chapters 7-8). Edit for your plate/text.
//   U0     = unit is 10th of a mm
//   SY540  = plate height 54.0 mm   (default; max plate 60 mm)
//   SX860  = plate width  86.0 mm   (default; max plate 90 mm)
// Then one embossing line:
//   Y300   = 30.0 mm from the top edge to the bottom of the characters
//   X100   = 10.0 mm from the left edge to the start of the characters
//   F0     = font 0,  CI10 = 10 characters per inch
// Each line of text we send maps to the next format line in order, so this
// single line matches a single typed line of text.
static const char *C330_FORMAT =
    "<]F0 U0 SY540 SX860\r\n"
    "Y300 X100 F0 CI10 >\r\n";

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

// The C330 may assert Xon/Xoff. For short hello-world messages its receive
// buffer never fills, so we just log anything it sends back.
static bool handle_rx(const uint8_t *data, size_t len, void *arg) {
    ESP_LOGI(TAG, "rx %u bytes", (unsigned)len);
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

        xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
        g_vcp.reset(dev);
        xSemaphoreGive(g_vcp_mutex);

        // Push our embossing layout (format 0) so we don't depend on a format
        // stored in the machine. Sent once per connect, before any text.
        if (c330_write_raw(C330_FORMAT, strlen(C330_FORMAT)) != ESP_OK) {
            ESP_LOGE(TAG, "failed to send format");
        }
        g_ready = true;
        ESP_LOGI(TAG, "C330 ready @ %u baud, format 0 sent", (unsigned)C330_BAUD);

        // Block until the device is unplugged.
        xSemaphoreTake(g_disconnected_sem, portMAX_DELAY);

        xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
        g_vcp.reset();
        xSemaphoreGive(g_vcp_mutex);
    }
}

// Low-level write to the C330. Locks g_vcp; does not depend on g_ready so it can
// push the format during connect setup. Returns ESP_OK on success.
static esp_err_t c330_write_raw(const char *data, size_t len) {
    xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (g_vcp) {
        err = g_vcp->tx_blocking(reinterpret_cast<uint8_t *>(const_cast<char *>(data)), len);
    }
    xSemaphoreGive(g_vcp_mutex);
    return err;
}

// Sink the UI hands framed payload bytes to. Writes to the C330 over USB.
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

    ui_init(g_ui, device_send, "Connect C330 via USB...");
    ui_render(M5Cardputer.Display, g_ui);
}

void loop() {
    M5Cardputer.update();

    bool dirty = ui_set_connected(g_ui, g_ready);

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();

        for (auto c : st.word) {
            if (ui_handle_input(g_ui, {InputKey::Char, c})) dirty = true;
        }
        if (st.del   && ui_handle_input(g_ui, {InputKey::Backspace, 0})) dirty = true;
        if (st.enter && ui_handle_input(g_ui, {InputKey::Enter, 0}))     dirty = true;
        // NOTE: the Cardputer key matrix has no dedicated arrow flags. Arrow
        // events (used by the sim today) can be mapped here from fn+key combos
        // once the exact on-hardware keycodes are confirmed.
    }

    if (dirty) ui_render(M5Cardputer.Display, g_ui);
    delay(10);
}
