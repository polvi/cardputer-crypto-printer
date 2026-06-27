// Desktop simulator for the Cardputer C330 UI.
//
// Renders the shared UI (ui.cpp) into an SDL window at the Cardputer's 240x135
// resolution (scaled up) and drives it from the host PC keyboard, including
// arrow keys. "Sending" prints the framed C330 payload to the terminal instead
// of talking to real hardware.
//
//   pio run -e sim -t exec      # build + run the window
//
// Only compiled for env:sim (build_src_filter); the body is additionally guarded
// on SDL so it is inert anywhere SDL is absent.
#include <M5GFX.h>
#include "ui.h"

#if defined(SDL_h_)

#include <mutex>
#include <queue>
#include <cstdio>

// An lgfx display backed by SDL at the Cardputer's native resolution. `scale`
// just zooms the window so it's comfortable on a desktop.
class SimDisplay : public lgfx::LGFX_Device {
    lgfx::Panel_sdl _panel;

public:
    SimDisplay(int w, int h, int scale) {
        auto cfg = _panel.config();
        cfg.memory_width  = cfg.panel_width  = w;
        cfg.memory_height = cfg.panel_height = h;
        _panel.config(cfg);
        _panel.setScaling(scale, scale);
        setPanel(&_panel);
    }
};

static SimDisplay        g_display(240, 135, 4);
static UiState           g_ui;
static std::queue<InputEvent> g_events;
static std::mutex        g_mtx;

static void push_event(const InputEvent &ev) {
    std::lock_guard<std::mutex> lk(g_mtx);
    g_events.push(ev);
}

// Print the framed bytes the device would send over USB to the C330.
static bool sim_send(const char *data, unsigned len) {
    std::fputs("TX -> C330: ", stdout);
    std::fwrite(data, 1, len, stdout);
    std::fflush(stdout);
    return true;
}

// The Cardputer has no arrow keys: Fn + ; , . / is the arrow cluster. macOS
// doesn't expose the hardware Fn key to SDL, so in the sim we use Option (Alt)
// as the Fn stand-in. (Desktop arrow keys also work, for convenience.)
static bool g_fn_held = false; // Alt held -> acts as Cardputer Fn

// Observe every SDL event without consuming it from M5GFX's own event pump.
// SDL_TEXTINPUT gives us shifted/layout-correct characters; SDL_KEYDOWN gives
// us the non-text keys (Enter, Backspace, arrows, Esc).
static int SDLCALL event_watch(void *, SDL_Event *e) {
    switch (e->type) {
        case SDL_KEYUP:
            if (e->key.keysym.sym == SDLK_LALT || e->key.keysym.sym == SDLK_RALT) {
                g_fn_held = false;
            }
            return 0;

        case SDL_TEXTINPUT:
            if (g_fn_held) return 0; // Fn chord held: don't type the character
            for (const char *p = e->text.text; *p; ++p) {
                if ((unsigned char)*p >= 0x20) push_event({InputKey::Char, *p}); // incl. space
            }
            return 0;

        case SDL_KEYDOWN:
            break; // handled below

        default:
            return 0;
    }

    if (e->key.keysym.sym == SDLK_LALT || e->key.keysym.sym == SDLK_RALT) {
        g_fn_held = true;
        return 0;
    }

    const bool fn = g_fn_held || (e->key.keysym.mod & KMOD_ALT);
    InputEvent ev;
    switch (e->key.keysym.sym) {
        // Cardputer Fn + ; , . / arrow cluster (Fn = Option in the sim)
        case SDLK_SEMICOLON: if (!fn) return 0; ev = {InputKey::Up, 0};    break;
        case SDLK_PERIOD:    if (!fn) return 0; ev = {InputKey::Down, 0};  break;
        case SDLK_COMMA:     if (!fn) return 0; ev = {InputKey::Left, 0};  break;
        case SDLK_SLASH:     if (!fn) return 0; ev = {InputKey::Right, 0}; break;
        // Desktop arrow keys, for convenience
        case SDLK_UP:        ev = {InputKey::Up, 0};        break;
        case SDLK_DOWN:      ev = {InputKey::Down, 0};      break;
        case SDLK_LEFT:      ev = {InputKey::Left, 0};      break;
        case SDLK_RIGHT:     ev = {InputKey::Right, 0};     break;
        // Cmd+Enter = the print button (G0 stand-in); plain Enter = newline/advance.
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            ev = {(e->key.keysym.mod & KMOD_GUI) ? InputKey::Print : InputKey::Enter, 0};
            break;
        case SDLK_BACKSPACE: ev = {InputKey::Backspace, 0}; break;
        case SDLK_ESCAPE:    ev = {InputKey::Esc, 0};       break;
        default:             return 0;
    }
    push_event(ev);
    return 0;
}

void setup(void) {
    g_display.init();

    // M5GFX's SDL panel has built-in debug shortcuts that default to NO modifier:
    // plain 'r'/'l' rotate the window and '1'-'6' rescale it (Panel_sdl.cpp).
    // Those collide with normal typing, so require Cmd to be held for them.
    lgfx::Panel_sdl::setShortcutKeymod(KMOD_GUI);

    SDL_StartTextInput();
    SDL_AddEventWatch(event_watch, nullptr);

    ui_init(g_ui, sim_send, nullptr);
    ui_set_connected(g_ui, true);    // pretend the printer is attached
    ui_set_battery(g_ui, 80, true); // fake gauge (charging) so the chrome shows in the sim
    ui_render(g_display, g_ui);
}

void loop(void) {
    bool dirty = false;
    for (;;) {
        InputEvent ev;
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            if (g_events.empty()) break;
            ev = g_events.front();
            g_events.pop();
        }
        if (ui_handle_input(g_ui, ev)) dirty = true;
    }

    if (dirty) ui_render(g_display, g_ui);

    // Run a requested print after the "Printing…" frame is on screen.
    if (ui_pending_print(g_ui)) {
        ui_run_print(g_ui);
        ui_render(g_display, g_ui);
    }
    SDL_Delay(16);
}

__attribute__((weak))
int user_func(bool *running) {
    setup();
    do {
        loop();
    } while (*running);
    return 0;
}

int main(int, char **) {
    return lgfx::Panel_sdl::main(user_func, 16);
}

#endif // SDL_h_
