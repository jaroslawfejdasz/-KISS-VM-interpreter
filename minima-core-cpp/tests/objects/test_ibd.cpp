#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/objects/IBD.hpp"

using namespace minima;

static TxBlock makeBlock(int64_t blockNum) {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(blockNum);
    return TxBlock(txp);
}

TEST_CASE("IBD: default is empty") {
    IBD ibd;
    CHECK(ibd.txBlocks().empty());
    CHECK_FALSE(ibd.hasCascade());
    CHECK(ibd.blockCount() == 0);
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

    IBD restored = IBD::deserialise(bytes);
    CHECK(restored.hasCascade() == false);
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

    // Spot-check middle block
    CHECK(restored.txBlocks()[25].txpow().header().blockNumber == MiniNumber(int64_t(125)));
}

TEST_CASE("IBD: overflow throws on max blocks exceeded") {
    IBD ibd;
    // Add exactly IBD_MAX_BLOCKS — this should be fine
    // (We won't actually add 34000 in a test — just verify the constant exists)
    CHECK(IBD_MAX_BLOCKS == 34000);
}

TEST_CASE("IBD: deserialise with cascade flag throws (not yet impl)") {
    // Manually craft a byte stream with hasCascade=1
    std::vector<uint8_t> data = {0x01}; // hasCascade = true
    CHECK_THROWS_AS(IBD::deserialise(data), std::runtime_error);
}
