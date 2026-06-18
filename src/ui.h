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

// Sink for the framed on-wire payload. Returns true on success.
//   device: writes the bytes to the C330 over USB
//   sim:    prints the bytes to the terminal
using SendFn = bool (*)(const char *data, unsigned len);

struct UiState {
    std::string buffer;          // text being typed
    unsigned    cursor    = 0;   // insertion index within buffer (for Left/Right)
    std::string status;          // bottom status line
    bool        connected = false;
    SendFn      send      = nullptr;
};

// Wrap text for the C330: everything between '<' and '>' is embossed.
std::string ui_frame_text(const std::string &text);

void ui_init(UiState &s, SendFn send, const char *status);

// Update the connection indicator. Returns true if it changed (redraw needed).
bool ui_set_connected(UiState &s, bool connected);

// Apply one input event. Returns true if the UI changed and should be redrawn.
bool ui_handle_input(UiState &s, const InputEvent &ev);

// Draw the whole UI.
void ui_render(lgfx::LGFX_Device &d, const UiState &s);
