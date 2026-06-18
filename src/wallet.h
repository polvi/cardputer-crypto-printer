// Crypto wallet generation + printing seam.
//
// This is the security boundary. The PRIVATE material (BIP39 mnemonic + seed) is
// born, embossed (streamed to the printer), and zeroized entirely inside
// wallet_print(); it is never returned to the caller and never stored in UiState.
// Only the public addresses come back.
//
// The real key generation lives behind WALLET_REAL_CRYPTO (device, trezor-crypto +
// SAR-ADC entropy); without it (the simulator) a fixed BIP39 test mnemonic is used
// so the UI/plate format can be exercised deterministically. The C330 plate format
// is in c330_format.{h,cpp}. Keep this header std-only so both targets build.
#pragma once

#include "ui.h"   // WalletType, SendFn
#include <string>

// The public, safe-to-retain result of generating a wallet. A wallet may yield
// more than one public key (e.g. BTC+ETH -> a BTC and an ETH address), each
// shown as its own QR on the result screen.
struct WalletPublic {
    std::vector<PubKey> keys; // PubKey is defined in ui.h
};

// Generate an HD wallet, compose all C330 plates, and stream them to the printer
// via `sink` in keyprint.go send order (info -> mnemonic 13-24 -> mnemonic 1-12 ->
// addresses). Fills out_pub with the public addresses and zeroizes every byte of
// private material before returning. Returns false if any plate failed to send.
bool wallet_print(WalletType type, const std::string &label,
                  SendFn sink, WalletPublic &out_public);
