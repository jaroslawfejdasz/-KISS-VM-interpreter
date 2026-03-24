#pragma once
/**
 * Value — the runtime value type for KISS VM.
 *
 * KISS VM is a stack-based interpreter. Every value on the stack is one of:
 *   BOOLEAN  — true / false
 *   NUMBER   — MiniNumber (arbitrary precision decimal)
 *   HEX      — MiniData  (byte array)
 *   SCRIPT   — MiniString (string, used for script fragments and EXEC/MAST)
 *
 * Minima reference: src/org/minima/kissvm/values/
 */

#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniString.hpp"
#include <variant>
#include <string>
#include <stdexcept>

namespace minima::kissvm {

enum class ValueType { BOOLEAN, NUMBER, HEX, SCRIPT };

class Value {
public:
    // Constructors
    static Value boolean(bool b);
    static Value number(const MiniNumber& n);
    static Value hex(const MiniData& d);
    static Value script(const MiniString& s);
    static Value script(const std::string& s);

    ValueType type() const { return m_type; }

    bool             asBoolean() const;
    const MiniNumber& asNumber()  const;
    const MiniData&   asHex()     const;
    const MiniString& asScript()  const;

    bool isTrue()  const;  // BOOLEAN=true, NUMBER!=0, HEX not empty, SCRIPT not empty
    bool isFalse() const { return !isTrue(); }

    std::string toString() const;

private:
    ValueType m_type;
    std::variant<bool, MiniNumber, MiniData, MiniString> m_data;
};

} // namespace minima::kissvm
