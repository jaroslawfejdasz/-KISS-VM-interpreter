#include "DataStream.hpp"
#include <stdexcept>
#include <cstring>

namespace minima {

// ─── Write ────────────────────────────────────────────────────────────────────

void DataStream::writeUInt8(uint8_t v)   { m_buf.push_back(v); }
void DataStream::writeBool(bool v)        { m_buf.push_back(v ? 1 : 0); }

void DataStream::writeUInt16(uint16_t v) {
    m_buf.push_back((v >> 8) & 0xff);
    m_buf.push_back(v & 0xff);
}

void DataStream::writeUInt32(uint32_t v) {
    m_buf.push_back((v >> 24) & 0xff);
    m_buf.push_back((v >> 16) & 0xff);
    m_buf.push_back((v >>  8) & 0xff);
    m_buf.push_back( v        & 0xff);
}

void DataStream::writeUInt64(uint64_t v) {
    for (int i = 7; i >= 0; --i) m_buf.push_back((v >> (i*8)) & 0xff);
}

void DataStream::writeBytes(const uint8_t* data, size_t len) {
    m_buf.insert(m_buf.end(), data, data + len);
}

void DataStream::writeBytes(const std::vector<uint8_t>& data) {
    m_buf.insert(m_buf.end(), data.begin(), data.end());
}

void DataStream::writeLengthPrefixedBytes(const std::vector<uint8_t>& data) {
    writeUInt32((uint32_t)data.size());
    writeBytes(data);
}

void DataStream::writeLengthPrefixedString(const std::string& s) {
    if (s.size() > 65535) throw std::runtime_error("DataStream: string too long");
    writeUInt16((uint16_t)s.size());
    writeBytes(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

// ─── Read ─────────────────────────────────────────────────────────────────────

DataStream::DataStream(const uint8_t* data, size_t len) : m_data(data), m_len(len), m_pos(0) {}

void DataStream::checkAvailable(size_t n) const {
    if (m_pos + n > m_len)
        throw std::out_of_range("DataStream: not enough bytes");
}

uint8_t DataStream::readUInt8() {
    checkAvailable(1);
    return m_data[m_pos++];
}

bool DataStream::readBool() {
    return readUInt8() != 0;
}

uint16_t DataStream::readUInt16() {
    checkAvailable(2);
    uint16_t v = ((uint16_t)m_data[m_pos] << 8) | m_data[m_pos+1];
    m_pos += 2;
    return v;
}

uint32_t DataStream::readUInt32() {
    checkAvailable(4);
    uint32_t v = ((uint32_t)m_data[m_pos]<<24)|((uint32_t)m_data[m_pos+1]<<16)|
                 ((uint32_t)m_data[m_pos+2]<<8)|(uint32_t)m_data[m_pos+3];
    m_pos += 4;
    return v;
}

uint64_t DataStream::readUInt64() {
    checkAvailable(8);
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | m_data[m_pos++];
    return v;
}

std::vector<uint8_t> DataStream::readBytes(size_t n) {
    checkAvailable(n);
    std::vector<uint8_t> out(m_data + m_pos, m_data + m_pos + n);
    m_pos += n;
    return out;
}

std::vector<uint8_t> DataStream::readLengthPrefixedBytes() {
    uint32_t len = readUInt32();
    return readBytes(len);
}

std::string DataStream::readLengthPrefixedString() {
    uint16_t len = readUInt16();
    checkAvailable(len);
    std::string s(reinterpret_cast<const char*>(m_data + m_pos), len);
    m_pos += len;
    return s;
}

} // namespace minima
