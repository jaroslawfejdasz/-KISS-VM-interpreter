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
    // 2-byte length + UTF-8 bytes
    if (m_value.size() > 65535) throw std::runtime_error("MiniString too long");
    uint16_t len = (uint16_t)m_value.size();
    std::vector<uint8_t> out;
    out.push_back((len >> 8) & 0xff);
    out.push_back(len & 0xff);
    for (char c : m_value) out.push_back((uint8_t)c);
    return out;
}

MiniString MiniString::deserialise(const uint8_t* data, size_t& offset) {
    uint16_t len = ((uint16_t)data[offset] << 8) | data[offset+1];
    offset += 2;
    std::string s(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return MiniString(s);
}

} // namespace minima
