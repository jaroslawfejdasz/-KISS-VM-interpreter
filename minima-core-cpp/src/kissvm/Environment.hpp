#pragma once
/**
 * Environment — variable scope for KISS VM execution.
 *
 * Holds:
 *  - Script-local variables (LET x = ...)
 *  - Transaction context injected before execution:
 *      @BLOCK, @COINAGE, @AMOUNT, @ADDRESS, @SCRIPT,
 *      @TOTIN, @TOTOUT, @INPUT, @OUTPUT
 */

#include "Value.hpp"
#include <unordered_map>
#include <string>
#include <optional>

namespace minima::kissvm {

class Environment {
public:
    Environment() = default;

    // Variable access
    void                   set(const std::string& name, const Value& v);
    std::optional<Value>   get(const std::string& name) const;
    bool                   has(const std::string& name) const;

    // Transaction context helpers
    void setBlock   (const MiniNumber& blockNum);
    void setCoinage (const MiniNumber& age);
    void setAmount  (const MiniNumber& amt);
    void setAddress (const MiniData& addr);
    void setScript  (const std::string& script);
    void setTotIn   (const MiniNumber& total);
    void setTotOut  (const MiniNumber& total);

private:
    std::unordered_map<std::string, Value> m_vars;
};

} // namespace minima::kissvm
