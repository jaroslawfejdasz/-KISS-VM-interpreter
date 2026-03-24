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
    // Address = SHA3(MiniString.serialise()) — wire-exact (Java: CreateMMRDataLeafNode)
    MiniString ms(script);
    auto msBytes = ms.serialise();
    MiniData addrHash = crypto::Hash::sha3_256(msBytes.data(), msBytes.size());
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
    // Address = SHA3(MiniString.serialise()) — wire-exact
    MiniString ms(script);
    auto msBytes = ms.serialise();
    MiniData addrHash = crypto::Hash::sha3_256(msBytes.data(), msBytes.size());
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
        MiniString __ms(script);
        auto __msBytes = __ms.serialise();
        MiniData addrHash = crypto::Hash::sha3_256(__msBytes.data(), __msBytes.size());
        ScriptProof sp;
        sp.mScript  = MiniString(script);
        // address computed automatically from sp.mScript
        witness.addScriptProof(sp);

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
        MiniString __ms(script);
        auto __msBytes = __ms.serialise();
        MiniData addrHash = crypto::Hash::sha3_256(__msBytes.data(), __msBytes.size());
        ScriptProof sp;
        sp.mScript  = MiniString(script);
        // address computed automatically from sp.mScript
        witness.addScriptProof(sp);

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
        sp.mScript  = MiniString(script);
        // address computed automatically from sp.mScript
        witness.addScriptProof(sp);

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
        sp.mScript  = MiniString(fakeScript);
        // address computed automatically from sp.mScript
        witness.addScriptProof(sp);

        // Also add real address → fake script mapping
        std::vector<uint8_t> realBytes(realScript.begin(), realScript.end());
        MiniData realHash = crypto::Hash::sha3_256(realBytes);
        ScriptProof sp2;
        sp2.mScript  = MiniString(fakeScript);  // fake content for real address
        // address is computed from sp2.mScript (will compute hash of fakeScript)
        witness.addScriptProof(sp2);

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
        MiniString __ms(script);
        auto __msBytes = __ms.serialise();
        MiniData addrHash = crypto::Hash::sha3_256(__msBytes.data(), __msBytes.size());
        ScriptProof sp;
        sp.mScript  = MiniString(script);
        // address computed automatically from sp.mScript
        witness.addScriptProof(sp);

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

        MiniString __ms(script);
        auto __msBytes = __ms.serialise();
        MiniData addrHash = crypto::Hash::sha3_256(__msBytes.data(), __msBytes.size());
        Witness witness;
        ScriptProof sp;
        sp.mScript  = MiniString(script);
        // address computed automatically from sp.mScript
        witness.addScriptProof(sp);

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
        sp.mScript  = MiniString(script);
        // address computed automatically from sp.mScript
        witness.addScriptProof(sp);

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
            sp.mScript  = MiniString(script);
        // address computed automatically from sp.mScript
            witness.addScriptProof(sp);
        };
        addScript(s1);
        addScript(s2);

        TxPoWValidator v(utxo.lookup());
        auto r = v.validate(buildTestTxPoW(txn, witness));
        CHECK(r.valid);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST SUITE 6 — WOTS Signature validation
// ═══════════════════════════════════════════════════════════════════════════════

#include "../../src/crypto/Schnorr.hpp"
#include "../../src/crypto/Winternitz.hpp"

