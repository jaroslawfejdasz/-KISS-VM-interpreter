#pragma once
/**
 * Magic — consensus parameters embedded in every TxHeader.
 *
 * Minima Java reference: src/org/minima/objects/Magic.java
 *
 * Wire format (writeDataStream):
 *   mCurrentMaxTxPoWSize     : MiniNumber
 *   mCurrentMaxKISSVMOps     : MiniNumber
 *   mCurrentMaxTxnPerBlock   : MiniNumber
 *   mDesiredMaxTxPoWSize     : MiniNumber
 *   mDesiredMaxKISSVMOps     : MiniNumber
 *   mDesiredMaxTxnPerBlock   : MiniNumber
 */

#include "../types/MiniNumber.hpp"
#include <vector>
#include <cstdint>

namespace minima {

class Magic {
public:
    MiniNumber currentMaxTxPoWSize     = MiniNumber(int64_t(64*1024));
    MiniNumber currentMaxKISSVMOps     = MiniNumber(int64_t(1024));
    MiniNumber currentMaxTxnPerBlock   = MiniNumber(int64_t(256));
    MiniNumber desiredMaxTxPoWSize     = MiniNumber(int64_t(64*1024));
    MiniNumber desiredMaxKISSVMOps     = MiniNumber(int64_t(1024));
    MiniNumber desiredMaxTxnPerBlock   = MiniNumber(int64_t(256));

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
               desiredMaxTxPoWSize   == o.desiredMaxTxPoWSize &&
               desiredMaxKISSVMOps   == o.desiredMaxKISSVMOps &&
               desiredMaxTxnPerBlock == o.desiredMaxTxnPerBlock;
    }
};

} // namespace minima
