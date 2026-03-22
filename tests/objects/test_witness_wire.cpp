/**
 * test_witness_wire.cpp — Wire format correctness for Witness, Signature, ScriptProof
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/objects/Witness.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniString.hpp"
#include "../../src/types/MiniNumber.hpp"
#include "../../src/crypto/Hash.hpp"

using namespace minima;

static MiniData fill32(uint8_t v) {
    return MiniData(std::vector<uint8_t>(32, v));
}
static MiniData fill2880(uint8_t v) {
    return MiniData(std::vector<uint8_t>(2880, v));
}

TEST_SUITE("Witness Wire Format") {

    TEST_CASE("Empty Witness roundtrip") {
        Witness w;
        CHECK(w.isEmpty());
        auto bytes = w.serialise();
        REQUIRE(bytes.size() >= 2);
        CHECK(bytes[0] == 0x00);
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);
        CHECK(w2.isEmpty());
    }

    TEST_CASE("Signature roundtrip") {
        Signature sig;
        sig.rootPublicKey = fill32(0xAA);
        sig.leafIndex     = MiniNumber(int64_t(42));
        sig.publicKey     = fill2880(0xBB);
        sig.signature     = fill2880(0xCC);
        sig.proof.nodes   = { fill32(0x01), fill32(0x02), fill32(0x03) };

        auto bytes = sig.serialise();
        size_t off = 0;
        Signature sig2 = Signature::deserialise(bytes.data(), off);

        CHECK(sig2.rootPublicKey == sig.rootPublicKey);
        CHECK(sig2.leafIndex == sig.leafIndex);
        CHECK(sig2.publicKey == sig.publicKey);
        CHECK(sig2.signature == sig.signature);
        CHECK(sig2.proof.size() == 3);
        CHECK(sig2.proof.nodes[0] == fill32(0x01));
        CHECK(sig2.proof.nodes[2] == fill32(0x03));
        CHECK(off == bytes.size());
    }

    TEST_CASE("Witness with one Signature roundtrip") {
        Signature sig;
        sig.rootPublicKey = fill32(0x11);
        sig.leafIndex     = MiniNumber(int64_t(7));
        sig.publicKey     = fill2880(0x22);
        sig.signature     = fill2880(0x33);

        Witness w;
        w.addSignature(sig);

        auto bytes = w.serialise();
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);

        REQUIRE(w2.signatures().size() == 1);
        CHECK(w2.signatures()[0].rootPublicKey == fill32(0x11));
        CHECK(w2.signatures()[0].leafIndex == MiniNumber(int64_t(7)));
        CHECK(w2.signatures()[0].publicKey.bytes().size() == 2880);
        CHECK(w2.scriptProofs().empty());
    }

    TEST_CASE("Witness with ScriptProof roundtrip") {
        ScriptProof sp;
        sp.script = MiniString("RETURN TRUE");

        Witness w;
        w.addScriptProof(sp);

        auto bytes = w.serialise();
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);

        REQUIRE(w2.scriptProofs().size() == 1);
        CHECK(w2.scriptProofs()[0].script.str() == "RETURN TRUE");
        CHECK(w2.signatures().empty());
    }

    TEST_CASE("ScriptProof::address() computes SHA3 of script") {
        ScriptProof sp;
        sp.script = MiniString("RETURN TRUE");

        MiniData addr = sp.address();
        CHECK(addr.bytes().size() == 32);

        const std::string s = "RETURN TRUE";
        MiniData expected = crypto::Hash::sha3_256(
            reinterpret_cast<const uint8_t*>(s.data()), s.size());
        CHECK(addr == expected);
    }

    TEST_CASE("scriptForAddress lookup") {
        const std::string script1 = "RETURN TRUE";
        const std::string script2 = "LET x = 5 RETURN x GT 3";

        ScriptProof sp1; sp1.script = MiniString(script1);
        ScriptProof sp2; sp2.script = MiniString(script2);

        Witness w;
        w.addScriptProof(sp1);
        w.addScriptProof(sp2);

        MiniData addr2 = sp2.address();
        auto found = w.scriptForAddress(addr2);
        REQUIRE(found.has_value());
        CHECK(found->str() == script2);

        MiniData unknown = fill32(0xFF);
        CHECK(!w.scriptForAddress(unknown).has_value());
    }

    TEST_CASE("Witness num_sigs byte is correct") {
        Witness w;
        for (int i = 0; i < 3; ++i) {
            Signature sig;
            sig.rootPublicKey = fill32(static_cast<uint8_t>(i));
            sig.leafIndex     = MiniNumber(int64_t(i));
            sig.publicKey     = fill2880(static_cast<uint8_t>(i + 10));
            sig.signature     = fill2880(static_cast<uint8_t>(i + 20));
            w.addSignature(sig);
        }
        ScriptProof sp; sp.script = MiniString("RETURN TRUE");
        w.addScriptProof(sp);

        auto bytes = w.serialise();
        CHECK(bytes[0] == 0x03);

        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);
        CHECK(w2.signatures().size() == 3);
        CHECK(w2.scriptProofs().size() == 1);
    }

    TEST_CASE("MerklePath roundtrip") {
        MerklePath mp;
        mp.nodes = { fill32(0xDE), fill32(0xAD), fill32(0xBE), fill32(0xEF) };

        auto bytes = mp.serialise();
        CHECK(bytes[0] == 4);

        size_t off = 0;
        MerklePath mp2 = MerklePath::deserialise(bytes.data(), off);
        REQUIRE(mp2.nodes.size() == 4);
        CHECK(mp2.nodes[0] == fill32(0xDE));
        CHECK(mp2.nodes[3] == fill32(0xEF));
        CHECK(off == bytes.size());
    }

    TEST_CASE("Signature::isValid()") {
        Signature sig;
        CHECK(!sig.isValid());
        sig.rootPublicKey = fill32(0x01);
        sig.publicKey     = fill2880(0x02);
        sig.signature     = fill2880(0x03);
        CHECK(sig.isValid());
    }

    TEST_CASE("Full Witness roundtrip: 2 sigs + 2 scripts") {
        Witness w;
        for (int i = 0; i < 2; ++i) {
            Signature sig;
            sig.rootPublicKey = fill32(static_cast<uint8_t>(0x10 + i));
            sig.leafIndex     = MiniNumber(int64_t(i * 100));
            sig.publicKey     = fill2880(static_cast<uint8_t>(0x20 + i));
            sig.signature     = fill2880(static_cast<uint8_t>(0x30 + i));
            sig.proof.nodes   = { fill32(static_cast<uint8_t>(0x40 + i)) };
            w.addSignature(sig);
        }
        ScriptProof sp1; sp1.script = MiniString("RETURN TRUE");
        ScriptProof sp2; sp2.script = MiniString("LET x = 1 RETURN x EQ 1");
        w.addScriptProof(sp1);
        w.addScriptProof(sp2);

        auto bytes = w.serialise();
        size_t off = 0;
        Witness w2 = Witness::deserialise(bytes.data(), off);

        CHECK(w2.signatures().size() == 2);
        CHECK(w2.scriptProofs().size() == 2);
        CHECK(w2.signatures()[0].rootPublicKey == fill32(0x10));
        CHECK(w2.signatures()[1].leafIndex == MiniNumber(int64_t(100)));
        CHECK(w2.scriptProofs()[0].script.str() == "RETURN TRUE");
        CHECK(w2.scriptProofs()[1].script.str() == "LET x = 1 RETURN x EQ 1");
        CHECK(off == bytes.size());
    }
}
