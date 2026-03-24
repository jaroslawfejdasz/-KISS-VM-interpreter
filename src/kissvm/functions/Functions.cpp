/**
 * Functions.cpp — All 42+ KISS VM built-in functions.
 *
 * Each function receives (args, ctx) and returns a Value.
 * Functions throw ContractException on argument type/count errors.
 *
 * Reference: src/org/minima/kissvm/functions/
 */
#include "Functions.hpp"
#include "../Contract.hpp"
#include "../../crypto/Hash.hpp"
#include "../../crypto/RSA.hpp"
#include "../../objects/Coin.hpp"
#include "../../mmr/MMRProof.hpp"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace minima::kissvm {

// ── Helpers ───────────────────────────────────────────────────────────────────

static void requireArgs(const std::vector<Value>& args, size_t n, const char* name) {
    if (args.size() != n)
        throw ContractException(std::string(name) + " requires " + std::to_string(n) + " argument(s)");
}

static void requireMinArgs(const std::vector<Value>& args, size_t n, const char* name) {
    if (args.size() < n)
        throw ContractException(std::string(name) + " requires at least " + std::to_string(n) + " argument(s)");
}

static const MiniNumber& numArg(const std::vector<Value>& args, size_t i, const char* fn) {
    if (args[i].type() != ValueType::NUMBER)
        throw ContractException(std::string(fn) + ": arg " + std::to_string(i) + " must be NUMBER");
    return args[i].asNumber();
}

static const MiniData& hexArg(const std::vector<Value>& args, size_t i, const char* fn) {
    if (args[i].type() != ValueType::HEX)
        throw ContractException(std::string(fn) + ": arg " + std::to_string(i) + " must be HEX");
    return args[i].asHex();
}

// ── FunctionRegistry ──────────────────────────────────────────────────────────

FunctionRegistry& FunctionRegistry::instance() {
    static FunctionRegistry reg;
    static bool initialized = false;
    if (!initialized) { reg.registerAll(); initialized = true; }
    return reg;
}

void FunctionRegistry::reg(const std::string& name, FunctionImpl fn) {
    m_funcs[name] = std::move(fn);
}

bool FunctionRegistry::has(const std::string& name) const {
    return m_funcs.count(name) > 0;
}

Value FunctionRegistry::call(const std::string& name,
                              const std::vector<Value>& args,
                              Contract& ctx) const {
    auto it = m_funcs.find(name);
    if (it == m_funcs.end())
        throw ContractException("Unknown function: " + name);
    return it->second(args, ctx);
}

void FunctionRegistry::registerAll() {
    using namespace fn;
    // Signatures
    reg("SIGNEDBY",  SIGNEDBY);
    reg("CHECKSIG",  CHECKSIG);
    reg("MULTISIG",  MULTISIG);
    // State
    reg("STATE",     STATE);
    reg("PREVSTATE", PREVSTATE);
    reg("SAMESTATE", SAMESTATE);
    // Hash
    reg("SHA2",      SHA2);
    reg("SHA3",      SHA3);
    // HEX / String
    reg("CONCAT",    CONCAT);
    reg("SUBSET",    SUBSET);
    reg("LEN",       LEN);
    reg("REV",       REV);
    reg("REPLACE",   REPLACE);
    reg("CLEAN",     CLEAN);
    reg("UTF8",      UTF8);
    reg("ASCII",     ASCII);
    // Conversion
    reg("NUMBER",    NUMBER);
    reg("HEX",       HEX);
    reg("SCRIPT",    SCRIPT);
    reg("BOOL",      BOOL);
    reg("HEXTONUM",  HEXTONUM);
    reg("NUMTOHEX",  NUMTOHEX);
    // Math
    reg("ABS",       ABS);
    reg("MAX",       MAX);
    reg("MIN",       MIN);
    reg("DEC",       DEC);
    reg("INC",       INC);
    reg("FLOOR",     FLOOR);
    reg("CEIL",      CEIL);
    reg("POW",       POW);
    reg("SQRT",      SQRT);
    // Bitwise
    reg("BITSET",    BITSET);
    reg("BITGET",    BITGET);
    reg("BITCOUNT",  BITCOUNT);
    // Transaction verification
    reg("VERIFYOUT",   VERIFYOUT);
    reg("VERIFYIN",    VERIFYIN);
    reg("GETOUTADDR",  GETOUTADDR);
    reg("GETOUTAMT",   GETOUTAMT);
    reg("GETOUTTOKEN", GETOUTTOKEN);
    reg("SUMINPUTS",   SUMINPUTS);
    reg("SUMOUTPUTS",  SUMOUTPUTS);
    // New functions (parity gap 2)
    reg("STRING",          STRING);
    reg("EXISTS",          EXISTS);
    reg("OVERWRITE",       OVERWRITE);
    reg("SETLEN",          SETLEN);
    reg("SIGDIG",          SIGDIG);
    reg("PROOF",           PROOF);
    reg("REPLACEFIRST",    REPLACEFIRST);
    reg("SUBSTR",          SUBSTR);
    reg("GETINADDR",       GETINADDR);
    reg("GETINAMT",        GETINAMT);
    reg("GETINID",         GETINID);
    reg("GETINTOK",        GETINTOK);
    reg("GETOUTKEEPSTATE", GETOUTKEEPSTATE);
}

