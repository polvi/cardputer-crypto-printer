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
#include <vector>
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
    Print,      // the dedicated print button (device G0 / sim Space)
};

struct InputEvent {
    InputKey key = InputKey::None;
    char     ch  = 0; // valid when key == InputKey::Char
};

// Sink for the on-wire payload to the printer. Returns true on success.
//   device: writes the bytes to the C330 over USB
//   sim:    prints the bytes to the terminal
using SendFn = bool (*)(const char *data, unsigned len);

// The printer flow. Wallet path: Select -> Label -> Confirm -> Printing -> Result.
// Custom path:  Select -> Custom -> Copies -> Printing -> Result.
// Printing is transient while plates stream.
enum class Screen { Select, Label, Confirm, Custom, Copies, Printing, Result };

// CUSTOM (free-form text) isn't a wallet — it routes to custom_print, never to
// wallet_print. SEED12 is a generic labeled 12-word BIP39 seed: key material
// only, no derived addresses (Result shows a plain confirmation, no QR).
enum class WalletType { BTC, ETH, BTC_ETH, XMR, SEED12, CUSTOM };

// Human-readable name ("BTC", "ETH", "BTC+ETH", "XMR", "Seed", "Custom").
const char *wallet_name(WalletType w);

// Max characters for the optional label.
static constexpr unsigned LABEL_MAX = 24;

// Custom card: a few free-form lines of up to 26 chars (the C330 line limit).
static constexpr unsigned CUSTOM_MAX_LINES = 4;

// Custom print: how many identical cards to emboss (the hopper auto-feeds each).
static constexpr unsigned COPIES_MAX = 99;

// One public key to display (a wallet may yield several, e.g. BTC+ETH -> 2).
struct PubKey {
    std::string chain; // short label, e.g. "BTC"
    std::string key;   // address / public key text
};

struct UiState {
    // --- existing, reused ---
    std::string buffer;          // REUSED as the label text (Label screen)
    unsigned    cursor    = 0;   // REUSED as the label caret
    std::string status;          // bottom status line
    bool        connected = false; // printer (C330) attached
    int         battery   = -1;   // battery %, 0-100 (-1 = unknown, e.g. the sim)
    bool        charging  = false; // battery is charging
    SendFn      send      = nullptr;

    // --- screen machine ---
    Screen      screen = Screen::Select;
    WalletType  wallet = WalletType::BTC_ETH;

    // --- Custom "Copies" screen ---
    unsigned copies = 1;          // number of identical Custom cards to print
    bool     copies_typing = false; // next digit replaces the default vs appends

    // --- result: PUBLIC keys only. No private-key field ever lives here. ---
    std::vector<PubKey> pubkeys;
};

void ui_init(UiState &s, SendFn send, const char *status);

// Update the connection indicator. Returns true if it changed (redraw needed).
bool ui_set_connected(UiState &s, bool connected);

// Update the battery gauge (level 0-100, or <0 if unknown). Returns true if it
// changed enough to warrant a redraw.
bool ui_set_battery(UiState &s, int level, bool charging);

// Apply one input event. Returns true if the UI changed and should be redrawn.
bool ui_handle_input(UiState &s, const InputEvent &ev);

// True while a print has been requested but not yet run. The front-end should
// render once (to show "Printing…"), then call ui_run_print().
bool ui_pending_print(const UiState &s);

// Perform the (blocking) plate stream: generate the wallet, emboss all plates,
// then transition to Result (or back to Confirm on failure). Returns true.
bool ui_run_print(UiState &s);

// Draw the whole UI.
void ui_render(lgfx::LGFX_Device &d, const UiState &s);
