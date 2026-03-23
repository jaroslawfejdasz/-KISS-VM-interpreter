#include "Value.hpp"
#include <stdexcept>

namespace minima::kissvm {

Value Value::boolean(bool b) {
    Value v; v.m_type = ValueType::BOOLEAN; v.m_data = b; return v;
}
Value Value::number(const MiniNumber& n) {
    Value v; v.m_type = ValueType::NUMBER; v.m_data = n; return v;
}
Value Value::hex(const MiniData& d) {
    Value v; v.m_type = ValueType::HEX; v.m_data = d; return v;
}
Value Value::script(const MiniString& s) {
    Value v; v.m_type = ValueType::SCRIPT; v.m_data = s; return v;
}
Value Value::script(const std::string& s) {
    return script(MiniString(s));
}

bool Value::asBoolean() const {
    if (m_type != ValueType::BOOLEAN)
        throw std::runtime_error("Value: expected BOOLEAN, got " + std::to_string((int)m_type));
    return std::get<bool>(m_data);
}

const MiniNumber& Value::asNumber() const {
    if (m_type != ValueType::NUMBER)
        throw std::runtime_error("Value: expected NUMBER");
    return std::get<MiniNumber>(m_data);
}

const MiniData& Value::asHex() const {
    if (m_type != ValueType::HEX)
        throw std::runtime_error("Value: expected HEX");
    return std::get<MiniData>(m_data);
}

const MiniString& Value::asScript() const {
    if (m_type != ValueType::SCRIPT)
        throw std::runtime_error("Value: expected SCRIPT");
    return std::get<MiniString>(m_data);
}

bool Value::isTrue() const {
    switch (m_type) {
        case ValueType::BOOLEAN: return std::get<bool>(m_data);
        case ValueType::NUMBER:  return !std::get<MiniNumber>(m_data).isZero();
        case ValueType::HEX:     return !std::get<MiniData>(m_data).empty();
        case ValueType::SCRIPT:  return !std::get<MiniString>(m_data).empty();
    }
    return false;
}

std::string Value::toString() const {
    switch (m_type) {
        case ValueType::BOOLEAN: return std::get<bool>(m_data) ? "TRUE" : "FALSE";
        case ValueType::NUMBER:  return std::get<MiniNumber>(m_data).toString();
        case ValueType::HEX:     return std::get<MiniData>(m_data).toHexString();
        case ValueType::SCRIPT:  return std::get<MiniString>(m_data).str();
    }
    return "";
}

} // namespace minima::kissvm
