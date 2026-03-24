#include "Magic.hpp"
#include "../serialization/DataStream.hpp"

namespace minima {

std::vector<uint8_t> Magic::serialise() const {
    DataStream ds;
    // Java order: 3×MiniNumber, 1×MiniData, 3×MiniNumber, 1×MiniData
    ds.writeBytes(currentMaxTxPoWSize.serialise());
    ds.writeBytes(currentMaxKISSVMOps.serialise());
    ds.writeBytes(currentMaxTxnPerBlock.serialise());
    ds.writeBytes(currentMinTxPoWWork.serialise());     // MiniData (writeDataStream = 4-byte len + bytes)
    ds.writeBytes(desiredMaxTxPoWSize.serialise());
    ds.writeBytes(desiredMaxKISSVMOps.serialise());
    ds.writeBytes(desiredMaxTxnPerBlock.serialise());
    ds.writeBytes(desiredMinTxPoWWork.serialise());     // MiniData
    return ds.buffer();
}

Magic Magic::deserialise(const uint8_t* data, size_t& offset) {
    Magic m;
    m.currentMaxTxPoWSize   = MiniNumber::deserialise(data, offset);
    m.currentMaxKISSVMOps   = MiniNumber::deserialise(data, offset);
    m.currentMaxTxnPerBlock = MiniNumber::deserialise(data, offset);
    m.currentMinTxPoWWork   = MiniData::deserialise(data, offset);   // MiniData!
    m.desiredMaxTxPoWSize   = MiniNumber::deserialise(data, offset);
    m.desiredMaxKISSVMOps   = MiniNumber::deserialise(data, offset);
    m.desiredMaxTxnPerBlock = MiniNumber::deserialise(data, offset);
    m.desiredMinTxPoWWork   = MiniData::deserialise(data, offset);   // MiniData!
    return m;
}

} // namespace minima
