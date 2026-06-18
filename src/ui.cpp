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
    s.pubkeys.clear();
}

bool ui_set_connected(UiState &s, bool connected) {
    if (s.connected == connected) return false;
    s.connected = connected;
    return true; // just repaint the connection indicator
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
            s.screen = Screen::Confirm;
            s.status = "Press button to PRINT   ESC=back";
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

// Generate + print, then move to the result screen. Private material lives only
// inside generate_and_print_wallet() and is zeroized there; we keep only pubkeys.
static bool do_print(UiState &s) {
    if (!s.connected) {
        s.status = "Printer not connected";
        return true;
    }
    WalletPublic pub;
    if (generate_and_print_wallet(s.wallet, s.buffer, s.send, pub)) {
        s.pubkeys = pub.keys;
        s.screen  = Screen::Result;
        s.status  = "Any key: new wallet";
    } else {
        s.status = "Print FAILED";
    }
    return true;
}

static bool handle_confirm(UiState &s, const InputEvent &ev) {
    if (ev.key == InputKey::Print) return do_print(s);
    if (ev.key == InputKey::Esc) {
        s.screen = Screen::Label;
        s.status = "ENTER=continue  ESC=back";
        return true;
    }
    return false;
}

static bool handle_result(UiState &s, const InputEvent &ev) {
    if (ev.key == InputKey::None) return false;
    // Any key: wipe the displayed keys and start over.
    s.pubkeys.clear();
    s.pubkeys.shrink_to_fit();
    s.buffer.clear();
    s.cursor = 0;
    s.screen = Screen::Select;
    s.status = "Press 1-4 to choose";
    return true;
}

bool ui_handle_input(UiState &s, const InputEvent &ev) {
    switch (s.screen) {
        case Screen::Select:  return handle_select(s, ev);
        case Screen::Label:   return handle_label(s, ev);
        case Screen::Confirm: return handle_confirm(s, ev);
        case Screen::Result:  return handle_result(s, ev);
    }
    return false;
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

static void render_confirm(lgfx::LGFX_Device &d, const UiState &s) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print("Confirm");

    d.setTextSize(2);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 34);
    d.print("Type: ");
    d.print(wallet_name(s.wallet));

    d.setCursor(4, 60);
    d.print("Label:");

    // Label value (up to 24 chars) at size 1 so it always fits the width.
    d.setTextSize(1);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(12, 84);
    d.print(s.buffer.empty() ? "(none)" : s.buffer.c_str());
}

// One key: large QR on the left, full key text wrapped on the right.
static void render_result_single(lgfx::LGFX_Device &d, const PubKey &pk) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(4, 3);
    d.print("PUBLIC KEY - ");
    d.print(pk.chain.c_str());

    const int qr = 100;
    d.qrcode(pk.key.c_str(), 4, 16, qr, 1, true);

    const int tx = 4 + qr + 6;
    const int cols = (d.width() - tx) / 6;
    const int bottom = d.height() - 14;
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    int ty = 16;
    for (size_t i = 0; i < pk.key.size() && ty < bottom && cols > 0; i += cols, ty += 9) {
        d.setCursor(tx, ty);
        d.print(pk.key.substr(i, cols).c_str());
    }
}

// Several keys: one QR per chain, side by side, each labeled.
static void render_result_multi(lgfx::LGFX_Device &d, const std::vector<PubKey> &keys) {
    const int n = (int)keys.size();
    const int cellw = d.width() / n;
    int qr = cellw - 12;
    if (qr > 96) qr = 96;
    for (int i = 0; i < n; ++i) {
        const int cx = i * cellw;
        d.setTextSize(1);
        d.setTextColor(TFT_CYAN, TFT_BLACK);
        d.setCursor(cx + 6, 4);
        d.print(keys[i].chain.c_str());

        const int qx = cx + (cellw - qr) / 2;
        d.qrcode(keys[i].key.c_str(), qx, 16, qr, 1, true);

        d.setTextColor(TFT_WHITE, TFT_BLACK);
        d.setCursor(cx + 4, 16 + qr + 2);
        const int cols = (cellw - 6) / 6;
        d.print(keys[i].key.substr(0, cols > 0 ? cols : 0).c_str());
    }
}

static void render_result(lgfx::LGFX_Device &d, const UiState &s) {
    if (s.pubkeys.empty()) return;
    if (s.pubkeys.size() == 1) render_result_single(d, s.pubkeys[0]);
    else                       render_result_multi(d, s.pubkeys);
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
        case Screen::Select:  render_select(d, s);  break;
        case Screen::Label:   render_label(d, s);   break;
        case Screen::Confirm: render_confirm(d, s); break;
        case Screen::Result:  render_result(d, s);  break;
    }
    draw_chrome(d, s);
}