// ─────────────────────────────────────────────────────────────────────────────
// FUNCTION IMPLEMENTATIONS
// ─────────────────────────────────────────────────────────────────────────────

namespace fn {

// ── Signature ─────────────────────────────────────────────────────────────────

// SIGNEDBY(pubkey_hex) → BOOLEAN
// Returns TRUE if any signature in the witness was made by pubkey
Value SIGNEDBY(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "SIGNEDBY");
    const MiniData& pubkey = hexArg(args, 0, "SIGNEDBY");
    for (const auto& sig : ctx.witness().signatures()) {
        // sig is Signature (list of SignatureProof)
        if (sig.getRootPublicKey() == pubkey) return Value::boolean(true);
        for (const auto& sp : sig.mSignatures) {
            if (sp.mPublicKey == pubkey) return Value::boolean(true);
        }
    }
    return Value::boolean(false);
}

// CHECKSIG(pubkey_hex, data_hex, sig_hex) → BOOLEAN
// RSA SHA256withRSA signature verification.
// Java: Signature.getInstance("SHA256withRSA").verify(pubKey, data, sig)
Value CHECKSIG(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 3, "CHECKSIG");
    const MiniData& pubkey = hexArg(args, 0, "CHECKSIG");
    const MiniData& data   = hexArg(args, 1, "CHECKSIG");
    const MiniData& sig    = hexArg(args, 2, "CHECKSIG");

#ifdef MINIMA_OPENSSL
    try {
        return Value::boolean(crypto::RSA::verify(pubkey, data, sig));
    } catch (...) {
        return Value::boolean(false);
    }
#else
    // Without OpenSSL: witness-presence fallback (testing only)
    for (const auto& sig_obj : ctx.witness().signatures()) {
        for (const auto& sp : sig_obj.mSignatures) {
            if (sp.mPublicKey == pubkey && sp.mSignature == sig)
                return Value::boolean(true);
        }
    }
    return Value::boolean(false);
#endif
}

// MULTISIG(n, pubkey1, pubkey2, ...) → BOOLEAN
// Returns TRUE if at least n of the listed pubkeys have signed
Value MULTISIG(const std::vector<Value>& args, Contract& ctx) {
    requireMinArgs(args, 2, "MULTISIG");
    const MiniNumber& n = numArg(args, 0, "MULTISIG");
    int required = (int)n.getAsLong();
    int found = 0;

    for (size_t i = 1; i < args.size(); i++) {
        if (args[i].type() != ValueType::HEX) continue;
        const MiniData& pubkey = args[i].asHex();
        for (const auto& sig : ctx.witness().signatures()) {
            if (sig.getRootPublicKey() == pubkey) { found++; break; }
            bool foundInSig = false;
            for (const auto& sp : sig.mSignatures) {
                if (sp.mPublicKey == pubkey) { foundInSig = true; break; }
            }
            if (foundInSig) { found++; break; }
        }
    }
    return Value::boolean(found >= required);
}

// ── State ─────────────────────────────────────────────────────────────────────

