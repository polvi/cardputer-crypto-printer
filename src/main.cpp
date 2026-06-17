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

#include <memory>

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

// ---- shared state between the USB task and the UI loop ----------------------
static std::unique_ptr<CdcAcmDevice> g_vcp;
static SemaphoreHandle_t g_vcp_mutex = nullptr;        // guards g_vcp
static SemaphoreHandle_t g_disconnected_sem = nullptr; // signalled on unplug
static volatile bool g_ready = false;                  // device open + configured

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
        g_ready = true;
        xSemaphoreGive(g_vcp_mutex);
        ESP_LOGI(TAG, "C330 ready @ %u baud", (unsigned)C330_BAUD);

        // Block until the device is unplugged.
        xSemaphoreTake(g_disconnected_sem, portMAX_DELAY);

        xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
        g_vcp.reset();
        xSemaphoreGive(g_vcp_mutex);
    }
}

// Frame the text per the C330 protocol and send it. Returns false if no device.
static bool c330_send(const String &text) {
    if (!g_ready) return false;
    String framed = "<" + text + ">\r\n";

    xSemaphoreTake(g_vcp_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (g_vcp) {
        err = g_vcp->tx_blocking(reinterpret_cast<uint8_t *>(const_cast<char *>(framed.c_str())),
                                 framed.length());
    }
    xSemaphoreGive(g_vcp_mutex);
    return err == ESP_OK;
}

// =============================================================================
// Cardputer UI
// =============================================================================

static String g_buffer;
static String g_status = "Connect C330 via USB...";

static void redraw() {
    auto &d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print("C330 Embosser");

    // connection status
    d.setTextSize(1);
    if (g_ready) {
        d.setTextColor(TFT_GREEN, TFT_BLACK);
        d.setCursor(4, 26);
        d.print("USB: connected (57600)");
    } else {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.setCursor(4, 26);
        d.print("USB: waiting for C330...");
    }

    // typed message
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 44);
    d.print("> ");
    d.print(g_buffer);
    d.print("_"); // cursor

    // status / hint line at bottom
    d.setTextSize(1);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(4, d.height() - 12);
    d.print(g_status);
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextWrap(true);

    g_vcp_mutex = xSemaphoreCreateMutex();
    g_disconnected_sem = xSemaphoreCreateBinary();

    // Run USB host on core 0 so the Arduino UI keeps core 1 to itself.
    xTaskCreatePinnedToCore(usb_host_task, "usb_host", 8192, nullptr, 5, nullptr, 0);

    redraw();
}

void loop() {
    M5Cardputer.update();

    static bool last_ready = false;
    if (g_ready != last_ready) {
        last_ready = g_ready;
        if (g_ready && g_status.startsWith("Connect")) g_status = "Type, then ENTER to emboss";
        redraw();
    }

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();

        for (auto c : st.word) {
            g_buffer += c;
        }
        if (st.del && g_buffer.length() > 0) {
            g_buffer.remove(g_buffer.length() - 1);
        }
        if (st.enter) {
            if (g_buffer.length() == 0) {
                g_status = "Type something first";
            } else if (!g_ready) {
                g_status = "No C330 connected";
            } else if (c330_send(g_buffer)) {
                g_status = "Sent: " + g_buffer;
                g_buffer = "";
            } else {
                g_status = "Send FAILED";
            }
        }
        redraw();
    }

    delay(10);
}
