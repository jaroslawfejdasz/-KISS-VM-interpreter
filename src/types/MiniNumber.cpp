#include "MiniNumber.hpp"
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <cmath>
#include <cctype>
#include <algorithm>

namespace minima {

// ============================================================================
// Helpers
// ============================================================================
static std::vector<uint8_t> stripLeadingZeros(const std::vector<uint8_t>& v) {
    size_t i = 0;
    while (i + 1 < v.size() && v[i] == 0) ++i;
    return std::vector<uint8_t>(v.begin() + i, v.end());
}

static BigInt pow10(int n) {
    BigInt r(1LL);
    BigInt ten(10LL);
    for (int i = 0; i < n; ++i) r = r * ten;
    return r;
}

// ============================================================================
// BigInt
// ============================================================================
BigInt::BigInt() { bytes = {0x00}; }

BigInt BigInt::zero() { return BigInt(0LL); }
BigInt BigInt::one()  { return BigInt(1LL); }

BigInt::BigInt(int64_t v) {
    if (v == 0) { bytes = {0x00}; return; }
    bool neg = (v < 0);
    uint64_t u;
    if (neg) u = (uint64_t)(-(v + 1)) + 1;
    else     u = (uint64_t)v;
    std::vector<uint8_t> mag;
    while (u > 0) { mag.insert(mag.begin(), (uint8_t)(u & 0xFF)); u >>= 8; }
    fromSignMag(neg, mag);
}

BigInt BigInt::fromJavaBytes(const std::vector<uint8_t>& b) {
    BigInt r; r.bytes = b;
    if (r.bytes.empty()) r.bytes = {0x00};
    return r;
}
BigInt BigInt::fromJavaBytes(const uint8_t* b, int len) {
    BigInt r; r.bytes.assign(b, b + len);
    if (r.bytes.empty()) r.bytes = {0x00};
    return r;
}

BigInt::BigInt(const std::string& decimal) {
    std::string s = decimal;
    while (!s.empty() && isspace((unsigned char)s[0])) s.erase(0,1);
    bool neg = false;
    if (!s.empty() && s[0] == '-') { neg = true; s.erase(0,1); }
    else if (!s.empty() && s[0] == '+') s.erase(0,1);
    if (s.empty()) { bytes = {0x00}; return; }
    std::vector<uint8_t> mag = {0};
    for (char c : s) {
        if (!isdigit((unsigned char)c))
            throw std::invalid_argument("BigInt: invalid decimal '" + decimal + "'");
        int carry = (c - '0');
        for (int i = (int)mag.size()-1; i >= 0; --i) {
            int v = mag[i] * 10 + carry;
            mag[i] = v & 0xFF;
            carry  = v >> 8;
        }
        while (carry > 0) { mag.insert(mag.begin(), carry & 0xFF); carry >>= 8; }
    }
    mag = stripLeadingZeros(mag);
    if (mag.empty()) mag = {0};
    bool isZ = (mag.size() == 1 && mag[0] == 0);
    fromSignMag(neg && !isZ, mag);
}

bool BigInt::isZero() const {
    for (auto b : bytes) if (b != 0) return false;
    return true;
}

void BigInt::normalise() {
    while (bytes.size() > 1) {
        bool topZero = (bytes[0] == 0x00);
        bool topFF   = (bytes[0] == 0xFF);
        bool nextNeg = (bytes[1] & 0x80) != 0;
        if      (topZero && !nextNeg) bytes.erase(bytes.begin());
        else if (topFF   &&  nextNeg) bytes.erase(bytes.begin());
        else break;
    }
}

std::vector<uint8_t> BigInt::magnitude() const {
    if (isNegative()) {
        std::vector<uint8_t> r = bytes;
        for (auto& b : r) b = ~b;
        int carry = 1;
        for (int i = (int)r.size()-1; i >= 0 && carry; --i) {
            int v = r[i] + carry;
            r[i] = v & 0xFF;
            carry = v >> 8;
        }
        return stripLeadingZeros(r);
    }
    size_t i = 0;
    while (i + 1 < bytes.size() && bytes[i] == 0x00) ++i;
    return std::vector<uint8_t>(bytes.begin() + i, bytes.end());
}

void BigInt::fromSignMag(bool negative, const std::vector<uint8_t>& mag) {
    if (mag.empty() || (mag.size()==1 && mag[0]==0)) { bytes = {0x00}; return; }
    if (!negative) {
        if (mag[0] & 0x80) {
            bytes.resize(mag.size() + 1);
            bytes[0] = 0x00;
            std::copy(mag.begin(), mag.end(), bytes.begin()+1);
        } else {
            bytes = mag;
        }
    } else {
        std::vector<uint8_t> r = mag;
        for (auto& b : r) b = ~b;
        int carry = 1;
        for (int i = (int)r.size()-1; i >= 0 && carry; --i) {
            int v = r[i] + carry; r[i] = v & 0xFF; carry = v >> 8;
        }
        if (carry) r.insert(r.begin(), 0x01);
        if (!(r[0] & 0x80)) r.insert(r.begin(), 0xFF);
        bytes = r;
    }
    normalise();
}

std::string BigInt::toDecimalString() const {
    if (isZero()) return "0";
    bool neg = isNegative();
    std::vector<uint8_t> mag = magnitude();
    std::string result;
    std::vector<uint8_t> rem = mag;
    while (!rem.empty() && !(rem.size()==1 && rem[0]==0)) {
        std::vector<uint8_t> q;
        int r = 0;
        for (uint8_t b : rem) {
            int val = r * 256 + b;
            if (!q.empty() || val/10 > 0) q.push_back(val/10);
            r = val % 10;
        }
        result += ('0' + r);
        if (q.empty()) break;
        rem = q;
    }
    if (neg) result += '-';
    std::reverse(result.begin(), result.end());
    return result;
}

int BigInt::compareAbsMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return (a.size() < b.size()) ? -1 : 1;
    return memcmp(a.data(), b.data(), a.size());
}