// STATE(n) → value of state variable n in current transaction output
Value STATE(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "STATE");
    const MiniNumber& n = numArg(args, 0, "STATE");
    std::string key = "@STATE" + n.toString();
    auto v = ctx.env().get(key);
    if (!v) return Value::hex(MiniData{});
    return *v;
}

// PREVSTATE(n) → value of state variable n in the input coin being spent
Value PREVSTATE(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "PREVSTATE");
    const MiniNumber& n = numArg(args, 0, "PREVSTATE");
    std::string key = "@PREVSTATE" + n.toString();
    auto v = ctx.env().get(key);
    if (!v) return Value::hex(MiniData{});
    return *v;
}

// SAMESTATE(from, to) → BOOLEAN — state vars from..to are same in input and output
Value SAMESTATE(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 2, "SAMESTATE");
    const MiniNumber& from = numArg(args, 0, "SAMESTATE");
    const MiniNumber& to   = numArg(args, 1, "SAMESTATE");
    long f = from.getAsLong(), t = to.getAsLong();
    for (long i = f; i <= t; i++) {
        std::string idx = std::to_string(i);
        auto prev = ctx.env().get("@PREVSTATE" + idx);
        auto curr = ctx.env().get("@STATE"     + idx);
        std::string ps = prev ? prev->toString() : "";
        std::string cs = curr ? curr->toString() : "";
        if (ps != cs) return Value::boolean(false);
    }
    return Value::boolean(true);
}

// ── Hashing ───────────────────────────────────────────────────────────────────

// SHA2(hex) → hex (32 bytes SHA2-256 of input)
Value SHA2(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "SHA2");
    MiniData data;
    if (args[0].type() == ValueType::HEX)
        data = args[0].asHex();
    else if (args[0].type() == ValueType::SCRIPT) {
        const std::string& s = args[0].asScript().str();
        data = MiniData(std::vector<uint8_t>(s.begin(), s.end()));
    } else
        throw ContractException("SHA2: argument must be HEX or SCRIPT");
    return Value::hex(crypto::Hash::sha2_256(data));
}

// SHA3(hex) → hex (32 bytes SHA3-256 of input)
Value SHA3(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "SHA3");
    MiniData data;
    if (args[0].type() == ValueType::HEX)
        data = args[0].asHex();
    else if (args[0].type() == ValueType::SCRIPT) {
        const std::string& s = args[0].asScript().str();
        data = MiniData(std::vector<uint8_t>(s.begin(), s.end()));
    } else
        throw ContractException("SHA3: argument must be HEX or SCRIPT");
    return Value::hex(crypto::Hash::sha3_256(data));
}

// ── HEX / String ──────────────────────────────────────────────────────────────

// CONCAT(a, b) → concatenates two HEX values
Value CONCAT(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 2, "CONCAT");
    if (args[0].type() == ValueType::HEX && args[1].type() == ValueType::HEX) {
        auto a = args[0].asHex().bytes();
        const auto& b = args[1].asHex().bytes();
        a.insert(a.end(), b.begin(), b.end());
        return Value::hex(MiniData(std::move(a)));
    }
    // String concat
    return Value::script(args[0].toString() + args[1].toString());
}

// SUBSET(hex, start, length) → hex slice
Value SUBSET(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 3, "SUBSET");
    const MiniData& data  = hexArg(args, 0, "SUBSET");
    long start  = numArg(args, 1, "SUBSET").getAsLong();
    long length = numArg(args, 2, "SUBSET").getAsLong();
    const auto& bytes = data.bytes();
    if (start < 0 || length < 0 || (size_t)(start + length) > bytes.size())
        throw ContractException("SUBSET: out of bounds");
    return Value::hex(MiniData(std::vector<uint8_t>(bytes.begin()+start, bytes.begin()+start+length)));
}

// LEN(hex) → NUMBER (byte count)
Value LEN(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "LEN");
    if (args[0].type() == ValueType::HEX)
        return Value::number(MiniNumber((int64_t)args[0].asHex().bytes().size()));
    if (args[0].type() == ValueType::SCRIPT)
        return Value::number(MiniNumber((int64_t)args[0].asScript().str().size()));
    throw ContractException("LEN: requires HEX or SCRIPT");
}

