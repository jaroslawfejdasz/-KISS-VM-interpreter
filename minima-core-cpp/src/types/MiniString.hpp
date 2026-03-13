#pragma once
/**
 * MiniString — UTF-8 string type used in KISS VM scripts and state variables.
 *
 * Wrapper around std::string with Minima-specific serialisation.
 */

#include <string>
#include <vector>
#include <cstdint>

namespace minima {

class MiniString {
public:
    MiniString() = default;
    explicit MiniString(const std::string& s) : m_value(s) {}
    explicit MiniString(std::string&& s) : m_value(std::move(s)) {}

    const std::string& str() const { return m_value; }
    size_t length() const { return m_value.size(); }
    bool   isEmpty() const { return m_value.empty(); }

    bool operator==(const MiniString& rhs) const { return m_value == rhs.m_value; }
    bool operator!=(const MiniString& rhs) const { return !(*this == rhs); }

    // KISS VM string operations
    MiniString concat(const MiniString& rhs) const;
    MiniString subset(size_t start, size_t len) const;

    std::vector<uint8_t> serialise() const;
    static MiniString    deserialise(const uint8_t* data, size_t& offset);

private:
    std::string m_value;
};

} // namespace minima