int BigInt::compareAbsTo(const BigInt& o) const {
    return compareAbsMag(magnitude(), o.magnitude());
}

int BigInt::compareTo(const BigInt& o) const {
    bool na = isNegative(), no = o.isNegative();
    if (na != no) return na ? -1 : 1;
    int c = compareAbsTo(o);
    return na ? -c : c;
}

bool BigInt::operator==(const BigInt& o) const { return bytes == o.bytes; }

std::vector<uint8_t> BigInt::addMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    const auto& big   = (a.size() >= b.size()) ? a : b;
    const auto& small = (a.size() >= b.size()) ? b : a;
    std::vector<uint8_t> r(big.size(), 0);
    int carry = 0;
    int ai = (int)big.size()-1, bi = (int)small.size()-1;
    for (int i = ai; i >= 0; --i, --bi) {
        int v = big[i] + carry + (bi >= 0 ? small[bi] : 0);
        r[i] = v & 0xFF; carry = v >> 8;
    }
    if (carry) r.insert(r.begin(), (uint8_t)carry);
    return r;
}

std::vector<uint8_t> BigInt::subMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    std::vector<uint8_t> r(a.size(), 0);
    int borrow = 0, bi = (int)b.size()-1;
    for (int i = (int)a.size()-1; i >= 0; --i, --bi) {
        int v = (int)a[i] - borrow - (bi >= 0 ? (int)b[bi] : 0);
        if (v < 0) { v += 256; borrow = 1; } else borrow = 0;
        r[i] = (uint8_t)v;
    }
    return stripLeadingZeros(r);
}

std::vector<uint8_t> BigInt::mulMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.empty() || b.empty()) return {0};
    std::vector<uint8_t> r(a.size() + b.size(), 0);
    for (int i = (int)a.size()-1; i >= 0; --i) {
        int carry = 0;
        for (int j = (int)b.size()-1; j >= 0; --j) {
            int v = r[i+j+1] + (int)a[i] * (int)b[j] + carry;
            r[i+j+1] = v & 0xFF; carry = v >> 8;
        }
        r[i] += carry;
    }
    return stripLeadingZeros(r);
}

