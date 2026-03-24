#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/crypto/Winternitz.hpp"
#include "../../src/crypto/Schnorr.hpp"
#include "../../src/crypto/Hash.hpp"
#include "../../src/types/MiniData.hpp"
#include <vector>
#include <array>
#include <cstring>

using namespace minima::crypto;
using minima::MiniData;

// ── Helpers ──────────────────────────────────────────────────────────────────
static std::vector<uint8_t> makeBytes(uint8_t fill, size_t len = 32) {
    return std::vector<uint8_t>(len, fill);
}

// Returns 32-byte SHA3-256 hash of the input
static std::vector<uint8_t> hashOf(const std::vector<uint8_t>& data) {
    return Hash::sha3_256(data.data(), data.size()).bytes();
}

// ── Winternitz constants ─────────────────────────────────────────────────────
TEST_CASE("WOTS: constants match Java BouncyCastle") {
    CHECK(Winternitz::W           == 8);
    CHECK(Winternitz::HASH_SIZE   == 32);
    CHECK(Winternitz::LOG_W       == 3);
    CHECK(Winternitz::KEY_BLOCKS  == 86);   // ceil(256/3)
    CHECK(Winternitz::CHKSUM_BLOCKS == 4);
    CHECK(Winternitz::SIG_BLOCKS  == 90);   // 86+4
    CHECK(Winternitz::PUBKEY_SIZE == 2880); // 90*32
    CHECK(Winternitz::SIG_SIZE    == 2880);
}

// ── Key generation ────────────────────────────────────────────────────────────
TEST_CASE("WOTS: public key generation is deterministic") {
    auto seed = makeBytes(0xAB);
    auto pub1 = Winternitz::generatePublicKey(seed);
    auto pub2 = Winternitz::generatePublicKey(seed);
    CHECK(pub1 == pub2);
    CHECK(pub1.size() == static_cast<size_t>(Winternitz::PUBKEY_SIZE));
}

TEST_CASE("WOTS: different seeds produce different public keys") {
    auto pub1 = Winternitz::generatePublicKey(makeBytes(0x01));
    auto pub2 = Winternitz::generatePublicKey(makeBytes(0x02));
    CHECK(pub1 != pub2);
}

TEST_CASE("WOTS: public key is not all zeros") {
    auto pub = Winternitz::generatePublicKey(makeBytes(0x42));
    bool allSame = true;
    for (size_t i = 1; i < pub.size(); ++i)
        if (pub[i] != pub[0]) { allSame = false; break; }
    CHECK_FALSE(allSame);
}

// ── Sign / Verify ─────────────────────────────────────────────────────────────
TEST_CASE("WOTS: basic sign and verify") {
    auto seed = makeBytes(0x55);
    auto msg  = hashOf(makeBytes(0xDE));
    auto pub  = Winternitz::generatePublicKey(seed);
    auto sig  = Winternitz::sign(seed, msg);

    CHECK(sig.size() == static_cast<size_t>(Winternitz::SIG_SIZE));
    CHECK(Winternitz::verify(pub, msg, sig));
}

TEST_CASE("WOTS: verify fails with wrong message") {
    auto seed = makeBytes(0x11);
    auto msg1 = hashOf(makeBytes(0xAA));
    auto msg2 = hashOf(makeBytes(0xBB));
    auto pub  = Winternitz::generatePublicKey(seed);
    auto sig  = Winternitz::sign(seed, msg1);

    CHECK(      Winternitz::verify(pub, msg1, sig));
    CHECK_FALSE(Winternitz::verify(pub, msg2, sig));
}

TEST_CASE("WOTS: verify fails with wrong public key") {
    auto seed1 = makeBytes(0xCC);
    auto seed2 = makeBytes(0xDD);
    auto msg   = hashOf(makeBytes(0x77));
    auto pub1  = Winternitz::generatePublicKey(seed1);
    auto pub2  = Winternitz::generatePublicKey(seed2);
    auto sig   = Winternitz::sign(seed1, msg);

    CHECK(      Winternitz::verify(pub1, msg, sig));
    CHECK_FALSE(Winternitz::verify(pub2, msg, sig));
}

TEST_CASE("WOTS: verify fails with tampered signature") {
    auto seed = makeBytes(0xEE);
    auto msg  = hashOf(makeBytes(0x33));
    auto pub  = Winternitz::generatePublicKey(seed);
    auto sig  = Winternitz::sign(seed, msg);

    // Flip a byte in the middle
    sig[1440] ^= 0xFF;
    CHECK_FALSE(Winternitz::verify(pub, msg, sig));
}

TEST_CASE("WOTS: sign is deterministic") {
    auto seed = makeBytes(0x99);
    auto msg  = hashOf(makeBytes(0x66));
    auto sig1 = Winternitz::sign(seed, msg);
    auto sig2 = Winternitz::sign(seed, msg);
    CHECK(sig1 == sig2);
}

