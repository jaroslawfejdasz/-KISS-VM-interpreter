#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/mining/TxPoWMiner.hpp"

using namespace minima;
using namespace minima::mining;

TEST_CASE("Mining: lessThan works correctly") {
    std::vector<uint8_t> a(32, 0x00);
    std::vector<uint8_t> b(32, 0xFF);
    CHECK(lessThan(a, b));
    CHECK_FALSE(lessThan(b, a));
    CHECK_FALSE(lessThan(a, a));
}

TEST_CASE("Mining: makeDifficulty(0) = all 0xFF = trivial") {
    auto d = makeDifficulty(0);
    CHECK(d.bytes() == std::vector<uint8_t>(32, 0xFF));
}

TEST_CASE("Mining: makeDifficulty(1) has leading 0x00") {
    auto d = makeDifficulty(1);
    CHECK(d.bytes()[0] == 0x00);
}

TEST_CASE("Mining: trivial difficulty always mine in 1 iter") {
    TxPoW txp;
    txp.header().blockNumber   = MiniNumber(int64_t(0));
    txp.header().blockDifficulty = makeDifficulty(0); // all 0xFF

    bool found = mineBlock(txp, 1);
    CHECK(found);
    // nonce = 0 should immediately satisfy 0xFF difficulty
}

TEST_CASE("Mining: easy difficulty (first byte 0x80) finds nonce fast") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(1));
    std::vector<uint8_t> diffBytes(32, 0xFF);
    diffBytes[0] = 0x80; // ~50% hit rate per hash
    txp.header().blockDifficulty = MiniData(diffBytes);

    bool found = mineBlock(txp, 100000);
    CHECK(found);
}

TEST_CASE("Mining: impossible difficulty returns false after maxIter") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(0));
    // Impossible: all 0x00 = hash must be less than 0, never true
    txp.header().blockDifficulty = MiniData(std::vector<uint8_t>(32, 0x00));

    bool found = mineBlock(txp, 10);
    CHECK_FALSE(found);
}

TEST_CASE("Mining: mined nonce satisfies isBlock()") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(42));
    std::vector<uint8_t> diffBytes(32, 0xFF);
    diffBytes[0] = 0x70; // moderately easy
    txp.header().blockDifficulty = MiniData(diffBytes);
    txp.body().txnDifficulty     = MiniData(diffBytes);

    bool found = mineBlock(txp, 500000);
    if (found) {
        CHECK(txp.isBlock());
    }
}

TEST_CASE("Mining: cancel via stop flag") {
    std::atomic<bool> stop{false};
    TxPoW txp;
    txp.header().blockNumber   = MiniNumber(int64_t(0));
    // Impossible difficulty
    txp.header().blockDifficulty = MiniData(std::vector<uint8_t>(32, 0x00));

    // Launch mining, cancel after short time
    stop.store(true);
    bool found = mineBlock(txp, 0, &stop);
    CHECK_FALSE(found);
}
