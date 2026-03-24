#include "StateVariable.hpp"
#include "../serialization/DataStream.hpp"
#include <stdexcept>

namespace minima {

// ── Constructors ──────────────────────────────────────────────────────────────

StateVariable::StateVariable(uint8_t port, const MiniNumber& v)
    : m_port(port), m_type(Type::VALUE), m_data(v) {}

StateVariable::StateVariable(uint8_t port, const MiniData& v)
    : m_port(port), m_type(Type::HEX), m_data(v) {}

StateVariable::StateVariable(uint8_t port, const MiniString& v)
    : m_port(port), m_type(Type::SCRIPT), m_data(v) {}

// ── Accessors ─────────────────────────────────────────────────────────────────

const MiniNumber& StateVariable::asValue() const {
    if (m_type != Type::VALUE)
        throw std::runtime_error("StateVariable: not a VALUE type");
    return std::get<MiniNumber>(m_data);
}

const MiniData& StateVariable::asHex() const {
    if (m_type != Type::HEX)
        throw std::runtime_error("StateVariable: not a HEX type");
    return std::get<MiniData>(m_data);
}

const MiniString& StateVariable::asScript() const {
    if (m_type != Type::SCRIPT)
        throw std::runtime_error("StateVariable: not a SCRIPT type");
    return std::get<MiniString>(m_data);
}

// ── Serialisation ─────────────────────────────────────────────────────────────
// Wire format:
//   port  : uint8
//   type  : uint8  (0=VALUE, 1=HEX, 2=SCRIPT)
//   data  : type-dependent (MiniNumber / MiniData / MiniString serialise())

std::vector<uint8_t> StateVariable::serialise() const {
    DataStream ds;
    ds.writeUInt8(m_port);
    ds.writeUInt8(static_cast<uint8_t>(m_type));

    switch (m_type) {
        case Type::VALUE: {
            auto b = std::get<MiniNumber>(m_data).serialise();
            ds.writeBytes(b);
            break;
        }
        case Type::HEX: {
            auto b = std::get<MiniData>(m_data).serialise();
            ds.writeBytes(b);
            break;
        }
        case Type::SCRIPT: {
            auto b = std::get<MiniString>(m_data).serialise();
            ds.writeBytes(b);
            break;
        }
    }
    return ds.buffer();
}

StateVariable StateVariable::deserialise(const uint8_t* data, size_t& offset) {
    uint8_t port = data[offset++];
    uint8_t typeB = data[offset++];

    Type t = static_cast<Type>(typeB);
    switch (t) {
        case Type::VALUE: {
            auto n = MiniNumber::deserialise(data, offset);
            return StateVariable(port, n);
        }
        case Type::HEX: {
            auto d = MiniData::deserialise(data, offset);
            return StateVariable(port, d);
        }
        case Type::SCRIPT: {
            auto s = MiniString::deserialise(data, offset);
            return StateVariable(port, s);
        }
        default:
            throw std::runtime_error("StateVariable: unknown type byte");
    }
}

} // namespace minima