void BigInt::divmodMag(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                       std::vector<uint8_t>& q, std::vector<uint8_t>& rem_out) {
    if (compareAbsMag(a, b) < 0) { q = {0}; rem_out = a; return; }
    q.clear(); rem_out.clear();
    std::vector<uint8_t> rem;
    for (uint8_t digit : a) {
        rem.push_back(digit);
        rem = stripLeadingZeros(rem);
        if (rem.empty()) rem = {0};
        int lo = 0, hi = 255;
        while (lo < hi) {
            int mid = (lo + hi + 1) / 2;
            auto t = mulMag(b, {(uint8_t)mid});
            if (compareAbsMag(t, rem) <= 0) lo = mid; else hi = mid - 1;
        }
        q.push_back((uint8_t)lo);
        if (lo > 0) rem = subMag(rem, mulMag(b, {(uint8_t)lo}));
    }
    q = stripLeadingZeros(q); if (q.empty()) q = {0};
    rem_out = stripLeadingZeros(rem); if (rem_out.empty()) rem_out = {0};
}

void BigInt::divmodMag_pub(const BigInt& a, const BigInt& b,
                            std::vector<uint8_t>& q, std::vector<uint8_t>& r) {
    divmodMag(a.magnitude(), b.magnitude(), q, r);
}

BigInt BigInt::operator+(const BigInt& o) const {
    bool na = isNegative(), no = o.isNegative();
    auto ma = magnitude(), mo = o.magnitude();
    BigInt r;
    if (na == no) { r.fromSignMag(na, addMag(ma, mo)); }
    else {
        int c = compareAbsMag(ma, mo);
        if (c == 0) { r.bytes = {0x00}; return r; }
        r.fromSignMag((c > 0) ? na : no, (c > 0) ? subMag(ma, mo) : subMag(mo, ma));
    }
    return r;
}
BigInt BigInt::operator-(const BigInt& o) const { return *this + (-o); }
BigInt BigInt::operator*(const BigInt& o) const {
    BigInt r;
    auto rm = mulMag(magnitude(), o.magnitude());
    bool neg = isNegative() != o.isNegative();
    r.fromSignMag(neg && !(rm.size()==1 && rm[0]==0), rm);
    return r;
}
BigInt BigInt::operator/(const BigInt& o) const {
    std::vector<uint8_t> q, rem;
    divmodMag(magnitude(), o.magnitude(), q, rem);
    BigInt r;
    bool neg = isNegative() != o.isNegative();
    r.fromSignMag(neg && !(q.size()==1 && q[0]==0), q);
    return r;
}
BigInt BigInt::operator%(const BigInt& o) const {
    std::vector<uint8_t> q, rem;
    divmodMag(magnitude(), o.magnitude(), q, rem);
    BigInt r;
    r.fromSignMag(isNegative() && !(rem.size()==1 && rem[0]==0), rem);
    return r;
}
BigInt BigInt::operator-() const {
    if (isZero()) return *this;
    BigInt r; r.fromSignMag(!isNegative(), magnitude()); return r;
}

// ============================================================================
// MiniNumber
// ============================================================================
MiniNumber::MiniNumber() : m_unscaled(0LL), m_scale(0) {}
MiniNumber::MiniNumber(int64_t v) : m_unscaled(v), m_scale(0) { checkLimits(); }

MiniNumber::MiniNumber(BigInt unscaled, int scale)
    : m_unscaled(std::move(unscaled)), m_scale(scale) {
    applyMathContext(); checkLimits();
}

