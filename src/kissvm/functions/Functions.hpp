#pragma once
/**
 * Functions — all 42+ built-in KISS VM functions.
 *
 * Each function takes a vector of Value arguments and the current Contract
 * context, and returns a Value.
 *
 * Organised by category (mirrors Java src/org/minima/kissvm/functions/):
 *   - Signature:    SIGNEDBY, CHECKSIG, MULTISIG
 *   - State:        STATE, PREVSTATE, SAMESTATE
 *   - Hashing:      SHA2, SHA3
 *   - String/HEX:   CONCAT, SUBSET, LEN, REV, REPLACE, CLEAN, UTF8, ASCII
 *   - Conversion:   NUMBER, HEX, SCRIPT, BOOL, HEXTONUM, NUMTOHEX
 *   - Math:         ABS, MAX, MIN, DEC, INC, FLOOR, CEIL, POW, SQRT
 *   - Bitwise:      BITSET, BITGET, BITCOUNT
 *   - Verification: VERIFYOUT, VERIFYIN, GETOUTADDR, GETOUTAMT, GETOUTTOKEN,
 *                   SUMINPUTS, SUMOUTPUTS
 *   - Chain:        @BLOCK, @COINAGE, @AMOUNT, @ADDRESS, @SCRIPT,
 *                   @TOTIN, @TOTOUT, @INPUT, @OUTPUT
 */

#include "../Value.hpp"
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace minima::kissvm {

class Contract;  // forward declaration

using FunctionImpl = std::function<Value(const std::vector<Value>&, Contract&)>;

/**
 * FunctionRegistry — maps function names (uppercase) to implementations.
 * Populated at startup via registerAll().
 */
class FunctionRegistry {
public:
    static FunctionRegistry& instance();

    void registerAll();
    bool has(const std::string& name) const;
    Value call(const std::string& name, const std::vector<Value>& args, Contract& ctx) const;

private:
    FunctionRegistry() = default;
    std::unordered_map<std::string, FunctionImpl> m_funcs;
    void reg(const std::string& name, FunctionImpl fn);
};

// ── Individual function declarations ─────────────────────────────────────────

namespace fn {
    // Signature functions
    Value SIGNEDBY (const std::vector<Value>& args, Contract& ctx);
    Value CHECKSIG (const std::vector<Value>& args, Contract& ctx);
    Value MULTISIG (const std::vector<Value>& args, Contract& ctx);

    // State functions
    Value STATE     (const std::vector<Value>& args, Contract& ctx);
    Value PREVSTATE (const std::vector<Value>& args, Contract& ctx);
    Value SAMESTATE (const std::vector<Value>& args, Contract& ctx);

    // Hashing
    Value SHA2 (const std::vector<Value>& args, Contract& ctx);
    Value SHA3 (const std::vector<Value>& args, Contract& ctx);

    // HEX / String manipulation
    Value CONCAT  (const std::vector<Value>& args, Contract& ctx);
    Value SUBSET  (const std::vector<Value>& args, Contract& ctx);
    Value LEN     (const std::vector<Value>& args, Contract& ctx);
    Value REV     (const std::vector<Value>& args, Contract& ctx);
    Value REPLACE (const std::vector<Value>& args, Contract& ctx);
    Value CLEAN   (const std::vector<Value>& args, Contract& ctx);
    Value UTF8    (const std::vector<Value>& args, Contract& ctx);
    Value ASCII   (const std::vector<Value>& args, Contract& ctx);

    // Type conversions
    Value NUMBER   (const std::vector<Value>& args, Contract& ctx);
    Value HEX      (const std::vector<Value>& args, Contract& ctx);
    Value SCRIPT   (const std::vector<Value>& args, Contract& ctx);
    Value BOOL     (const std::vector<Value>& args, Contract& ctx);
    Value HEXTONUM (const std::vector<Value>& args, Contract& ctx);
    Value NUMTOHEX (const std::vector<Value>& args, Contract& ctx);

    // Math
    Value ABS   (const std::vector<Value>& args, Contract& ctx);
    Value MAX   (const std::vector<Value>& args, Contract& ctx);
    Value MIN   (const std::vector<Value>& args, Contract& ctx);
    Value DEC   (const std::vector<Value>& args, Contract& ctx);
    Value INC   (const std::vector<Value>& args, Contract& ctx);
    Value FLOOR (const std::vector<Value>& args, Contract& ctx);
    Value CEIL  (const std::vector<Value>& args, Contract& ctx);
    Value POW   (const std::vector<Value>& args, Contract& ctx);
    Value SQRT  (const std::vector<Value>& args, Contract& ctx);

    // Bitwise
    Value BITSET   (const std::vector<Value>& args, Contract& ctx);
    Value BITGET   (const std::vector<Value>& args, Contract& ctx);
    Value BITCOUNT (const std::vector<Value>& args, Contract& ctx);

    // Transaction verification
    Value VERIFYOUT   (const std::vector<Value>& args, Contract& ctx);
    Value VERIFYIN    (const std::vector<Value>& args, Contract& ctx);
    Value GETOUTADDR  (const std::vector<Value>& args, Contract& ctx);
    Value GETOUTAMT   (const std::vector<Value>& args, Contract& ctx);
    Value GETOUTTOKEN (const std::vector<Value>& args, Contract& ctx);
    Value SUMINPUTS   (const std::vector<Value>& args, Contract& ctx);
    Value SUMOUTPUTS  (const std::vector<Value>& args, Contract& ctx);
    Value STRING         (const std::vector<Value>& args, Contract& ctx);
    Value EXISTS         (const std::vector<Value>& args, Contract& ctx);
    Value OVERWRITE      (const std::vector<Value>& args, Contract& ctx);
    Value SETLEN         (const std::vector<Value>& args, Contract& ctx);
    Value SIGDIG         (const std::vector<Value>& args, Contract& ctx);
    Value PROOF          (const std::vector<Value>& args, Contract& ctx);
    Value REPLACEFIRST   (const std::vector<Value>& args, Contract& ctx);
    Value SUBSTR         (const std::vector<Value>& args, Contract& ctx);
    Value GETINADDR      (const std::vector<Value>& args, Contract& ctx);
    Value GETINAMT       (const std::vector<Value>& args, Contract& ctx);
    Value GETINID        (const std::vector<Value>& args, Contract& ctx);
    Value GETINTOK       (const std::vector<Value>& args, Contract& ctx);
    Value GETOUTKEEPSTATE(const std::vector<Value>& args, Contract& ctx);
} // namespace fn

} // namespace minima::kissvm
