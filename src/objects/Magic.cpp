#include "Magic.hpp"
#include "../serialization/DataStream.hpp"

namespace minima {

std::vector<uint8_t> Magic::serialise() const {
    DataStream ds;
    ds.writeBytes(currentMaxTxPoWSize.serialise());
    ds.writeBytes(currentMaxKISSVMOps.serialise());
    ds.writeBytes(currentMaxTxnPerBlock.serialise());
    ds.writeBytes(desiredMaxTxPoWSize.serialise());
    ds.writeBytes(desiredMaxKISSVMOps.serialise());
    ds.writeBytes(desiredMaxTxnPerBlock.serialise());
    return ds.buffer();
}

Magic Magic::deserialise(const uint8_t* data, size_t& offset) {
    Magic m;
    m.currentMaxTxPoWSize   = MiniNumber::deserialise(data, offset);
    m.currentMaxKISSVMOps   = MiniNumber::deserialise(data, offset);
    m.currentMaxTxnPerBlock = MiniNumber::deserialise(data, offset);
    m.desiredMaxTxPoWSize   = MiniNumber::deserialise(data, offset);
    m.desiredMaxKISSVMOps   = MiniNumber::deserialise(data, offset);
    m.desiredMaxTxnPerBlock = MiniNumber::deserialise(data, offset);
    return m;
}

} // namespace minima