MiniNumber::MiniNumber(const std::string& s) {
    std::string str = s;
    while (!str.empty() && isspace((unsigned char)str[0])) str.erase(0,1);
    while (!str.empty() && isspace((unsigned char)str.back())) str.pop_back();
    bool neg = false;
    if (!str.empty() && str[0] == '-') { neg = true; str.erase(0,1); }
    else if (!str.empty() && str[0] == '+') str.erase(0,1);
    size_t epos = str.find_first_of("Ee");
    int exp = 0;
    std::string mantissa = str;
    if (epos != std::string::npos) {
        exp = std::stoi(str.substr(epos+1));
        mantissa = str.substr(0, epos);
    }
    size_t dot = mantissa.find('.');
    int scale = 0;
    std::string digits;
    if (dot == std::string::npos) { digits = mantissa; }
    else {
        digits = mantissa.substr(0, dot) + mantissa.substr(dot+1);
        scale = (int)(mantissa.size() - dot - 1);
    }
    size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) digits = "0";
    else digits = digits.substr(first);
    if (digits.empty()) digits = "0";
    if (neg && digits != "0") digits = "-" + digits;
    m_unscaled = BigInt(digits);
    m_scale = scale - exp;
    applyMathContext(); checkLimits();
}

void MiniNumber::checkLimits() const { /* range check deferred */ }

void MiniNumber::applyMathContext() {
    std::string dec = m_unscaled.toDecimalString();
    bool neg = (!dec.empty() && dec[0] == '-');
    std::string digits = neg ? dec.substr(1) : dec;
    size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) { m_unscaled = BigInt(0LL); m_scale = 0; return; }
    digits = digits.substr(first);
    int sigDigits = (int)digits.size();
    if (sigDigits > MAX_DIGITS) {
        int excess = sigDigits - MAX_DIGITS;
        digits = digits.substr(0, MAX_DIGITS);
        m_scale -= excess;
        if (neg) digits = "-" + digits;
        m_unscaled = BigInt(digits);
    }
    if (m_scale > MAX_DECIMAL_PLACES) {
        int excess = m_scale - MAX_DECIMAL_PLACES;
        BigInt divisor = pow10(excess);
        bool un = m_unscaled.isNegative();
        BigInt abs_u = un ? (-m_unscaled) : m_unscaled;
        std::vector<uint8_t> q, r;
        BigInt::divmodMag_pub(abs_u, divisor, q, r);
        BigInt result = BigInt::fromJavaBytes(q);
        m_unscaled = un ? (-result) : result;
        m_scale = MAX_DECIMAL_PLACES;
    }
}

void MiniNumber::alignScales(const MiniNumber& a, const MiniNumber& b,
                              BigInt& ua, BigInt& ub, int& scale) {
    scale = std::max(a.m_scale, b.m_scale);
    int da = scale - a.m_scale, db = scale - b.m_scale;
    ua = a.m_unscaled; ub = b.m_unscaled;
    if (da > 0) ua = ua * pow10(da);
    if (db > 0) ub = ub * pow10(db);
}

BigInt MiniNumber::scaledUp(const BigInt& u, int steps) {
    return (steps <= 0) ? u : u * pow10(steps);
}

