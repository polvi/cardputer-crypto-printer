#include "c330_format.h"

#include <cstdio>

namespace c330 {

const char *kCharSet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.',& ";

namespace {

std::string to_upper(const std::string &s) {
    std::string o = s;
    for (char &c : o)
        if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    return o;
}

// Go's `strings.ToUpper(s) == s`: true iff there are no lowercase letters.
bool all_upper(const std::string &s) {
    for (char c : s)
        if (c >= 'a' && c <= 'z') return false;
    return true;
}

std::string slice_from(const std::string &s, size_t i) {
    return i < s.size() ? s.substr(i) : std::string();
}
std::string slice_to(const std::string &s, size_t n) {
    return s.substr(0, n < s.size() ? n : s.size());
}

} // namespace

bool validate_line(const std::string &s) {
    if (s.size() > kMaxLine) return false;
    for (char c : s) {
        char u = (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
        bool ok = false;
        for (const char *p = kCharSet; *p; ++p)
            if (*p == u) { ok = true; break; }
        if (!ok) return false;
    }
    return true;
}

std::string yx_layout(int y, int x) {
    char buf[24];
    snprintf(buf, sizeof(buf), "Y%03dX%03d CI10", y, x);
    return buf;
}

std::string mx_layout(const std::string &s, int y, int x) {
    if (all_upper(s)) return yx_layout(y, x);
    return yx_layout(y, x) + "\n" + yx_layout(y + 7, x);
}

std::string mx_message(const std::string &s) {
    if (all_upper(s)) return s;
    std::string su, sl;
    for (char c : s) {
        if (c >= 'a' && c <= 'z') { su += ' '; sl += c; } // lowercase -> bottom row
        else                      { su += c;  sl += ' '; } // upper/digit/sym -> top row
    }
    return su + "\n" + sl;
}

// ---------------------------------------------------------------------------
std::string plate_info() {
    static const char *kInfo =
        "\n<]F0 SY540SX860\n"
        "Y050X100 CI10\n"
        "Y125X100 CI10\n"
        "Y200X100 CI10\n"
        "Y275X100 CI10\n"
        "Y350X100 CI10\n"
        "Y425X100 CI10>\n"
        "\n"
        "<POLVI HD WALLET\n"
        "24 WORD BIP32 MNEMONIC\n"
        "BTC BIP84 PATH\n"
        "M/84'/0'/0'/0/0\n"
        "ETH BIP44 PATH\n"
        "M/44'/60'/0'/0/0>\n";
    return to_upper(kInfo);
}

namespace {

// Two words per line, "%2d %-11s%2d %s", 6 lines for a 12-word half.
std::string mnemonic_plate(char fmt, const std::array<std::string, 24> &w, int off) {
    std::string out =
        std::string("\n<]F") + fmt + " SY540SX860\n"
        "Y050X100 CI10\n"
        "Y125X100 CI10\n"
        "Y200X100 CI10\n"
        "Y275X100 CI10\n"
        "Y350X100 CI10\n"
        "Y425X100 CI10>\n"
        "\n<";
    for (int i = 0; i < 12; i += 2) {
        char line[48];
        snprintf(line, sizeof(line), "%2d %-11s%2d %s",
                 off + i + 1, w[off + i].c_str(),
                 off + i + 2, w[off + i + 1].c_str());
        out += line;
        if (i < 10) out += "\n";
    }
    out += ">\n";
    return to_upper(out);
}

} // namespace

std::string plate_mnemonic_1_12(const std::array<std::string, 24> &words) {
    return mnemonic_plate('1', words, 0);
}
std::string plate_mnemonic_13_24(const std::array<std::string, 24> &words) {
    return mnemonic_plate('2', words, 12);
}

std::string plate_addresses(const std::string &btc, const std::string &eth,
                            const std::string &btc_header, const std::string &eth_header,
                            const std::string &message, const std::string &date) {
    // Logical messages, each already split into its (possibly two) rows.
    const std::string MNT = mx_message(to_upper("MINTED ON " + date));
    const std::string MSG = mx_message(to_upper(message)); // label; uppercased -> single row
    const std::string m2 = mx_message(to_upper(btc_header) + " " + slice_to(btc, 19) + "...");
    const std::string m3 = mx_message("..." + slice_from(btc, 19));
    const std::string m4 = mx_message(to_upper(eth_header) + " " + slice_to(eth, 19) + "...");
    const std::string m5 = mx_message("..." + slice_from(eth, 19));

    // Layout order (keyprint.go PKey_Layout): MNT, MSG, m2..m5 at y=50,125,…step 75.
    const std::string layout_msgs[6] = {MNT, MSG, m2, m3, m4, m5};
    std::string layout;
    for (int i = 0, y = 50; i < 6; ++i, y += 75) {
        layout += mx_layout(layout_msgs[i], y, 100);
        if (i < 5) layout += "\n";
    }

    // Text order (keyprint.go PKey_Print_Template): MSG, MNT, m2..m5.
    const std::string text_msgs[6] = {MSG, MNT, m2, m3, m4, m5};
    std::string text;
    for (int i = 0; i < 6; ++i) {
        text += text_msgs[i];
        if (i < 5) text += "\n";
    }

    std::string out = "\n<]F3 SY540SX860\n" + layout + ">\n\n<" + text + ">\n";
    return to_upper(out); // drum is uppercase; case is encoded by row position
}

} // namespace c330