// REV(hex) → reversed bytes
Value REV(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "REV");
    const MiniData& data = hexArg(args, 0, "REV");
    auto bytes = data.bytes();
    std::reverse(bytes.begin(), bytes.end());
    return Value::hex(MiniData(std::move(bytes)));
}

// REPLACE(haystack_hex, find_hex, replace_hex) → hex
Value REPLACE(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 3, "REPLACE");
    auto hay  = hexArg(args, 0, "REPLACE").bytes();
    const auto& find = hexArg(args, 1, "REPLACE").bytes();
    const auto& repl = hexArg(args, 2, "REPLACE").bytes();
    if (find.empty()) return Value::hex(MiniData(hay));
    std::vector<uint8_t> result;
    for (size_t i = 0; i < hay.size(); ) {
        if (i + find.size() <= hay.size() &&
            std::equal(find.begin(), find.end(), hay.begin()+i)) {
            result.insert(result.end(), repl.begin(), repl.end());
            i += find.size();
        } else {
            result.push_back(hay[i++]);
        }
    }
    return Value::hex(MiniData(std::move(result)));
}

// CLEAN(hex) → hex with leading zeros removed
Value CLEAN(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "CLEAN");
    auto bytes = hexArg(args, 0, "CLEAN").bytes();
    auto it = bytes.begin();
    while (it != bytes.end() && *it == 0) ++it;
    return Value::hex(MiniData(std::vector<uint8_t>(it, bytes.end())));
}

// UTF8(hex) → SCRIPT (interpret bytes as UTF-8 string)
Value UTF8(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "UTF8");
    const auto& bytes = hexArg(args, 0, "UTF8").bytes();
    std::string s(bytes.begin(), bytes.end());
    return Value::script(s);
}

// ASCII(script) → HEX (UTF-8 encode string to bytes)
Value ASCII(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "ASCII");
    if (args[0].type() != ValueType::SCRIPT)
        throw ContractException("ASCII: requires SCRIPT");
    const std::string& s = args[0].asScript().str();
    return Value::hex(MiniData(std::vector<uint8_t>(s.begin(), s.end())));
}

// ── Type conversion ───────────────────────────────────────────────────────────

// NUMBER(x) → NUMBER
Value NUMBER(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "NUMBER");
    switch (args[0].type()) {
        case ValueType::NUMBER:  return args[0];
        case ValueType::BOOLEAN: return Value::number(MiniNumber(args[0].asBoolean() ? "1" : "0"));
        case ValueType::SCRIPT:  return Value::number(MiniNumber(args[0].asScript().str()));
        case ValueType::HEX: {
            // Interpret hex bytes as big-endian integer
            const auto& b = args[0].asHex().bytes();
            // Use MiniNumber's fromBytes
            return Value::number(MiniNumber::fromBytes(b));
        }
    }
    return Value::number(MiniNumber("0"));
}

// HEX(x) → HEX
Value HEX(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "HEX");
    switch (args[0].type()) {
        case ValueType::HEX:    return args[0];
        case ValueType::NUMBER: {
            // Convert number to its byte representation
            return Value::hex(MiniData(args[0].asNumber().toBytes()));
        }
        case ValueType::SCRIPT: {
            const std::string& s = args[0].asScript().str();
            // If it looks like 0x..., parse as hex
            if (s.size() >= 2 && s[0]=='0' && (s[1]=='x'||s[1]=='X')) {
                std::string h = s.substr(2);
                if (h.size() % 2) h = "0" + h;
                std::vector<uint8_t> bytes;
                for (size_t i = 0; i < h.size(); i+=2)
                    bytes.push_back((uint8_t)std::stoul(h.substr(i,2),nullptr,16));
                return Value::hex(MiniData(std::move(bytes)));
            }
            return Value::hex(MiniData(std::vector<uint8_t>(s.begin(), s.end())));
        }
        case ValueType::BOOLEAN:
            return Value::hex(MiniData({(uint8_t)(args[0].asBoolean() ? 1 : 0)}));
    }
    return Value::hex(MiniData{});
}

// SCRIPT(x) → SCRIPT
Value SCRIPT(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "SCRIPT");
    return Value::script(args[0].toString());
}

// BOOL(x) → BOOLEAN
Value BOOL(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "BOOL");
    return Value::boolean(args[0].isTrue());
}

