#include "ui.h"
#include "wallet.h"

#include <cstdio>

// ============================================================================
// Wallet names
// ============================================================================
const char *wallet_name(WalletType w) {
    switch (w) {
        case WalletType::BTC:     return "BTC";
        case WalletType::ETH:     return "ETH";
        case WalletType::BTC_ETH: return "BTC+ETH";
        case WalletType::XMR:     return "XMR";
    }
    return "?";
}

// ============================================================================
// State setup
// ============================================================================
void ui_init(UiState &s, SendFn send, const char *status) {
    s.send      = send;
    s.status    = status ? status : "Press 1-4 to choose";
    s.buffer.clear();
    s.cursor    = 0;
    s.connected = false;
    s.screen    = Screen::Select;
    s.wallet    = WalletType::BTC;
    s.holdActive  = false;
    s.holdStartMs = 0;
    s.nowMs       = 0;
    s.printed     = false;
    s.pubkey.clear();
}

bool ui_set_connected(UiState &s, bool connected) {
    if (s.connected == connected) return false;
    s.connected = connected;
    return true; // just repaint the connection indicator
}

static void reset_hold(UiState &s) {
    s.holdActive  = false;
    s.holdStartMs = 0;
    s.printed     = false;
}

// ============================================================================
// Input handling (per-screen)
// ============================================================================
static bool handle_select(UiState &s, const InputEvent &ev) {
    if (ev.key != InputKey::Char) return false;
    WalletType w;
    switch (ev.ch) {
        case '1': w = WalletType::BTC;     break;
        case '2': w = WalletType::ETH;     break;
        case '3': w = WalletType::BTC_ETH; break;
        case '4': w = WalletType::XMR;     break;
        default:  return false;
    }
    s.wallet = w;
    s.buffer.clear();
    s.cursor = 0;
    s.screen = Screen::Label;
    s.status = "ENTER=continue  ESC=back";
    return true;
}

static bool handle_label(UiState &s, const InputEvent &ev) {
    switch (ev.key) {
        case InputKey::Char:
            if (s.buffer.size() >= LABEL_MAX) return false; // 24-char cap
            s.buffer.insert(s.buffer.begin() + s.cursor, ev.ch);
            s.cursor++;
            return true;

        case InputKey::Backspace:
            if (s.cursor == 0) return false;
            s.buffer.erase(s.buffer.begin() + (s.cursor - 1));
            s.cursor--;
            return true;

        case InputKey::Left:
            if (s.cursor == 0) return false;
            s.cursor--;
            return true;

        case InputKey::Right:
            if (s.cursor >= s.buffer.size()) return false;
            s.cursor++;
            return true;

        case InputKey::Up: // Home
            if (s.cursor == 0) return false;
            s.cursor = 0;
            return true;

        case InputKey::Down: // End
            if (s.cursor == s.buffer.size()) return false;
            s.cursor = s.buffer.size();
            return true;

        case InputKey::Enter: // label is optional; may be empty
            s.screen = Screen::Hold;
            reset_hold(s);
            s.status = "Hold top button 5s";
            return true;

        case InputKey::Esc:
            s.screen = Screen::Select;
            s.buffer.clear();
            s.cursor = 0;
            s.status = "Press 1-4 to choose";
            return true;

        default:
            return false;
    }
}

static bool handle_hold(UiState &s, const InputEvent &ev) {
    if (ev.key == InputKey::Esc) {
        s.screen = Screen::Label;
        reset_hold(s);
        s.status = "ENTER=continue  ESC=back";
        return true;
    }
    return false; // the hold gesture is the button, not a key
}

static bool handle_result(UiState &s, const InputEvent &ev) {
    if (ev.key == InputKey::None) return false;
    // Any key: wipe the displayed key and start over.
    s.pubkey.clear();
    s.pubkey.shrink_to_fit();
    s.buffer.clear();
    s.cursor = 0;
    reset_hold(s);
    s.screen = Screen::Select;
    s.status = "Press 1-4 to choose";
    return true;
}

bool ui_handle_input(UiState &s, const InputEvent &ev) {
    switch (s.screen) {
        case Screen::Select: return handle_select(s, ev);
        case Screen::Label:  return handle_label(s, ev);
        case Screen::Hold:   return handle_hold(s, ev);
        case Screen::Result: return handle_result(s, ev);
    }
    return false;
}

// ============================================================================
// Hold-to-print timing
// ============================================================================
// Generate + print, then move to the result screen. Private material lives only
// inside generate_and_print_wallet() and is zeroized there; we keep only pubkey.
static void do_print(UiState &s) {
    if (!s.connected) {
        s.status     = "Printer not connected";
        s.holdActive = false; // require a fresh press once connected
        return;
    }
    WalletPublic pub;
    if (generate_and_print_wallet(s.wallet, s.buffer, s.send, pub)) {
        s.pubkey  = pub.pubkey;
        s.printed = true;
        s.screen  = Screen::Result;
        s.status  = "Any key: new wallet";
    } else {
        s.status     = "Print FAILED";
        s.holdActive = false;
    }
}

