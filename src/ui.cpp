#include "ui.h"
#include "wallet.h"
#include "c330_format.h"

#include <cstdio>

// Map the UI wallet type to the C330 format module's kind.
static c330::WalletKind kind_of(WalletType w) {
    return w == WalletType::XMR ? c330::WalletKind::Xmr : c330::WalletKind::BtcEth;
}

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
    s.wallet    = WalletType::BTC_ETH;
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
        case '1': w = WalletType::BTC_ETH; break;
        case '2': w = WalletType::XMR;     break;
        // Solo BTC/ETH are hidden until their crypto is implemented.
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
        case InputKey::Char: {
            if (s.buffer.size() >= LABEL_MAX) return false; // 24-char cap
            char c = c330::sanitize_label_char(ev.ch); // uppercase + C330 charset
            if (!c) return false;                       // reject non-embossable chars
            s.buffer.insert(s.buffer.begin() + s.cursor, c);
            s.cursor++;
            return true;
        }

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
            s.status = "G0 or ENTER = print   ESC=back";
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

static bool handle_confirm(UiState &s, const InputEvent &ev) {
    // Print on the G0 button (device) or Enter (works everywhere, incl. the sim).
    if (ev.key == InputKey::Print || ev.key == InputKey::Enter) {
        if (!s.connected) {
            s.status = "Printer not connected";
            return true;
        }
        // Enter the transient Printing screen; the front-end renders it, then
        // calls ui_run_print() to do the actual (blocking) plate stream.
        s.screen = Screen::Printing;
        s.status = "Printing... do not remove plates";
        return true;
    }
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
        case Screen::Select:   return handle_select(s, ev);
        case Screen::Label:    return handle_label(s, ev);
        case Screen::Confirm:  return handle_confirm(s, ev);
        case Screen::Printing: return false; // ignore input while streaming
        case Screen::Result:   return handle_result(s, ev);
    }
    return false;
}

bool ui_pending_print(const UiState &s) {
    return s.screen == Screen::Printing;
}

// Blocking: generate the wallet, stream all plates, then transition. Private
// material lives only inside wallet_print() and is zeroized there; we keep only
// the public addresses.
bool ui_run_print(UiState &s) {
    WalletPublic pub;
    if (wallet_print(s.wallet, s.buffer, s.send, pub)) {
        s.pubkeys = pub.keys;
        s.screen  = Screen::Result;
        s.status  = "Any key: new wallet";
    } else {
        s.screen = Screen::Confirm;
        s.status = "Print FAILED";
    }
    return true;
}

// ============================================================================
// Rendering (per-screen) + shared chrome
// ============================================================================
static void render_select(lgfx::LGFX_Device &d, const UiState &) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print("Crypto Wallet");

    static const char *opts[2] = {"1 BTC+ETH", "2 XMR"};
    d.setTextSize(2);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    for (int i = 0; i < 2; ++i) {
        d.setCursor(8, 34 + i * 26);
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
    d.setCursor(4, 28);
    d.print(wallet_name(s.wallet));

    // Per-key details — the same lines embossed on the info plate.
    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    int y = 52;
    for (const auto &line : c330::wallet_info_lines(kind_of(s.wallet))) {
        d.setCursor(8, y);
        d.print(line.c_str());
        y += 10;
    }

    // Label value (up to 24 chars) at size 1 so it always fits the width.
    y += 4;
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, y);
    d.print("Label: ");
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.print(s.buffer.empty() ? "(none)" : s.buffer.c_str());
}

// One key: large QR on the left, full key text wrapped on the right.
static void render_result_single(lgfx::LGFX_Device &d, const PubKey &pk) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(4, 3);
    d.print("PUBLIC KEY - ");
    d.print(pk.chain.c_str());

    const int qr = 102;
    d.qrcode(pk.key.c_str(), 4, 16, qr, 1, false); // margin=false -> minimal padding

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
        d.qrcode(keys[i].key.c_str(), qx, 16, qr, 1, false); // margin=false

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

static void render_printing(lgfx::LGFX_Device &d, const UiState &s) {
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print("Printing");

    d.setTextSize(2);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 44);
    d.print(wallet_name(s.wallet));
    d.print(" wallet");

    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 72);
    d.print("Embossing plates - please wait");
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
        case Screen::Select:   render_select(d, s);   break;
        case Screen::Label:    render_label(d, s);    break;
        case Screen::Confirm:  render_confirm(d, s);  break;
        case Screen::Printing: render_printing(d, s); break;
        case Screen::Result:   render_result(d, s);   break;
    }
    draw_chrome(d, s);
}
