# Cardputer crypto wallet printer

An **M5Stack Cardputer** that generates a crypto wallet and prints the key
material to an attached printer (a **Matica Fintec C330**), while the **public
key** is shown on screen for capture. **Private key material is never retained.**

The Cardputer's USB-C port runs in **USB host** mode and drives the printer's
**FTDI** USB-serial chip directly — plug the printer into the Cardputer with a
USB host/OTG cable, no PC in the loop.

> Status: the **device UI flow** is built. The actual key generation and the
> printer payload format are **stubbed** behind a clean seam (`src/wallet.cpp`)
> and dropped in later — see "Crypto / printer seam" below.

**Air-gapped by design.** The only thing this firmware ever drives is USB-serial
to the printer. There is no network path: every cloud / radio / network component
is stripped from the build, the Bluetooth controller is disabled, and WiFi is
never initialized. The linked image contains **0** WiFi symbols and **0**
Bluetooth/BLE symbols (verified with `nm` on `firmware.elf`), so the radios
cannot be brought up at all — critical for a device that handles key material.

## The flow

A screen state machine (`src/ui.cpp`), hardware-independent so it runs on both
the device and the desktop simulator:

1. **Select** — choose a wallet type with a number key: `1` BTC, `2` ETH,
   `3` BTC+ETH, `4` XMR. (Crypto is BTC+ETH for now; see status below.)
2. **Label** — optionally type a label (max 24 chars), editable with the arrow
   caret. Enter continues, Esc goes back.
3. **Confirm** — shows the chosen wallet type and label. **Press the top G0
   button once** and the whole wallet is printed (Esc goes back to edit).
4. **Printing** — generates the wallet and streams every plate to the C330.
5. **Result** — the **public addresses** are shown as **QR codes** (BTC + ETH,
   side by side) for capture. Any key wipes them and returns to Select.

### What gets printed (`src/c330_format.{h,cpp}`)

One G0 press emboss-prints the full wallet as a stack of metal plates, in the
exact format and order of the reference tool `docs/keyprint.go` (verified
byte-for-byte against it):

1. **Derivation/BIP info** (`]F0`): "POLVI HD WALLET / 24 WORD BIP32 MNEMONIC /
   BTC BIP84 PATH M/84'/0'/0'/0/0 / ETH BIP44 PATH M/44'/60'/0'/0/0".
2. **Mnemonic words 13-24** (`]F2`) and **1-12** (`]F1`) — two words per line.
3. **Public-key page** (`]F3`): "MINTED ON <date>" + the BTC and ETH addresses.

The C330 drum is uppercase-only (≤26 chars/line); mixed-case addresses are
embossed by stacking two rows 7 units apart, so case is recovered by vertical
position (`mx_message` in `c330_format.cpp`).

### Security seam (`src/wallet.h` / `wallet.cpp`)

`wallet_print(type, label, sink, out_public)` is the boundary: the **mnemonic**
(the printed secret) is generated, composed into the plate payloads, streamed to
the printer, and `secure_zero`'d before the call returns. It never enters
`UiState`, is never shown on screen, and never persists across interactions; only
the public addresses come back for the QR screen.

> **Key-generation status.** The C330 **print format is complete and verified**.
> Real HD key generation (trezor-crypto on-device) is **not yet wired**: the
> simulator (and the unflashed device) use a fixed BIP39 test mnemonic with
> placeholder addresses so the format/flow are exercisable. The trezor-crypto
> vendoring closure was resolved (BTC+ETH only: `USE_BIP32_25519_CURVES=0`,
> precomputed `.table`s), but its EC math hung under native verification and was
> not shippable unverified — so it lives behind the `WALLET_REAL_CRYPTO` seam
> pending resolution + on-hardware verification. SAR-ADC entropy
> (`bootloader_random_enable`) wires in at the same point.

## Printer link

1. The ESP32-S3 USB host stack (`usb_host_vcp` + `usb_host_ftdi`) opens the
   C330's FTDI port and configures it to **57600 8N1** (manual §6.2).
2. On print, all plates stream back-to-back. The firmware honors the C330's
   **Xon/Xoff** (`main.cpp`): it pauses on XOFF (0x13) and resumes on XON (0x11)
   so a multi-plate stream never overflows the machine's buffer (error E64).
   Each plate carries its own `]Fn` layout, so no format pre-load is needed.

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

## Develop the UI on your Mac (simulator)

The UI is hardware-independent (`src/ui.cpp`) and also builds as a native macOS
app via M5GFX's SDL2 backend, so you can iterate on layout/flow without a
Cardputer or a C330. It renders at the real 240×135 resolution (scaled up) and
reads your Mac keyboard, **including arrow keys**.

```bash
brew install sdl2          # one-time
pio run -e sim -t exec     # build + open the window
```

Walk the whole flow in the window:

| Key (sim) | Action |
| --- | --- |
| `1`–`4` | choose wallet type (Select screen) |
| typing | edit the label (max 24); caret moves with the arrows |
| **⌥ + `;,./`** or desktop arrows | move the caret |
| **Enter** | Label → Confirm |
| **Space** | the print button — one press = print (stands in for the device G0) |
| **Esc** | go back a screen |

On print, the stub payload prints to the terminal as `TX -> C330: …` and the
Result screen shows the QR(s) + public key(s). The same `ui_*` code runs
unchanged on the device.

**Arrow keys.** The real Cardputer has no arrow keys — it uses **Fn + `;`(up)
`,`(left) `.`(down) `/`(right)**. macOS can't expose the hardware Fn key to SDL,
so the sim uses **Option (⌥) as Fn**. Likewise the **G0 print button** maps to
**Space** in the sim (so Space is not a typeable label character there).

> The simulator covers the **screen + keyboard UI** only. The USB-host→FTDI link
> to the printer has no emulator and is exercised on real hardware (or a bench
> USB-serial adapter).

## Usage (device)

1. Power on the C330 printer, press **CLEAR** until it shows **READY** (manual §5).
2. Connect it to the Cardputer with the host cable. The connection dot turns green.
3. Press `1`–`4` to pick a wallet type, optionally type a label, review the
   **Confirm** screen, then **press the top button once** to generate + print.
   Capture the public-key QR(s) on the result screen. Press any key to wipe and
   start over.

## Files

| Path | What |
| --- | --- |
| `platformio.ini` | Two envs: `m5stack-cardputer` (firmware) + `sim` (desktop UI) |
| `sdkconfig.defaults` | IDF settings: 1000 Hz tick, C++ exceptions, Arduino autostart, BT off |
| `src/ui.h` / `src/ui.cpp` | Hardware-independent UI: screen state machine, input, rendering |
| `src/c330_format.h` / `.cpp` | C330 plate format ported from `keyprint.go` (layout, case-stacking) |
| `src/wallet.h` / `src/wallet.cpp` | Security seam: generate → emboss → zeroize; key-gen behind `WALLET_REAL_CRYPTO` |
| `src/main.cpp` | Device front-end: keyboard + G0 button + USB-host FTDI bridge + Xon/Xoff |
| `src/sim_main.cpp` | Desktop front-end: SDL window + Mac keyboard + Space=print |
| `src/idf_component.yml` | ESP-IDF USB-host components (FTDI/CP210x/CH34x) |
| `docs/keyprint.go` | Reference Go implementation (key derivation + C330 format) |
| `docs/` | C330 Operator Manual + Cardputer docs link |
