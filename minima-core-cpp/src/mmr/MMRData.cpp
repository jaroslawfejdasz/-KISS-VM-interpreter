#include "MMRData.hpp"
#include "../crypto/Hash.hpp"

namespace minima {

MMRData MMRData::combine(const MMRData& left, const MMRData& right) {
    const auto& lb = left.m_data.bytes();
    const auto& rb = right.m_data.bytes();
    std::vector<uint8_t> combined;
    combined.reserve(lb.size() + rb.size());
    combined.insert(combined.end(), lb.begin(), lb.end());
    combined.insert(combined.end(), rb.begin(), rb.end());
    return MMRData(crypto::Hash::sha3_256(combined));
}

void MMRData::serialise(DataStream& ds) const {
    auto dataBytes  = m_data.serialise();
    auto valueBytes = m_value.serialise();
    ds.writeUInt32(static_cast<uint32_t>(dataBytes.size()));
    for (auto b : dataBytes)  ds.writeUInt8(b);
    ds.writeUInt32(static_cast<uint32_t>(valueBytes.size()));
    for (auto b : valueBytes) ds.writeUInt8(b);
    ds.writeUInt8(m_spent ? 1 : 0);
}

void MMRData::deserialise(DataStream& ds) {
    uint32_t dlen = ds.readUInt32();
    std::vector<uint8_t> dbuf(dlen);
    for (auto& b : dbuf) b = ds.readUInt8();
    size_t off = 0;
    m_data = MiniData::deserialise(dbuf.data(), off);

    uint32_t vlen = ds.readUInt32();
    std::vector<uint8_t> vbuf(vlen);
    for (auto& b : vbuf) b = ds.readUInt8();
    off = 0;
    m_value = MiniNumber::deserialise(vbuf.data(), off);

    m_spent = (ds.readUInt8() != 0);
}

} // namespace minima
