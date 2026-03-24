#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include <memory>
#include "../../src/crypto/RSA.hpp"
#include "../../src/crypto/Hash.hpp"
#include "../../src/kissvm/Contract.hpp"
#include "../../src/objects/Transaction.hpp"
#include "../../src/objects/Witness.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"

using namespace minima;
using namespace minima::crypto;
using namespace minima::kissvm;

// ── Helpers ───────────────────────────────────────────────────────────────────

static MiniData trueAddr() {
    static const std::string s = "RETURN TRUE";
    return Hash::sha3_256(std::vector<uint8_t>(s.begin(), s.end()));
}

// Fixture: keeps txn and witness alive so Contract refs remain valid.
struct MsFixture {
    Transaction              txn;
    Witness                  w;
    std::unique_ptr<Contract> c;

    MsFixture(const std::string&                                script,
              const std::vector<std::pair<MiniData, MiniData>>& sigs,
              const MiniData&                                   txpowId)
    {
        static const std::string rt = "RETURN TRUE";
        Coin coin;
        coin.setAddress(Address(trueAddr()));
        coin.setTokenID(MiniData(std::vector<uint8_t>{0x00}));
        coin.setAmount(MiniNumber(100.0));
        coin.setCoinID(MiniData(std::vector<uint8_t>(32, 0x01)));
        txn.addInput(coin);

        ScriptProof sp;
        sp.script  = MiniString(rt);
        sp.address = trueAddr();
        w.addScript(sp);

        for (auto& [pub, sig] : sigs) {
            SignatureProof sigp;
            sigp.pubKey    = pub;
            sigp.signature = sig;
            w.addSignature(sigp);
        }

        // txn and w fully built — now construct Contract (holds refs)
        c = std::make_unique<Contract>(script, txn, w, 0);
        c->setBlockNumber(MiniNumber(int64_t(1)));
        c->setCoinAge(MiniNumber::ZERO);
        c->setTxPoWID(txpowId);
    }
    Contract& contract() { return *c; }
};

// Sign txpowId with a fresh keypair, return {kp, sig}
static std::pair<RSAKeyPair, MiniData> gen(const MiniData& txid) {
    auto kp  = RSA::generateKeyPair();
    auto sig = RSA::sign(kp.privateKey, txid);
    return {kp, sig};
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_SUITE("MULTISIG — real RSA-1024") {

TEST_CASE("1-of-1: valid sig → TRUE") {
    MiniData txid(std::vector<uint8_t>(32, 0xAA));
    auto [kp, sig] = gen(txid);
    std::string script = "RETURN MULTISIG(1," + kp.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp.publicKey, sig}}, txid);
    fx.contract().execute();
    CHECK(fx.contract().isTrue());
}

TEST_CASE("1-of-1: sig over wrong data → FALSE") {
    MiniData txid(std::vector<uint8_t>(32, 0xAA));
    MiniData wrong(std::vector<uint8_t>(32, 0xBB));
    auto [kp, sig] = gen(wrong);  // signed wrong txid
    std::string script = "RETURN MULTISIG(1," + kp.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp.publicKey, sig}}, txid);  // contract has txid
    fx.contract().execute();
    CHECK_FALSE(fx.contract().isTrue());
}

TEST_CASE("2-of-2: both sign → TRUE") {
    MiniData txid(std::vector<uint8_t>(32, 0x11));
    auto [kp1, sig1] = gen(txid);
    auto [kp2, sig2] = gen(txid);
    std::string script = "RETURN MULTISIG(2,"
        + kp1.publicKey.toHexString() + ","
        + kp2.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp1.publicKey, sig1}, {kp2.publicKey, sig2}}, txid);
    fx.contract().execute();
    CHECK(fx.contract().isTrue());
}

