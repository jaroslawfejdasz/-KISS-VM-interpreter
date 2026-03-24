#pragma once
/**
 * Schnorr.hpp — Minima signature interface
 * 
 * NOTE: Minima does NOT use Schnorr/secp256k1.
 * The actual signature scheme is Winternitz OTS (WOTS) with SHA3-256, W=8.
 * This file provides a backward-compatible interface that delegates to WOTS.
 * 
 * Java reference: org.minima.objects.keys.Winternitz
 *   - WinternitzOTSignature(seed, SHA3Digest(256), W=8)
 *   - WinternitzOTSVerify(SHA3Digest(256), W=8)
 */

#include "Winternitz.hpp"
#include "../types/MiniData.hpp"
#include <stdexcept>
#include <vector>

namespace minima::crypto {

/**
 * KeyPair for Minima WOTS.
 * privateKey = 32-byte seed
 * publicKey  = 2880-byte WOTS public key (SIG_BLOCKS * KEY_SIZE)
 */
struct KeyPair {
    MiniData privateKey;   // 32-byte seed
    MiniData publicKey;    // Winternitz::PUBKEY_SIZE bytes (2880)
};

/**
 * Schnorr — public interface for Minima signatures.
 * Internally uses Winternitz OTS (WOTS), W=8, SHA3-256.
 */
class Schnorr {
public:
    // ── Key generation ────────────────────────────────────────────────────

    /**
     * Generate a new WOTS keypair from a random 32-byte seed.
     */
    static KeyPair generateKeyPair() {
        static uint64_t counter = 0xDEADBEEFCAFEBABEull;
        counter ^= (counter << 13);
        counter ^= (counter >> 7);
        counter ^= (counter << 17);

        std::vector<uint8_t> seed(32);
        for (int i = 0; i < 4; ++i) {
            uint64_t v = counter + i * 0x9E3779B97F4A7C15ull;
            for (int b = 0; b < 8; ++b)
                seed[i*8 + b] = static_cast<uint8_t>(v >> (b*8));
        }
        return fromSeed(MiniData(seed));
    }

    /**
     * Derive public key from a 32-byte private seed.
     */
    static KeyPair fromSeed(const MiniData& seed) {
        auto pub = Winternitz::generatePublicKey(seed.bytes());
        return { seed, MiniData(pub) };
    }

    /**
     * Derive only the public key.
     */
    static MiniData publicKeyFromPrivate(const MiniData& privateKey) {
        return fromSeed(privateKey).publicKey;
    }

    // ── Signing ──────────────────────────────────────────────────────────

    /**
     * Sign a 32-byte message hash with the private seed.
     * Returns SIG_SIZE (2880) byte signature.
     */
    static MiniData sign(const MiniData& privateKey,
                         const MiniData& message) {
        std::vector<uint8_t> msg = ensureHash(message);
        auto sig = Winternitz::sign(privateKey.bytes(), msg);
        return MiniData(sig);
    }

    // ── Verification ─────────────────────────────────────────────────────

    /**
     * Verify a WOTS signature.
     * pubKey    = 2880-byte WOTS public key
     * message   = 32-byte hash
     * signature = 2880-byte signature
     */
    static bool verify(const MiniData& pubKey,
                       const MiniData& message,
                       const MiniData& signature) {
        auto msg = ensureHash(message);
        return Winternitz::verify(
            pubKey.bytes(),
            msg,
            signature.bytes());
    }

    // ── Multi-sig ─────────────────────────────────────────────────────────

    /**
     * Aggregate public keys for MULTISIG.
     * In WOTS context: hash all pubkeys together → 32-byte aggregate key.
     */
    static MiniData aggregatePublicKeys(const std::vector<MiniData>& pubKeys) {
        std::vector<uint8_t> combined;
        for (const auto& pk : pubKeys) {
            const auto& b = pk.bytes();
            combined.insert(combined.end(), b.begin(), b.end());
        }
        // Hash::sha3_256 returns MiniData — use .bytes() to get vector
        return Hash::sha3_256(combined.data(), combined.size());
    }

private:
    static std::vector<uint8_t> ensureHash(const MiniData& msg) {
        const auto& b = msg.bytes();
        if (static_cast<int>(b.size()) == Winternitz::HASH_SIZE) return b;
        // Hash to 32 bytes
        return Hash::sha3_256(b.data(), b.size()).bytes();
    }
};

} // namespace minima::crypto
