# CLAUDE.md

Guidance for working in this repo. Read `README.md` for the full product story; this
file is the short, high-signal version plus the non-obvious rules.

## What this is

Firmware for an **M5Stack Cardputer (ESP32-S3)** that generates HD crypto wallets
(BTC+ETH, XMR) and emboss-prints the seed/keys onto metal plates via a **Matica
Fintec C330** embosser over **USB-host → the printer's FTDI** chip. Public addresses
are shown on-screen as QR codes. **Air-gapped; private key material is never
retained.**

## Build / run (PlatformIO)

```bash
pio run -e sim                      # build the desktop UI simulator (SDL2; needs `brew install sdl2`)
pio run -e sim -t exec              # build + open the sim window
pio run -e m5stack-cardputer        # build the device firmware
pio run -e m5stack-cardputer -t upload   # build + flash (USB-C as a device; auto-detect port)
```

**Always build BOTH envs after touching shared code (`ui.cpp`, `wallet.cpp`,
`c330_format.cpp`).** The `clang`/clangd diagnostics in-editor are noise (missing
ESP-IDF/M5GFX headers) — trust the `pio run` result.

## Architecture

- `src/ui.{h,cpp}` — **hardware-independent** screen state machine + input + render.
  Runs unchanged on the device and the sim. Talks to the world only through
  `SendFn` (a byte sink) and `UiState`.
- `src/main.cpp` — **device** front-end: M5 keyboard + G0 button + battery, and the
  USB-host↔FTDI bridge to the printer.
- `src/sim_main.cpp` — **desktop** front-end: SDL window + Mac keyboard.
- `src/wallet.{h,cpp}` — the **security seam**. `wallet_print()` generates the
  secret, composes plates, streams them, and `secure_zero`s everything before
  returning. Only public addresses come back.
- `src/wallet_crypto.cpp` — real on-device key derivation (behind
  `-DWALLET_REAL_CRYPTO`, device only) using vendored `lib/trezor-crypto`. The sim
  uses fixed test vectors.
- `src/c330_format.{h,cpp}` — the C330 plate byte format. **Must stay byte-for-byte
  faithful to `docs/keyprint.go`** (the proven reference tool).
- `docs/keyprint.go` — reference implementation; `docs/C330_*` — printer manuals.

## Non-obvious rules (learned the hard way)

1. **Air-gap is sacred.** Never add networking/radio/Bluetooth. The build strips all
   of it; keep it that way.
2. **Secrets never leak.** Private/mnemonic material lives only inside
   `wallet_print()`, is zeroized there, and must never enter `UiState`, the screen,
   or logs.
3. **Match `keyprint.go`.** The C330 stream is `F0` info, `F1` word cards (re-emitted
   per card), `F3` address — exact formats, exact order (XMR words are sent
   **descending**: 21-25, 11-20, 1-10). Verify crypto/format changes natively
   against `docs/keyprint.go` before trusting them.
4. **Printer transport — do NOT retry a timed-out `tx_blocking` chunk.** cdc_acm's
   timeout resets the endpoint, which *completes* the in-flight transfer, so a
   resend duplicates bytes and corrupts the stream → the C330 throws **E37**. Flow
   control is the **FTDI chip's hardware Xon/Xoff** (enabled at connect in
   `main.cpp` via `SET_FLOW_CTRL`); `tx_blocking` just blocks on the full FIFO with a
   long timeout. Don't reintroduce software Xon/Xoff or per-chunk retries.
5. **C-string safety.** The original E37 was an *unterminated* address buffer: a
   trezor base58 encoder writes the chars but doesn't NUL-terminate, and
   `out = addr` read past the end into stack garbage → over-long field → E37. **Use
   the length the encoder returns** (`out.assign(addr, n)`), don't rely on
   termination. Watch for this pattern in any new encoder use.
6. **C330 field limit ≈ 32 chars/line** at X100/CI10; lines are uppercase-only and
   mixed case (addresses) is faked by stacking two rows 7 units apart (`mx_message`).
7. **UI is shared.** Device-specific keyboard quirks live in `main.cpp` (e.g. the
   wide space bar lands in `st.word` repeatedly — emit one space via `st.space`;
   Esc/arrows are the Fn layer reported as `st.esc/up/down/left/right`). The sim's
   SDL path is separate. Platform-specific prompt text uses the **`UI_PRINT_KEY`**
   macro (set per env in `platformio.ini`: "G0" device / "Cmd+Enter" sim).

## Testing the printer without hardware reflash

The C330 can be driven from a PC over its FTDI serial port (e.g.
`/dev/cu.usbserial-*`, 9600 8N1, Xon/Xoff) with a small `pyserial` script — useful
for reproducing/bisecting C330 behavior with known bytes. The sim has **no** printer
emulator; the USB-host→FTDI link is only exercised on real hardware or such a bench
setup.

## Commits

End commit messages with:

```
Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
```

Branch before committing if on `main`; only commit/push when asked.