TEST_CASE("2-of-2: only one signs → FALSE") {
    MiniData txid(std::vector<uint8_t>(32, 0x22));
    auto [kp1, sig1] = gen(txid);
    auto [kp2, sig2] = gen(txid);
    std::string script = "RETURN MULTISIG(2,"
        + kp1.publicKey.toHexString() + ","
        + kp2.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp1.publicKey, sig1}}, txid);  // kp2 absent
    fx.contract().execute();
    CHECK_FALSE(fx.contract().isTrue());
}

TEST_CASE("2-of-3: any two of three sign → TRUE") {
    MiniData txid(std::vector<uint8_t>(32, 0x33));
    auto [kp1, sig1] = gen(txid);
    auto [kp2, sig2] = gen(txid);
    auto [kp3, sig3] = gen(txid);
    std::string script = "RETURN MULTISIG(2,"
        + kp1.publicKey.toHexString() + ","
        + kp2.publicKey.toHexString() + ","
        + kp3.publicKey.toHexString() + ")";

    SUBCASE("keys 1+2") {
        MsFixture fx(script, {{kp1.publicKey,sig1},{kp2.publicKey,sig2}}, txid);
        fx.contract().execute(); CHECK(fx.contract().isTrue());
    }
    SUBCASE("keys 1+3") {
        MsFixture fx(script, {{kp1.publicKey,sig1},{kp3.publicKey,sig3}}, txid);
        fx.contract().execute(); CHECK(fx.contract().isTrue());
    }
    SUBCASE("keys 2+3") {
        MsFixture fx(script, {{kp2.publicKey,sig2},{kp3.publicKey,sig3}}, txid);
        fx.contract().execute(); CHECK(fx.contract().isTrue());
    }
    SUBCASE("only key 1 → FALSE") {
        MsFixture fx(script, {{kp1.publicKey,sig1}}, txid);
        fx.contract().execute(); CHECK_FALSE(fx.contract().isTrue());
    }
}

TEST_CASE("3-of-3: all sign → TRUE") {
    MiniData txid(std::vector<uint8_t>(32, 0x44));
    auto [kp1, sig1] = gen(txid);
    auto [kp2, sig2] = gen(txid);
    auto [kp3, sig3] = gen(txid);
    std::string script = "RETURN MULTISIG(3,"
        + kp1.publicKey.toHexString() + ","
        + kp2.publicKey.toHexString() + ","
        + kp3.publicKey.toHexString() + ")";
    MsFixture fx(script,
        {{kp1.publicKey,sig1},{kp2.publicKey,sig2},{kp3.publicKey,sig3}}, txid);
    fx.contract().execute();
    CHECK(fx.contract().isTrue());
}

TEST_CASE("3-of-3: two sign → FALSE") {
    MiniData txid(std::vector<uint8_t>(32, 0x55));
    auto [kp1, sig1] = gen(txid);
    auto [kp2, sig2] = gen(txid);
    auto [kp3, sig3] = gen(txid);
    std::string script = "RETURN MULTISIG(3,"
        + kp1.publicKey.toHexString() + ","
        + kp2.publicKey.toHexString() + ","
        + kp3.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp1.publicKey,sig1},{kp2.publicKey,sig2}}, txid);
    fx.contract().execute();
    CHECK_FALSE(fx.contract().isTrue());
}

TEST_CASE("tampered signature → FALSE") {
    MiniData txid(std::vector<uint8_t>(32, 0x66));
    auto [kp, sig] = gen(txid);
    auto bad = sig.bytes(); bad[0] ^= 0xFF; bad[10] ^= 0xAB;
    std::string script = "RETURN MULTISIG(1," + kp.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp.publicKey, MiniData(bad)}}, txid);
    fx.contract().execute();
    CHECK_FALSE(fx.contract().isTrue());
}

TEST_CASE("cross-key attack: sig from key1 presented as key2 → FALSE") {
    MiniData txid(std::vector<uint8_t>(32, 0x77));
    auto [kp1, sig1] = gen(txid);
    auto  kp2        = RSA::generateKeyPair();
    // present sig1 (made with kp1) but labelled with kp2.pubkey
    std::string script = "RETURN MULTISIG(1," + kp2.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp2.publicKey, sig1}}, txid);
    fx.contract().execute();
    CHECK_FALSE(fx.contract().isTrue());
}

