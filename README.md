# Cardputer → C330 embosser

Type a message on the **M5Stack Cardputer** keyboard, press **ENTER**, and it is
embossed on a metal plate by a **Matica Fintec C330** metal-plate embosser.

The Cardputer's USB-C port runs in **USB host** mode and drives the C330's
built-in **FTDI** USB-serial chip directly — i.e. you plug the C330 into the
Cardputer with a USB host/OTG cable, no PC in the loop.

**Air-gapped by design.** The only thing this firmware ever drives is USB-serial
to the C330. There is no network path: every cloud / radio / network component
is stripped from the build, the Bluetooth controller is disabled, and WiFi is
never initialized. The linked image contains **0** WiFi symbols and **0**
Bluetooth/BLE symbols (verified with `nm` on `firmware.elf`), so the radios
cannot be brought up at all.

## How it works

The C330 emboss-prints whatever text sits **between `<` and `>`**, positioned by
a *layout format* (see the Operator Manual, `docs/`). Rather than depend on a
format stored in the machine, this firmware **pushes its own "format 0" on every
connect**, then sends the text. So the wire traffic for a hello-world is:

```
<]F0 U0 SY540 SX860          <- layout: 54.0 x 86.0 mm plate (pushed on connect)
Y300 X100 F0 CI10 >          <- one line at 30.0/10.0 mm, font 0, 10 cpi
<HELLO WORLD>                <- your text, maps to the line above
```

Firmware flow:

1. `M5Cardputer` drives the keyboard + screen — you type a line.
2. The ESP32-S3 USB host stack (`usb_host_vcp` + `usb_host_ftdi`) opens the
   C330's FTDI port and configures it to the C330's serial settings:
   **57600 baud, 8 data bits, no parity, 1 stop bit** (manual §6.2).
3. On connect it pushes the layout format above (the `C330_FORMAT` constant in
   `src/main.cpp` — edit it for your plate size / line position).
4. On ENTER, the text is framed as `<text>\r\n` and written to the port.

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
2. Connect the C330 to the Cardputer with the host cable. On connect the
   firmware pushes its layout format automatically, then the screen shows
   **USB: connected (57600)**. No format needs to be pre-stored in the machine.
3. Type your message, press **ENTER**. The plate feeds and embosses.

## Files

| Path | What |
| --- | --- |
| `platformio.ini` | Build config + air-gap component removals (commented inline) |
| `sdkconfig.defaults` | IDF settings: 1000 Hz tick, C++ exceptions, Arduino autostart, BT off |
| `src/main.cpp` | Keyboard UI + USB-host FTDI bridge to the C330 |
| `src/idf_component.yml` | ESP-IDF USB-host components (FTDI/CP210x/CH34x) |
| `docs/` | C330 Operator Manual + Cardputer docs link |