TEST_CASE("WOTS: different messages produce different signatures") {
    auto seed = makeBytes(0x12);
    auto msg1 = hashOf(makeBytes(0x01));
    auto msg2 = hashOf(makeBytes(0x02));
    auto sig1 = Winternitz::sign(seed, msg1);
    auto sig2 = Winternitz::sign(seed, msg2);
    CHECK(sig1 != sig2);
}

TEST_CASE("WOTS: recover() returns correct public key") {
    auto seed = makeBytes(0xAC);
    auto msg  = hashOf(makeBytes(0xBD));
    auto pub  = Winternitz::generatePublicKey(seed);
    auto sig  = Winternitz::sign(seed, msg);
    auto rec  = Winternitz::recover(msg, sig);
    CHECK(rec == pub);
}

// ── Schnorr interface (delegates to WOTS) ─────────────────────────────────────
TEST_CASE("Schnorr: fromSeed produces correct key pair") {
    MiniData seed(makeBytes(0xF0));
    auto kp = Schnorr::fromSeed(seed);

    CHECK(kp.privateKey.bytes() == makeBytes(0xF0));
    CHECK(kp.publicKey.bytes().size() ==
          static_cast<size_t>(Winternitz::PUBKEY_SIZE));
}

TEST_CASE("Schnorr: publicKeyFromPrivate matches fromSeed") {
    MiniData seed(makeBytes(0x3A));
    auto kp   = Schnorr::fromSeed(seed);
    auto pub2 = Schnorr::publicKeyFromPrivate(seed);
    CHECK(kp.publicKey.bytes() == pub2.bytes());
}

TEST_CASE("Schnorr: sign returns 2880-byte signature") {
    MiniData seed(makeBytes(0x5C));
    MiniData msg(hashOf(makeBytes(0x7D)));
    auto sig = Schnorr::sign(seed, msg);
    CHECK(sig.bytes().size() == static_cast<size_t>(Winternitz::SIG_SIZE));
}

TEST_CASE("Schnorr: verify succeeds for valid signature") {
    MiniData seed(makeBytes(0x2B));
    MiniData msg(hashOf(makeBytes(0x4E)));
    auto kp  = Schnorr::fromSeed(seed);
    auto sig = Schnorr::sign(seed, msg);
    CHECK(Schnorr::verify(kp.publicKey, msg, sig));
}

TEST_CASE("Schnorr: verify fails for wrong message") {
    MiniData seed(makeBytes(0x6A));
    MiniData msg1(hashOf(makeBytes(0x10)));
    MiniData msg2(hashOf(makeBytes(0x20)));
    auto kp  = Schnorr::fromSeed(seed);
    auto sig = Schnorr::sign(seed, msg1);
    CHECK(      Schnorr::verify(kp.publicKey, msg1, sig));
    CHECK_FALSE(Schnorr::verify(kp.publicKey, msg2, sig));
}

TEST_CASE("Schnorr: verify fails for wrong public key") {
    MiniData seed1(makeBytes(0x11));
    MiniData seed2(makeBytes(0x22));
    MiniData msg(hashOf(makeBytes(0x33)));
    auto kp1 = Schnorr::fromSeed(seed1);
    auto kp2 = Schnorr::fromSeed(seed2);
    auto sig = Schnorr::sign(seed1, msg);
    CHECK(      Schnorr::verify(kp1.publicKey, msg, sig));
    CHECK_FALSE(Schnorr::verify(kp2.publicKey, msg, sig));
}

TEST_CASE("Schnorr: non-32-byte message is hashed before verify") {
    MiniData seed(makeBytes(0x77));
    MiniData bigMsg(makeBytes(0x88, 64));
    auto kp  = Schnorr::fromSeed(seed);
    auto sig = Schnorr::sign(seed, bigMsg);
    CHECK(Schnorr::verify(kp.publicKey, bigMsg, sig));
}

TEST_CASE("Schnorr: aggregatePublicKeys produces 32-byte result") {
    std::vector<MiniData> keys = {
        MiniData(makeBytes(0xA1, 32)),
        MiniData(makeBytes(0xB2, 32)),
        MiniData(makeBytes(0xC3, 32))
    };
    auto agg = Schnorr::aggregatePublicKeys(keys);
    CHECK(agg.bytes().size() == 32u);
}

TEST_CASE("Schnorr: aggregate is deterministic") {
    std::vector<MiniData> keys = {
        MiniData(makeBytes(0xD4, 32)),
        MiniData(makeBytes(0xE5, 32))
    };
    auto a1 = Schnorr::aggregatePublicKeys(keys);
    auto a2 = Schnorr::aggregatePublicKeys(keys);
    CHECK(a1.bytes() == a2.bytes());
}
