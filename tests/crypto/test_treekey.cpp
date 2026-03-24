#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/crypto/TreeKey.hpp"
#include "../../src/crypto/Hash.hpp"
#include "../../src/types/MiniData.hpp"
#include <vector>

using namespace minima::crypto;
using minima::MiniData;

static MiniData makeSeed(uint8_t fill) {
    return MiniData(std::vector<uint8_t>(32, fill));
}

static MiniData makeMsg(uint8_t fill) {
    std::vector<uint8_t> data(32, fill);
    return MiniData(Hash::sha3_256(data.data(), data.size()).bytes());
}

// Use depth=4 (16 leaves) for speed
static constexpr int D = 4;

TEST_CASE("TreeKey: construction produces non-empty 32-byte root") {
    TreeKey tk(makeSeed(0x01), D);
    auto root = tk.getPublicKey();
    CHECK(root.bytes().size() == 32u);
    bool allZero = true;
    for (auto b : root.bytes()) if (b) { allZero = false; break; }
    CHECK_FALSE(allZero);
}

TEST_CASE("TreeKey: same seed produces same root") {
    TreeKey tk1(makeSeed(0x42), D);
    TreeKey tk2(makeSeed(0x42), D);
    CHECK(tk1.getPublicKey().bytes() == tk2.getPublicKey().bytes());
}

TEST_CASE("TreeKey: different seeds produce different roots") {
    TreeKey tk1(makeSeed(0x01), D);
    TreeKey tk2(makeSeed(0x02), D);
    CHECK(tk1.getPublicKey().bytes() != tk2.getPublicKey().bytes());
}

TEST_CASE("TreeKey: depth and key count") {
    TreeKey tk(makeSeed(0x55), D);
    CHECK(tk.depth()    == D);
    CHECK(tk.keyUsed()  == 0);
    CHECK(tk.keysLeft() == (1 << D));
    CHECK(tk.canSign());
}

TEST_CASE("TreeKey: sign produces valid SignatureProof") {
    TreeKey tk(makeSeed(0xAB), D);
    auto msg   = makeMsg(0xCD);
    auto proof = tk.sign(msg);

    CHECK(proof.isValid());
    CHECK(proof.leafIndex == 0);
    CHECK(proof.signature.bytes().size() ==
          static_cast<size_t>(Winternitz::SIG_SIZE));
    CHECK(proof.publicKey.bytes().size() ==
          static_cast<size_t>(Winternitz::PUBKEY_SIZE));
    CHECK_FALSE(proof.proof.empty());
    CHECK(proof.proof.size() == static_cast<size_t>(D));
}

TEST_CASE("TreeKey: sign advances key counter") {
    TreeKey tk(makeSeed(0x11), D);
    CHECK(tk.keyUsed() == 0);
    tk.sign(makeMsg(0x22));
    CHECK(tk.keyUsed() == 1);
    tk.sign(makeMsg(0x33));
    CHECK(tk.keyUsed() == 2);
}

TEST_CASE("TreeKey: verify succeeds for valid proof") {
    TreeKey tk(makeSeed(0xDE), D);
    auto root  = tk.getPublicKey();
    auto msg   = makeMsg(0xAD);
    auto proof = tk.sign(msg);

    CHECK(TreeKey::verify(root, msg, proof));
}

TEST_CASE("TreeKey: verify fails for wrong message") {
    TreeKey tk(makeSeed(0xBE), D);
    auto root  = tk.getPublicKey();
    auto msg1  = makeMsg(0x10);
    auto msg2  = makeMsg(0x20);
    auto proof = tk.sign(msg1);

    CHECK(      TreeKey::verify(root, msg1, proof));
    CHECK_FALSE(TreeKey::verify(root, msg2, proof));
}

TEST_CASE("TreeKey: verify fails for tampered WOTS signature") {
    TreeKey tk(makeSeed(0xCA), D);
    auto root  = tk.getPublicKey();
    auto msg   = makeMsg(0xFE);
    auto proof = tk.sign(msg);

    auto sigBytes = proof.signature.bytes();
    sigBytes[500] ^= 0xFF;
    proof.signature = MiniData(sigBytes);

    CHECK_FALSE(TreeKey::verify(root, msg, proof));
}

TEST_CASE("TreeKey: verify fails for wrong root") {
    TreeKey tk1(makeSeed(0x01), D);
    TreeKey tk2(makeSeed(0x02), D);
    auto msg   = makeMsg(0x77);
    auto proof = tk1.sign(msg);

    CHECK_FALSE(TreeKey::verify(tk2.getPublicKey(), msg, proof));
}

TEST_CASE("TreeKey: sequential signs use different leaves") {
    TreeKey tk(makeSeed(0x88), D);
    auto p1 = tk.sign(makeMsg(0x01));
    auto p2 = tk.sign(makeMsg(0x02));

    CHECK(p1.leafIndex == 0);
    CHECK(p2.leafIndex == 1);
    CHECK(p1.publicKey.bytes() != p2.publicKey.bytes());
}

TEST_CASE("TreeKey: each proof verifies independently") {
    TreeKey tk(makeSeed(0x99), D);
    auto root = tk.getPublicKey();

    for (int i = 0; i < 4; ++i) {
        auto msg   = makeMsg(static_cast<uint8_t>(i + 1));
        auto proof = tk.sign(msg);
        CHECK(TreeKey::verify(root, msg, proof));
    }
}

TEST_CASE("TreeKey: exhaustion throws") {
    TreeKey tk(makeSeed(0xFF), 1);  // depth=1 → only 2 keys
    tk.sign(makeMsg(0x01));
    tk.sign(makeMsg(0x02));
    CHECK_THROWS(tk.sign(makeMsg(0x03)));
}

TEST_CASE("TreeKey: Merkle auth path length equals depth") {
    TreeKey tk(makeSeed(0x5A), D);
    for (int i = 0; i < 4; ++i) {
        auto proof = tk.sign(makeMsg(static_cast<uint8_t>(i)));
        CHECK(proof.proof.size() == static_cast<size_t>(D));
    }
}

TEST_CASE("TreeKey: root public key matches SignatureProof.rootPublicKey") {
    TreeKey tk(makeSeed(0x3C), D);
    auto root  = tk.getPublicKey();
    auto proof = tk.sign(makeMsg(0x7B));
    CHECK(proof.rootPublicKey.bytes() == root.bytes());
}
