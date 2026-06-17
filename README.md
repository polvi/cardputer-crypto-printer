# Cardputer → C330 embosser

Type a message on the **M5Stack Cardputer** keyboard, press **ENTER**, and it is
embossed on a metal plate by a **Matica Fintec C330** metal-plate embosser.

The Cardputer's USB-C port runs in **USB host** mode and drives the C330's
built-in **FTDI** USB-serial chip directly — i.e. you plug the C330 into the
Cardputer with a USB host/OTG cable, no PC in the loop.

## How it works

The C330 accepts plain text over a serial line and emboss-prints whatever sits
**between `<` and `>`**, using a layout ("format 0") already stored in the
machine (see the Operator Manual, `docs/`). So a hello-world is just:

```
<HELLO WORLD>
```

Firmware flow:

1. `M5Cardputer` drives the keyboard + screen — you type a line.
2. The ESP32-S3 USB host stack (`usb_host_vcp` + `usb_host_ftdi`) opens the
   C330's FTDI port and configures it to the C330's serial settings:
   **57600 baud, 8 data bits, no parity, 1 stop bit** (manual §6.2).
3. On ENTER, the text is framed as `<text>\r\n` and written to the port.

## Hardware / wiring

- **USB host/OTG cable** from the Cardputer USB-C to the C330's USB-B port.
- The C330 is **self-powered** (it has its own PSU), so it does not draw bus
  power from the Cardputer — good, because the Cardputer can't supply much.
- Because the single ESP32-S3 USB-OTG controller is busy being a *host*, it
  **cannot** also be a USB CDC serial device. There's no USB serial monitor
  while this runs — status is shown on the Cardputer screen instead.

## Build & flash

Requires [PlatformIO](https://platformio.org/).

```bash
pio run -e m5stack-cardputer            # build
pio run -e m5stack-cardputer -t upload  # flash over USB-C
```

> First build downloads the arduino-esp32 3.x toolchain, ESP-IDF 5.3, and the
> FTDI USB-host components — it takes a few minutes. Subsequent builds are fast.

To flash, the Cardputer's USB-C must be a *device* (normal). After flashing,
unplug from the PC and connect the C330 with the host cable.

### Why not the stock M5Stack PlatformIO config?

The M5Stack docs suggest `platform = espressif32@6.7.0` +
`ARDUINO_USB_CDC_ON_BOOT=1`. Two changes were required for USB-host-to-FTDI and
are documented inline in `platformio.ini`:

1. **Platform** → pioarduino (arduino-esp32 3.1.3 / IDF 5.3). The FTDI USB-host
   driver is an IDF 5.x component; `espressif32@6.7.0` is IDF 4.4 and lacks it.
2. **`ARDUINO_USB_CDC_ON_BOOT=0`** — one USB-OTG controller can't be a CDC
   serial *device* and a USB *host* at the same time.

Everything else (board `esp32-s3-devkitc-1`, `upload_speed=1500000`, debug
level, M5Cardputer from GitHub) matches the M5Stack-recommended config.

## Usage

1. Power on the C330, press **CLEAR** to finish its init cycle until it shows
   **READY** (manual §5).
2. Make sure the C330 already has **format 0** stored (a plain text print uses
   it). To (re)define a layout, send a line starting with `<]F0 ...` — see
   manual §7. The firmware sends only the text; it doesn't push a format.
3. Connect the C330 to the Cardputer with the host cable. The screen shows
   **USB: connected (57600)**.
4. Type your message, press **ENTER**. The plate feeds and embosses.

## Files

| Path | What |
| --- | --- |
| `platformio.ini` | Build config (USB-host deviations documented inline) |
| `src/main.cpp` | Keyboard UI + USB-host FTDI bridge to the C330 |
| `src/idf_component.yml` | ESP-IDF USB-host components (FTDI/CP210x/CH34x) |
| `docs/` | C330 Operator Manual + Cardputer docs link |