bool ui_set_print_button(UiState &s, bool pressed, uint32_t now_ms) {
    if (s.screen != Screen::Hold) return false;
    s.nowMs = now_ms;
    bool changed = false;

    if (pressed && !s.holdActive && !s.printed) {
        s.holdActive  = true;
        s.holdStartMs = now_ms;
        s.status      = "Keep holding...";
        changed = true;
    } else if (!pressed && s.holdActive) {
        s.holdActive = false;
        if (!s.printed) s.status = "Cancelled";
        changed = true;
    }

    if (s.holdActive && !s.printed && (now_ms - s.holdStartMs) >= HOLD_MS) {
        do_print(s);
        changed = true;
    }
    return changed;
}

bool ui_tick(UiState &s, uint32_t now_ms) {
    if (s.screen != Screen::Hold) return false;
    s.nowMs = now_ms;
    if (s.holdActive && !s.printed && (now_ms - s.holdStartMs) >= HOLD_MS) {
        do_print(s);
        return true;
    }
    return s.holdActive; // keep the progress bar animating while held
}

// ============================================================================
// Rendering (per-screen) + shared chrome
// ============================================================================
static void render_select(lgfx::LGFX_Device &d, const UiState &) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print("Crypto Wallet");

    static const char *opts[4] = {"1 BTC", "2 ETH", "3 BTC+ETH", "4 XMR"};
    d.setTextSize(2);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    for (int i = 0; i < 4; ++i) {
        d.setCursor(8, 30 + i * 22);
        d.print(opts[i]);
    }
}

static void render_label(lgfx::LGFX_Device &d, const UiState &s) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print(wallet_name(s.wallet));
    d.print(" label");

    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 26);
    char buf[40];
    snprintf(buf, sizeof(buf), "optional  (%u/%u)", (unsigned)s.buffer.size(), LABEL_MAX);
    d.print(buf);

    const int size = 2, cw = 6 * size, chh = 8 * size, x0 = 4, y0 = 46;
    d.setTextSize(size);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(x0, y0);
    d.print("> ");
    d.print(s.buffer.c_str());
    int caretX = x0 + (2 + (int)s.cursor) * cw;
    d.fillRect(caretX, y0, 2, chh, TFT_YELLOW);
}

static void render_hold(lgfx::LGFX_Device &d, const UiState &s) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print("Print ");
    d.print(wallet_name(s.wallet));

    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 26);
    d.print("Label: ");
    d.print(s.buffer.empty() ? "(none)" : s.buffer.c_str());

    d.setCursor(4, 42);
    d.print("HOLD top button 5s");

    // progress bar
    const int bx = 4, by = 60, bw = d.width() - 8, bh = 20;
    d.drawRect(bx, by, bw, bh, TFT_WHITE);
    uint32_t elapsed = (s.holdActive && s.nowMs >= s.holdStartMs) ? (s.nowMs - s.holdStartMs) : 0;
    if (elapsed > HOLD_MS) elapsed = HOLD_MS;
    int fill = (int)((uint64_t)(bw - 2) * elapsed / HOLD_MS);
    d.fillRect(bx + 1, by + 1, fill, bh - 2, TFT_GREEN);
}

static void render_result(lgfx::LGFX_Device &d, const UiState &s) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(4, 3);
    d.print("PUBLIC KEY - ");
    d.print(wallet_name(s.wallet));

    if (s.pubkey.empty()) return;

    // QR on the left for scanning
    const int qr = 100;
    d.qrcode(s.pubkey.c_str(), 4, 16, qr, 1, true);

    // Key text to the right, manually wrapped into the remaining width.
    // (Newlines in the key, e.g. BTC+ETH, are flattened to spaces for layout.)
    const int tx = 4 + qr + 6;
    const int cols = (d.width() - tx) / 6;
    const int bottom = d.height() - 14;
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    std::string flat = s.pubkey;
    for (char &c : flat) if (c == '\n') c = ' ';
    int ty = 16;
    for (size_t i = 0; i < flat.size() && ty < bottom && cols > 0; i += cols, ty += 9) {
        d.setCursor(tx, ty);
        d.print(flat.substr(i, cols).c_str());
    }
}

static void draw_chrome(lgfx::LGFX_Device &d, const UiState &s) {
    // connection indicator dot, top-right
    d.fillCircle(d.width() - 7, 7, 4, s.connected ? TFT_GREEN : TFT_RED);

    // shared status line, bottom
    d.setTextSize(1);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(4, d.height() - 11);
    d.print(s.status.c_str());
}

void ui_render(lgfx::LGFX_Device &d, const UiState &s) {
    d.fillScreen(TFT_BLACK);
    switch (s.screen) {
        case Screen::Select: render_select(d, s); break;
        case Screen::Label:  render_label(d, s);  break;
        case Screen::Hold:   render_hold(d, s);   break;
        case Screen::Result: render_result(d, s); break;
    }
    draw_chrome(d, s);
}
