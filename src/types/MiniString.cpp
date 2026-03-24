#include "MiniString.hpp"
#include <stdexcept>

namespace minima {

MiniString MiniString::concat(const MiniString& rhs) const {
    return MiniString(m_value + rhs.m_value);
}

MiniString MiniString::subset(size_t start, size_t len) const {
    if (start + len > m_value.size())
        throw std::out_of_range("MiniString::subset out of range");
    return MiniString(m_value.substr(start, len));
}

std::vector<uint8_t> MiniString::serialise() const {
    // Java: MiniData(getData()).writeDataStream(out)
    // MiniData.writeDataStream: [4-byte BE int: length][raw bytes]
    if (m_value.size() > 0x0FFFFFFF)
        throw std::runtime_error("MiniString too long");
    uint32_t len = static_cast<uint32_t>(m_value.size());
    std::vector<uint8_t> out;
    out.push_back((len >> 24) & 0xFF);
    out.push_back((len >> 16) & 0xFF);
    out.push_back((len >>  8) & 0xFF);
    out.push_back( len        & 0xFF);
    for (char c : m_value) out.push_back(static_cast<uint8_t>(c));
    return out;
}

MiniString MiniString::deserialise(const uint8_t* data, size_t& offset) {
    // Java: MiniData.readDataStream -> readInt (4 bytes), then readFully
    uint32_t len = (static_cast<uint32_t>(data[offset])   << 24) |
                   (static_cast<uint32_t>(data[offset+1]) << 16) |
                   (static_cast<uint32_t>(data[offset+2]) <<  8) |
                    static_cast<uint32_t>(data[offset+3]);
    offset += 4;
    if (len > 0x0FFFFFFF)
        throw std::runtime_error("MiniString::deserialise: length too large");
    std::string s(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return MiniString(s);
}

} // namespace minima
