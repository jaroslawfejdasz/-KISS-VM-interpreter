#pragma once
/**
 * MiniByte — single-byte wrapper for Minima wire protocol.
 * Port of org.minima.objects.base.MiniByte.
 * Wire format: 1 byte (raw value)
 */
#include <cstdint>
#include <vector>

namespace minima {

class MiniByte {
public:
    MiniByte() = default;
    explicit MiniByte(uint8_t v) : m_value(v) {}

    uint8_t value() const { return m_value; }
    void    setValue(uint8_t v) { m_value = v; }

    bool operator==(const MiniByte& o) const { return m_value == o.m_value; }
    bool operator!=(const MiniByte& o) const { return m_value != o.m_value; }

    std::vector<uint8_t> serialise() const { return { m_value }; }

    static MiniByte deserialise(const uint8_t* data, size_t& offset) {
        return MiniByte(data[offset++]);
    }

private:
    uint8_t m_value{0};
};

} // namespace minima
