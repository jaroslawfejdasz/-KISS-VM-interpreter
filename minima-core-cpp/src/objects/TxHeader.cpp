/**
 * TxHeader.cpp — wire-exact serialisation.
 *
 * writeHashToStream: [int32 big-endian length][bytes]
 * writeDataStream:   [MiniNumber length][bytes]
 */
#include "TxHeader.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"

namespace minima {

TxHeader::TxHeader() {
    // chainID = 0x00 (mainnet)
    chainID = MiniData(std::vector<uint8_t>{0x00});
    // blockDifficulty = 32 bytes of 0xFF (max = zero work required default)
    blockDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
    // zeros for hash fields
    mmrRoot    = MiniData(std::vector<uint8_t>(32, 0x00));
    customHash = MiniData(std::vector<uint8_t>(32, 0x00));
    bodyHash   = MiniData(std::vector<uint8_t>(32, 0x00));
    // superParents all zero
    for (auto& sp : superParents)
        sp = MiniData(std::vector<uint8_t>(32, 0x00));
}

MiniData TxHeader::computeHash() const {
    return crypto::Hash::sha3_256(serialise());
}

// writeHashToStream: int32 len (big-endian) + bytes
static void writeHash(DataStream& ds, const MiniData& d) {
    const auto& b = d.bytes();
    uint32_t len = (uint32_t)b.size();
    ds.writeUInt8((len >> 24) & 0xFF);
    ds.writeUInt8((len >> 16) & 0xFF);
    ds.writeUInt8((len >>  8) & 0xFF);
    ds.writeUInt8( len        & 0xFF);
    ds.writeBytes(b);
}

static MiniData readHash(const uint8_t* data, size_t& offset) {
    uint32_t len = 0;
    len |= (uint32_t)data[offset++] << 24;
    len |= (uint32_t)data[offset++] << 16;
    len |= (uint32_t)data[offset++] <<  8;
    len |= (uint32_t)data[offset++];
    std::vector<uint8_t> bytes(data + offset, data + offset + len);
    offset += len;
    return MiniData(bytes);
}

std::vector<uint8_t> TxHeader::serialise() const {
    DataStream ds;

    ds.writeBytes(nonce.serialise());           // MiniNumber
    ds.writeBytes(chainID.serialise());         // MiniData full
    ds.writeBytes(timeMilli.serialise());       // MiniNumber
    ds.writeBytes(blockNumber.serialise());     // MiniNumber
    ds.writeBytes(blockDifficulty.serialise()); // MiniData full

    // superParents RLE: runs of identical hashes
    const MiniData* prev  = nullptr;
    int             count = 0;
    for (int i = 0; i < CASCADE_LEVELS; ++i) {
        const MiniData& curr = superParents[i];
        if (prev == nullptr) {
            prev = &curr; count = 1;
        } else if (prev->bytes() == curr.bytes()) {
            ++count;
        } else {
            ds.writeUInt8((uint8_t)count);
            writeHash(ds, *prev);
            prev = &curr; count = 1;
        }
        if (i == CASCADE_LEVELS - 1) {
            ds.writeUInt8((uint8_t)count);
            writeHash(ds, *prev);
        }
    }

    writeHash(ds, mmrRoot);                 // writeHashToStream
    ds.writeBytes(mmrTotal.serialise());    // MiniNumber
    ds.writeBytes(magic.serialise());       // Magic (6 × MiniNumber)
    writeHash(ds, customHash);              // writeHashToStream
    writeHash(ds, bodyHash);               // writeHashToStream

    return ds.buffer();
}

TxHeader TxHeader::deserialise(const uint8_t* data, size_t& offset) {
    TxHeader h;
    h.nonce           = MiniNumber::deserialise(data, offset);
    h.chainID         = MiniData::deserialise(data, offset);
    h.timeMilli       = MiniNumber::deserialise(data, offset);
    h.blockNumber     = MiniNumber::deserialise(data, offset);
    h.blockDifficulty = MiniData::deserialise(data, offset);

    // RLE decode superParents
    int filled = 0;
    while (filled < CASCADE_LEVELS) {
        uint8_t  cnt  = data[offset++];
        MiniData hash = readHash(data, offset);
        for (int i = 0; i < (int)cnt && filled < CASCADE_LEVELS; ++i)
            h.superParents[filled++] = hash;
    }

    h.mmrRoot   = readHash(data, offset);
    h.mmrTotal  = MiniNumber::deserialise(data, offset);
    h.magic     = Magic::deserialise(data, offset);
    h.customHash = readHash(data, offset);
    h.bodyHash   = readHash(data, offset);
    return h;
}

} // namespace minima
