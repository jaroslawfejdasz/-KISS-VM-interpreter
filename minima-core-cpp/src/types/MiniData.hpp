#pragma once
/**
 * MiniData — arbitrary-length byte array (HEX type in KISS VM).
 *
 * Used for: public keys, signatures, hashes, script bytecode, custom data.
 * Always serialised as length-prefixed byte array on wire.
 */

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace minima {

class MiniData {
public:
    MiniData() = default;
    explicit MiniData(const std::vector<uint8_t>& bytes);
    explicit MiniData(const std::string& hexStr);  // "0xABCD..." or "ABCD..."

    // Factory
    static MiniData fromHex(const std::string& hex);
    static MiniData fromBytes(const uint8_t* data, size_t len);
    static MiniData zeroes(size_t len);

    // Access
    const std::vector<uint8_t>& bytes() const { return m_bytes; }
    size_t size() const { return m_bytes.size(); }
    bool   isEmpty() const { return m_bytes.empty(); }

    // Operations (mirror KISS VM HEX functions)
    MiniData   concat(const MiniData& other) const;
    MiniData   subset(size_t start, size_t len) const;
    MiniData   rev() const;

    // Hashing (returns 32-byte MiniData)
    MiniData   sha2() const;
    MiniData   sha3() const;

    // Comparison
    bool operator==(const MiniData& rhs) const { return m_bytes == rhs.m_bytes; }
    bool operator!=(const MiniData& rhs) const { return !(*this == rhs); }

    // Conversions
    std::string toHexString(bool prefix0x = true) const;

    // Serialisation
    std::vector<uint8_t> serialise() const;
    static MiniData      deserialise(const uint8_t* data, size_t& offset);

private:
    std::vector<uint8_t> m_bytes;
};

// Alias — 32-byte hash
using MiniHash = MiniData;

} // namespace minima
