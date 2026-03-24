/**
 * test_witness_wire.cpp — Wire format correctness for Witness, Signature, SignatureProof,
 *                          CoinProof, ScriptProof.
 *
 * Java reference: Witness.java, Signature.java, SignatureProof.java,
 *                 CoinProof.java, ScriptProof.java
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/objects/Witness.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniString.hpp"
#include "../../src/types/MiniNumber.hpp"
#include "../../src/mmr/MMRSet.hpp"
#include "../../src/crypto/Hash.hpp"

using namespace minima;

static MiniData fill32(uint8_t v) {
    return MiniData(std::vector<uint8_t>(32, v));
}
static MiniData fill2880(uint8_t v) {
    return MiniData(std::vector<uint8_t>(2880, v));
}

// Build a valid SignatureProof with a real 1-leaf MMRProof
static SignatureProof makeSignatureProof(uint8_t keyByte) {
    MiniData pubKey = fill32(keyByte);
    MiniData sig    = fill2880(keyByte + 0x10);

    // Build 1-leaf MMR so we have a real proof
    MMRSet mmr;
    // leaf = hash(pubKey.serialise())
    auto pkBytes = pubKey.serialise();
    MiniData leafHash = crypto::Hash::sha3_256(pkBytes.data(), pkBytes.size());
    MMRData  leafData(leafHash, MiniNumber(int64_t(0)), false);
    MMREntry entry = mmr.addLeaf(leafData);

    MMRProof proof = mmr.getProof(entry.getEntry());

    SignatureProof sp;
    sp.mPublicKey = pubKey;
    sp.mSignature = sig;
    sp.mProof     = proof;
    return sp;
}

// Build a valid ScriptProof with a real 1-leaf MMRProof
static ScriptProof makeScriptProof(const std::string& script) {
    MiniString ms(script);
    auto scriptBytes = ms.serialise();
    MiniData leafHash = crypto::Hash::sha3_256(scriptBytes.data(), scriptBytes.size());

    MMRSet mmr;
    MMRData leafData(leafHash, MiniNumber(int64_t(0)), false);
    MMREntry entry = mmr.addLeaf(leafData);
    MMRProof proof = mmr.getProof(entry.getEntry());

    ScriptProof sp;
    sp.mScript = ms;
    sp.mProof  = proof;
    return sp;
}

TEST_SUITE("Witness Wire Format") {

    TEST_CASE("Empty Witness roundtrip") {
        Witness w;
        CHECK(w.isEmpty());
        auto bytes = w.serialise();
        REQUIRE(bytes.size() >= 3); // 3x MiniNumber(0) = 3x {scale=0, len=1, byte=0}
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);
        CHECK(w2.isEmpty());
        CHECK(off == bytes.size());
    }

    TEST_CASE("SignatureProof roundtrip") {
        SignatureProof sp = makeSignatureProof(0xAA);
        auto bytes = sp.serialise();
        size_t off = 0;
        SignatureProof sp2 = SignatureProof::deserialise(bytes.data(), off);
        CHECK(sp2.mPublicKey == sp.mPublicKey);
        CHECK(sp2.mSignature == sp.mSignature);
        CHECK(off == bytes.size());
    }

    TEST_CASE("SignatureProof::getRootPublicKey() is deterministic") {
        SignatureProof sp = makeSignatureProof(0x55);
        MiniData root1 = sp.getRootPublicKey();
        MiniData root2 = sp.getRootPublicKey();
        CHECK(root1 == root2);
        CHECK(root1.bytes().size() == 32);
    }

    TEST_CASE("Signature roundtrip (list of SignatureProof)") {
        Signature sig;
        sig.addSignatureProof(makeSignatureProof(0x01));
        sig.addSignatureProof(makeSignatureProof(0x02));

        auto bytes = sig.serialise();
        size_t off = 0;
        Signature sig2 = Signature::deserialise(bytes.data(), off);

        REQUIRE(sig2.mSignatures.size() == 2);
        CHECK(sig2.mSignatures[0].mPublicKey == fill32(0x01));
        CHECK(sig2.mSignatures[1].mPublicKey == fill32(0x02));
        CHECK(off == bytes.size());
    }

    TEST_CASE("Signature::getRootPublicKey() equals first proof root") {
        Signature sig;
        SignatureProof sp = makeSignatureProof(0x77);
        sig.addSignatureProof(sp);
        MiniData root = sig.getRootPublicKey();
        MiniData expected = sp.getRootPublicKey();
        CHECK(root == expected);
    }

    TEST_CASE("CoinProof roundtrip") {
        Coin coin;
        coin.setCoinID(fill32(0xCC));
        coin.setAmount(MiniNumber(int64_t(999)));
        Address addr(fill32(0xAB));
        coin.setAddress(addr);

        MMRSet mmr;
        auto coinBytes = coin.serialise();
        MiniData leafHash = crypto::Hash::sha3_256(coinBytes.data(), coinBytes.size());
        MMRData leafData(leafHash, coin.amount(), false);
        MMREntry entry = mmr.addLeaf(leafData);
        MMRProof proof = mmr.getProof(entry.getEntry());

        CoinProof cp;
        cp.coin()  = coin;
        cp.proof() = proof;

        auto bytes = cp.serialise();
        size_t off = 0;
        CoinProof cp2 = CoinProof::deserialise(bytes.data(), off);
        CHECK(cp2.coin().coinID() == fill32(0xCC));
        CHECK(cp2.coin().amount() == MiniNumber(int64_t(999)));
        CHECK(off == bytes.size());
    }

    TEST_CASE("ScriptProof roundtrip") {
        ScriptProof sp = makeScriptProof("RETURN TRUE");
        auto bytes = sp.serialise();
        size_t off = 0;
        ScriptProof sp2 = ScriptProof::deserialise(bytes.data(), off);
        CHECK(sp2.mScript.str() == "RETURN TRUE");
        CHECK(off == bytes.size());
    }

    TEST_CASE("ScriptProof::address() is deterministic and non-empty") {
        ScriptProof sp = makeScriptProof("LET x = 5 RETURN x GT 3");
        MiniData addr1 = sp.address();
        MiniData addr2 = sp.address();
        CHECK(addr1 == addr2);
        CHECK(addr1.bytes().size() == 32);
    }

    TEST_CASE("ScriptProof::address() differs for different scripts") {
        ScriptProof sp1 = makeScriptProof("RETURN TRUE");
        ScriptProof sp2 = makeScriptProof("RETURN FALSE");
        CHECK(sp1.address() != sp2.address());
    }

    TEST_CASE("Witness with one Signature roundtrip") {
        Signature sig;
        sig.addSignatureProof(makeSignatureProof(0x11));

        Witness w;
        w.addSignature(sig);

        auto bytes = w.serialise();
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);

        REQUIRE(w2.signatures().size() == 1);
        REQUIRE(w2.signatures()[0].mSignatures.size() == 1);
        CHECK(w2.signatures()[0].mSignatures[0].mPublicKey == fill32(0x11));
        CHECK(w2.coinProofs().empty());
        CHECK(w2.scriptProofs().empty());
        CHECK(off == bytes.size());
    }

    TEST_CASE("Witness with CoinProof roundtrip") {
        Coin coin;
        coin.setCoinID(fill32(0xDD));
        coin.setAmount(MiniNumber(int64_t(500)));
        Address addr(fill32(0xEE));
        coin.setAddress(addr);

        MMRSet mmr;
        auto cb = coin.serialise();
        MiniData lh = crypto::Hash::sha3_256(cb.data(), cb.size());
        MMREntry e = mmr.addLeaf(MMRData(lh, coin.amount(), false));

        CoinProof cp; cp.coin() = coin; cp.proof() = mmr.getProof(e.getEntry());

        Witness w;
        w.addCoinProof(cp);

        auto bytes = w.serialise();
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);
        REQUIRE(w2.coinProofs().size() == 1);
        CHECK(w2.coinProofs()[0].coin().coinID() == fill32(0xDD));
        CHECK(off == bytes.size());
    }

    TEST_CASE("Witness with ScriptProof roundtrip") {
        ScriptProof sp = makeScriptProof("RETURN TRUE");
        Witness w;
        w.addScriptProof(sp);

        auto bytes = w.serialise();
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);
        REQUIRE(w2.scriptProofs().size() == 1);
        CHECK(w2.scriptProofs()[0].mScript.str() == "RETURN TRUE");
        CHECK(w2.signatures().empty());
        CHECK(off == bytes.size());
    }

    TEST_CASE("Witness::scriptForAddress lookup") {
        ScriptProof sp1 = makeScriptProof("RETURN TRUE");
        ScriptProof sp2 = makeScriptProof("LET x = 5 RETURN x GT 3");

        Witness w;
        w.addScriptProof(sp1);
        w.addScriptProof(sp2);

        MiniData addr2 = sp2.address();
        auto found = w.scriptForAddress(addr2);
        REQUIRE(found.has_value());
        CHECK(found->str() == "LET x = 5 RETURN x GT 3");

        CHECK(!w.scriptForAddress(fill32(0xFF)).has_value());
    }

    TEST_CASE("Witness::isSignedBy") {
        SignatureProof sp = makeSignatureProof(0x42);
        MiniData root = sp.getRootPublicKey();

        Signature sig;
        sig.addSignatureProof(sp);

        Witness w;
        w.addSignature(sig);

        CHECK(w.isSignedBy(root));
        CHECK(!w.isSignedBy(fill32(0xFF)));
    }

    TEST_CASE("Full Witness: 2 sigs + 2 coinproofs + 2 scripts") {
        Witness w;

        // 2 Signatures (each with 1 SignatureProof)
        for (int i = 0; i < 2; ++i) {
            Signature sig;
            sig.addSignatureProof(makeSignatureProof(static_cast<uint8_t>(0x10 + i)));
            w.addSignature(sig);
        }

        // 2 CoinProofs
        for (int i = 0; i < 2; ++i) {
            Coin coin;
            coin.setCoinID(fill32(static_cast<uint8_t>(0x20 + i)));
            coin.setAmount(MiniNumber(int64_t(100 + i)));
            Address addr(fill32(static_cast<uint8_t>(0x30 + i)));
            coin.setAddress(addr);

            MMRSet mmr;
            auto cb = coin.serialise();
            MiniData lh = crypto::Hash::sha3_256(cb.data(), cb.size());
            MMREntry e = mmr.addLeaf(MMRData(lh, coin.amount(), false));

            CoinProof cp; cp.coin() = coin; cp.proof() = mmr.getProof(e.getEntry());
            w.addCoinProof(cp);
        }

        // 2 ScriptProofs
        w.addScriptProof(makeScriptProof("RETURN TRUE"));
        w.addScriptProof(makeScriptProof("LET x = 1 RETURN x EQ 1"));

        // Roundtrip
        auto bytes = w.serialise();
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);

        CHECK(w2.signatures().size() == 2);
        CHECK(w2.coinProofs().size() == 2);
        CHECK(w2.scriptProofs().size() == 2);
        CHECK(w2.signatures()[0].mSignatures[0].mPublicKey == fill32(0x10));
        CHECK(w2.signatures()[1].mSignatures[0].mPublicKey == fill32(0x11));
        CHECK(w2.coinProofs()[0].coin().coinID() == fill32(0x20));
        CHECK(w2.coinProofs()[1].coin().coinID() == fill32(0x21));
        CHECK(w2.scriptProofs()[0].mScript.str() == "RETURN TRUE");
        CHECK(w2.scriptProofs()[1].mScript.str() == "LET x = 1 RETURN x EQ 1");
        CHECK(off == bytes.size());
    }

    TEST_CASE("CoinProof::getMMRData() is deterministic") {
        Coin coin;
        coin.setCoinID(fill32(0x99));
        coin.setAmount(MiniNumber(int64_t(42)));
        Address addr(fill32(0xAA));
        coin.setAddress(addr);

        MMRSet mmr;
        auto cb = coin.serialise();
        MiniData lh = crypto::Hash::sha3_256(cb.data(), cb.size());
        MMREntry e = mmr.addLeaf(MMRData(lh, coin.amount(), false));

        CoinProof cp; cp.coin() = coin; cp.proof() = mmr.getProof(e.getEntry());
        MMRData d1 = cp.getMMRData();
        MMRData d2 = cp.getMMRData();
        CHECK(d1.getData() == d2.getData());
        CHECK(!d1.getData().empty());
    }
}
