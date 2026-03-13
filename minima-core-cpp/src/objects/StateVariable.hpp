#pragma once
/**
 * StateVariable — a named slot (0–255) on a coin that stores typed data.
 *
 * KISS VM can read state[N] and prevstate[N] to implement stateful contracts.
 * Type can be: VALUE (MiniNumber), HEX (MiniData), or SCRIPT (string).
 */

#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniString.hpp"
#include <variant>
#include <cstdint>

namespace minima {

class StateVariable {
public:
    enum class Type { VALUE, HEX, SCRIPT };

    StateVariable() = default;
    StateVariable(uint8_t port, const MiniNumber& v);
    StateVariable(uint8_t port, const MiniData& v);
    StateVariable(uint8_t port, const MiniString& v);

    uint8_t port() const { return m_port; }
    Type    type() const { return m_type; }

    const MiniNumber& asValue()  const;
    const MiniData&   asHex()    const;
    const MiniString& asScript() const;

    std::vector<uint8_t> serialise() const;
    static StateVariable deserialise(const uint8_t* data, size_t& offset);

private:
    uint8_t m_port{0};
    Type    m_type{Type::VALUE};
    std::variant<MiniNumber, MiniData, MiniString> m_data;
};

} // namespace minima
