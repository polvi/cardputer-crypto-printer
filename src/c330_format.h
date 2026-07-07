// C330 emboss-print format for the wallet plates.
//
// Faithful port of the formatting in docs/keyprint.go: the C330 takes a layout
// block `<]Fn SY540SX860 …Yyyy Xxxx CI10…>` then a text block `<…lines…>`, is
// uppercase-only with a ≤26-char line limit, and fakes mixed-case (addresses) by
// stacking two rows — uppercase-origin chars on the top row, lowercase-origin on
// a row 7 units below, both embossed with the uppercase drum so case is recovered
// by vertical position.
//
// Media / font the layout numbers are sized for:
//   - Font: the C330's Simplex 2 emboss drum, 3 mm character height.
//   - Plate: CR-80 metal plate, 3.375" x 2.215" x 0.015", 304 bright-annealed
//     stainless steel, with (4) 0.156" holes (for bolting a stack together,
//     e.g. cover plates over a seed-words plate).
//   - Layout units are ~0.1 mm: SY540 spans the plate's short side; rows sit
//     at Y050..Y425 in steps of 75 (7.5 mm pitch for the 3 mm glyphs), and the
//     mixed-case stack offset of 7 units is 0.7 mm.
//
// Pure string logic (no Arduino/SDL/crypto) so it builds and is testable on both
// the device and the simulator.
#pragma once

#include <array>
#include <string>
#include <vector>

namespace c330 {

// Which wallet a plate/info page is for. Kept M5GFX-free (not ui.h's WalletType)
// so this module stays standalone-testable; the callers map WalletType -> kind.
enum class WalletKind { BtcEth, Xmr, Seed };

// The C330 drum character set (uppercase only) and the per-line limit.
extern const char *kCharSet; // "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.',& "
static constexpr unsigned kMaxLine = 26;

// The per-key "instruction" lines shown on the Confirm screen and embossed on the
// info plate (so the two roughly match). No "POLVI HD WALLET" header here.
std::vector<std::string> wallet_info_lines(WalletKind k);

// True if every char of `s` is in the C330 set and the line is <= kMaxLine.
bool validate_line(const std::string &s);

// For label input: returns the uppercased char if it is embossable on the C330
// (A-Z 0-9 - / . ' , & space), or 0 to reject. Lowercase is folded to uppercase
// since the drum is uppercase-only (matches keyprint.go's C330CharSet).
char sanitize_label_char(char c);

// "Y%03dX%03d CI10" — one layout coordinate line.
std::string yx_layout(int y, int x);

// One logical message's layout: a single coordinate line, or two (y and y+7)
// when `s` is not all-uppercase (i.e. it is a stacked two-row message).
std::string mx_layout(const std::string &s, int y, int x);

// Split a mixed-case string into two stacked rows: uppercase-origin chars (and
// digits/symbols/spaces) on the top row, lowercase-origin chars on the bottom
// row (each blanked with spaces on the other row), joined by '\n'. All-uppercase
// input is returned unchanged (single row).
std::string mx_message(const std::string &s);

// ---- Plate payload builders (each returns one self-contained <]Fn …><…> block,
//      fully uppercased, matching keyprint.go) -------------------------------

// F0 info plate: "POLVI HD WALLET" + wallet_info_lines(k).
std::string plate_info(WalletKind k);

// --- BTC+ETH (BIP39) ---
// F1 mnemonic words 1-12 / F2 words 13-24, two words per line.
std::string plate_mnemonic_1_12(const std::array<std::string, 24> &words);
std::string plate_mnemonic_13_24(const std::array<std::string, 24> &words);

// F3 public-key page — "MINTED ON <date>" + optional message + BTC and ETH
// addresses, each address split across two stacked rows. `message` is the
// optional user label (uppercased); `date` is "YYYY-MM-DD".
std::string plate_addresses(const std::string &btc, const std::string &eth,
                            const std::string &btc_header, const std::string &eth_header,
                            const std::string &message, const std::string &date);

// --- XMR (legacy 25-word seed) ---
// Exactly as docs/keyprint.go (the proven `lp` stream): every word card RE-EMITS
// its own <]F1 ..> format block (re-defining F1 resets the printer's field buffer
// each card; text-only continuations accumulate it -> E37 on a later card). Words
// 1-10 and 11-20 are two-per-line (pad 12); 21-25 is one-per-line. Same 5-row
// Y085 layout on each.
std::string plate_xmr_words_1_10(const std::array<std::string, 25> &words);   // <]F1 ..><text> 2/line
std::string plate_xmr_words_11_20(const std::array<std::string, 25> &words);  // <]F1 ..><text> 2/line
std::string plate_xmr_words_21_25(const std::array<std::string, 25> &words);  // <]F1 ..><text> 1/line

// F3 public-key page — "MINTED ON <date>" + message + the 95-char monero address
// split across four stacked, hyphen-joined lines (keyprint.go PKey_Print_Template).
std::string plate_xmr_address(const std::string &addr, const std::string &header,
                              const std::string &message, const std::string &date);

// --- Generic labeled 12-word BIP39 seed (no derived addresses) ---
// F3 words plate: nothing but the 12 numbered words, two per line — the same
// row format as the BTC+ETH mnemonic plates, just 128-bit entropy.
std::string plate_seed_words(const std::array<std::string, 12> &words);

// F0 label plate (sent last, so it stacks on top of the words plate): the
// optional label, "12 WORD BIP39 SEED", and "MINTED ON <date>".
std::string plate_seed_info(const std::string &label, const std::string &date);

// --- Custom: a few free-form lines on a single card ---
// F0 plate, one row per line (Y050 step 75). `lines` should already be uppercase
// and C330-valid (the UI sanitizes input); the whole plate is uppercased anyway.
std::string plate_custom(const std::vector<std::string> &lines);

} // namespace c330
