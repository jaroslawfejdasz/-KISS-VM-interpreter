#pragma once
/**
 * Magic — consensus parameters embedded in every TxHeader.
 *
 * Minima Java reference: src/org/minima/objects/Magic.java
 *
 * Wire format (writeDataStream) — EXACT Java order:
 *   mCurrentMaxTxPoWSize     : MiniNumber
 *   mCurrentMaxKISSVMOps     : MiniNumber
 *   mCurrentMaxTxnPerBlock   : MiniNumber
 *   mCurrentMinTxPoWWork     : MiniData   ← NOT MiniNumber!
 *   mDesiredMaxTxPoWSize     : MiniNumber
 *   mDesiredMaxKISSVMOps     : MiniNumber
 *   mDesiredMaxTxnPerBlock   : MiniNumber
 *   mDesiredMinTxPoWWork     : MiniData   ← NOT MiniNumber!
 */

#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"
#include <vector>
#include <cstdint>

namespace minima {

// Default MIN_TXPOW_WORK — 32-byte hash target (Java: Crypto.MAX_HASH)
static const std::vector<uint8_t> MAGIC_DEFAULT_MIN_WORK = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

class Magic {
public:
    // Current values
    MiniNumber currentMaxTxPoWSize     = MiniNumber(int64_t(64*1024));
    MiniNumber currentMaxKISSVMOps     = MiniNumber(int64_t(1024));
    MiniNumber currentMaxTxnPerBlock   = MiniNumber(int64_t(256));
    MiniData   currentMinTxPoWWork     = MiniData(MAGIC_DEFAULT_MIN_WORK);

    // Desired values
    MiniNumber desiredMaxTxPoWSize     = MiniNumber(int64_t(64*1024));
    MiniNumber desiredMaxKISSVMOps     = MiniNumber(int64_t(1024));
    MiniNumber desiredMaxTxnPerBlock   = MiniNumber(int64_t(256));
    MiniData   desiredMinTxPoWWork     = MiniData(MAGIC_DEFAULT_MIN_WORK);

    Magic() = default;

    std::vector<uint8_t> serialise() const;
    static Magic         deserialise(const uint8_t* data, size_t& offset);
    static Magic         deserialise(const std::vector<uint8_t>& data, size_t& offset) {
        size_t off = 0; return deserialise(data.data(), off);
    }

    bool operator==(const Magic& o) const {
        return currentMaxTxPoWSize   == o.currentMaxTxPoWSize &&
               currentMaxKISSVMOps   == o.currentMaxKISSVMOps &&
               currentMaxTxnPerBlock == o.currentMaxTxnPerBlock &&
               currentMinTxPoWWork   == o.currentMinTxPoWWork &&
               desiredMaxTxPoWSize   == o.desiredMaxTxPoWSize &&
               desiredMaxKISSVMOps   == o.desiredMaxKISSVMOps &&
               desiredMaxTxnPerBlock == o.desiredMaxTxnPerBlock &&
               desiredMinTxPoWWork   == o.desiredMinTxPoWWork;
    }
};

} // namespace minima
