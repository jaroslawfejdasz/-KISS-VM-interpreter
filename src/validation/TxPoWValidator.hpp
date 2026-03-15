#pragma once
/**
 * TxPoWValidator — validates TxPoW units against chain rules.
 *
 * Responsible for:
 *  1. PoW check        — hash meets minimum difficulty
 *  2. Script execution — all input coin scripts return TRUE
 *  3. Balance check    — sum(inputs) >= sum(outputs) + burn
 *  4. CoinID check     — output coin IDs are correctly derived
 *  5. State checks     — state variables are well-formed
 *  6. Size check       — TxPoW within magic number limits
 *
 * Chain-level validation (MMR proofs, block ordering) is out of scope here —
 * that belongs in a higher-level ChainProcessor module.
 */

#include "../objects/TxPoW.hpp"
#include "../objects/Coin.hpp"
#include <string>
#include <functional>
#include <vector>

namespace minima::validation {

struct ValidationResult {
    bool        valid{false};
    std::string error;    // empty if valid

    static ValidationResult ok()                       { return {true, ""}; }
    static ValidationResult fail(const std::string& e) { return {false, e}; }
};

/**
 * CoinLookup — callback to resolve input coins from the UTxO set.
 * The validator asks for each input coin by its coinID.
 * Returns nullptr if coin not found (validation fails).
 */
using CoinLookup = std::function<const Coin*(const MiniData& coinID)>;

class TxPoWValidator {
public:
    explicit TxPoWValidator(CoinLookup coinLookup);

    // Full validation
    ValidationResult validate(const TxPoW& txpow) const;

    // Individual checks (useful for unit testing)
    ValidationResult checkPoW         (const TxPoW& txpow) const;
    ValidationResult checkScripts     (const TxPoW& txpow) const;
    ValidationResult checkBalance     (const TxPoW& txpow) const;
    ValidationResult checkCoinIDs     (const TxPoW& txpow) const;
    ValidationResult checkStateVars   (const TxPoW& txpow) const;
    ValidationResult checkSize        (const TxPoW& txpow) const;

private:
    CoinLookup m_coinLookup;
};

} // namespace minima::validation
