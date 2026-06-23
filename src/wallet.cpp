#include "wallet.h"
#include "c330_format.h"

#include <array>
#include <vector>

#if defined(WALLET_REAL_CRYPTO)
// Real derivation on the device (behind the crypto foundation). Declared here,
// defined in the device-only path.
bool wallet_generate(std::array<std::string, 24> &words, std::string &btc, std::string &eth);
bool wallet_generate_xmr(std::array<std::string, 16> &words, std::string &addr);
#endif

namespace {

// Overwrite a string's bytes so private material does not linger in freed heap.
void secure_zero(std::string &s) {
    volatile char *p = const_cast<volatile char *>(s.data());
    for (size_t i = 0; i < s.size(); ++i) p[i] = 0;
    s.clear();
}

// MINTED ON <date>. No RTC/NTP on an air-gapped device, so this is a fixed stamp
// for now (the firmware build date would also work). TODO: real date source.
const char *kMintDate = "2026-01-01";

#if !defined(WALLET_REAL_CRYPTO)
// Simulator / no-crypto build: fixed test seeds so the mnemonic plates are exactly
// correct, plus PLACEHOLDER addresses (not derived) so the address-page format can
// be exercised. Real addresses come from the device crypto path.

bool wallet_generate(std::array<std::string, 24> &words, std::string &btc, std::string &eth) {
    static const char *kTest[24] = { // BIP39 256-bit all-zero test mnemonic
        "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
        "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
        "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
        "abandon", "abandon", "abandon", "abandon", "abandon", "art",
    };
    for (int i = 0; i < 24; ++i) words[i] = kTest[i];
    // Real addresses for this exact test mnemonic (verified natively with trezor);
    // the device derives these from the same seed.
    btc = "bc1qzmtrqsfuaf6l6kkcsseumq26ukaphfj9skkug6";
    eth = "0xF278cF59F82eDcf871d630F28EcC8056f25C1cdb";
    return true;
}

bool wallet_generate_xmr(std::array<std::string, 16> &words, std::string &addr) {
    static const char *kPoly[16] = { // canonical polyseed test phrase
        "raven", "tail", "swear", "infant", "grief", "assist", "regular", "lamp",
        "duck", "valid", "someone", "little", "harsh", "puppy", "airport", "language",
    };
    for (int i = 0; i < 16; ++i) words[i] = kPoly[i];
    // Real monero address for this exact polyseed phrase (verified natively;
    // key->address matches keyprint.go byte-for-byte). The device derives it.
    addr = "47AjPj7DVPQVGGXJXbbTMZWcKQDejGHYZChVkeujy8qPLjKkgdsxge4DzvkRMgU4sDUigGLuBN9stKBMowhuXH2HJHWAuRf";
    return true;
}
#endif

// Stream every plate via the sink, then zeroize all plate buffers (the mnemonic
// plates carry the secret). Returns false if a send failed.
bool send_plates(std::vector<std::string> &plates, SendFn sink) {
    bool ok = true;
    for (auto &p : plates)
        if (ok) ok = sink && sink(p.data(), (unsigned)p.size());
    for (auto &p : plates) secure_zero(p);
    return ok;
}

// BTC+ETH (one 24-word BIP39 seed): info, mnemonic 13-24, mnemonic 1-12, addresses.
bool print_btceth(const std::string &label, SendFn sink, WalletPublic &out) {
    std::array<std::string, 24> words;
    std::string btc, eth;
    if (!wallet_generate(words, btc, eth)) return false;
    out.keys.push_back({"BTC", btc});
    out.keys.push_back({"ETH", eth});
    std::vector<std::string> plates = {
        c330::plate_info(c330::WalletKind::BtcEth),
        c330::plate_mnemonic_13_24(words),
        c330::plate_mnemonic_1_12(words),
        c330::plate_addresses(btc, eth, "BTC", "ETH", label, kMintDate),
    };
    bool ok = send_plates(plates, sink);
    for (auto &w : words) secure_zero(w);
    return ok;
}

// XMR (16-word polyseed): info, polyseed 9-16, polyseed 1-8, monero address.
bool print_xmr(const std::string &label, SendFn sink, WalletPublic &out) {
    std::array<std::string, 16> words;
    std::string addr;
    if (!wallet_generate_xmr(words, addr)) return false;
    out.keys.push_back({"XMR", addr});
    std::vector<std::string> plates = {
        c330::plate_info(c330::WalletKind::Xmr),
        c330::plate_polyseed_9_16(words),
        c330::plate_polyseed_1_8(words),
        c330::plate_xmr_address(addr, "XMR", label, kMintDate),
    };
    bool ok = send_plates(plates, sink);
    for (auto &w : words) secure_zero(w);
    return ok;
}

} // namespace

bool wallet_print(WalletType type, const std::string &label,
                  SendFn sink, WalletPublic &out_public) {
    out_public.keys.clear();
    if (type == WalletType::XMR) return print_xmr(label, sink, out_public);
    return print_btceth(label, sink, out_public); // BTC_ETH (and BTC/ETH) for now
}

bool custom_print(const std::string &text, SendFn sink) {
    std::vector<std::string> lines;
    for (size_t start = 0; start <= text.size() && lines.size() < CUSTOM_MAX_LINES;) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) { lines.push_back(text.substr(start)); break; }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    while (lines.size() > 1 && lines.back().empty()) lines.pop_back();
    if (lines.empty() || (lines.size() == 1 && lines[0].empty())) return false;
    std::vector<std::string> plates = {c330::plate_custom(lines)};
    return send_plates(plates, sink);
}
