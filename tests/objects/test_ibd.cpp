#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/objects/IBD.hpp"
#include "../../src/chain/cascade/Cascade.hpp"
#include "../../src/objects/TxHeader.hpp"  // for CASCADE_LEVELS

using namespace minima;
using namespace minima::cascade;

// ── Helpers ──────────────────────────────────────────────────────────────────

static TxBlock makeBlock(int64_t blockNum) {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(blockNum);
    return TxBlock(txp);
}

static TxPoW makeTxPoW(int64_t blockNum, int superLevel = 0) {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(blockNum);
    // superParents[0..superLevel] share the same non-zero hash → deriveSuperLevel returns superLevel
    std::vector<uint8_t> hashBytes(32, static_cast<uint8_t>(blockNum & 0xFF));
    std::vector<uint8_t> zeroBytes(32, 0x00);
    MiniData parentHash(hashBytes);
    MiniData zeroHash(zeroBytes);
    for (int i = 0; i < CASCADE_LEVELS; ++i) {
        txp.header().superParents[i] = (i <= superLevel) ? parentHash : zeroHash;
    }
    return txp;
}

// ── Basic IBD tests ───────────────────────────────────────────────────────────

TEST_CASE("IBD: default is empty") {
    IBD ibd;
    CHECK(ibd.txBlocks().empty());
    CHECK_FALSE(ibd.hasCascade());
    CHECK(ibd.blockCount() == 0);
    CHECK(ibd.cascade() == nullptr);
}

TEST_CASE("IBD: treeRoot / treeTip on empty returns -1") {
    IBD ibd;
    CHECK(ibd.treeRoot() == MiniNumber(int64_t(-1)));
    CHECK(ibd.treeTip()  == MiniNumber(int64_t(-1)));
}

TEST_CASE("IBD: addBlock and accessor") {
    IBD ibd;
    ibd.addBlock(makeBlock(10));
    ibd.addBlock(makeBlock(11));
    ibd.addBlock(makeBlock(12));
    CHECK(ibd.blockCount() == 3);
    CHECK(ibd.treeRoot() == MiniNumber(int64_t(10)));
    CHECK(ibd.treeTip()  == MiniNumber(int64_t(12)));
}

TEST_CASE("IBD: serialise / deserialise roundtrip (no cascade)") {
    IBD orig;
    orig.addBlock(makeBlock(5));
    orig.addBlock(makeBlock(6));
    orig.addBlock(makeBlock(7));

    auto bytes = orig.serialise();
    CHECK_FALSE(bytes.empty());
    // First byte must be 0x00 (no cascade)
    CHECK(bytes[0] == 0x00);

    IBD restored = IBD::deserialise(bytes);
    CHECK(restored.hasCascade() == false);
    CHECK(restored.cascade() == nullptr);
    CHECK(restored.blockCount() == 3);
    CHECK(restored.treeRoot() == MiniNumber(int64_t(5)));
    CHECK(restored.treeTip()  == MiniNumber(int64_t(7)));
}

TEST_CASE("IBD: empty serialise / deserialise") {
    IBD orig;
    auto bytes = orig.serialise();
    IBD restored = IBD::deserialise(bytes);
    CHECK(restored.blockCount() == 0);
    CHECK_FALSE(restored.hasCascade());
}

TEST_CASE("IBD: larger IBD preserves block order") {
    IBD orig;
    for (int64_t i = 100; i < 150; ++i)
        orig.addBlock(makeBlock(i));

    auto bytes = orig.serialise();
    IBD restored = IBD::deserialise(bytes);

    CHECK(restored.blockCount() == 50);
    CHECK(restored.treeRoot() == MiniNumber(int64_t(100)));
    CHECK(restored.treeTip()  == MiniNumber(int64_t(149)));
    CHECK(restored.txBlocks()[25].txpow().header().blockNumber == MiniNumber(int64_t(125)));
}

TEST_CASE("IBD: IBD_MAX_BLOCKS constant") {
    CHECK(IBD_MAX_BLOCKS == 34000);
}

// ── Cascade integration tests ─────────────────────────────────────────────────

TEST_CASE("IBD: setCascade sets hasCascade flag") {
    IBD ibd;
    CHECK_FALSE(ibd.hasCascade());
    CHECK(ibd.cascade() == nullptr);

    Cascade c;
    c.addToTip(makeTxPoW(1));
    ibd.setCascade(std::move(c));

    CHECK(ibd.hasCascade());
    CHECK(ibd.cascade() != nullptr);
    CHECK(ibd.cascade()->length() == 1);
}

TEST_CASE("IBD: clearCascade removes cascade") {
    IBD ibd;
    Cascade c;
    c.addToTip(makeTxPoW(1));
    ibd.setCascade(std::move(c));
    CHECK(ibd.hasCascade());

    ibd.clearCascade();
    CHECK_FALSE(ibd.hasCascade());
    CHECK(ibd.cascade() == nullptr);
}

