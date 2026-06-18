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

// Distinguish repeated prints in the stub output. Not cryptographic.
unsigned g_stub_counter = 0;

// ---- STUBS (replace with real key generation) -------------------------------
// A placeholder "private key": 64 hex chars. Real impl: CSPRNG -> curve scalar.
std::string stub_generate_private() {
    unsigned seed = 2654435761u + (++g_stub_counter) * 40503u;
    static const char *hex = "0123456789abcdef";
    std::string priv;
    priv.reserve(64);
    for (int i = 0; i < 64; ++i) {
        seed = seed * 1664525u + 1013904223u; // LCG, stub only
        priv.push_back(hex[(seed >> 24) & 0xF]);
    }
    return priv;
}

// One chain's worth of keys. `priv` is zeroized by the caller after use.
struct ChainKey {
    std::string chain;
    std::string priv;
    std::string pub;
};

// A placeholder public key/address per chain. Real impl: curve public-key
// derivation + chain-specific address encoding.
ChainKey gen_chain(const char *chain, const std::string &prefix) {
    ChainKey c;
    c.chain = chain;
    c.priv  = stub_generate_private();
    c.pub   = prefix + c.priv.substr(0, 32);
    return c;
}

// The per-chain key set a wallet type yields. Combined types yield several.
std::vector<ChainKey> gen_wallet(WalletType type) {
    std::vector<ChainKey> v;
    switch (type) {
        case WalletType::BTC:     v.push_back(gen_chain("BTC", "bc1q")); break;
        case WalletType::ETH:     v.push_back(gen_chain("ETH", "0x"));   break;
        case WalletType::XMR:     v.push_back(gen_chain("XMR", "4"));    break;
        case WalletType::BTC_ETH:
            v.push_back(gen_chain("BTC", "bc1q"));
            v.push_back(gen_chain("ETH", "0x"));
            break;
    }
    return v;
}

// The bytes actually sent to the printer. This is the analog of the old
// ui_frame_text(): the C330 emboss-prints what sits between '<' and '>'. The
// real engraving layout for private key material is OUT OF SCOPE; for now we
// frame a human-readable record so the simulator can show what would print.
std::string compose_print_payload(WalletType type, const std::string &label,
                                  const std::vector<ChainKey> &chains) {
    std::string body = wallet_name(type);
    if (!label.empty()) body += " \"" + label + "\"";
    for (const auto &c : chains) {
        body += " " + c.chain + "[PRIV:" + c.priv + " PUB:" + c.pub + "]";
    }
    return "<" + body + ">\r\n";
}

} // namespace

bool generate_and_print_wallet(WalletType type, const std::string &label,
                               SendFn sink, WalletPublic &out_public) {
    std::vector<ChainKey> chains = gen_wallet(type);

    std::string payload = compose_print_payload(type, label, chains);
    bool ok = sink && sink(payload.data(), (unsigned)payload.size());

    // Hand back only the public keys.
    out_public.keys.clear();
    for (const auto &c : chains) out_public.keys.push_back({c.chain, c.pub});

    // Private material must not outlive this call.
    for (auto &c : chains) secure_zero(c.priv);
    secure_zero(payload);
    return ok;
}
