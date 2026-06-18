#include "wallet.h"

#include <cstddef>

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
std::string stub_generate_private(WalletType type) {
    unsigned seed = (static_cast<unsigned>(type) + 1) * 2654435761u + (++g_stub_counter);
    static const char *hex = "0123456789abcdef";
    std::string priv;
    priv.reserve(64);
    for (int i = 0; i < 64; ++i) {
        seed = seed * 1664525u + 1013904223u; // LCG, stub only
        priv.push_back(hex[(seed >> 24) & 0xF]);
    }
    return priv;
}

// A placeholder public key/address derived from the private key. Real impl:
// per-chain public-key derivation + address encoding.
std::string stub_derive_public(WalletType type, const std::string &priv) {
    const std::string tag = priv.substr(0, 8);
    switch (type) {
        case WalletType::BTC:     return "bc1q" + tag + "stubbtc0";
        case WalletType::ETH:     return "0x" + priv.substr(0, 40);
        case WalletType::BTC_ETH: return "BTC:bc1q" + tag + "\nETH:0x" + priv.substr(0, 20);
        case WalletType::XMR:     return "4" + tag + "stubxmraddress";
    }
    return tag;
}

// The bytes actually sent to the printer. This is the analog of the old
// ui_frame_text(): the C330 emboss-prints what sits between '<' and '>'. The
// real engraving layout for private key material is OUT OF SCOPE; for now we
// frame a human-readable record so the simulator can show what would print.
std::string compose_print_payload(WalletType type, const std::string &label,
                                  const std::string &priv, const std::string &pub) {
    std::string body = wallet_name(type);
    if (!label.empty()) body += " " + label;
    body += " PRIV:" + priv + " PUB:" + pub;
    return "<" + body + ">\r\n";
}

} // namespace

bool generate_and_print_wallet(WalletType type, const std::string &label,
                               SendFn sink, WalletPublic &out_public) {
    std::string priv = stub_generate_private(type);
    out_public.pubkey = stub_derive_public(type, priv);

    std::string payload = compose_print_payload(type, label, priv, out_public.pubkey);
    bool ok = sink && sink(payload.data(), (unsigned)payload.size());

    // Private material must not outlive this call.
    secure_zero(priv);
    secure_zero(payload);
    return ok;
}
