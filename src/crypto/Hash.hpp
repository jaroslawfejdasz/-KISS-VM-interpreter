#pragma once
/**
 * Hash — SHA2 and SHA3 implementations.
 *
 * Minima uses:
 *  - SHA3-256 for: addresses, TxPoW IDs, body hashes, coin IDs
 *  - SHA2-256 for: PoW mining hash (in TxHeader)
 *
 * Interface is the same for both — input bytes, output 32 bytes.
 */

#include "../types/MiniData.hpp"
#include <cstdint>
#include <vector>

namespace minima::crypto {

class Hash {
public:
    // Returns 32-byte digest
    static MiniData sha2_256(const uint8_t* data, size_t len);
    static MiniData sha3_256(const uint8_t* data, size_t len);

    // Convenience overloads
    static MiniData sha2_256(const std::vector<uint8_t>& data);
    static MiniData sha3_256(const std::vector<uint8_t>& data);
    static MiniData sha2_256(const MiniData& data);
    static MiniData sha3_256(const MiniData& data);

    // Double-hash (used for TxPoW ID)
    static MiniData sha3_256_double(const std::vector<uint8_t>& data);
};

} // namespace minima::crypto
