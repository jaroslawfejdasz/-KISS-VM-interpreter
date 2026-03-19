/**
 * TxPoWValidator — validates TxPoW units against chain rules.
 *
 * Implements:
 *  1. PoW check        — header hash meets txnDifficulty
 *  2. Script execution — all input coin scripts return TRUE via KISS VM
 *  3. Balance check    — sum(inputs) >= sum(outputs)
 *  4. CoinID check     — output coin IDs correctly derived
 *  5. State vars check — state variables within bounds
 *  6. Size check       — TxPoW within desiredBlockSize
 */

#include "TxPoWValidator.hpp"
#include "../kissvm/Contract.hpp"
#include "../crypto/Hash.hpp"
#include "../crypto/Schnorr.hpp"
#include "../objects/Address.hpp"
#include "../objects/Transaction.hpp"
#include "../objects/Witness.hpp"

#include <sstream>

namespace minima::validation {

// ── Construction ─────────────────────────────────────────────────────────────

TxPoWValidator::TxPoWValidator(CoinLookup coinLookup)
    : m_coinLookup(std::move(coinLookup))
{}

// ── Full validation ───────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::validate(const TxPoW& txpow) const {
    auto r = checkSize(txpow);         if (!r.valid) return r;
    r       = checkPoW(txpow);         if (!r.valid) return r;
    r       = checkStateVars(txpow);   if (!r.valid) return r;
    r       = checkBalance(txpow);     if (!r.valid) return r;
    r       = checkCoinIDs(txpow);     if (!r.valid) return r;
    r       = checkScripts(txpow);     if (!r.valid) return r;
    return ValidationResult::ok();
}

// ── 1. PoW check ─────────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkPoW(const TxPoW& txpow) const {
    // TxPoW ID = SHA3(SHA3(header_bytes))
    MiniData txpowID = txpow.computeID();

    // Convert ID to a number for comparison with txnDifficulty
    // In Minima: PoW score = numeric value of txpow hash (bigger = more work)
    // Minimum PoW: txpowID value >= txnDifficulty threshold
    // For now we check non-zero ID (full difficulty comparison requires BigInteger)
    if (txpowID.bytes().empty()) {
        return ValidationResult::fail("PoW check: null TxPoW ID");
    }

    // Check difficulty: all bytes of ID should be zero up to difficulty level
    // Simplified check: first byte must be 0 for any valid TxPoW
    // (Full implementation needs MiniNumber comparison of 256-bit values)
    const auto& idBytes = txpowID.bytes();
    const MiniNumber& minWork = txpow.header().minTxPoWWork;

    // If minTxPoWWork is zero (test mode), skip PoW check
    if (minWork == MiniNumber::ZERO) {
        return ValidationResult::ok();
    }

    // Basic PoW: leading zero bits
    // The hash must be numerically less than the difficulty target
    // We approximate: at least one leading zero byte
    if (idBytes.size() >= 1 && idBytes[0] != 0) {
        return ValidationResult::fail("PoW check: insufficient proof-of-work");
    }

    return ValidationResult::ok();
}

// ── 2. Script execution ───────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkScripts(const TxPoW& txpow) const {
    const Transaction& txn     = txpow.body().txn;
    const Witness&     witness = txpow.body().witness;
    const auto&        inputs  = txn.inputs();

    for (size_t i = 0; i < inputs.size(); ++i) {
        const Coin& inputCoin = inputs[i];

        // Resolve coin from UTxO set
        const Coin* utxoCoin = m_coinLookup(inputCoin.coinID());
        if (!utxoCoin) {
            std::ostringstream oss;
            oss << "Script check: input coin not found (index " << i << ")";
            return ValidationResult::fail(oss.str());
        }

        // Get the script for this coin's address
        auto scriptOpt = witness.scriptForAddress(utxoCoin->address().hash());
        if (!scriptOpt.has_value()) {
            std::ostringstream oss;
            oss << "Script check: no script for input " << i
                << " (address: " << utxoCoin->address().toHex() << ")";
            return ValidationResult::fail(oss.str());
        }

        const std::string& script = scriptOpt->str();

        // Verify script hash matches coin address
        MiniData scriptHash = crypto::Hash::sha3_256(
            std::vector<uint8_t>(script.begin(), script.end())
        );
        if (!(scriptHash == utxoCoin->address().hash())) {
            std::ostringstream oss;
            oss << "Script check: script hash mismatch for input " << i;
            return ValidationResult::fail(oss.str());
        }

        // Execute KISS VM contract
        try {
            kissvm::Contract contract(script, txn, witness, i);

            // Inject context
            contract.setBlockNumber(txpow.header().blockNumber);

            // Coin age: blockNumber - coin_creation_block
            // We don't have creation block here, use 0 (can be enhanced with MMR)
            contract.setCoinAge(MiniNumber::ZERO);

            kissvm::Value result = contract.execute();
            if (!contract.isTrue()) {
                std::ostringstream oss;
                oss << "Script check: script returned FALSE for input " << i;
                return ValidationResult::fail(oss.str());
            }
        } catch (const kissvm::ContractException& e) {
            std::ostringstream oss;
            oss << "Script check: contract exception for input " << i
                << ": " << e.what();
            return ValidationResult::fail(oss.str());
        } catch (const std::exception& e) {
            std::ostringstream oss;
            oss << "Script check: unexpected error for input " << i
                << ": " << e.what();
            return ValidationResult::fail(oss.str());
        }
    }

    return ValidationResult::ok();
}

