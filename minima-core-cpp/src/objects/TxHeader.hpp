#pragma once
/**
 * TxHeader — the PoW-bearing portion of a TxPoW unit.
 *
 * Only this struct is hashed during mining. Changing the nonce is the only
 * operation performed during the ~1-second user-side PoW.
 */

#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include <cstdint>

namespace minima {

class TxHeader {
public:
    // Block / chain position
    MiniNumber blockNumber;
    uint64_t   timestamp{0};      // milliseconds since epoch
    MiniData   parentID;          // parent TxPoW ID (32 bytes)
    MiniData   mmrRoot;           // MMR accumulator root (32 bytes)
    MiniData   bodyHash;          // SHA3(TxBody bytes) — commits body into header

    // PoW fields
    uint64_t   nonce{0};
    MiniNumber blockDifficulty;   // required PoW for block-level status
    MiniNumber txnDifficulty;     // required PoW for transaction broadcast

    // Magic numbers (consensus params carried in every header)
    MiniNumber desiredBlockSize;   // bytes
    MiniNumber minTxPoWWork;       // minimum PoW per transaction

    // ── Hash (the value miners search over) ──────────────────────────────────
    MiniData   computeHash() const;   // SHA3(serialise())

    std::vector<uint8_t> serialise() const;
    static TxHeader      deserialise(const uint8_t* data, size_t& offset);
};

} // namespace minima
