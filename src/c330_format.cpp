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

char sanitize_label_char(char c) {
    char u = (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
    for (const char *p = kCharSet; *p; ++p)
        if (*p == u) return u;
    return 0;
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
namespace {

// `n` coordinate lines, Y050 then +75 each, all at X100.
std::string layout_rows(int n, int y0 = 50) {
    std::string s;
    for (int i = 0, y = y0; i < n; ++i, y += 75) {
        char l[24];
        snprintf(l, sizeof(l), "Y%03dX100 CI10", y);
        s += l;
        if (i < n - 1) s += "\n";
    }
    return s;
}

// Two words per line, "%2d %-<pad>s%2d %s", count/2 lines starting at word `start`.
std::string word_plate(char fmt, const std::string *w, int start, int count, int pad,
                       int y0 = 50) {
    std::string out = std::string("\n<]F") + fmt + " SY540SX860\n" +
                      layout_rows(count / 2, y0) + ">\n\n<";
    for (int i = 0; i < count; i += 2) {
        char line[48];
        snprintf(line, sizeof(line), "%2d %-*s%2d %s",
                 start + i + 1, pad, w[start + i].c_str(),
                 start + i + 2, w[start + i + 1].c_str());
        out += line;
        if (i < count - 2) out += "\n";
    }
    out += ">\n";
    return to_upper(out);
}

// One word per line, "%2d %s", `count` lines starting at word `start`.
std::string word_plate_single(char fmt, const std::string *w, int start, int count,
                              int y0) {
    std::string out = std::string("\n<]F") + fmt + " SY540SX860\n" +
                      layout_rows(count, y0) + ">\n\n<";
    for (int i = 0; i < count; ++i) {
        char line[40];
        snprintf(line, sizeof(line), "%2d %s", start + i + 1, w[start + i].c_str());
        out += line;
        if (i < count - 1) out += "\n";
    }
    out += ">\n";
    return to_upper(out);
}

} // namespace

std::vector<std::string> wallet_info_lines(WalletKind k) {
    if (k == WalletKind::Xmr)
        return {"MONERO", "25 WORD MNEMONIC SEED", "ACCOUNT NUMBER 0"};
    return {"24 WORD BIP32 MNEMONIC", "BTC BIP84 PATH", "M/84'/0'/0'/0/0",
            "ETH BIP44 PATH", "M/44'/60'/0'/0/0"};
}

std::string plate_info(WalletKind k) {
    std::vector<std::string> lines = wallet_info_lines(k);
    lines.insert(lines.begin(), "POLVI HD WALLET");
    std::string text;
    for (size_t i = 0; i < lines.size(); ++i) {
        text += lines[i];
        if (i + 1 < lines.size()) text += "\n";
    }
    std::string out = "\n<]F0 SY540SX860\n" + layout_rows((int)lines.size()) +
                      ">\n\n<" + text + ">\n";
    return to_upper(out);
}

std::string plate_mnemonic_1_12(const std::array<std::string, 24> &words) {
    return word_plate('1', words.data(), 0, 12, 11);
}
std::string plate_mnemonic_13_24(const std::array<std::string, 24> &words) {
    return word_plate('2', words.data(), 12, 12, 11);
}

// XMR 25-word seed: 5 rows/plate at Y085 step 75, words padded to 12 (per
// keyprint.go). Plates 1-10 and 11-20 are two-per-row; 21-25 is one-per-row.
std::string plate_xmr_words_1_10(const std::array<std::string, 25> &words) {
    return word_plate('1', words.data(), 0, 10, 12, 85);
}
std::string plate_xmr_words_11_20(const std::array<std::string, 25> &words) {
    return word_plate('1', words.data(), 10, 10, 12, 85);
}
std::string plate_xmr_words_21_25(const std::array<std::string, 25> &words) {
    return word_plate_single('1', words.data(), 20, 5, 85);
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

std::string plate_xmr_address(const std::string &addr, const std::string &header,
                              const std::string &message, const std::string &date) {
    // The 95-char monero address split into 4 hyphen-joined lines (22/24/24/25)
    // per keyprint.go PKey_XMR_Addr_Message2..5; mixed-case -> row-stacking.
    const std::string MNT = mx_message(to_upper("MINTED ON " + date));
    const std::string MSG = mx_message(to_upper(message));
    const std::string m2 = mx_message(to_upper(header) + addr.substr(0, 22) + "-");
    const std::string m3 = mx_message("-" + (addr.size() > 22 ? addr.substr(22, 24) : std::string()) + "-");
    const std::string m4 = mx_message("-" + (addr.size() > 46 ? addr.substr(46, 24) : std::string()) + "-");
    const std::string m5 = mx_message("-" + (addr.size() > 70 ? addr.substr(70) : std::string()));

    const std::string layout_msgs[6] = {MNT, MSG, m2, m3, m4, m5};
    std::string layout;
    for (int i = 0, y = 50; i < 6; ++i, y += 75) {
        layout += mx_layout(layout_msgs[i], y, 100);
        if (i < 5) layout += "\n";
    }
    const std::string text_msgs[6] = {MSG, MNT, m2, m3, m4, m5};
    std::string text;
    for (int i = 0; i < 6; ++i) {
        text += text_msgs[i];
        if (i < 5) text += "\n";
    }
    std::string out = "\n<]F3 SY540SX860\n" + layout + ">\n\n<" + text + ">\n";
    return to_upper(out);
}

std::string plate_custom(const std::vector<std::string> &lines) {
    std::string text;
    for (size_t i = 0; i < lines.size(); ++i) {
        text += lines[i];
        if (i + 1 < lines.size()) text += "\n";
    }
    std::string out = "\n<]F0 SY540SX860\n" + layout_rows((int)lines.size()) +
                      ">\n\n<" + text + ">\n";
    return to_upper(out);
}

} // namespace c330