TEST_CASE("IBD: serialise with cascade — first byte is 0x01") {
    IBD ibd;
    Cascade c;
    c.addToTip(makeTxPoW(42));
    ibd.setCascade(std::move(c));
    ibd.addBlock(makeBlock(100));

    auto bytes = ibd.serialise();
    REQUIRE_FALSE(bytes.empty());
    CHECK(bytes[0] == 0x01);
}

TEST_CASE("IBD: serialise/deserialise roundtrip WITH cascade (no blocks)") {
    IBD orig;

    Cascade c;
    c.addToTip(makeTxPoW(10));
    c.addToTip(makeTxPoW(11));
    c.addToTip(makeTxPoW(12));
    orig.setCascade(std::move(c));

    auto bytes = orig.serialise();
    CHECK(bytes[0] == 0x01);

    IBD restored = IBD::deserialise(bytes);
    CHECK(restored.hasCascade());
    CHECK(restored.cascade() != nullptr);
    CHECK(restored.cascade()->length() == 3);
    CHECK(restored.blockCount() == 0);
}

TEST_CASE("IBD: serialise/deserialise roundtrip WITH cascade AND blocks") {
    IBD orig;

    // Build cascade with 5 nodes
    Cascade c;
    for (int64_t i = 1; i <= 5; ++i)
        c.addToTip(makeTxPoW(i));
    orig.setCascade(std::move(c));

    // Add 3 blocks (newer than cascade tip)
    for (int64_t i = 6; i <= 8; ++i)
        orig.addBlock(makeBlock(i));

    auto bytes = orig.serialise();
    IBD restored = IBD::deserialise(bytes);

    CHECK(restored.hasCascade());
    REQUIRE(restored.cascade() != nullptr);
    CHECK(restored.cascade()->length() == 5);
    CHECK(restored.blockCount() == 3);
    CHECK(restored.treeRoot() == MiniNumber(int64_t(6)));
    CHECK(restored.treeTip()  == MiniNumber(int64_t(8)));
}

TEST_CASE("IBD: totalWeight without cascade equals blockCount") {
    IBD ibd;
    ibd.addBlock(makeBlock(1));
    ibd.addBlock(makeBlock(2));
    ibd.addBlock(makeBlock(3));
    CHECK(ibd.totalWeight() == doctest::Approx(3.0));
}

TEST_CASE("IBD: totalWeight with cascade > blockCount") {
    IBD ibd;

    Cascade c;
    c.addToTip(makeTxPoW(1));
    c.addToTip(makeTxPoW(2));
    ibd.setCascade(std::move(c));

    ibd.addBlock(makeBlock(3));

    double w = ibd.totalWeight();
    CHECK(w > 1.0);
}

TEST_CASE("IBD: cascade cascadeChain then serialise roundtrip") {
    IBD orig;

    Cascade c;
    for (int64_t i = 1; i <= 10; ++i)
        c.addToTip(makeTxPoW(i, static_cast<int>(i % 4)));
    c.cascadeChain();

    int lenAfterCascade = c.length();
    CHECK(lenAfterCascade > 0);
    CHECK(lenAfterCascade <= 10);

    orig.setCascade(std::move(c));
    orig.addBlock(makeBlock(11));
    orig.addBlock(makeBlock(12));

    auto bytes = orig.serialise();
    IBD restored = IBD::deserialise(bytes);

    CHECK(restored.hasCascade());
    REQUIRE(restored.cascade() != nullptr);
    CHECK(restored.cascade()->length() == lenAfterCascade);
    CHECK(restored.blockCount() == 2);
}

TEST_CASE("IBD: double serialise produces identical bytes") {
    IBD ibd;
    Cascade c;
    c.addToTip(makeTxPoW(7));
    c.addToTip(makeTxPoW(8));
    ibd.setCascade(std::move(c));
    ibd.addBlock(makeBlock(9));

    auto b1 = ibd.serialise();
    auto b2 = ibd.serialise();
    CHECK(b1 == b2);
}

TEST_CASE("IBD: cascade-only roundtrip — re-serialise gives identical bytes") {
    IBD orig;
    Cascade c;
    c.addToTip(makeTxPoW(1));
    c.addToTip(makeTxPoW(2));
    c.addToTip(makeTxPoW(3));
    orig.setCascade(std::move(c));

    auto b1 = orig.serialise();
    IBD restored = IBD::deserialise(b1);
    auto b2 = restored.serialise();

    CHECK(b1 == b2);
}

TEST_CASE("IBD: hasCascade=false deserialisable even with 0 blocks") {
    // Wire: [0x00][MiniNumber(0)]
    std::vector<uint8_t> wire;
    wire.push_back(0x00);
    auto numBlocks = MiniNumber(int64_t(0)).serialise();
    wire.insert(wire.end(), numBlocks.begin(), numBlocks.end());

    IBD ibd = IBD::deserialise(wire);
    CHECK_FALSE(ibd.hasCascade());
    CHECK(ibd.blockCount() == 0);
}
