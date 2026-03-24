#pragma once
/**
 * Winternitz One-Time Signature Scheme (WOTS)
 * Java-exact port of BouncyCastle WinternitzOTSignature/WinternitzOTSVerify
 * 
 * Parameters (matching Minima Java):
 *   W = 8            (Winternitz parameter)
 *   HASH_SIZE = 32   (SHA3-256 output bytes)
 *   KEY_SIZE  = 32   (private key element size)
 * 
 * Derived constants:
 *   keyBlocks = ceil(HASH_SIZE * 8 / log2(W))  = ceil(256/3) = 86
 *   checksumBlocks = ceil(log2(keyBlocks*(W-1)+1) / log2(W)) = ceil(log2(86*7+1)/3) = ceil(9.24/3) = 4
 *   sigLen = (keyBlocks + checksumBlocks) * HASH_SIZE = 90 * 32 = 2880 bytes
 */

#include "Hash.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace minima::crypto {

class Winternitz {
public:
    // BouncyCastle-exact constants (W=8, SHA3-256)
    static constexpr int W           = 8;
    static constexpr int HASH_BITS   = 256;
    static constexpr int HASH_SIZE   = HASH_BITS / 8;   // 32 bytes
    static constexpr int LOG_W       = 3;                // log2(8) = 3
    static constexpr int KEY_BLOCKS  = (HASH_BITS + LOG_W - 1) / LOG_W;  // ceil(256/3) = 86
    // checksum: ceil( log2(KEY_BLOCKS*(W-1)+1) / LOG_W )
    // KEY_BLOCKS*(W-1)+1 = 86*7+1 = 603, log2(603)≈9.24, ceil(9.24/3) = 4
    static constexpr int CHKSUM_BLOCKS = 4;
    static constexpr int SIG_BLOCKS  = KEY_BLOCKS + CHKSUM_BLOCKS; // 90
    static constexpr int KEY_SIZE    = HASH_SIZE;                   // 32 bytes per element
    static constexpr int PUBKEY_SIZE = SIG_BLOCKS * KEY_SIZE;       // 90 * 32 = 2880 bytes
    static constexpr int SIG_SIZE    = SIG_BLOCKS * KEY_SIZE;       // same: 2880 bytes

    using Hash32  = std::array<uint8_t, HASH_SIZE>;
    using PubKey  = std::vector<uint8_t>;   // PUBKEY_SIZE bytes
    using SigData = std::vector<uint8_t>;   // SIG_SIZE bytes

    // ── Key generation ────────────────────────────────────────────────────
    /**
     * Derive the public key from a private seed.
     * BouncyCastle derives private key elements as:
     *   privElem[i] = SHA3-256(seed || i_big_endian_4bytes)
     * Then pubElem[i] = hash^(W-1)(privElem[i])
     */
    static PubKey generatePublicKey(const std::vector<uint8_t>& seed) {
        PubKey pubkey(PUBKEY_SIZE);
        for (int i = 0; i < SIG_BLOCKS; ++i) {
            Hash32 elem = privateElement(seed, i);
            Hash32 pub  = chainHash(elem, W - 1);
            std::memcpy(pubkey.data() + i * KEY_SIZE, pub.data(), KEY_SIZE);
        }
        return pubkey;
    }

    // ── Signing ──────────────────────────────────────────────────────────
    /**
     * Sign msg (32 bytes) with seed.
     * Produces SIG_SIZE byte signature.
     */
    static SigData sign(const std::vector<uint8_t>& seed,
                        const std::vector<uint8_t>& msg) {
        if (msg.size() < HASH_SIZE)
            throw std::invalid_argument("WOTS sign: message must be 32 bytes");

        // 1. Expand message hash into base-W digits
        auto digits = messageDigits(msg);

        // 2. Append checksum digits
        appendChecksum(digits);

        // 3. For each digit d[i]: sig[i] = hash^d[i](priv[i])
        SigData sig(SIG_SIZE);
        for (int i = 0; i < SIG_BLOCKS; ++i) {
            Hash32 priv = privateElement(seed, i);
            Hash32 s    = chainHash(priv, digits[i]);
            std::memcpy(sig.data() + i * KEY_SIZE, s.data(), KEY_SIZE);
        }
        return sig;
    }

