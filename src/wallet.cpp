#include "wallet.h"
#include "c330_format.h"

#include <array>

#if defined(WALLET_REAL_CRYPTO)
// Real HD-wallet derivation on the device. See wallet_crypto.cpp / the
// trezor-crypto integration; declared here, defined in the device-only path.
bool wallet_generate(std::array<std::string, 24> &words, std::string &btc, std::string &eth);
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
// Simulator / no-crypto build: a fixed BIP39 256-bit all-zero test mnemonic so the
// mnemonic plates are exactly correct, plus PLACEHOLDER addresses (not derived) so
// the address-page format can be exercised. Real addresses come from the device.
bool wallet_generate(std::array<std::string, 24> &words, std::string &btc, std::string &eth) {
    static const char *kTest[24] = {
        "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
        "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
        "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
        "abandon", "abandon", "abandon", "abandon", "abandon", "art",
    };
    for (int i = 0; i < 24; ++i) words[i] = kTest[i];
    btc = "bc1qsimulatoraddressplaceholderonlyq8z306"; // SIM placeholder (not real)
    eth = "0xSiMuLaToRPlaceholderAddr0000000000000000"; // SIM placeholder (not real)
    return true;
}
#endif

} // namespace

bool wallet_print(WalletType /*type*/, const std::string &label,
                  SendFn sink, WalletPublic &out_public) {
    // BTC+ETH only for now (XMR deferred); any selection prints the BTC+ETH wallet.
    std::array<std::string, 24> words;
    std::string btc, eth;
    if (!wallet_generate(words, btc, eth)) return false;

    out_public.keys.clear();
    out_public.keys.push_back({"BTC", btc});
    out_public.keys.push_back({"ETH", eth});

    // Compose the four plates in keyprint.go send order. plate_mnemonic_* uppercase
    // the (lowercase) BIP39 words for the drum.
    std::string plates[4] = {
        c330::plate_info(),                                              // str0 (F0)
        c330::plate_mnemonic_13_24(words),                              // str1 (F2)
        c330::plate_mnemonic_1_12(words),                               // str2 (F1)
        c330::plate_addresses(btc, eth, "BTC", "ETH", label, kMintDate) // str3 (F3)
    };

    bool ok = true;
    for (int i = 0; i < 4 && ok; ++i) {
        ok = sink && sink(plates[i].data(), (unsigned)plates[i].size());
    }

    // Private material must not outlive this call: the mnemonic words and the
    // mnemonic plate payloads (plates[1], plates[2]) carry the secret. Wipe all.
    for (auto &w : words) secure_zero(w);
    for (auto &p : plates) secure_zero(p);
    return ok;
}