MiniNumber MiniNumber::add(const MiniNumber& o) const {
    BigInt ua, ub; int scale;
    alignScales(*this, o, ua, ub, scale);
    return MiniNumber(ua + ub, scale);
}
MiniNumber MiniNumber::sub(const MiniNumber& o) const {
    BigInt ua, ub; int scale;
    alignScales(*this, o, ua, ub, scale);
    return MiniNumber(ua - ub, scale);
}
MiniNumber MiniNumber::mult(const MiniNumber& o) const {
    return MiniNumber(m_unscaled * o.m_unscaled, m_scale + o.m_scale);
}
MiniNumber MiniNumber::div(const MiniNumber& o) const {
    if (o.m_unscaled.isZero()) throw std::runtime_error("MiniNumber: division by zero");
    int extra = MAX_DIGITS;
    BigInt scaled_a = m_unscaled * pow10(extra);
    bool na = scaled_a.isNegative(), nb = o.m_unscaled.isNegative();
    BigInt abs_a = na ? (-scaled_a) : scaled_a;
    BigInt abs_b = nb ? (-o.m_unscaled) : o.m_unscaled;
    std::vector<uint8_t> qv, rv;
    BigInt::divmodMag_pub(abs_a, abs_b, qv, rv);
    BigInt q_abs = BigInt::fromJavaBytes(qv);
    bool neg = (na != nb) && !q_abs.isZero();
    return MiniNumber(neg ? (-q_abs) : q_abs, m_scale - o.m_scale + extra);
}
MiniNumber MiniNumber::modulo(const MiniNumber& o) const {
    BigInt ua, ub; int scale;
    alignScales(*this, o, ua, ub, scale);
    return MiniNumber(ua % ub, scale);
}
MiniNumber MiniNumber::pow(int n) const {
    MiniNumber r(1LL), base = *this;
    while (n > 0) { if (n&1) r = r.mult(base); base = base.mult(base); n >>= 1; }
    return r;
}
MiniNumber MiniNumber::sqrt() const {
    if (m_unscaled.isNegative()) throw std::runtime_error("MiniNumber: sqrt of negative");
    if (m_unscaled.isZero()) return MiniNumber(0LL);
    MiniNumber x = *this, two(2LL);
    for (int i = 0; i < 200; ++i) {
        MiniNumber next = (x.add(this->div(x))).div(two);
        if (next.compareTo(x) == 0) break;
        x = next;
    }
    return x;
}
MiniNumber MiniNumber::floor() const {
    if (m_scale <= 0) return *this;
    bool neg = m_unscaled.isNegative();
    BigInt abs_u = neg ? (-m_unscaled) : m_unscaled;
    BigInt divisor = pow10(m_scale);
    std::vector<uint8_t> qv, rv;
    BigInt::divmodMag_pub(abs_u, divisor, qv, rv);
    BigInt q = BigInt::fromJavaBytes(qv);
    if (neg) {
        bool hasRem = !(rv.size()==1 && rv[0]==0);
        if (hasRem) q = q + BigInt(1LL);
        return MiniNumber(-(q), 0);
    }
    return MiniNumber(q, 0);
}
MiniNumber MiniNumber::ceil() const {
    if (m_scale <= 0) return *this;
    bool neg = m_unscaled.isNegative();
    BigInt abs_u = neg ? (-m_unscaled) : m_unscaled;
    BigInt divisor = pow10(m_scale);
    std::vector<uint8_t> qv, rv;
    BigInt::divmodMag_pub(abs_u, divisor, qv, rv);
    BigInt q = BigInt::fromJavaBytes(qv);
    if (!neg) {
        bool hasRem = !(rv.size()==1 && rv[0]==0);
        if (hasRem) q = q + BigInt(1LL);
        return MiniNumber(q, 0);
    }
    return MiniNumber(-(q), 0);
}
MiniNumber MiniNumber::abs() const {
    if (!m_unscaled.isNegative()) return *this;
    return MiniNumber(-m_unscaled, m_scale);
}
MiniNumber MiniNumber::increment() const { return add(ONE); }
MiniNumber MiniNumber::decrement() const { return sub(ONE); }
MiniNumber MiniNumber::setSignificantDigits(int n) const {
    if (n > MAX_DIGITS || n < 0) throw std::runtime_error("MiniNumber: invalid significant digits");
    std::string dec = m_unscaled.toDecimalString();
    bool neg = (!dec.empty() && dec[0] == '-');
    std::string digits = neg ? dec.substr(1) : dec;
    size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) return MiniNumber(0LL);
    digits = digits.substr(first);
    int sigDigits = (int)digits.size();
    if (sigDigits <= n) return *this;
    int excess = sigDigits - n;
    digits = digits.substr(0, n);
    if (neg) digits = "-" + digits;
    return MiniNumber(BigInt(digits), m_scale - excess);
}

int MiniNumber::compareTo(const MiniNumber& o) const {
    BigInt ua, ub; int scale;
    alignScales(*this, o, ua, ub, scale);
    return ua.compareTo(ub);
}

