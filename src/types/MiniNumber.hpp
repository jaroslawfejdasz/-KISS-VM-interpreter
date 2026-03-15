#pragma once
/**
 * MiniNumber — Java-compatible BigDecimal.
 *
 * Wire format (Java writeDataStream):
 *   [int8  scale]       — BigDecimal.scale()
 *   [uint8 len]         — byte length of unscaled BigInteger
 *   [len bytes]         — BigInteger.toByteArray() (big-endian two's-complement)
 *
 * Arithmetic: 64 significant digits, RoundingMode.DOWN (truncation toward zero)
 * Range: [-(2^64-1), +(2^64-1)]
 */
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <algorithm>

namespace minima {

// ===========================================================================
// BigInt — two's-complement big-endian (mirrors Java BigInteger.toByteArray())
// ===========================================================================
class BigInt {
public:
    std::vector<uint8_t> bytes; // Java BigInteger.toByteArray() format

    BigInt();
    explicit BigInt(int64_t v);
    explicit BigInt(const std::string& decimal);  // decimal string, optional '-'

    static BigInt fromJavaBytes(const std::vector<uint8_t>& b);
    static BigInt fromJavaBytes(const uint8_t* b, int len);
    static BigInt zero();
    static BigInt one();

    const std::vector<uint8_t>& toJavaBytes() const { return bytes; }

    bool isNegative() const { return !bytes.empty() && (bytes[0] & 0x80) != 0; }
    bool isZero() const;

    int  compareTo(const BigInt& o) const;
    bool operator==(const BigInt& o) const;
    bool operator!=(const BigInt& o) const { return !(*this == o); }
    bool operator< (const BigInt& o) const { return compareTo(o) <  0; }
    bool operator<=(const BigInt& o) const { return compareTo(o) <= 0; }
    bool operator> (const BigInt& o) const { return compareTo(o) >  0; }
    bool operator>=(const BigInt& o) const { return compareTo(o) >= 0; }

    BigInt operator+(const BigInt& o) const;
    BigInt operator-(const BigInt& o) const;
    BigInt operator*(const BigInt& o) const;
    BigInt operator/(const BigInt& o) const;
    BigInt operator%(const BigInt& o) const;
    BigInt operator-() const;

    std::string toDecimalString() const;
    int compareAbsTo(const BigInt& o) const;
    void normalise();

    // Public divmod for use by MiniNumber
    static void divmodMag_pub(const BigInt& a, const BigInt& b,
                              std::vector<uint8_t>& q, std::vector<uint8_t>& r);

private:
    static std::vector<uint8_t> addMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
    static std::vector<uint8_t> subMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
    static std::vector<uint8_t> mulMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
    static void divmodMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                          std::vector<uint8_t>& q, std::vector<uint8_t>& r);
    static int compareAbsMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
    std::vector<uint8_t> magnitude() const;
    void fromSignMag(bool negative, const std::vector<uint8_t>& mag);
};

// ===========================================================================
// MiniNumber
// ===========================================================================
class MiniNumber {
public:
    static constexpr int MAX_DIGITS         = 64;
    static constexpr int MAX_DECIMAL_PLACES = MAX_DIGITS - 20; // 44

    static const MiniNumber ZERO;
    static const MiniNumber ONE;
    static const MiniNumber TWO;
    static const MiniNumber THREE;
    static const MiniNumber FOUR;
    static const MiniNumber EIGHT;
    static const MiniNumber TWELVE;
    static const MiniNumber SIXTEEN;
    static const MiniNumber TWENTY;
    static const MiniNumber THIRTYTWO;
    static const MiniNumber FIFTY;
    static const MiniNumber SIXTYFOUR;
    static const MiniNumber TWOFIVESIX;
    static const MiniNumber FIVEONE12;
    static const MiniNumber THOUSAND24;
    static const MiniNumber TEN;
    static const MiniNumber HUNDRED;
    static const MiniNumber THOUSAND;
    static const MiniNumber MILLION;
    static const MiniNumber HUNDMILLION;
    static const MiniNumber BILLION;
    static const MiniNumber TRILLION;
    static const MiniNumber MINUSONE;

    MiniNumber();
    explicit MiniNumber(int64_t v);
    explicit MiniNumber(const std::string& s);
    MiniNumber(BigInt unscaled, int scale);
    static MiniNumber fromBytes(const std::vector<uint8_t>& bigEndianBytes);  // from raw big-endian bytes (e.g. SHA2 hash)

    MiniNumber add(const MiniNumber& o) const;
    MiniNumber sub(const MiniNumber& o) const;
    MiniNumber mult(const MiniNumber& o) const;
    MiniNumber div(const MiniNumber& o) const;
    MiniNumber modulo(const MiniNumber& o) const;
    MiniNumber pow(int n) const;
    MiniNumber sqrt() const;
    MiniNumber floor() const;
    MiniNumber ceil() const;
    MiniNumber abs() const;
    MiniNumber increment() const;
    MiniNumber decrement() const;
    MiniNumber setSignificantDigits(int n) const;

    int  compareTo    (const MiniNumber& o) const;
    bool isEqual      (const MiniNumber& o) const { return compareTo(o)==0; }
    bool isLess       (const MiniNumber& o) const { return compareTo(o)<0; }
    bool isLessEqual  (const MiniNumber& o) const { return compareTo(o)<=0; }
    bool isMore       (const MiniNumber& o) const { return compareTo(o)>0; }
    bool isMoreEqual  (const MiniNumber& o) const { return compareTo(o)>=0; }

    int64_t     getAsLong()   const;
    int         getAsInt()    const { return (int)getAsLong(); }
    double      getAsDouble() const;
    std::string toString()    const;

    std::vector<uint8_t> serialise() const;
    static MiniNumber deserialise(const uint8_t* data, size_t& offset);
    static MiniNumber deserialise(const std::vector<uint8_t>& data, size_t& offset) {
        return deserialise(data.data(), offset);
    }

    const BigInt& getUnscaled() const { return m_unscaled; }
    int           getScale()    const { return m_scale; }

private:
    BigInt m_unscaled;
    int    m_scale = 0;

    void applyMathContext();
    void checkLimits() const;
    static void alignScales(const MiniNumber& a, const MiniNumber& b,
                            BigInt& ua, BigInt& ub, int& scale);
    static BigInt scaledUp(const BigInt& u, int steps);
};

} // namespace minima