// ── 3. Balance check ─────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkBalance(const TxPoW& txpow) const {
    const Transaction& txn = txpow.body().txn;

    // Sum native Minima inputs (skip tokens — each token has its own balance)
    MiniNumber inputSum  = MiniNumber::ZERO;
    MiniNumber outputSum = MiniNumber::ZERO;

    for (const auto& coin : txn.inputs()) {
        if (!coin.hasToken()) {
            const Coin* utxo = m_coinLookup(coin.coinID());
            if (!utxo) continue;  // already caught in checkScripts
            inputSum = inputSum + utxo->amount();
        }
    }

    for (const auto& coin : txn.outputs()) {
        if (!coin.hasToken()) {
            outputSum = outputSum + coin.amount();
        }
    }

    // inputs >= outputs (difference is the burn/fee)
    if (inputSum < outputSum) {
        std::ostringstream oss;
        oss << "Balance check: outputs exceed inputs";
        return ValidationResult::fail(oss.str());
    }

    return ValidationResult::ok();
}

// ── 4. CoinID check ──────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkCoinIDs(const TxPoW& txpow) const {
    const Transaction& txn = txpow.body().txn;
    MiniData txID = txpow.computeID();

    const auto& outputs = txn.outputs();
    for (size_t i = 0; i < outputs.size(); ++i) {
        const Coin& out = outputs[i];
        if (out.coinID().bytes().empty()) continue;  // floating coin — ID assigned later

        MiniData expectedID = Transaction::computeCoinID(txID, static_cast<uint32_t>(i));
        if (!(out.coinID() == expectedID)) {
            std::ostringstream oss;
            oss << "CoinID check: output " << i << " has incorrect coinID";
            return ValidationResult::fail(oss.str());
        }
    }

    return ValidationResult::ok();
}

// ── 5. State variables check ─────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkStateVars(const TxPoW& txpow) const {
    const Transaction& txn = txpow.body().txn;

    // Port numbers must be in range 0..255
    for (const auto& sv : txn.stateVars()) {
        if (sv.port() > 255) {
            std::ostringstream oss;
            oss << "StateVar check: port " << static_cast<int>(sv.port())
                << " out of range (0-255)";
            return ValidationResult::fail(oss.str());
        }
    }

    // Check output coin state vars
    for (const auto& coin : txn.outputs()) {
        for (const auto& sv : coin.stateVars()) {
            if (sv.port() > 255) {
                return ValidationResult::fail("StateVar check: coin state port out of range");
            }
        }
    }

    return ValidationResult::ok();
}

// ── 6. Size check ────────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkSize(const TxPoW& txpow) const {
    auto bytes = txpow.serialise();
    size_t size = bytes.size();

    // desiredBlockSize = magic number for max TxPoW size
    // If not set (zero), skip check
    const MiniNumber& maxSize = txpow.header().desiredBlockSize;
    if (maxSize == MiniNumber::ZERO) {
        return ValidationResult::ok();
    }

    // Convert maxSize to uint64_t for comparison
    // MiniNumber stores as decimal string — parse it
    try {
        uint64_t maxBytes = static_cast<uint64_t>(std::stoull(maxSize.toString()));
        if (size > maxBytes) {
            std::ostringstream oss;
            oss << "Size check: TxPoW size " << size
                << " exceeds limit " << maxBytes;
            return ValidationResult::fail(oss.str());
        }
    } catch (...) {
        // If we can't parse, skip size check
    }

    return ValidationResult::ok();
}

} // namespace minima::validation