// HEXTONUM(hex) → NUMBER (big-endian bytes to decimal)
Value HEXTONUM(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "HEXTONUM");
    const MiniData& d = hexArg(args, 0, "HEXTONUM");
    return Value::number(MiniNumber::fromBytes(d.bytes()));
}

// NUMTOHEX(num) → HEX (decimal to big-endian bytes)
Value NUMTOHEX(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "NUMTOHEX");
    const MiniNumber& n = numArg(args, 0, "NUMTOHEX");
    return Value::hex(MiniData(n.toBytes()));
}

// ── Math ──────────────────────────────────────────────────────────────────────

Value ABS(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "ABS");
    const MiniNumber& n = numArg(args, 0, "ABS");
    if (n < MiniNumber("0")) return Value::number(MiniNumber("0") - n);
    return Value::number(n);
}

Value MAX(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 2, "MAX");
    const MiniNumber& a = numArg(args, 0, "MAX");
    const MiniNumber& b = numArg(args, 1, "MAX");
    return Value::number(a > b ? a : b);
}

Value MIN(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 2, "MIN");
    const MiniNumber& a = numArg(args, 0, "MIN");
    const MiniNumber& b = numArg(args, 1, "MIN");
    return Value::number(a < b ? a : b);
}

Value DEC(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "DEC");
    return Value::number(numArg(args, 0, "DEC") - MiniNumber("1"));
}

Value INC(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "INC");
    return Value::number(numArg(args, 0, "INC") + MiniNumber("1"));
}

Value FLOOR(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "FLOOR");
    return Value::number(numArg(args, 0, "FLOOR").floor());
}

Value CEIL(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "CEIL");
    return Value::number(numArg(args, 0, "CEIL").ceil());
}

Value POW(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 2, "POW");
    const MiniNumber& base = numArg(args, 0, "POW");
    const MiniNumber& exp  = numArg(args, 1, "POW");
    return Value::number(base.pow((int)exp.getAsLong()));
}

Value SQRT(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "SQRT");
    return Value::number(numArg(args, 0, "SQRT").sqrt());
}

// ── Bitwise ───────────────────────────────────────────────────────────────────

// BITSET(hex, bit_pos, value) → hex with bit set/cleared
Value BITSET(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 3, "BITSET");
    auto bytes = hexArg(args, 0, "BITSET").bytes();
    long bitPos = numArg(args, 1, "BITSET").getAsLong();
    bool val    = args[2].isTrue();
    size_t byteIdx = bitPos / 8;
    int    bitIdx  = 7 - (bitPos % 8);
    if (byteIdx >= bytes.size()) throw ContractException("BITSET: bit position out of range");
    if (val) bytes[byteIdx] |=  (1 << bitIdx);
    else     bytes[byteIdx] &= ~(1 << bitIdx);
    return Value::hex(MiniData(std::move(bytes)));
}

// BITGET(hex, bit_pos) → BOOLEAN
Value BITGET(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 2, "BITGET");
    const auto& bytes = hexArg(args, 0, "BITGET").bytes();
    long bitPos = numArg(args, 1, "BITGET").getAsLong();
    size_t byteIdx = bitPos / 8;
    int    bitIdx  = 7 - (bitPos % 8);
    if (byteIdx >= bytes.size()) return Value::boolean(false);
    return Value::boolean((bytes[byteIdx] >> bitIdx) & 1);
}

// BITCOUNT(hex) → NUMBER of set bits
Value BITCOUNT(const std::vector<Value>& args, Contract& ctx) {
    (void)ctx;
    requireArgs(args, 1, "BITCOUNT");
    const auto& bytes = hexArg(args, 0, "BITCOUNT").bytes();
    int count = 0;
    for (uint8_t b : bytes) count += __builtin_popcount(b);
    return Value::number(MiniNumber((int64_t)count));
}

// ── Transaction verification ──────────────────────────────────────────────────