TEST_CASE("empty witness → always FALSE") {
    MiniData txid(std::vector<uint8_t>(32, 0x88));
    auto kp = RSA::generateKeyPair();
    std::string script = "RETURN MULTISIG(1," + kp.publicKey.toHexString() + ")";
    MsFixture fx(script, {}, txid);
    fx.contract().execute();
    CHECK_FALSE(fx.contract().isTrue());
}

TEST_CASE("n=0 (degenerate): always TRUE") {
    MiniData txid(std::vector<uint8_t>(32, 0xCC));
    auto kp = RSA::generateKeyPair();
    std::string script = "RETURN MULTISIG(0," + kp.publicKey.toHexString() + ")";
    MsFixture fx(script, {}, txid);
    fx.contract().execute();
    CHECK(fx.contract().isTrue());
}

TEST_CASE("1-of-2: first key signs → TRUE") {
    MiniData txid(std::vector<uint8_t>(32, 0xA0));
    auto [kp1, sig1] = gen(txid);
    auto  kp2        = RSA::generateKeyPair();
    std::string script = "RETURN MULTISIG(1,"
        + kp1.publicKey.toHexString() + ","
        + kp2.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp1.publicKey, sig1}}, txid);
    fx.contract().execute();
    CHECK(fx.contract().isTrue());
}

TEST_CASE("1-of-2: second key signs → TRUE") {
    MiniData txid(std::vector<uint8_t>(32, 0xB0));
    auto  kp1        = RSA::generateKeyPair();
    auto [kp2, sig2] = gen(txid);
    std::string script = "RETURN MULTISIG(1,"
        + kp1.publicKey.toHexString() + ","
        + kp2.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp2.publicKey, sig2}}, txid);
    fx.contract().execute();
    CHECK(fx.contract().isTrue());
}

TEST_CASE("SHA3-hashed txid: real Minima signing pattern") {
    std::string raw = "minima:txpow:cafebabe:block100";
    MiniData txid = Hash::sha3_256(
        std::vector<uint8_t>(raw.begin(), raw.end()));
    auto [kp1, sig1] = gen(txid);
    auto [kp2, sig2] = gen(txid);
    std::string script = "RETURN MULTISIG(2,"
        + kp1.publicKey.toHexString() + ","
        + kp2.publicKey.toHexString() + ")";
    MsFixture fx(script, {{kp1.publicKey,sig1},{kp2.publicKey,sig2}}, txid);
    fx.contract().execute();
    CHECK(fx.contract().isTrue());
}

TEST_CASE("MULTISIG combined with CHECKSIG in same script") {
    // Pattern: 1-of-2 multisig AND explicit CHECKSIG on payload
    // KISS VM: AND is infix operator, not a function — use `IF ms THEN ... RETURN cs`
    MiniData txid(std::vector<uint8_t>(32, 0x99));
    MiniData payload(std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF});
    auto [kp1, sig1]  = gen(txid);
    auto [kp2, sig2]  = gen(txid);
    MiniData sigExtra = RSA::sign(kp1.privateKey, payload);
    // Use IF/THEN/ELSE to combine: if multisig passes, check explicit sig too
    std::string script =
        "LET ms = MULTISIG(1,"
            + kp1.publicKey.toHexString() + ","
            + kp2.publicKey.toHexString() + ")"
        " LET cs = CHECKSIG("
            + kp1.publicKey.toHexString() + ","
            + payload.toHexString() + ","
            + sigExtra.toHexString() + ")"
        " IF ms EQ TRUE THEN RETURN cs ENDIF"
        " RETURN FALSE";
    MsFixture fx(script, {{kp1.publicKey, sig1}}, txid);
    fx.contract().execute();
    CHECK(fx.contract().isTrue());
}

} // TEST_SUITE
