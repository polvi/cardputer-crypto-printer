#include "ui.h"

std::string ui_frame_text(const std::string &text) {
    return "<" + text + ">\r\n";
}

void ui_init(UiState &s, SendFn send, const char *status) {
    s.send      = send;
    s.status    = status ? status : "";
    s.buffer.clear();
    s.cursor    = 0;
    s.connected = false;
}

bool ui_set_connected(UiState &s, bool connected) {
    if (s.connected == connected) return false;
    s.connected = connected;
    // Replace the initial "waiting" hint once we first connect.
    if (connected && s.status.rfind("Connect", 0) == 0) {
        s.status = "Type, then ENTER to emboss";
    }
    return true;
}

bool ui_handle_input(UiState &s, const InputEvent &ev) {
    switch (ev.key) {
        case InputKey::Char:
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

        // Up/Down act as Home/End for now; reserved for history / fields later.
        case InputKey::Up:
            if (s.cursor == 0) return false;
            s.cursor = 0;
            return true;

        case InputKey::Down:
            if (s.cursor == s.buffer.size()) return false;
            s.cursor = s.buffer.size();
            return true;

        case InputKey::Esc:
            if (s.buffer.empty()) return false;
            s.buffer.clear();
            s.cursor = 0;
            s.status = "Cleared";
            return true;

        case InputKey::Enter: {
            if (s.buffer.empty()) {
                s.status = "Type something first";
                return true;
            }
            if (!s.connected) {
                s.status = "No C330 connected";
                return true;
            }
            std::string payload = ui_frame_text(s.buffer);
            bool ok = s.send && s.send(payload.data(), (unsigned)payload.size());
            if (ok) {
                s.status = "Sent: " + s.buffer;
                s.buffer.clear();
                s.cursor = 0;
            } else {
                s.status = "Send FAILED";
            }
            return true;
        }

        default:
            return false;
    }
}

void ui_render(lgfx::LGFX_Device &d, const UiState &s) {
    d.fillScreen(TFT_BLACK);

    // title
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    d.print("C330 Embosser");

    // connection status
    d.setTextSize(1);
    d.setCursor(4, 26);
    if (s.connected) {
        d.setTextColor(TFT_GREEN, TFT_BLACK);
        d.print("USB: connected (57600)");
    } else {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.print("USB: waiting for C330...");
    }

    // input line with a caret at the cursor position
    const int size = 2;
    const int cw   = 6 * size; // default font is 6px wide
    const int chh  = 8 * size; // ...and 8px tall
    const int x0   = 4;
    const int y0   = 44;
    d.setTextSize(size);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(x0, y0);
    d.print("> ");
    d.print(s.buffer.c_str());
    const int prefix = 2; // "> "
    int caretX = x0 + (prefix + (int)s.cursor) * cw;
    d.fillRect(caretX, y0, 2, chh, TFT_YELLOW);

    // status line
    d.setTextSize(1);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(4, d.height() - 12);
    d.print(s.status.c_str());
}
