// Crypto wallet generation + printing seam.
//
// This is the security boundary. The PRIVATE key material is born, used (sent to
// the printer), and zeroized entirely inside generate_and_print_wallet(); it is
// never returned to the caller and never stored in UiState. Only the public part
// comes back.
//
// The actual cryptography (libsecp256k1 / Monero keygen) and the real C330 print
// payload format are OUT OF SCOPE here — the functions below are stubs with a
// stable signature so the real implementation drops in without touching ui.cpp
// or the front-ends. Keep this file std-only (no Arduino/SDL/USB headers) so it
// builds for both the device and the simulator.
#pragma once

#include "ui.h"   // WalletType, SendFn
#include <string>

// The public, safe-to-retain result of generating a wallet. A wallet may yield
// more than one public key (e.g. BTC+ETH -> a BTC and an ETH address), each
// shown as its own QR on the result screen.
struct WalletPublic {
    std::vector<PubKey> keys; // PubKey is defined in ui.h
};

// Generate a wallet, compose the printer payload, send it via `sink`, then
// zeroize every byte of private material before returning. Only the public part
// is written to `out_public`. Returns true if the payload was sent successfully.
bool generate_and_print_wallet(WalletType type, const std::string &label,
                               SendFn sink, WalletPublic &out_public);