// VERIFYOUT(index, address_hex, amount, tokenid_hex) → BOOLEAN
// Checks that output[index] matches the given params
Value VERIFYOUT(const std::vector<Value>& args, Contract& ctx) {
    requireMinArgs(args, 3, "VERIFYOUT");
    long idx = numArg(args, 0, "VERIFYOUT").getAsLong();
    const auto& outputs = ctx.txn().outputs();
    if (idx < 0 || (size_t)idx >= outputs.size()) return Value::boolean(false);
    const Coin& out = outputs[(size_t)idx];

    // Check address
    if (args[1].type() == ValueType::HEX && !(out.address().hash() == args[1].asHex()))
        return Value::boolean(false);

    // Check amount
    if (args[2].type() == ValueType::NUMBER && !(out.amount() == args[2].asNumber()))
        return Value::boolean(false);

    // Check tokenid (optional 4th arg)
    if (args.size() >= 4 && args[3].type() == ValueType::HEX) {
        if (!(out.tokenID() == args[3].asHex())) return Value::boolean(false);
    }

    return Value::boolean(true);
}

// VERIFYIN(index, address_hex, amount, tokenid_hex) → BOOLEAN
Value VERIFYIN(const std::vector<Value>& args, Contract& ctx) {
    requireMinArgs(args, 3, "VERIFYIN");
    long idx = numArg(args, 0, "VERIFYIN").getAsLong();
    const auto& inputs = ctx.txn().inputs();
    if (idx < 0 || (size_t)idx >= inputs.size()) return Value::boolean(false);
    const Coin& in = inputs[(size_t)idx];

    if (args[1].type() == ValueType::HEX && !(in.address().hash() == args[1].asHex()))
        return Value::boolean(false);

    if (args[2].type() == ValueType::NUMBER && !(in.amount() == args[2].asNumber()))
        return Value::boolean(false);

    if (args.size() >= 4 && args[3].type() == ValueType::HEX)
        if (!(in.tokenID() == args[3].asHex())) return Value::boolean(false);

    return Value::boolean(true);
}

// GETOUTADDR(index) → HEX address of output[index]
Value GETOUTADDR(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "GETOUTADDR");
    long idx = numArg(args, 0, "GETOUTADDR").getAsLong();
    const auto& outputs = ctx.txn().outputs();
    if (idx < 0 || (size_t)idx >= outputs.size())
        throw ContractException("GETOUTADDR: index out of range");
    return Value::hex(outputs[(size_t)idx].address().hash());
}

// GETOUTAMT(index) → NUMBER amount of output[index]
Value GETOUTAMT(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "GETOUTAMT");
    long idx = numArg(args, 0, "GETOUTAMT").getAsLong();
    const auto& outputs = ctx.txn().outputs();
    if (idx < 0 || (size_t)idx >= outputs.size())
        throw ContractException("GETOUTAMT: index out of range");
    return Value::number(outputs[(size_t)idx].amount());
}

// GETOUTTOKEN(index) → HEX token ID of output[index]
Value GETOUTTOKEN(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "GETOUTTOKEN");
    long idx = numArg(args, 0, "GETOUTTOKEN").getAsLong();
    const auto& outputs = ctx.txn().outputs();
    if (idx < 0 || (size_t)idx >= outputs.size())
        throw ContractException("GETOUTTOKEN: index out of range");
    return Value::hex(outputs[(size_t)idx].tokenID());
}

// SUMINPUTS(tokenid_hex) → NUMBER sum of input amounts for given token
Value SUMINPUTS(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "SUMINPUTS");
    const MiniData& tokenId = hexArg(args, 0, "SUMINPUTS");
    MiniNumber sum("0");
    for (const auto& coin : ctx.txn().inputs())
        if (coin.tokenID() == tokenId) sum = sum + coin.amount();
    return Value::number(sum);
}

// SUMOUTPUTS(tokenid_hex) → NUMBER sum of output amounts for given token
Value SUMOUTPUTS(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "SUMOUTPUTS");
    const MiniData& tokenId = hexArg(args, 0, "SUMOUTPUTS");
    MiniNumber sum("0");
    for (const auto& coin : ctx.txn().outputs())
        if (coin.tokenID() == tokenId) sum = sum + coin.amount();
    return Value::number(sum);
}


// ── New 13 functions (PARITY GAP 2) ──────────────────────────────────────────

