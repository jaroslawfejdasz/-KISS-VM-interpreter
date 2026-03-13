#pragma once
/**
 * MiniNumber — Minima's arbitrary-precision fixed-point number type.
 *
 * Minima uses a decimal-based representation with up to 44 significant digits.
 * This type is the foundation of all value arithmetic in the protocol.
 *
 * Design (C++ port):
 *  - Backed by std::string internally (matches Java BigDecimal behaviour)
 *  - Immutable by default — all operators return new instances
 *  - No external dependencies (no GMP, no Boost)
 */

#include <string>
#include <stdexcept>
#include <cstdint>
#include <vector>

namespace minima {

class MiniNumber {
public:
    MiniNumber();
    explicit MiniNumber(const std::string& s);
    explicit MiniNumber(long long v);

    static MiniNumber zero();
    static MiniNumber one();
    static MiniNumber two();
    static MiniNumber maxAmount();  // 1,000,000,000 Minima

    // Arithmetic
    MiniNumber add(const MiniNumber& rhs) const;
    MiniNumber sub(const MiniNumber& rhs) const;
    MiniNumber mul(const MiniNumber& rhs) const;
    MiniNumber div(const MiniNumber& rhs) const;
    MiniNumber mod(const MiniNumber& rhs) const;

    // Bitwise (integer mode only — used by KISS VM)
    MiniNumber bitwiseAnd(const MiniNumber& rhs) const;
    MiniNumber bitwiseOr (const MiniNumber& rhs) const;
    MiniNumber bitwiseXor(const MiniNumber& rhs) const;
    MiniNumber shiftLeft (int bits) const;
    MiniNumber shiftRight(int bits) const;
    MiniNumber bitwiseNot() const;

    // Comparison
    bool isEqual(const MiniNumber& rhs) const;
    bool isLessThan(const MiniNumber& rhs) const;
    bool isGreaterThan(const MiniNumber& rhs) const;
    bool isLessOrEqual(const MiniNumber& rhs) const;
    bool isGreaterOrEqual(const MiniNumber& rhs) const;

    // Queries
    bool isZero()     const;
    bool isPositive() const;
    bool isNegative() const;
    bool isInteger()  const;

    // Conversions
    std::string toString()  const;
    long long   toLong()    const;

    // Serialisation (Minima wire format)
    std::vector<uint8_t> serialise() const;
    static MiniNumber    deserialise(const uint8_t* data, size_t& offset);

private:
    std::string m_value;
    static std::string normalise(const std::string& s);
    static int         compareDecimal(const std::string& a, const std::string& b);
};

} // namespace minima
