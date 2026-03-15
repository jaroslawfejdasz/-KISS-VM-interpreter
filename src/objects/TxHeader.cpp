#include "TxHeader.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"

namespace minima {

MiniData TxHeader::computeHash() const {
    auto bytes = serialise();
    return crypto::Hash::sha3_256(bytes);
}

// Wire format (mirrors Minima Java TxHeader.writeDataStream):
//   blockNumber       : MiniNumber
//   timestamp         : uint64 (big-endian ms since epoch)
//   parentID          : MiniData
//   mmrRoot           : MiniData
//   bodyHash          : MiniData
//   nonce             : uint64
//   blockDifficulty   : MiniNumber
//   txnDifficulty     : MiniNumber
//   desiredBlockSize  : MiniNumber
//   minTxPoWWork      : MiniNumber

std::vector<uint8_t> TxHeader::serialise() const {
    DataStream ds;
    ds.writeBytes(blockNumber.serialise());
    ds.writeUInt64(timestamp);
    ds.writeBytes(parentID.serialise());
    ds.writeBytes(mmrRoot.serialise());
    ds.writeBytes(bodyHash.serialise());
    ds.writeUInt64(nonce);
    ds.writeBytes(blockDifficulty.serialise());
    ds.writeBytes(txnDifficulty.serialise());
    ds.writeBytes(desiredBlockSize.serialise());
    ds.writeBytes(minTxPoWWork.serialise());
    return ds.buffer();
}

TxHeader TxHeader::deserialise(const uint8_t* data, size_t& offset) {
    TxHeader h;
    h.blockNumber = MiniNumber::deserialise(data, offset);

    // timestamp: uint64 big-endian
    uint64_t ts = 0;
    for (int i = 0; i < 8; ++i) ts = (ts << 8) | data[offset++];
    h.timestamp = ts;

    h.parentID = MiniData::deserialise(data, offset);
    h.mmrRoot  = MiniData::deserialise(data, offset);
    h.bodyHash = MiniData::deserialise(data, offset);

    uint64_t nc = 0;
    for (int i = 0; i < 8; ++i) nc = (nc << 8) | data[offset++];
    h.nonce = nc;

    h.blockDifficulty  = MiniNumber::deserialise(data, offset);
    h.txnDifficulty    = MiniNumber::deserialise(data, offset);
    h.desiredBlockSize = MiniNumber::deserialise(data, offset);
    h.minTxPoWWork     = MiniNumber::deserialise(data, offset);

    return h;
}

} // namespace minima