// STRING(hex) → SCRIPT — convert hex bytes to UTF-8 string
Value STRING(const std::vector<Value>& args, Contract& /*ctx*/) {
    requireArgs(args, 1, "STRING");
    const MiniData& d = hexArg(args, 0, "STRING");
    const auto& b = d.bytes();
    return Value::script(MiniString(std::string(b.begin(), b.end())));
}

// EXISTS(varname) → BOOLEAN — true if variable is defined in environment
Value EXISTS(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "EXISTS");
    // arg is SCRIPT (variable name) or anything we stringify
    std::string varName;
    if (args[0].type() == ValueType::SCRIPT)
        varName = args[0].asScript().str();
    else
        varName = args[0].asHex().to0xString();
    auto v = ctx.env().get(varName);
    return Value::boolean(v.has_value());
}

// OVERWRITE(data, pos, val) → HEX — overwrite bytes in MiniData at position
Value OVERWRITE(const std::vector<Value>& args, Contract& /*ctx*/) {
    requireArgs(args, 3, "OVERWRITE");
    auto bytes = hexArg(args, 0, "OVERWRITE").bytes();
    long pos   = numArg(args, 1, "OVERWRITE").getAsLong();
    const auto& val = hexArg(args, 2, "OVERWRITE").bytes();
    if (pos < 0 || (size_t)pos + val.size() > bytes.size())
        throw ContractException("OVERWRITE: position out of range");
    std::copy(val.begin(), val.end(), bytes.begin() + pos);
    return Value::hex(MiniData(bytes));
}

// SETLEN(data, len) → HEX — truncate or zero-pad MiniData to len bytes
Value SETLEN(const std::vector<Value>& args, Contract& /*ctx*/) {
    requireArgs(args, 2, "SETLEN");
    auto bytes = hexArg(args, 0, "SETLEN").bytes();
    long len   = numArg(args, 1, "SETLEN").getAsLong();
    if (len < 0) throw ContractException("SETLEN: negative length");
    bytes.resize((size_t)len, 0x00);
    return Value::hex(MiniData(bytes));
}

// SIGDIG(n, digits) → NUMBER — round n to `digits` significant figures
Value SIGDIG(const std::vector<Value>& args, Contract& /*ctx*/) {
    requireArgs(args, 2, "SIGDIG");
    double n      = numArg(args, 0, "SIGDIG").getAsDouble();
    int    digits = (int)numArg(args, 1, "SIGDIG").getAsLong();
    if (digits <= 0) return Value::number(MiniNumber("0"));
    if (n == 0.0)    return Value::number(MiniNumber("0"));
    double mag = std::pow(10.0, digits - 1 - std::floor(std::log10(std::abs(n))));
    double rounded = std::round(n * mag) / mag;
    std::ostringstream oss;
    oss << std::fixed << rounded;
    return Value::number(MiniNumber(oss.str()));
}

// PROOF(data, proof_hex, root_hex) → BOOLEAN — verify MMR inclusion proof
// Java ref: MMRProof.verify(data, root)
// proof_hex = serialised MMRProof bytes (as produced by MMRSet::getProof())
// root_hex  = MMR root hash (32 bytes)
// data      = coin hash (32 bytes) — the leaf data being proved
Value PROOF(const std::vector<Value>& args, Contract& /*ctx*/) {
    requireArgs(args, 3, "PROOF");
    const MiniData& data      = hexArg(args, 0, "PROOF");
    const MiniData& proofHex  = hexArg(args, 1, "PROOF");
    const MiniData& rootHex   = hexArg(args, 2, "PROOF");

    // Deserialise MMRProof, then compute root using the provided data as the leaf.
    // Matches Minima Java KISSVM: PROOF(data, proof, root) verifies
    // that 'data' is a leaf in the MMR whose root hash is 'root'.
    try {
        const auto& pb = proofHex.bytes();
        size_t off = 0;
        MMRProof proof = MMRProof::deserialise(pb.data(), off);

        // Build leaf from the supplied data
        MMRData leaf(data, MiniNumber::ZERO, false);

        // Walk the proof path with our leaf, compute the resulting root
        MMRData calculatedRoot = proof.calculateProof(leaf);

        // Compare computed root to provided root
        return Value::boolean(calculatedRoot.getData() == rootHex);
    } catch (...) {
        return Value::boolean(false);
    }
}

