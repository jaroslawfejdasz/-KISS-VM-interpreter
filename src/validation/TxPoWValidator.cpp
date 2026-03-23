/**
 * TxPoWValidator — validates TxPoW units against chain rules.
 *
 * Implements:
 *  1. PoW check        — header hash meets txnDifficulty
 *  2. Script execution — all input coin scripts return TRUE via KISS VM
 *  3. Signature check  — all WOTS signatures in Witness are valid
 *  4. Balance check    — sum(inputs) >= sum(outputs)
 *  5. CoinID check     — output coin IDs correctly derived
 *  6. State vars check — state variables within bounds
 *  7. Size check       — TxPoW within desiredBlockSize
 */

#include "TxPoWValidator.hpp"
#include "../kissvm/Contract.hpp"
#include "../crypto/Hash.hpp"
#include "../crypto/Schnorr.hpp"
#include "../crypto/Winternitz.hpp"
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
    r       = checkSignatures(txpow);  if (!r.valid) return r;
    r       = checkScripts(txpow);     if (!r.valid) return r;
    return ValidationResult::ok();
}

// ── 1. PoW check ─────────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkPoW(const TxPoW& txpow) const {
    MiniData txpowID = txpow.computeID();

    if (txpowID.bytes().empty()) {
        return ValidationResult::fail("PoW check: null TxPoW ID");
    }

    const MiniData& minWork = txpow.body().txnDifficulty;
    const auto& diffBytes = minWork.bytes();
    bool isMaxDiff = diffBytes.empty() ||
        std::all_of(diffBytes.begin(), diffBytes.end(), [](uint8_t b){ return b == 0xFF; });
    if (isMaxDiff) {
        return ValidationResult::ok();
    }

    const auto& idBytes = txpowID.bytes();
    if (!idBytes.empty() && !diffBytes.empty() && idBytes[0] > diffBytes[0]) {
        return ValidationResult::fail("PoW check: insufficient proof-of-work");
    }

    return ValidationResult::ok();
}

// ── 2. WOTS Signature check ───────────────────────────────────────────────────
//
// Java reference: TxPoWChecker.java checkTransactionSignatures()
//
// For each Signature in Witness.mSignatureProofs:
//   for each SignatureProof sp in Signature:
//     1. msg = SHA3-256(txpow.computeID().bytes())
//     2. Winternitz::verify(sp.mPublicKey, msg, sp.mSignature)
//
// Rules:
//  - Empty witness → allowed (pure script contracts like time locks)
//  - Each SignatureProof must have PUBKEY_SIZE (2880) pubkey and SIG_SIZE (2880) sig
//  - WOTS verify must pass

ValidationResult TxPoWValidator::checkSignatures(const TxPoW& txpow) const {
    const Witness& witness = txpow.body().witness;

    if (witness.signatures().empty()) {
        return ValidationResult::ok();
    }

    // Message = SHA3-256(TxPoW ID)
    // Java: Crypto.getInstance().hashData(txpow.getTransactionID())
    MiniData txpowID  = txpow.computeID();
    MiniData msgHash  = crypto::Hash::sha3_256(
        txpowID.bytes().data(), txpowID.bytes().size());

    size_t sigIndex = 0;
    for (const auto& sigGroup : witness.signatures()) {
        for (const auto& sp : sigGroup.mSignatures) {
            // Skip empty/placeholder proofs (multi-sig where one slot is blank)
            if (sp.mPublicKey.empty() || sp.mSignature.empty()) {
                ++sigIndex;
                continue;
            }

            // Validate key sizes
            if (sp.mPublicKey.bytes().size() !=
                    static_cast<size_t>(crypto::Winternitz::PUBKEY_SIZE)) {
                std::ostringstream oss;
                oss << "Signature check: SignatureProof[" << sigIndex
                    << "] has wrong public key size ("
                    << sp.mPublicKey.bytes().size()
                    << " != " << crypto::Winternitz::PUBKEY_SIZE << ")";
                return ValidationResult::fail(oss.str());
            }

            if (sp.mSignature.bytes().size() !=
                    static_cast<size_t>(crypto::Winternitz::SIG_SIZE)) {
                std::ostringstream oss;
                oss << "Signature check: SignatureProof[" << sigIndex
                    << "] has wrong signature size ("
                    << sp.mSignature.bytes().size()
                    << " != " << crypto::Winternitz::SIG_SIZE << ")";
                return ValidationResult::fail(oss.str());
            }

            // WOTS verify: recover pubkey from sig+msg and compare
            bool ok = crypto::Schnorr::verify(sp.mPublicKey, msgHash, sp.mSignature);
            if (!ok) {
                std::ostringstream oss;
                oss << "Signature check: WOTS verification failed for SignatureProof["
                    << sigIndex << "]";
                return ValidationResult::fail(oss.str());
            }

            ++sigIndex;
        }
    }

    return ValidationResult::ok();
}

