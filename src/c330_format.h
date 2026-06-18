// C330 emboss-print format for the wallet plates.
//
// Faithful port of the formatting in docs/keyprint.go: the C330 takes a layout
// block `<]Fn SY540SX860 …Yyyy Xxxx CI10…>` then a text block `<…lines…>`, is
// uppercase-only with a ≤26-char line limit, and fakes mixed-case (addresses) by
// stacking two rows — uppercase-origin chars on the top row, lowercase-origin on
// a row 7 units below, both embossed with the uppercase drum so case is recovered
// by vertical position.
//
// Pure string logic (no Arduino/SDL/crypto) so it builds and is testable on both
// the device and the simulator.
#pragma once

#include <array>
#include <string>

namespace c330 {

// The C330 drum character set (uppercase only) and the per-line limit.
extern const char *kCharSet; // "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.',& "
static constexpr unsigned kMaxLine = 26;

// True if every char of `s` is in the C330 set and the line is <= kMaxLine.
bool validate_line(const std::string &s);

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

// str0: F0 derivation/BIP info page ("POLVI HD WALLET / 24 WORD BIP32 MNEMONIC /
// BTC BIP84 PATH … / ETH BIP44 PATH …").
std::string plate_info();

// str2: F1 mnemonic words 1-12 (two words per line).
std::string plate_mnemonic_1_12(const std::array<std::string, 24> &words);

// str1: F2 mnemonic words 13-24 (two words per line).
std::string plate_mnemonic_13_24(const std::array<std::string, 24> &words);

// str3: F3 public-key page — "MINTED ON <date>" + optional message + BTC and ETH
// addresses, each address split across two stacked rows. `message` is the
// optional user label (uppercased); `date` is "YYYY-MM-DD".
std::string plate_addresses(const std::string &btc, const std::string &eth,
                            const std::string &btc_header, const std::string &eth_header,
                            const std::string &message, const std::string &date);

} // namespace c330
