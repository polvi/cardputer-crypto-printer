// Real wallet key generation on the device, via vendored trezor-crypto.
// Compiled only in the device build (the sim uses fixed test vectors in
// wallet.cpp). Verified against the canonical BIP39 test vectors natively before
// being wired here (12-word "abandon…about" -> bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyu
// / 0x9858EfFD232B4033E47d90003D41EC34EcaEda94).
#if defined(WALLET_REAL_CRYPTO)

#include <array>
#include <string>
#include <cstring>

extern "C" {
#include "bip39.h"
#include "bip32.h"
#include "curves.h"
#include "secp256k1.h"
#include "sha2.h"
#include "sha3.h"
#include "ripemd160.h"
#include "segwit_addr.h"
#include "ecdsa.h"
#include "memzero.h"
}

#include "esp_random.h"
#include "bootloader_random.h"

// trezor-crypto needs these for EC side-channel blinding (curve_to_jacobian
// randomizes Z and loops until it gets nonzero bytes — so these MUST return real
// entropy, not zeros). C linkage to override trezor's weak defaults.
extern "C" uint32_t random32(void) { return esp_random(); }
extern "C" void random_buffer(uint8_t *buf, size_t len) { esp_fill_random(buf, len); }

namespace {

// 32 bytes of seed entropy. WiFi/BT are off (air-gapped), so esp_random() alone
// is not a true RNG — enable the SAR-ADC noise source around the read.
void get_entropy(uint8_t *e, size_t n) {
    bootloader_random_enable();
    esp_fill_random(e, n);
    bootloader_random_disable();
}

// BTC BIP84 native segwit: m/84'/0'/0'/0/0 -> bech32 "bc1...".
void derive_btc(const HDNode &root, std::string &out) {
    HDNode n = root;
    const uint32_t path[5] = {84u | 0x80000000u, 0u | 0x80000000u, 0u | 0x80000000u, 0, 0};
    for (int i = 0; i < 5; ++i) hdnode_private_ckd(&n, path[i]);
    hdnode_fill_public_key(&n);
    uint8_t sha[32], h160[20];
    sha256_Raw(n.public_key, 33, sha);
    ripemd160(sha, 32, h160);
    char addr[100];
    segwit_addr_encode(addr, "bc", 0, h160, 20);
    out = addr;
    memzero(&n, sizeof(n));
}

// ETH BIP44: m/44'/60'/0'/0/0 -> keccak-256(uncompressed pubkey)[12:] with EIP-55.
void derive_eth(const HDNode &root, std::string &out) {
    HDNode n = root;
    const uint32_t path[5] = {44u | 0x80000000u, 60u | 0x80000000u, 0u | 0x80000000u, 0, 0};
    for (int i = 0; i < 5; ++i) hdnode_private_ckd(&n, path[i]);
    hdnode_fill_public_key(&n);
    uint8_t pub65[65];
    ecdsa_get_public_key65(&secp256k1, n.private_key, pub65);
    uint8_t hash[32];
    keccak_256(pub65 + 1, 64, hash);
    static const char *hx = "0123456789abcdef";
    char hex[41];
    for (int i = 0; i < 20; ++i) {
        hex[i * 2]     = hx[hash[12 + i] >> 4];
        hex[i * 2 + 1] = hx[hash[12 + i] & 0xf];
    }
    hex[40] = 0;
    uint8_t cs[32];
    keccak_256(reinterpret_cast<const uint8_t *>(hex), 40, cs); // EIP-55 over lowercase hex
    out = "0x";
    for (int i = 0; i < 40; ++i) {
        char c = hex[i];
        int nib = (i % 2 == 0) ? (cs[i / 2] >> 4) : (cs[i / 2] & 0xf);
        if (c >= 'a' && c <= 'f' && nib >= 8) c = char(c - 'a' + 'A');
        out += c;
    }
    memzero(&n, sizeof(n));
    memzero(pub65, sizeof(pub65));
}

} // namespace

bool wallet_generate(std::array<std::string, 24> &words, std::string &btc, std::string &eth) {
    uint8_t entropy[32];
    get_entropy(entropy, 32);
    const char *mn = mnemonic_from_data(entropy, 32); // 256-bit -> 24 words (static buf)
    memzero(entropy, sizeof(entropy));
    if (!mn) return false;

    for (int wi = 0; wi < 24; ++wi) words[wi].clear();
    int wi = 0;
    for (const char *p = mn; *p && wi < 24;) {
        const char *sp = strchr(p, ' ');
        size_t len = sp ? size_t(sp - p) : strlen(p);
        words[wi++].assign(p, len);
        p = sp ? sp + 1 : p + len;
    }

    uint8_t seed[64];
    mnemonic_to_seed(mn, "", seed, nullptr);
    HDNode root;
    hdnode_from_seed(seed, 64, SECP256K1_NAME, &root);
    derive_btc(root, btc);
    derive_eth(root, eth);

    memzero(&root, sizeof(root));
    memzero(seed, sizeof(seed));
    mnemonic_clear(); // wipe trezor's static mnemonic buffer
    return true;
}

// XMR real keygen arrives in the polyseed phase. Until then a placeholder so the
// device XMR flow still composes (polyseed test phrase + non-derived address).
bool wallet_generate_xmr(std::array<std::string, 16> &words, std::string &addr) {
    static const char *kPoly[16] = {
        "raven", "tail", "swear", "infant", "grief", "assist", "regular", "lamp",
        "duck", "valid", "someone", "little", "harsh", "puppy", "airport", "language"};
    for (int i = 0; i < 16; ++i) words[i] = kPoly[i];
    addr = "4PLACEHOLDERmoneroSIMaddressNOTrealDOnotSEND";
    while (addr.size() < 95) addr += "x";
    addr.resize(95);
    return true;
}

#endif // WALLET_REAL_CRYPTO