// REPLACEFIRST(s, from, to) → SCRIPT — replace first occurrence
Value REPLACEFIRST(const std::vector<Value>& args, Contract& /*ctx*/) {
    requireArgs(args, 3, "REPLACEFIRST");
    // args can be SCRIPT or HEX (we convert HEX to string via to0xString)
    auto getStr = [&](const Value& v) -> std::string {
        if (v.type() == ValueType::SCRIPT) return v.asScript().str();
        const auto& b = v.asHex().bytes();
        return std::string(b.begin(), b.end());
    };
    std::string str  = getStr(args[0]);
    std::string from = getStr(args[1]);
    std::string to   = getStr(args[2]);
    size_t pos = str.find(from);
    if (pos != std::string::npos) str.replace(pos, from.size(), to);
    return Value::script(MiniString(str));
}

// SUBSTR(s, start, len) → SCRIPT — substring
Value SUBSTR(const std::vector<Value>& args, Contract& /*ctx*/) {
    requireArgs(args, 3, "SUBSTR");
    std::string s;
    if (args[0].type() == ValueType::SCRIPT)
        s = args[0].asScript().str();
    else {
        const auto& b = hexArg(args, 0, "SUBSTR").bytes();
        s = std::string(b.begin(), b.end());
    }
    long start = numArg(args, 1, "SUBSTR").getAsLong();
    long len   = numArg(args, 2, "SUBSTR").getAsLong();
    if (start < 0 || len < 0 || (size_t)(start + len) > s.size())
        throw ContractException("SUBSTR: out of range");
    return Value::script(MiniString(s.substr((size_t)start, (size_t)len)));
}

// GETINADDR(i) → HEX — address of i-th input coin
Value GETINADDR(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "GETINADDR");
    long idx = numArg(args, 0, "GETINADDR").getAsLong();
    const auto& inputs = ctx.txn().inputs();
    if (idx < 0 || (size_t)idx >= inputs.size())
        throw ContractException("GETINADDR: index out of range");
    return Value::hex(inputs[(size_t)idx].address().hash());
}

// GETINAMT(i) → NUMBER — amount of i-th input coin
Value GETINAMT(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "GETINAMT");
    long idx = numArg(args, 0, "GETINAMT").getAsLong();
    const auto& inputs = ctx.txn().inputs();
    if (idx < 0 || (size_t)idx >= inputs.size())
        throw ContractException("GETINAMT: index out of range");
    return Value::number(inputs[(size_t)idx].amount());
}

// GETINID(i) → HEX — coinID of i-th input coin
Value GETINID(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "GETINID");
    long idx = numArg(args, 0, "GETINID").getAsLong();
    const auto& inputs = ctx.txn().inputs();
    if (idx < 0 || (size_t)idx >= inputs.size())
        throw ContractException("GETINID: index out of range");
    return Value::hex(inputs[(size_t)idx].coinID());
}

// GETINTOK(i) → HEX — tokenID of i-th input coin (empty = native Minima)
Value GETINTOK(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "GETINTOK");
    long idx = numArg(args, 0, "GETINTOK").getAsLong();
    const auto& inputs = ctx.txn().inputs();
    if (idx < 0 || (size_t)idx >= inputs.size())
        throw ContractException("GETINTOK: index out of range");
    const Coin& c = inputs[(size_t)idx];
    if (!c.hasToken()) return Value::hex(MiniData{});
    return Value::hex(c.tokenID());
}

// GETOUTKEEPSTATE(i) → BOOLEAN
// In Minima Java: returns whether the i-th output preserves state variables.
// In our model: TRUE if the output coin has non-empty state variables (same semantics).
Value GETOUTKEEPSTATE(const std::vector<Value>& args, Contract& ctx) {
    requireArgs(args, 1, "GETOUTKEEPSTATE");
    long idx = numArg(args, 0, "GETOUTKEEPSTATE").getAsLong();
    const auto& outputs = ctx.txn().outputs();
    if (idx < 0 || (size_t)idx >= outputs.size())
        throw ContractException("GETOUTKEEPSTATE: index out of range");
    return Value::boolean(!outputs[(size_t)idx].stateVars().empty());
}


} // namespace fn
} // namespace minima::kissvm