    // ── Verification ─────────────────────────────────────────────────────
    /**
     * Recover public key from signature + message.
     * Returns recovered pubkey — caller compares to stored pubkey.
     * (Mirrors BouncyCastle WinternitzOTSVerify.Verify())
     */
    static PubKey recover(const std::vector<uint8_t>& msg,
                          const SigData& sig) {
        if (sig.size() < static_cast<size_t>(SIG_SIZE))
            throw std::invalid_argument("WOTS recover: invalid signature size");

        auto digits = messageDigits(msg);
        appendChecksum(digits);

        PubKey recovered(PUBKEY_SIZE);
        for (int i = 0; i < SIG_BLOCKS; ++i) {
            Hash32 s;
            std::memcpy(s.data(), sig.data() + i * KEY_SIZE, KEY_SIZE);
            // Each sig element was hashed d[i] times from private key
            // Public key = hash^(W-1-d[i])(sig[i])
            Hash32 pub = chainHash(s, (W - 1) - digits[i]);
            std::memcpy(recovered.data() + i * KEY_SIZE, pub.data(), KEY_SIZE);
        }
        return recovered;
    }

    static bool verify(const PubKey& pubkey,
                       const std::vector<uint8_t>& msg,
                       const SigData& sig) {
        PubKey recovered = recover(msg, sig);
        return recovered == pubkey;
    }

private:
    // ── Internals ────────────────────────────────────────────────────────

    /**
     * Derive private key element i from seed.
     * BouncyCastle: SHA3-256(seed || toBigEndian4(i))
     */
    static Hash32 privateElement(const std::vector<uint8_t>& seed, int i) {
        std::vector<uint8_t> data;
        data.reserve(seed.size() + 4);
        data.insert(data.end(), seed.begin(), seed.end());
        data.push_back(static_cast<uint8_t>((i >> 24) & 0xFF));
        data.push_back(static_cast<uint8_t>((i >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((i >>  8) & 0xFF));
        data.push_back(static_cast<uint8_t>( i        & 0xFF));
        return sha3_256(data);
    }

    /**
     * Apply SHA3-256 n times (chain hash).
     * hash^0(x) = x, hash^n(x) = SHA3(hash^(n-1)(x))
     */
    static Hash32 chainHash(Hash32 val, int n) {
        for (int j = 0; j < n; ++j) {
            std::vector<uint8_t> v(val.begin(), val.end());
            val = sha3_256(v);
        }
        return val;
    }

    /**
     * Convert 32-byte message into base-W (base-8) digits.
     * Each byte → ceil(8/3) = 3 digits (but last only 2 bits of last byte used).
     * BouncyCastle: splits each byte into groups of LOG_W bits, MSB first.
     * For W=8: 3 bits per digit, 256/3 = 85.33... → 86 digits.
     */
    static std::vector<int> messageDigits(const std::vector<uint8_t>& msg) {
        std::vector<int> digits;
        digits.reserve(KEY_BLOCKS);

        int bits = 0;
        int buf  = 0;
        for (uint8_t b : msg) {
            buf  = (buf << 8) | b;
            bits += 8;
            while (bits >= LOG_W) {
                bits -= LOG_W;
                digits.push_back((buf >> bits) & (W - 1));
                if (static_cast<int>(digits.size()) == KEY_BLOCKS) goto done;
            }
        }
    done:
        // Pad to KEY_BLOCKS if needed
        while (static_cast<int>(digits.size()) < KEY_BLOCKS)
            digits.push_back(0);
        return digits;
    }

    /**
     * Compute and append checksum digits.
     * checksum = sum(W-1-d[i]) for i in [0, KEY_BLOCKS)
     * Encode checksum in base-W, big-endian, CHKSUM_BLOCKS digits.
     */
    static void appendChecksum(std::vector<int>& digits) {
        int checksum = 0;
        for (int i = 0; i < KEY_BLOCKS; ++i)
            checksum += (W - 1) - digits[i];

        // Encode in base-W, big-endian, CHKSUM_BLOCKS digits
        std::vector<int> cdigits(CHKSUM_BLOCKS);
        for (int i = CHKSUM_BLOCKS - 1; i >= 0; --i) {
            cdigits[i] = checksum & (W - 1);
            checksum >>= LOG_W;
        }
        digits.insert(digits.end(), cdigits.begin(), cdigits.end());
    }

    static Hash32 sha3_256(const std::vector<uint8_t>& data) {
        auto h = Hash::sha3_256(data.data(), data.size());
        Hash32 out;
        std::memcpy(out.data(), h.data(), HASH_SIZE);
        return out;
    }
};

} // namespace minima::crypto
