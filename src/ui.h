// Hardware-independent UI for the C330 keyboard.
//
// The same UiState + render + input logic runs on two targets:
//   - the real Cardputer (env:m5stack-cardputer), driven by the M5 keyboard
//     and drawing to M5Cardputer.Display (an M5GFX)
//   - a desktop SDL window (env:sim), driven by the host keyboard and drawing
//     to an SDL-backed lgfx display
//
// Both M5GFX and the SDL display derive from lgfx::LGFX_Device, so ui_render()
// takes that common base and neither knows which target it is on.
#pragma once

#include <M5GFX.h>
#include <string>
#include <cstdint>

// One input event, target-agnostic. The device/sim front-ends translate their
// native key events into these. Arrow keys are first-class so the UI can grow
// into multi-field editing / history without changing the event model.
enum class InputKey {
    None,
    Char,       // a printable character (see InputEvent::ch)
    Enter,
    Backspace,
    Left,
    Right,
    Up,
    Down,
    Esc,
};

struct InputEvent {
    InputKey key = InputKey::None;
    char     ch  = 0; // valid when key == InputKey::Char
};

// Sink for the on-wire payload to the printer. Returns true on success.
//   device: writes the bytes to the C330 over USB
//   sim:    prints the bytes to the terminal
using SendFn = bool (*)(const char *data, unsigned len);

// The crypto wallet printer flow. A linear screen stack:
//   Select -> Label -> Hold -> Result   (forward via select/Enter/hold; back via Esc)
enum class Screen { Select, Label, Hold, Result };

enum class WalletType { BTC, ETH, BTC_ETH, XMR };

// Human-readable wallet name ("BTC", "ETH", "BTC+ETH", "XMR").
const char *wallet_name(WalletType w);

// How long the print button must be held, in milliseconds.
static constexpr uint32_t HOLD_MS = 5000;

// Max characters for the optional label.
static constexpr unsigned LABEL_MAX = 24;

struct UiState {
    // --- existing, reused ---
    std::string buffer;          // REUSED as the label text (Label screen)
    unsigned    cursor    = 0;   // REUSED as the label caret
    std::string status;          // bottom status line
    bool        connected = false; // printer (C330) attached
    SendFn      send      = nullptr;

    // --- screen machine ---
    Screen      screen = Screen::Select;
    WalletType  wallet = WalletType::BTC;

    // --- hold-to-print timing (the caller supplies the clock; the core never
    //     reads a clock itself, so it stays hardware-independent) ---
    bool        holdActive  = false;
    uint32_t    holdStartMs = 0;
    uint32_t    nowMs       = 0;
    bool        printed     = false;

    // --- result: PUBLIC key only. No private-key field ever lives here. ---
    std::string pubkey;
};

void ui_init(UiState &s, SendFn send, const char *status);

// Update the connection indicator. Returns true if it changed (redraw needed).
bool ui_set_connected(UiState &s, bool connected);

// Apply one input event. Returns true if the UI changed and should be redrawn.
bool ui_handle_input(UiState &s, const InputEvent &ev);

// Feed the print-button state + current clock (ms). Drives the hold timer:
// starts on press, cancels on early release, completes (prints) at HOLD_MS.
// No-op unless on the Hold screen. Returns true if a redraw is needed.
bool ui_set_print_button(UiState &s, bool pressed, uint32_t now_ms);

// Advance the clock so the progress bar repaints and completion is checked even
// when the button state did not change. No-op off the Hold screen.
bool ui_tick(UiState &s, uint32_t now_ms);

// Draw the whole UI.
void ui_render(lgfx::LGFX_Device &d, const UiState &s);
