#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace minima {

class MiniData {
public:
    static constexpr int MINIMA_MAX_HASH_LENGTH     = 64;
    static constexpr int MINIMA_MAX_MINIDATA_LENGTH  = 512 * 1024 * 1024;

    static const MiniData ZERO_TXPOWID;
    static const MiniData ONE_TXPOWID;

    MiniData() = default;
    explicit MiniData(const std::vector<uint8_t>& b) : m_bytes(b) {}
    explicit MiniData(std::vector<uint8_t>&& b) : m_bytes(std::move(b)) {}

    static MiniData fromHex(const std::string& hex);
    static MiniData fromBytes(const uint8_t* data, size_t len) {
        return MiniData(std::vector<uint8_t>(data, data + len));
    }
    static MiniData withMinLength(const std::vector<uint8_t>& data, int minLen);

    // Java getRandomData(int len) — for PRNG initialisation
    static MiniData getRandomData(int len);

    const std::vector<uint8_t>& bytes() const { return m_bytes; }
    const uint8_t* data() const { return m_bytes.data(); }
    size_t size() const { return m_bytes.size(); }
    int    getLength() const { return (int)m_bytes.size(); }
    bool   empty() const { return m_bytes.empty(); }

    bool isEqual(const MiniData& o) const { return m_bytes == o.m_bytes; }
    bool isLess(const MiniData& o) const;
    bool isMore(const MiniData& o) const { return o.isLess(*this); }

    std::string to0xString() const;
    std::string toHexString(bool prefix = true) const;

    MiniData sha3() const;  // NIST SHA3-256
    MiniData sha2() const;  // SHA-256

    // Normal serialisation: [int32 BE length][bytes]
    std::vector<uint8_t> serialise() const;
    static MiniData deserialise(const uint8_t* data, size_t& offset);
    static MiniData deserialise(const uint8_t* data, size_t& offset, int expectedSize);
    static MiniData deserialise(const std::vector<uint8_t>& data, size_t& offset) {
        return deserialise(data.data(), offset);
    }

    // Hash serialisation: same but max 64 bytes enforced
    std::vector<uint8_t> serialiseHash() const;
    static MiniData deserialiseHash(const uint8_t* data, size_t& offset);
    static MiniData deserialiseHash(const std::vector<uint8_t>& data, size_t& offset) {
        return deserialiseHash(data.data(), offset);
    }

private:
    std::vector<uint8_t> m_bytes;
};

using MiniHash = MiniData;

} // namespace minima
