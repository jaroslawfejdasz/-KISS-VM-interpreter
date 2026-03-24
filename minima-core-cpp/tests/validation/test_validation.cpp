/**
 * test_validation.cpp — TxPoWValidator tests
 *
 * Tests validation pipeline with mock UTxO set and KISS VM scripts.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/validation/TxPoWValidator.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/objects/Transaction.hpp"
#include "../../src/objects/Witness.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/crypto/Hash.hpp"
#include "../../src/types/MiniNumber.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniString.hpp"

#include <unordered_map>
#include <string>

using namespace minima;
using namespace minima::validation;

// ── Helper: build a mock UTxO set ─────────────────────────────────────────────

struct MockUTxO {
    std::unordered_map<std::string, Coin> store;

    void add(const Coin& c) {
        store[c.coinID().toHexString()] = c;
    }

    CoinLookup lookup() {
        return [this](const MiniData& id) -> const Coin* {
            auto it = store.find(id.toHexString());
            if (it == store.end()) return nullptr;
            return &it->second;
        };
    }
};

// ── Helper: make a coin with known script ────────────────────────────────────

static Coin makeCoin(const std::string& coinIDHex,
                     const std::string& script,
                     const std::string& amountStr = "10") {
    // Address = SHA3(script bytes)
    std::vector<uint8_t> scriptBytes(script.begin(), script.end());
    MiniData addrHash = crypto::Hash::sha3_256(scriptBytes);
    Address addr(addrHash);

    MiniData coinID = MiniData::fromHex(coinIDHex);
    Coin c;
    c.setCoinID(coinID)
     .setAddress(addr)
     .setAmount(MiniNumber(amountStr));
    return c;
}

// ── Helper: build TxPoW with no PoW requirement (test mode) ──────────────────

static Coin makeOutputCoin(const std::string& script,
                           const std::string& amountStr = "10") {
    // Output coin has no coinID — assigned by network after acceptance
    std::vector<uint8_t> scriptBytes(script.begin(), script.end());
    MiniData addrHash = crypto::Hash::sha3_256(scriptBytes);
    Address addr(addrHash);
    Coin c;
    c.setAddress(addr).setAmount(MiniNumber(amountStr));
    // coinID intentionally empty — passes checkCoinIDs
    return c;
}

static TxPoW buildTestTxPoW(const Transaction& txn,
                             const Witness& witness) {
    TxPoW txpow;
    txpow.body().txn     = txn;
    txpow.body().witness = witness;
    // In new format: txnDifficulty defaults to 0xFF...FF (max = trivially satisfied)
    // No special setup needed — default TxBody already has max difficulty
    return txpow;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST SUITE 1 — Basic structure validation
// ═══════════════════════════════════════════════════════════════════════════════

TEST_SUITE("TxPoWValidator - Basic") {

    TEST_CASE("Empty transaction validates OK") {
        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());

        TxPoW txpow = buildTestTxPoW(Transaction{}, Witness{});
        auto r = v.validate(txpow);
        CHECK(r.valid);
    }

    TEST_CASE("ValidationResult helpers") {
        auto ok   = ValidationResult::ok();
        auto fail = ValidationResult::fail("test error");

        CHECK(ok.valid);
        CHECK(ok.error.empty());
        CHECK_FALSE(fail.valid);
        CHECK(fail.error == "test error");
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST SUITE 2 — Script validation
// ═══════════════════════════════════════════════════════════════════════════════

TEST_SUITE("TxPoWValidator - Scripts") {

    TEST_CASE("Script RETURN TRUE passes") {
        const std::string script = "RETURN TRUE";
        Coin coin = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000001",
            script, "10"
        );

        MockUTxO utxo;
        utxo.add(coin);

        Transaction txn;
        txn.addInput(coin);

        Coin output = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000002",
            "RETURN TRUE", "9"
        );
        txn.addOutput(output);

        Witness witness;
        std::vector<uint8_t> scriptBytes(script.begin(), script.end());
        MiniData addrHash = crypto::Hash::sha3_256(scriptBytes);
        ScriptProof sp;
        sp.script  = MiniString(script);
        sp.address = addrHash;
        witness.addScript(sp);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(buildTestTxPoW(txn, witness));
        CHECK(r.valid);
    }

    TEST_CASE("Script RETURN FALSE fails") {
        const std::string script = "RETURN FALSE";
        Coin coin = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000011",
            script, "10"
        );

        MockUTxO utxo;
        utxo.add(coin);

        Transaction txn;
        txn.addInput(coin);

        Witness witness;
        std::vector<uint8_t> scriptBytes(script.begin(), script.end());
        MiniData addrHash = crypto::Hash::sha3_256(scriptBytes);
        ScriptProof sp;
        sp.script  = MiniString(script);
        sp.address = addrHash;
        witness.addScript(sp);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(buildTestTxPoW(txn, witness));
        CHECK_FALSE(r.valid);
        CHECK(r.error.find("FALSE") != std::string::npos);
    }

    TEST_CASE("Missing script fails") {
        const std::string script = "RETURN TRUE";
        Coin coin = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000021",
            script, "5"
        );

        MockUTxO utxo;
        utxo.add(coin);

        Transaction txn;
        txn.addInput(coin);

        Witness witness;  // no script added

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(buildTestTxPoW(txn, witness));
        CHECK_FALSE(r.valid);
    }

    TEST_CASE("Coin not in UTxO fails") {
        const std::string script = "RETURN TRUE";
        Coin coin = makeCoin(
            "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF",
            script, "5"
        );

        MockUTxO utxo;  // empty - coin not registered

        Transaction txn;
        txn.addInput(coin);

        Witness witness;
        ScriptProof sp;
        sp.script  = MiniString(script);
        sp.address = coin.address().hash();
        witness.addScript(sp);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(buildTestTxPoW(txn, witness));
        CHECK_FALSE(r.valid);
        CHECK(r.error.find("not found") != std::string::npos);
    }

    TEST_CASE("Script hash mismatch fails") {
        const std::string realScript  = "RETURN TRUE";
        const std::string fakeScript  = "RETURN FALSE";

        Coin coin = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000031",
            realScript, "5"
        );

        MockUTxO utxo;
        utxo.add(coin);

        Transaction txn;
        txn.addInput(coin);

        Witness witness;
        // Provide fake script — hash won't match coin address
        std::vector<uint8_t> fakeBytes(fakeScript.begin(), fakeScript.end());
        MiniData fakeHash = crypto::Hash::sha3_256(fakeBytes);
        ScriptProof sp;
        sp.script  = MiniString(fakeScript);
        sp.address = fakeHash;  // wrong address
        witness.addScript(sp);

        // Also add real address → fake script mapping
        std::vector<uint8_t> realBytes(realScript.begin(), realScript.end());
        MiniData realHash = crypto::Hash::sha3_256(realBytes);
        ScriptProof sp2;
        sp2.script  = MiniString(fakeScript);  // fake content for real address
        sp2.address = realHash;
        witness.addScript(sp2);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(buildTestTxPoW(txn, witness));
        CHECK_FALSE(r.valid);
    }

    TEST_CASE("KISS VM arithmetic script passes") {
        const std::string script = "LET x = 5 LET y = 3 RETURN x GT y";
        Coin coin = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000041",
            script, "5"
        );

        MockUTxO utxo;
        utxo.add(coin);

        Transaction txn;
        txn.addInput(coin);

        Coin output = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000042",
            "RETURN TRUE", "4"
        );
        txn.addOutput(output);

        Witness witness;
        std::vector<uint8_t> scriptBytes(script.begin(), script.end());
        MiniData addrHash = crypto::Hash::sha3_256(scriptBytes);
        ScriptProof sp;
        sp.script  = MiniString(script);
        sp.address = addrHash;
        witness.addScript(sp);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(buildTestTxPoW(txn, witness));
        CHECK(r.valid);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST SUITE 3 — Balance validation
// ═══════════════════════════════════════════════════════════════════════════════

TEST_SUITE("TxPoWValidator - Balance") {

    TEST_CASE("Balanced transaction (inputs == outputs)") {
        Coin inp = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000051",
            "RETURN TRUE", "10"
        );
        Coin out = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000052",
            "RETURN TRUE", "10"
        );

        MockUTxO utxo;
        utxo.add(inp);

        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkBalance(buildTestTxPoW(txn, Witness{}));
        CHECK(r.valid);
    }

    TEST_CASE("Inputs > outputs (burn/fee) passes") {
        Coin inp = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000061",
            "RETURN TRUE", "10"
        );
        Coin out = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000062",
            "RETURN TRUE", "9"  // 1 burned
        );

        MockUTxO utxo;
        utxo.add(inp);

        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkBalance(buildTestTxPoW(txn, Witness{}));
        CHECK(r.valid);
    }

    TEST_CASE("Outputs > inputs fails") {
        Coin inp = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000071",
            "RETURN TRUE", "5"
        );
        Coin out = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000072",
            "RETURN TRUE", "10"  // 10 > 5
        );

        MockUTxO utxo;
        utxo.add(inp);

        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkBalance(buildTestTxPoW(txn, Witness{}));
        CHECK_FALSE(r.valid);
        CHECK(r.error.find("exceed") != std::string::npos);
    }

    TEST_CASE("Multiple inputs and outputs balanced") {
        Coin inp1 = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000081",
            "RETURN TRUE", "7"
        );
        Coin inp2 = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000082",
            "RETURN TRUE", "3"
        );
        Coin out1 = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000083",
            "RETURN TRUE", "6"
        );
        Coin out2 = makeCoin(
            "0000000000000000000000000000000000000000000000000000000000000084",
            "RETURN TRUE", "4"
        );

        MockUTxO utxo;
        utxo.add(inp1);
        utxo.add(inp2);

        Transaction txn;
        txn.addInput(inp1);
        txn.addInput(inp2);
        txn.addOutput(out1);
        txn.addOutput(out2);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkBalance(buildTestTxPoW(txn, Witness{}));
        CHECK(r.valid);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST SUITE 4 — State variables
// ═══════════════════════════════════════════════════════════════════════════════

TEST_SUITE("TxPoWValidator - StateVars") {

    TEST_CASE("Valid state vars pass") {
        Transaction txn;
        // StateVariable(port, data)
        StateVariable sv(0, MiniData::fromHex("DEADBEEF"));
        txn.addStateVar(sv);

        TxPoWValidator v([](const MiniData&) { return nullptr; });
        auto r = v.checkStateVars(buildTestTxPoW(txn, Witness{}));
        CHECK(r.valid);
    }

    TEST_CASE("Empty transaction state check passes") {
        TxPoWValidator v([](const MiniData&) { return nullptr; });
        auto r = v.checkStateVars(buildTestTxPoW(Transaction{}, Witness{}));
        CHECK(r.valid);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST SUITE 5 — Full pipeline
// ═══════════════════════════════════════════════════════════════════════════════

TEST_SUITE("TxPoWValidator - Full Pipeline") {

    TEST_CASE("Complete valid transaction passes full validation") {
        const std::string script = "RETURN TRUE";
        Coin inp = makeCoin(
            "AAAA000000000000000000000000000000000000000000000000000000000001",
            script, "100"
        );
        Coin out = makeOutputCoin(script, "99");

        MockUTxO utxo;
        utxo.add(inp);

        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        std::vector<uint8_t> scriptBytes(script.begin(), script.end());
        MiniData addrHash = crypto::Hash::sha3_256(scriptBytes);
        Witness witness;
        ScriptProof sp;
        sp.script  = MiniString(script);
        sp.address = addrHash;
        witness.addScript(sp);

        TxPoWValidator v(utxo.lookup());
        auto r = v.validate(buildTestTxPoW(txn, witness));
        CHECK(r.valid);
        CHECK(r.error.empty());
    }

    TEST_CASE("Balance failure short-circuits full validation") {
        const std::string script = "RETURN TRUE";
        Coin inp = makeCoin(
            "CCCC000000000000000000000000000000000000000000000000000000000001",
            script, "5"
        );
        Coin out = makeOutputCoin(script, "999");

        MockUTxO utxo;
        utxo.add(inp);

        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        Witness witness;
        std::vector<uint8_t> sb(script.begin(), script.end());
        MiniData ah = crypto::Hash::sha3_256(sb);
        ScriptProof sp;
        sp.script  = MiniString(script);
        sp.address = ah;
        witness.addScript(sp);

        TxPoWValidator v(utxo.lookup());
        auto r = v.validate(buildTestTxPoW(txn, witness));
        CHECK_FALSE(r.valid);
    }

    TEST_CASE("Multi-input validated individually") {
        const std::string s1 = "RETURN TRUE";
        const std::string s2 = "LET a = 1 RETURN a EQ 1";

        Coin inp1 = makeCoin(
            "EEEE000000000000000000000000000000000000000000000000000000000001",
            s1, "10"
        );
        Coin inp2 = makeCoin(
            "EEEE000000000000000000000000000000000000000000000000000000000002",
            s2, "10"
        );
        Coin out = makeOutputCoin("RETURN TRUE", "19");

        MockUTxO utxo;
        utxo.add(inp1);
        utxo.add(inp2);

        Transaction txn;
        txn.addInput(inp1);
        txn.addInput(inp2);
        txn.addOutput(out);

        Witness witness;
        auto addScript = [&](const std::string& script) {
            std::vector<uint8_t> sb(script.begin(), script.end());
            MiniData ah = crypto::Hash::sha3_256(sb);
            ScriptProof sp;
            sp.script  = MiniString(script);
            sp.address = ah;
            witness.addScript(sp);
        };
        addScript(s1);
        addScript(s2);

        TxPoWValidator v(utxo.lookup());
        auto r = v.validate(buildTestTxPoW(txn, witness));
        CHECK(r.valid);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST SUITE 6 — MULTISIG end-to-end through TxPoWValidator
// ═══════════════════════════════════════════════════════════════════════════════

#include "../../src/crypto/RSA.hpp"

TEST_SUITE("TxPoWValidator - MULTISIG end-to-end") {

    // Build a full TxPoW with a MULTISIG script, sign its ID, validate
    auto buildMultisigTx = [](
        const std::string& script,
        const std::vector<std::pair<MiniData, MiniData>>& sigs,
        MockUTxO& utxo)
        -> TxPoW
    {
        // Input coin locked by MULTISIG script
        Coin inp = makeCoin(
            "FFFF000000000000000000000000000000000000000000000000000000000001",
            script, "10"
        );
        utxo.add(inp);

        Coin out = makeOutputCoin("RETURN TRUE", "9");

        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        Witness witness;
        std::vector<uint8_t> sb(script.begin(), script.end());
        MiniData ah = crypto::Hash::sha3_256(sb);
        ScriptProof sp;
        sp.script  = MiniString(script);
        sp.address = ah;
        witness.addScript(sp);

        for (const auto& [pub, sig] : sigs) {
            SignatureProof sigp;
            sigp.pubKey    = pub;
            sigp.signature = sig;
            witness.addSignature(sigp);
        }

        TxPoW txpow;
        txpow.body().txn     = txn;
        txpow.body().witness = witness;
        return txpow;
    };

    TEST_CASE("1-of-1 MULTISIG: valid sig → validates OK") {
        auto kp  = crypto::RSA::generateKeyPair();

        // We must sign the TxPoW ID — but the ID depends on the TxPoW content.
        // Build TxPoW first with a placeholder sig, compute ID, re-sign.
        // Use a known approach: assemble TxPoW, compute ID, sign ID, inject sig.

        // Step 1: assemble without sig to get stable structure
        std::string script = "RETURN MULTISIG(1," + kp.publicKey.toHexString() + ")";
        Coin inp = makeCoin(
            "FFFF000000000000000000000000000000000000000000000000000000000002",
            script, "10"
        );
        Coin out = makeOutputCoin("RETURN TRUE", "9");

        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        // Step 2: compute TxPoW ID without signatures (signatures don't affect header)
        TxPoW txpow;
        txpow.body().txn = txn;
        MiniData txpowID = txpow.computeID();

        // Step 3: sign the ID
        MiniData sig = crypto::RSA::sign(kp.privateKey, txpowID);

        // Step 4: add witness with signature
        Witness witness;
        std::vector<uint8_t> sb(script.begin(), script.end());
        MiniData ah = crypto::Hash::sha3_256(sb);
        ScriptProof sp; sp.script = MiniString(script); sp.address = ah;
        witness.addScript(sp);
        SignatureProof sigp; sigp.pubKey = kp.publicKey; sigp.signature = sig;
        witness.addSignature(sigp);
        txpow.body().witness = witness;

        MockUTxO utxo;
        utxo.add(inp);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(txpow);
        CHECK(r.valid);
        if (!r.valid) MESSAGE(r.error);
    }

    TEST_CASE("1-of-1 MULTISIG: no sig → fails validation") {
        auto kp = crypto::RSA::generateKeyPair();
        std::string script = "RETURN MULTISIG(1," + kp.publicKey.toHexString() + ")";

        Coin inp = makeCoin(
            "FFFF000000000000000000000000000000000000000000000000000000000003",
            script, "10"
        );
        Coin out = makeOutputCoin("RETURN TRUE", "9");

        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        Witness witness;
        std::vector<uint8_t> sb(script.begin(), script.end());
        MiniData ah = crypto::Hash::sha3_256(sb);
        ScriptProof sp; sp.script = MiniString(script); sp.address = ah;
        witness.addScript(sp);
        // NO signature in witness

        TxPoW txpow;
        txpow.body().txn     = txn;
        txpow.body().witness = witness;

        MockUTxO utxo;
        utxo.add(inp);

        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(txpow);
        CHECK_FALSE(r.valid);
    }

    TEST_CASE("2-of-2 MULTISIG: both sign → validates OK") {
        auto kp1 = crypto::RSA::generateKeyPair();
        auto kp2 = crypto::RSA::generateKeyPair();
        std::string script = "RETURN MULTISIG(2,"
            + kp1.publicKey.toHexString() + ","
            + kp2.publicKey.toHexString() + ")";

        Coin inp = makeCoin(
            "FFFF000000000000000000000000000000000000000000000000000000000004",
            script, "10"
        );
        Coin out = makeOutputCoin("RETURN TRUE", "9");
        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        TxPoW txpow;
        txpow.body().txn = txn;
        MiniData txpowID = txpow.computeID();

        MiniData sig1 = crypto::RSA::sign(kp1.privateKey, txpowID);
        MiniData sig2 = crypto::RSA::sign(kp2.privateKey, txpowID);

        Witness witness;
        std::vector<uint8_t> sb(script.begin(), script.end());
        MiniData ah = crypto::Hash::sha3_256(sb);
        ScriptProof sp; sp.script = MiniString(script); sp.address = ah;
        witness.addScript(sp);
        SignatureProof s1; s1.pubKey = kp1.publicKey; s1.signature = sig1; witness.addSignature(s1);
        SignatureProof s2; s2.pubKey = kp2.publicKey; s2.signature = sig2; witness.addSignature(s2);
        txpow.body().witness = witness;

        MockUTxO utxo; utxo.add(inp);
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(txpow);
        CHECK(r.valid);
        if (!r.valid) MESSAGE(r.error);
    }

    TEST_CASE("2-of-2 MULTISIG: only one sig → fails validation") {
        auto kp1 = crypto::RSA::generateKeyPair();
        auto kp2 = crypto::RSA::generateKeyPair();
        std::string script = "RETURN MULTISIG(2,"
            + kp1.publicKey.toHexString() + ","
            + kp2.publicKey.toHexString() + ")";

        Coin inp = makeCoin(
            "FFFF000000000000000000000000000000000000000000000000000000000005",
            script, "10"
        );
        Coin out = makeOutputCoin("RETURN TRUE", "9");
        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        TxPoW txpow;
        txpow.body().txn = txn;
        MiniData txpowID = txpow.computeID();

        MiniData sig1 = crypto::RSA::sign(kp1.privateKey, txpowID);
        // kp2 does NOT sign

        Witness witness;
        std::vector<uint8_t> sb(script.begin(), script.end());
        MiniData ah = crypto::Hash::sha3_256(sb);
        ScriptProof sp; sp.script = MiniString(script); sp.address = ah;
        witness.addScript(sp);
        SignatureProof s1; s1.pubKey = kp1.publicKey; s1.signature = sig1; witness.addSignature(s1);
        txpow.body().witness = witness;

        MockUTxO utxo; utxo.add(inp);
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(txpow);
        CHECK_FALSE(r.valid);
    }

    TEST_CASE("MULTISIG: tampered signature → fails validation") {
        auto kp = crypto::RSA::generateKeyPair();
        std::string script = "RETURN MULTISIG(1," + kp.publicKey.toHexString() + ")";

        Coin inp = makeCoin(
            "FFFF000000000000000000000000000000000000000000000000000000000006",
            script, "10"
        );
        Coin out = makeOutputCoin("RETURN TRUE", "9");
        Transaction txn;
        txn.addInput(inp);
        txn.addOutput(out);

        TxPoW txpow;
        txpow.body().txn = txn;
        MiniData txpowID = txpow.computeID();

        MiniData sig = crypto::RSA::sign(kp.privateKey, txpowID);
        auto badBytes = sig.bytes();
        badBytes[0] ^= 0xFF;  // tamper
        MiniData badSig(badBytes);

        Witness witness;
        std::vector<uint8_t> sb(script.begin(), script.end());
        MiniData ah = crypto::Hash::sha3_256(sb);
        ScriptProof sp; sp.script = MiniString(script); sp.address = ah;
        witness.addScript(sp);
        SignatureProof sigp; sigp.pubKey = kp.publicKey; sigp.signature = badSig;
        witness.addSignature(sigp);
        txpow.body().witness = witness;

        MockUTxO utxo; utxo.add(inp);
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkScripts(txpow);
        CHECK_FALSE(r.valid);
    }
}
