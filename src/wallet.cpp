#include "wallet.h"

#include <cstddef>
#include <vector>

namespace {

// Overwrite a string's bytes so private material does not linger in freed heap.
// The volatile writes prevent the compiler from optimizing the loop away.
void secure_zero(std::string &s) {
    volatile char *p = const_cast<volatile char *>(s.data());
    for (size_t i = 0; i < s.size(); ++i) p[i] = 0;
    s.clear();
}

// Distinguish repeated generations in the stub output. Not cryptographic.
unsigned g_stub_counter = 0;

// ---- STUBS (replace with real BIP32/39/44 HD wallet code) -------------------
// The wallet is an HD wallet: ONE seed, from which every chain's keys derive.
// The seed is the only private material — it is what gets printed on the wallet.
// Real impl: CSPRNG entropy -> BIP39 mnemonic -> BIP32 master seed.
std::string stub_generate_seed() {
    unsigned x = 2654435761u + (++g_stub_counter) * 40503u;
    static const char *hex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (int i = 0; i < 64; ++i) {
        x = x * 1664525u + 1013904223u; // LCG, stub only
        out.push_back(hex[(x >> 24) & 0xF]);
    }
    return out;
}

struct Chain {
    const char *name;   // "BTC"
    const char *path;   // BIP44 derivation path
    const char *prefix; // address prefix for the stub
};

// BIP44 paths per chain (coin types: BTC=0, ETH=60). A wallet type maps to the
// set of chains it derives from the single seed.
const std::vector<Chain> &chains_for(WalletType type) {
    static const std::vector<Chain> btc  = {{"BTC", "m/44'/0'/0'/0/0", "bc1q"}};
    static const std::vector<Chain> eth  = {{"ETH", "m/44'/60'/0'/0/0", "0x"}};
    static const std::vector<Chain> xmr  = {{"XMR", "m/44'/128'/0'/0/0", "4"}};
    static const std::vector<Chain> both = {{"BTC", "m/44'/0'/0'/0/0", "bc1q"},
                                            {"ETH", "m/44'/60'/0'/0/0", "0x"}};
    switch (type) {
        case WalletType::BTC:     return btc;
        case WalletType::ETH:     return eth;
        case WalletType::XMR:     return xmr;
        case WalletType::BTC_ETH: return both;
    }
    return btc;
}

// STUB: derive a chain's PUBLIC address from the seed + its derivation path.
// Deterministic in (seed, path) so one seed yields stable addresses (the HD
// property). Real impl: BIP32 child key derivation + chain address encoding.
std::string stub_derive_public(const std::string &seed, const Chain &c) {
    unsigned h = 2166136261u; // FNV-ish mix of seed + path; stub only
    for (char ch : seed)      h = (h ^ (unsigned char)ch) * 16777619u;
    for (const char *p = c.path; *p; ++p) h = (h ^ (unsigned char)*p) * 16777619u;

    static const char *b36 = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string addr = c.prefix;
    for (int i = 0; i < 32; ++i) {
        h = h * 1664525u + 1013904223u;
        addr.push_back(b36[(h >> 24) % 36]);
    }
    return addr;
}

// The bytes printed on the physical wallet: the seed (PRIVATE) plus, for the
// record, each chain's derivation path + public address. The real C330 engraving
// layout is OUT OF SCOPE. Framed <...> so the simulator can show what prints.
std::string compose_print_payload(WalletType type, const std::string &label,
                                  const std::string &seed,
                                  const std::vector<PubKey> &pubs,
                                  const std::vector<Chain> &chains) {
    std::string body = wallet_name(type);
    if (!label.empty()) body += " \"" + label + "\"";
    body += " SEED:" + seed;
    for (size_t i = 0; i < pubs.size(); ++i) {
        body += " " + pubs[i].chain + "(" + chains[i].path + "):" + pubs[i].key;
    }
    return "<" + body + ">\r\n";
}

} // namespace

bool generate_and_print_wallet(WalletType type, const std::string &label,
                               SendFn sink, WalletPublic &out_public) {
    const std::vector<Chain> &chains = chains_for(type);

    // One HD seed; all chains derive from it (BIP32/39/44).
    std::string seed = stub_generate_seed();

    out_public.keys.clear();
    for (const auto &c : chains) {
        out_public.keys.push_back({c.name, stub_derive_public(seed, c)});
    }

    std::string payload = compose_print_payload(type, label, seed, out_public.keys, chains);
    bool ok = sink && sink(payload.data(), (unsigned)payload.size());

    // The seed is the only private material; it must not outlive this call.
    secure_zero(seed);
    secure_zero(payload);
    return ok;
}