int64_t MiniNumber::getAsLong() const {
    if (m_scale < 0) {
        BigInt v = m_unscaled * pow10(-m_scale);
        try { return std::stoll(v.toDecimalString()); } catch(...) { return 0; }
    }
    if (m_scale == 0) {
        try { return std::stoll(m_unscaled.toDecimalString()); } catch(...) { return 0; }
    }
    BigInt divisor = pow10(m_scale);
    bool neg = m_unscaled.isNegative();
    BigInt abs_u = neg ? (-m_unscaled) : m_unscaled;
    std::vector<uint8_t> qv, rv;
    BigInt::divmodMag_pub(abs_u, divisor, qv, rv);
    BigInt q = BigInt::fromJavaBytes(qv);
    if (neg) q = -q;
    try { return std::stoll(q.toDecimalString()); } catch(...) { return 0; }
}

double MiniNumber::getAsDouble() const { return std::stod(toString()); }

std::string MiniNumber::toString() const {
    std::string u = m_unscaled.toDecimalString();
    bool neg = (!u.empty() && u[0] == '-');
    std::string digits = neg ? u.substr(1) : u;
    int scale = m_scale;
    while (digits.size() > 1 && digits.back() == '0' && scale > 0) {
        digits.pop_back(); scale--;
    }
    if (scale <= 0) {
        for (int i = 0; i < -scale; ++i) digits += '0';
        return (neg ? "-" : "") + digits;
    }
    if ((int)digits.size() <= scale) {
        std::string r = "0.";
        for (int i = 0; i < scale - (int)digits.size(); ++i) r += '0';
        return (neg ? "-" : "") + r + digits;
    }
    int intPart = (int)digits.size() - scale;
    return (neg ? "-" : "") + digits.substr(0, intPart) + "." + digits.substr(intPart);
}

std::vector<uint8_t> MiniNumber::serialise() const {
    const auto& ub = m_unscaled.toJavaBytes();
    if (ub.size() > 32) throw std::runtime_error("MiniNumber: unscaled too large");
    std::vector<uint8_t> out;
    out.push_back((uint8_t)(int8_t)m_scale);
    out.push_back((uint8_t)ub.size());
    out.insert(out.end(), ub.begin(), ub.end());
    return out;
}

MiniNumber MiniNumber::deserialise(const uint8_t* data, size_t& offset) {
    int8_t  scale = (int8_t)data[offset++];
    uint8_t len   = data[offset++];
    if (len > 32) throw std::runtime_error("MiniNumber: len > 32: " + std::to_string(len));
    BigInt u = BigInt::fromJavaBytes(data + offset, (int)len);
    offset += len;
    MiniNumber r; r.m_unscaled = u; r.m_scale = (int)scale;
    return r;
}

// Static constants
const MiniNumber MiniNumber::ZERO       ("0");
const MiniNumber MiniNumber::ONE        ("1");
const MiniNumber MiniNumber::TWO        ("2");
const MiniNumber MiniNumber::THREE      ("3");
const MiniNumber MiniNumber::FOUR       ("4");
const MiniNumber MiniNumber::EIGHT      ("8");
const MiniNumber MiniNumber::TWELVE     ("12");
const MiniNumber MiniNumber::SIXTEEN    ("16");
const MiniNumber MiniNumber::TWENTY     ("20");
const MiniNumber MiniNumber::THIRTYTWO  ("32");
const MiniNumber MiniNumber::FIFTY      ("50");
const MiniNumber MiniNumber::SIXTYFOUR  ("64");
const MiniNumber MiniNumber::TWOFIVESIX ("256");
const MiniNumber MiniNumber::FIVEONE12  ("512");
const MiniNumber MiniNumber::THOUSAND24 ("1024");
const MiniNumber MiniNumber::TEN        ("10");
const MiniNumber MiniNumber::HUNDRED    ("100");
const MiniNumber MiniNumber::THOUSAND   ("1000");
const MiniNumber MiniNumber::MILLION    ("1000000");
const MiniNumber MiniNumber::HUNDMILLION("100000000");
const MiniNumber MiniNumber::BILLION    ("1000000000");
const MiniNumber MiniNumber::TRILLION   ("1000000000000");
const MiniNumber MiniNumber::MINUSONE   ("-1");

} // namespace minima
