#pragma once
/**
 * Schnorr — Minima's signature scheme.
 *
 * Minima uses Schnorr signatures over a custom elliptic curve
 * (not secp256k1 — Minima uses its own curve for quantum-resistance roadmap).
 *
 * This header provides the interface. Two backends are supported:
 *   1. OpenSSL backend (default for nodes with OpenSSL)
 *   2. Pure C++ reference backend (for FPGA / bare-metal)
 *
 * Select backend via CMake: -DMINIMA_CRYPTO_BACKEND=openssl|reference
 */

#include "../types/MiniData.hpp"
#include <array>

namespace minima::crypto {

struct KeyPair {
    MiniData privateKey;  // 32 bytes
    MiniData publicKey;   // 64 bytes (uncompressed point)
};

class Schnorr {
public:
    // Key generation
    static KeyPair generateKeyPair();
    static MiniData publicKeyFromPrivate(const MiniData& privateKey);

    // Signing
    // Returns 64-byte signature
    static MiniData sign(const MiniData& privateKey, const MiniData& message);

    // Verification
    // Returns true if signature is valid for (pubKey, message)
    static bool verify(const MiniData& publicKey,
                       const MiniData& message,
                       const MiniData& signature);

    // Aggregate (for MULTISIG n-of-n Schnorr aggregation — future)
    static MiniData aggregatePublicKeys(const std::vector<MiniData>& pubKeys);
};

} // namespace minima::crypto
