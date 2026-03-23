#pragma once
/**
 * DataStream — Minima wire-format serialiser/deserialiser.
 *
 * Minima uses a simple big-endian binary format for all protocol objects.
 * This class provides the read/write primitives used by all serialise() methods.
 *
 * Wire format conventions:
 *  - Integers:    big-endian, 1/2/4/8 bytes
 *  - MiniNumber:  length-prefixed decimal string (1 byte length + chars)
 *  - MiniData:    4-byte length prefix + raw bytes
 *  - MiniString:  2-byte length prefix + UTF-8 bytes
 *  - Booleans:    1 byte (0x00 = false, 0x01 = true)
 */

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace minima {

class DataStream {
public:
    // ── Write ─────────────────────────────────────────────────────────────────

    DataStream() = default;  // write mode
    void writeUInt8 (uint8_t  v);
    void writeUInt16(uint16_t v);   // big-endian
    void writeUInt32(uint32_t v);   // big-endian
    void writeUInt64(uint64_t v);   // big-endian
    void writeBool  (bool v);
    void writeBytes (const uint8_t* data, size_t len);
    void writeBytes (const std::vector<uint8_t>& data);
    void writeLengthPrefixedBytes(const std::vector<uint8_t>& data);  // 4-byte len + bytes
    void writeLengthPrefixedString(const std::string& s);             // 2-byte len + utf8

    const std::vector<uint8_t>& buffer() const { return m_buf; }

    // ── Read ──────────────────────────────────────────────────────────────────

    explicit DataStream(const uint8_t* data, size_t len);

    uint8_t              readUInt8();
    uint16_t             readUInt16();
    uint32_t             readUInt32();
    uint64_t             readUInt64();
    bool                 readBool();
    std::vector<uint8_t> readBytes(size_t n);
    std::vector<uint8_t> readLengthPrefixedBytes();
    std::string          readLengthPrefixedString();

    size_t remaining() const { return m_len - m_pos; }
    size_t position()  const { return m_pos; }

private:
    // Write mode
    std::vector<uint8_t> m_buf;

    // Read mode
    const uint8_t* m_data{nullptr};
    size_t         m_len{0};
    size_t         m_pos{0};

    void checkAvailable(size_t n) const;
};

} // namespace minima