TEST_SUITE("TxPoWValidator - WOTS Signatures") {

    // Helper: build valid WOTS keypair + signature for a given TxPoW ID
    static auto makeWOTSSig(const MiniData& txpowID) {
        using namespace minima::crypto;
        std::vector<uint8_t> seed(32);
        for (int i = 0; i < 32; ++i) seed[i] = static_cast<uint8_t>(i * 7 + 3);

        auto pubkeyBytes = Winternitz::generatePublicKey(seed);
        MiniData pubkey(pubkeyBytes);

        MiniData msgHash = Hash::sha3_256(txpowID.bytes().data(), txpowID.bytes().size());
        MiniData sig = Schnorr::sign(MiniData(seed), msgHash);

        return std::make_tuple(pubkey, sig, msgHash);
    }

    TEST_CASE("Empty witness (no signatures) always passes") {
        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());
        TxPoW txpow = buildTestTxPoW(Transaction{}, Witness{});
        auto r = v.checkSignatures(txpow);
        CHECK(r.valid);
    }

    TEST_CASE("Valid WOTS signature passes checkSignatures") {
        TxPoW txpow = buildTestTxPoW(Transaction{}, Witness{});
        MiniData txpowID = txpow.computeID();

        auto [pubkey, sig, msg] = makeWOTSSig(txpowID);

        SignatureProof sp;
        sp.mPublicKey = pubkey;
        sp.mSignature = sig;
        Signature sigGroup;
        sigGroup.addSignatureProof(sp);

        Witness witness;
        witness.addSignature(sigGroup);

        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());
        // Note: ID is derived from header only — witness doesn't affect it
        auto r = v.checkSignatures(buildTestTxPoW(Transaction{}, witness));
        CHECK(r.valid);
        CHECK(r.error.empty());
    }

    TEST_CASE("Invalid WOTS signature (corrupted) fails") {
        TxPoW txpow = buildTestTxPoW(Transaction{}, Witness{});
        MiniData txpowID = txpow.computeID();

        auto [pubkey, sig, msg] = makeWOTSSig(txpowID);

        // Corrupt first byte of signature
        auto sigBytes = sig.bytes();
        sigBytes[0] ^= 0xFF;
        MiniData badSig(sigBytes);

        SignatureProof sp;
        sp.mPublicKey = pubkey;
        sp.mSignature = badSig;
        Signature sigGroup;
        sigGroup.addSignatureProof(sp);

        Witness witness;
        witness.addSignature(sigGroup);

        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkSignatures(buildTestTxPoW(Transaction{}, witness));
        CHECK_FALSE(r.valid);
        CHECK(r.error.find("WOTS verification failed") != std::string::npos);
    }

    TEST_CASE("Wrong public key size fails") {
        Witness witness;
        SignatureProof sp;
        sp.mPublicKey = MiniData(std::vector<uint8_t>(64, 0xAB));  // too small
        sp.mSignature = MiniData(std::vector<uint8_t>(crypto::Winternitz::SIG_SIZE, 0x00));
        Signature sigGroup;
        sigGroup.addSignatureProof(sp);
        witness.addSignature(sigGroup);

        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkSignatures(buildTestTxPoW(Transaction{}, witness));
        CHECK_FALSE(r.valid);
        CHECK(r.error.find("wrong public key size") != std::string::npos);
    }

    TEST_CASE("Wrong signature size fails") {
        Witness witness;
        SignatureProof sp;
        sp.mPublicKey = MiniData(std::vector<uint8_t>(crypto::Winternitz::PUBKEY_SIZE, 0xAB));
        sp.mSignature = MiniData(std::vector<uint8_t>(64, 0x00));  // too small
        Signature sigGroup;
        sigGroup.addSignatureProof(sp);
        witness.addSignature(sigGroup);

        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkSignatures(buildTestTxPoW(Transaction{}, witness));
        CHECK_FALSE(r.valid);
        CHECK(r.error.find("wrong signature size") != std::string::npos);
    }

    TEST_CASE("Multiple valid signatures (multi-signer) all pass") {
        TxPoW txpow = buildTestTxPoW(Transaction{}, Witness{});
        MiniData txpowID = txpow.computeID();

        Witness witness;
        for (int signer = 0; signer < 2; ++signer) {
            std::vector<uint8_t> seed(32);
            for (int i = 0; i < 32; ++i) seed[i] = static_cast<uint8_t>(signer * 100 + i);

            auto pubkeyBytes = crypto::Winternitz::generatePublicKey(seed);
            MiniData pubkey(pubkeyBytes);
            MiniData msgHash = crypto::Hash::sha3_256(
                txpowID.bytes().data(), txpowID.bytes().size());
            MiniData sig = crypto::Schnorr::sign(MiniData(seed), msgHash);

            SignatureProof sp;
            sp.mPublicKey = pubkey;
            sp.mSignature = sig;
            Signature sigGroup;
            sigGroup.addSignatureProof(sp);
            witness.addSignature(sigGroup);
        }

        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkSignatures(buildTestTxPoW(Transaction{}, witness));
        CHECK(r.valid);
    }

    TEST_CASE("Signature for wrong TxPoW ID fails") {
        MiniData wrongID = MiniData::fromHex(
            "DEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF");

        auto [pubkey, sig, msg] = makeWOTSSig(wrongID);

        SignatureProof sp;
        sp.mPublicKey = pubkey;
        sp.mSignature = sig;
        Signature sigGroup;
        sigGroup.addSignatureProof(sp);

        Witness witness;
        witness.addSignature(sigGroup);

        // This TxPoW has a different ID than wrongID → verify must fail
        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkSignatures(buildTestTxPoW(Transaction{}, witness));
        CHECK_FALSE(r.valid);
    }

    TEST_CASE("Empty SignatureProof slots skipped (multi-sig placeholders)") {
        TxPoW txpow = buildTestTxPoW(Transaction{}, Witness{});
        MiniData txpowID = txpow.computeID();

        Witness witness;
        {
            std::vector<uint8_t> seed(32, 0x42);
            auto pubkeyBytes = crypto::Winternitz::generatePublicKey(seed);
            MiniData pubkey(pubkeyBytes);
            MiniData msgHash = crypto::Hash::sha3_256(
                txpowID.bytes().data(), txpowID.bytes().size());
            MiniData sig = crypto::Schnorr::sign(MiniData(seed), msgHash);

            Signature sigGroup;
            SignatureProof sp1; sp1.mPublicKey = pubkey; sp1.mSignature = sig;
            sigGroup.addSignatureProof(sp1);
            sigGroup.addSignatureProof(SignatureProof{});  // empty placeholder
            witness.addSignature(sigGroup);
        }

        MockUTxO utxo;
        TxPoWValidator v(utxo.lookup());
        auto r = v.checkSignatures(buildTestTxPoW(Transaction{}, witness));
        CHECK(r.valid);
    }
}
