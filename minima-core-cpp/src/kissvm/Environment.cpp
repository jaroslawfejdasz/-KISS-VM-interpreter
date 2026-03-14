#include "Environment.hpp"
#include "../types/MiniData.hpp"

namespace minima::kissvm {

void Environment::set(const std::string& name, const Value& v) {
    m_vars[name] = v;
}

std::optional<Value> Environment::get(const std::string& name) const {
    auto it = m_vars.find(name);
    if (it == m_vars.end()) return std::nullopt;
    return it->second;
}

bool Environment::has(const std::string& name) const {
    return m_vars.count(name) > 0;
}

void Environment::setBlock(const MiniNumber& blockNum) {
    set("@BLOCK", Value::number(blockNum));
}
void Environment::setCoinage(const MiniNumber& age) {
    set("@COINAGE", Value::number(age));
}
void Environment::setAmount(const MiniNumber& amt) {
    set("@AMOUNT", Value::number(amt));
}
void Environment::setAddress(const MiniData& addr) {
    set("@ADDRESS", Value::hex(addr));
}
void Environment::setScript(const std::string& script) {
    set("@SCRIPT", Value::script(script));
}
void Environment::setTotIn(const MiniNumber& total) {
    set("@TOTIN", Value::number(total));
}
void Environment::setTotOut(const MiniNumber& total) {
    set("@TOTOUT", Value::number(total));
}

} // namespace minima::kissvm
