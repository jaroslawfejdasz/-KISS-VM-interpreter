/**
 * TxPoW.cpp
 *
 * ID   = SHA3(SHA3(header_bytes))  — double SHA3
 * PoW  = SHA2(header_bytes)        — single SHA2
 * Comparison: big-endian unsigned integer, lower = more work
 */
#include "TxPoW.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"
#include <algorithm>

namespace minima {

MiniData TxPoW::computeID() const {
    auto hb = m_header.serialise();
    return crypto::Hash::sha3_256_double(hb);
}

MiniData TxPoW::getPoWHash() const {
    auto hb = m_header.serialise();
    return crypto::Hash::sha2_256(hb);
}

// a <= b as big-endian unsigned integers
bool TxPoW::hashLE(const MiniData& a, const MiniData& b) {
    const auto& ab = a.bytes();
    const auto& bb = b.bytes();
    size_t maxLen = std::max(ab.size(), bb.size());
    for (size_t i = 0; i < maxLen; ++i) {
        size_t ia = i + ab.size() < maxLen ? 0 : i - (maxLen - ab.size());
        size_t ib = i + bb.size() < maxLen ? 0 : i - (maxLen - bb.size());
        uint8_t av = (i < maxLen - ab.size()) ? 0 : ab[ia];
        uint8_t bv = (i < maxLen - bb.size()) ? 0 : bb[ib];
        if (av < bv) return true;
        if (av > bv) return false;
    }
    return true; // equal
}

bool TxPoW::isBlock() const {
    return hashLE(getPoWHash(), m_header.blockDifficulty);
}

bool TxPoW::isTransaction() const {
    return hashLE(getPoWHash(), m_body.txnDifficulty);
}

int TxPoW::getSuperLevel() const {
    // Simple: count leading zero bytes beyond block difficulty
    const auto& diff = m_header.blockDifficulty.bytes();
    const auto& id   = computeID().bytes();
    int level = 0;
    size_t len = std::min(diff.size(), id.size());
    for (size_t i = 0; i < len; ++i) {
        if (id[i] == 0 && diff[i] != 0) { ++level; }
        else break;
    }
    return level;
}

void TxPoW::mine() {
    m_header.incrementNonce();
    m_body.resetPRNG();
}

std::vector<uint8_t> TxPoW::serialise(bool includeBody) const {
    DataStream ds;
    ds.writeBytes(m_header.serialise());
    ds.writeUInt8(includeBody ? 0x01 : 0x00);
    if (includeBody) ds.writeBytes(m_body.serialise());
    return ds.buffer();
}

TxPoW TxPoW::deserialise(const uint8_t* data, size_t& offset) {
    TxPoW t;
    t.m_header = TxHeader::deserialise(data, offset);
    uint8_t present = data[offset++];
    if (present) t.m_body = TxBody::deserialise(data, offset);
    return t;
}

} // namespace minima
