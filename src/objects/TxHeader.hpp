#pragma once
/**
 * TxHeader — PoW-bearing header of a TxPoW unit.
 *
 * Minima Java reference: src/org/minima/objects/TxHeader.java
 *
 * Wire format — EXACT Java writeDataStream order:
 *   nonce           : MiniNumber
 *   chainID         : MiniData  (full writeDataStream)
 *   timeMilli       : MiniNumber
 *   blockNumber     : MiniNumber
 *   blockDifficulty : MiniData  (full writeDataStream, 32-byte target)
 *   superParents[32]: RLE-encoded runs: [uint8 count][MiniData writeHashToStream]
 *   mmrRoot         : MiniData  (writeHashToStream = int32 len + bytes)
 *   mmrTotal        : MiniNumber
 *   magic           : Magic
 *   customHash      : MiniData  (writeHashToStream)
 *   bodyHash        : MiniData  (writeHashToStream)
 */

#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "Magic.hpp"
#include <array>
#include <cstdint>

namespace minima {

static constexpr int CASCADE_LEVELS = 32;

class TxHeader {
public:
    // Wire fields (exact Java order)
    MiniNumber nonce          = MiniNumber(int64_t(0));
    MiniData   chainID;          // 0x00 = mainnet, 0x01 = testnet
    MiniNumber timeMilli      = MiniNumber(int64_t(0));
    MiniNumber blockNumber    = MiniNumber(int64_t(0));
    MiniData   blockDifficulty;  // 32-byte hash target (lower = harder)
    std::array<MiniData, CASCADE_LEVELS> superParents;
    MiniData   mmrRoot;
    MiniNumber mmrTotal       = MiniNumber(int64_t(0));
    Magic      magic;
    MiniData   customHash;
    MiniData   bodyHash;

    TxHeader();

    MiniData computeHash() const;   // SHA3(serialise())
    void     incrementNonce()       { nonce = nonce + MiniNumber::ONE; }

    std::vector<uint8_t> serialise() const;
    static TxHeader      deserialise(const uint8_t* data, size_t& offset);
    static TxHeader      deserialise(const std::vector<uint8_t>& data, size_t& offset) {
        return deserialise(data.data(), offset);
    }
};

} // namespace minima
