// Real wallet key generation on the device, via vendored trezor-crypto.
// Compiled only in the device build (the sim uses fixed test vectors in
// wallet.cpp). Verified against the canonical BIP39 test vectors natively before
// being wired here (12-word "abandon…about" -> bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyu
// / 0x9858EfFD232B4033E47d90003D41EC34EcaEda94).
#if defined(WALLET_REAL_CRYPTO)

#include <array>
#include <string>
#include <cstring>

#include <cstdlib>

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
#include "monero/xmr.h"
#include "monero/base58.h"
}

#include "xmr_wordlist.h" // Monero English wordlist (1626) for the 25-word seed
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

// Generic 12-word BIP39 seed: 128-bit entropy -> 12 words. Key material only;
// nothing is derived from it here.
bool wallet_generate_seed12(std::array<std::string, 12> &words) {
    uint8_t entropy[16];
    get_entropy(entropy, 16);
    const char *mn = mnemonic_from_data(entropy, 16); // 128-bit -> 12 words (static buf)
    memzero(entropy, sizeof(entropy));
    if (!mn) return false;

    for (int wi = 0; wi < 12; ++wi) words[wi].clear();
    int wi = 0;
    for (const char *p = mn; *p && wi < 12;) {
        const char *sp = strchr(p, ' ');
        size_t len = sp ? size_t(sp - p) : strlen(p);
        words[wi++].assign(p, len);
        p = sp ? sp + 1 : p + len;
    }
    mnemonic_clear(); // wipe trezor's static mnemonic buffer
    return true;
}

// ---- XMR: legacy 25-word Monero seed -> Monero address ----------------------
// The 25-word seed encodes the (reduced) secret spend key directly: 32 bytes ->
// 24 words (4 bytes -> 3 words) + 1 checksum word. Matches docs/keyprint.go's
// xmr_mnemonic byte-for-byte (verified natively against it).
static uint32_t crc32_ieee(const uint8_t *d, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= d[i];
        for (int k = 0; k < 8; ++k)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return ~crc;
}
static void xmr_words_25(const uint8_t spend[32], std::array<std::string, 25> &out) {
    const uint32_t size = XMR_WORDS_EN_COUNT;
    for (int i = 0; i < 32; i += 4) {
        uint32_t x = (uint32_t)spend[i] | ((uint32_t)spend[i + 1] << 8) |
                     ((uint32_t)spend[i + 2] << 16) | ((uint32_t)spend[i + 3] << 24);
        uint32_t w1 = x % size, w2 = (x / size + w1) % size, w3 = (x / size / size + w2) % size;
        out[i / 4 * 3]     = XMR_WORDS_EN[w1];
        out[i / 4 * 3 + 1] = XMR_WORDS_EN[w2];
        out[i / 4 * 3 + 2] = XMR_WORDS_EN[w3];
    }
    // Checksum word: CRC32 over the first 3 chars of each of the 24 words.
    char buf[72];
    int n = 0;
    for (int i = 0; i < 24; ++i) { buf[n++] = out[i][0]; buf[n++] = out[i][1]; buf[n++] = out[i][2]; }
    out[24] = out[crc32_ieee((const uint8_t *)buf, (size_t)n) % 24];
}

// Monero address from a 32-byte key: spend = reduce(key); view = reduce(keccak(spend));
// pubs = base*scalar; address = base58check(0x12 || pub_spend || pub_view). Verified
// byte-for-byte against keyprint.go's scheme natively.
static void xmr_address_from_key(const uint8_t key[32], std::string &out) {
    bignum256modm spend, view;
    expand256_modm(spend, key, 32);
    uint8_t sb[32];
    contract256_modm(sb, spend);
    uint8_t kh[32];
    keccak_256(sb, 32, kh);
    expand256_modm(view, kh, 32);
    ge25519 Ps, Pv;
    ge25519_scalarmult_base_wrapper(&Ps, spend);
    ge25519_scalarmult_base_wrapper(&Pv, view);
    uint8_t data[64];
    ge25519_pack(data, &Ps);
    ge25519_pack(data + 32, &Pv);
    char addr[160];
    // xmr_base58_addr_encode_check writes the chars but does NOT NUL-terminate, so
    // assign exactly the returned length -- `out = addr` would read uninitialized
    // stack garbage past the address, yielding an over-long final field -> C330 E37.
    int n = xmr_base58_addr_encode_check(18, data, 64, addr, sizeof(addr));
    out.assign(addr, n > 0 ? (size_t)n : 0);
    memzero(spend, sizeof(spend));
    memzero(view, sizeof(view));
    memzero(sb, sizeof(sb));
}

bool wallet_generate_xmr(std::array<std::string, 25> &words, std::string &addr) {
    uint8_t entropy[32];
    get_entropy(entropy, 32); // SAR-ADC (no RF; air-gap holds)

    // spend = sc_reduce32(entropy): the 32 bytes the 25 words encode and the
    // address derives from.
    bignum256modm sc;
    expand256_modm(sc, entropy, 32);
    uint8_t spend[32];
    contract256_modm(spend, sc);
    memzero(entropy, sizeof(entropy));
    memzero(sc, sizeof(sc));

    xmr_words_25(spend, words);
    xmr_address_from_key(spend, addr);

    memzero(spend, sizeof(spend));
    return true;
}

#endif // WALLET_REAL_CRYPTO