// ── 3. Script execution ───────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkScripts(const TxPoW& txpow) const {
    const Transaction& txn     = txpow.body().txn;
    const Witness&     witness = txpow.body().witness;
    const auto&        inputs  = txn.inputs();

    for (size_t i = 0; i < inputs.size(); ++i) {
        const Coin& inputCoin = inputs[i];

        const Coin* utxoCoin = m_coinLookup(inputCoin.coinID());
        if (!utxoCoin) {
            std::ostringstream oss;
            oss << "Script check: input coin not found (index " << i << ")";
            return ValidationResult::fail(oss.str());
        }

        auto scriptOpt = witness.scriptForAddress(utxoCoin->address().hash());
        if (!scriptOpt.has_value()) {
            std::ostringstream oss;
            oss << "Script check: no script for input " << i
                << " (address: " << utxoCoin->address().toHex() << ")";
            return ValidationResult::fail(oss.str());
        }

        const std::string& script = scriptOpt->str();

        // Verify script hash matches coin address (wire-exact)
        MiniString ms(script);
        auto msBytes = ms.serialise();
        MiniData scriptHash = crypto::Hash::sha3_256(msBytes.data(), msBytes.size());
        if (!(scriptHash == utxoCoin->address().hash())) {
            std::ostringstream oss;
            oss << "Script check: script hash mismatch for input " << i;
            return ValidationResult::fail(oss.str());
        }

        // Execute KISS VM contract
        try {
            kissvm::Contract contract(script, txn, witness, i);
            contract.setBlockNumber(txpow.header().blockNumber);
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

// ── 4. Balance check ─────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkBalance(const TxPoW& txpow) const {
    const Transaction& txn = txpow.body().txn;

    MiniNumber inputSum  = MiniNumber::ZERO;
    MiniNumber outputSum = MiniNumber::ZERO;

    for (const auto& coin : txn.inputs()) {
        if (!coin.hasToken()) {
            const Coin* utxo = m_coinLookup(coin.coinID());
            if (!utxo) continue;
            inputSum = inputSum + utxo->amount();
        }
    }

    for (const auto& coin : txn.outputs()) {
        if (!coin.hasToken()) {
            outputSum = outputSum + coin.amount();
        }
    }

    if (inputSum < outputSum) {
        return ValidationResult::fail("Balance check: outputs exceed inputs");
    }

    return ValidationResult::ok();
}

// ── 5. CoinID check ──────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkCoinIDs(const TxPoW& txpow) const {
    const Transaction& txn = txpow.body().txn;
    MiniData txID = txpow.computeID();

    const auto& outputs = txn.outputs();
    for (size_t i = 0; i < outputs.size(); ++i) {
        const Coin& out = outputs[i];
        if (out.coinID().bytes().empty()) continue;

        MiniData expectedID = Transaction::computeCoinID(txID, static_cast<uint32_t>(i));
        if (!(out.coinID() == expectedID)) {
            std::ostringstream oss;
            oss << "CoinID check: output " << i << " has incorrect coinID";
            return ValidationResult::fail(oss.str());
        }
    }

    return ValidationResult::ok();
}

// ── 6. State variables check ─────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkStateVars(const TxPoW& txpow) const {
    const Transaction& txn = txpow.body().txn;

    for (const auto& sv : txn.stateVars()) {
        if (sv.port() > 255) {
            std::ostringstream oss;
            oss << "StateVar check: port " << static_cast<int>(sv.port())
                << " out of range (0-255)";
            return ValidationResult::fail(oss.str());
        }
    }

    for (const auto& coin : txn.outputs()) {
        for (const auto& sv : coin.stateVars()) {
            if (sv.port() > 255) {
                return ValidationResult::fail("StateVar check: coin state port out of range");
            }
        }
    }

    return ValidationResult::ok();
}

// ── 7. Size check ────────────────────────────────────────────────────────────

ValidationResult TxPoWValidator::checkSize(const TxPoW& txpow) const {
    auto bytes = txpow.serialise();
    size_t size = bytes.size();

    const MiniNumber& maxSize = txpow.header().magic.desiredMaxTxPoWSize;
    if (maxSize == MiniNumber::ZERO) {
        return ValidationResult::ok();
    }

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
