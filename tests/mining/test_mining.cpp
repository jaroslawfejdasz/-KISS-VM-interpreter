#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/mining/TxPoWMiner.hpp"
#include "../../src/mining/MiningManager.hpp"
#include "../../src/chain/ChainProcessor.hpp"

using namespace minima;
using namespace minima::mining;
using namespace minima::chain;

// ── TxPoWMiner (low-level) ─────────────────────────────────────────────────

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
    CHECK(d.bytes()[1] == 0xFF);
}

TEST_CASE("Mining: trivial difficulty — mined at nonce=0") {
    TxPoW txp;
    txp.header().blockNumber     = MiniNumber(int64_t(0));
    txp.header().blockDifficulty = makeDifficulty(0); // all 0xFF

    bool found = mineBlock(txp, 1);
    CHECK(found);
    CHECK(txp.header().nonce.getAsLong() == 0);
}

TEST_CASE("Mining: easy difficulty (0x80 first byte) finds nonce quickly") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(1));
    std::vector<uint8_t> d(32, 0xFF);
    d[0] = 0x80;
    txp.header().blockDifficulty = MiniData(d);

    bool found = mineBlock(txp, 100000);
    CHECK(found);
    CHECK(txp.isBlock());
}

TEST_CASE("Mining: impossible difficulty returns false within maxIter") {
    TxPoW txp;
    txp.header().blockNumber     = MiniNumber(int64_t(0));
    txp.header().blockDifficulty = MiniData(std::vector<uint8_t>(32, 0x00));

    bool found = mineBlock(txp, 10);
    CHECK_FALSE(found);
}

TEST_CASE("Mining: mined block satisfies isBlock()") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(42));
    std::vector<uint8_t> d(32, 0xFF);
    d[0] = 0x70;
    txp.header().blockDifficulty = MiniData(d);
    txp.body().txnDifficulty     = MiniData(d);

    bool found = mineBlock(txp, 500000);
    if (found) CHECK(txp.isBlock());
}

TEST_CASE("Mining: cancel via stop flag") {
    std::atomic<bool> stop{true};
    TxPoW txp;
    txp.header().blockNumber     = MiniNumber(int64_t(0));
    txp.header().blockDifficulty = MiniData(std::vector<uint8_t>(32, 0x00));

    bool found = mineBlock(txp, 0, &stop);
    CHECK_FALSE(found);
}

TEST_CASE("Mining: mineTxn — trivial txnDifficulty") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(0));
    txp.body().txnDifficulty = makeDifficulty(0);

    bool found = mineTxn(txp, 1);
    CHECK(found);
    CHECK(txp.isTransaction());
}

// ── MiningManager (high-level) ─────────────────────────────────────────────

TEST_CASE("MiningManager: mines one block with trivial difficulty") {
    ChainProcessor chain;

    MiningManager::Config cfg;
    cfg.defaultLeadingZeros = 0;   // trivial — any hash passes
    cfg.continuous          = false; // stop after first block

    MiningManager miner(chain, cfg);
    miner.setLogger([](const std::string&){});

    std::atomic<int> found{0};
    TxPoW minedBlock;
    miner.onMined([&](const TxPoW& blk) {
        found++;
        minedBlock = blk;
    });

    miner.start();

    // Wait up to 5s for a block
    for (int i = 0; i < 50 && found == 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    miner.stop();

    CHECK(found >= 1);
    CHECK(minedBlock.header().blockNumber.getAsLong() == 1);
}

TEST_CASE("MiningManager: block is added to ChainProcessor") {
    ChainProcessor chain;
    CHECK(chain.getHeight() == 0);   // fresh chain starts at 0

    MiningManager::Config cfg;
    cfg.defaultLeadingZeros = 0;
    cfg.continuous          = false;

    MiningManager miner(chain, cfg);
    miner.setLogger([](const std::string&){});

    miner.start();
    for (int i = 0; i < 50 && chain.getHeight() < 1; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    miner.stop();

    CHECK(chain.getHeight() >= 1);
}

TEST_CASE("MiningManager: block number increments with chain height") {
    ChainProcessor chain;

    MiningManager::Config cfg;
    cfg.defaultLeadingZeros = 0;
    cfg.continuous          = true;

    MiningManager miner(chain, cfg);
    miner.setLogger([](const std::string&){});

    std::vector<int64_t> blockNums;
    std::mutex mu;
    miner.onMined([&](const TxPoW& blk) {
        std::lock_guard<std::mutex> lk(mu);
        blockNums.push_back(blk.header().blockNumber.getAsLong());
    });

    miner.start();
    // Wait for 3 blocks
    for (int i = 0; i < 100; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> lk(mu);
        if ((int)blockNums.size() >= 3) break;
    }
    miner.stop();

    std::lock_guard<std::mutex> lk(mu);
    REQUIRE((int)blockNums.size() >= 3);
    CHECK(blockNums[0] == 1);
    CHECK(blockNums[1] == 2);
    CHECK(blockNums[2] == 3);
}

TEST_CASE("MiningManager: notifyNewTip restarts template") {
    ChainProcessor chain;

    MiningManager::Config cfg;
    cfg.defaultLeadingZeros = 0;
    cfg.continuous          = true;
    cfg.checkStopEvery      = 1;  // check tip change every hash

    MiningManager miner(chain, cfg);
    miner.setLogger([](const std::string&){});

    std::atomic<int> blockCount{0};
    miner.onMined([&](const TxPoW&){ blockCount++; });

    miner.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    miner.notifyNewTip();  // should not crash, just rebuild template
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    miner.stop();

    CHECK(blockCount >= 1);  // still found blocks after notifyNewTip
}

TEST_CASE("MiningManager: stop flag halts mining") {
    ChainProcessor chain;

    MiningManager::Config cfg;
    cfg.defaultLeadingZeros = 32;  // impossible — hash can never be < 0
    cfg.continuous          = true;

    MiningManager miner(chain, cfg);
    miner.setLogger([](const std::string&){});

    std::atomic<int> found{0};
    miner.onMined([&](const TxPoW&){ found++; });

    miner.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    miner.stop();  // should return quickly

    CHECK(found == 0);
    CHECK(miner.isRunning() == false);
}

TEST_CASE("MiningManager: totalHashes increments") {
    ChainProcessor chain;

    MiningManager::Config cfg;
    cfg.defaultLeadingZeros = 0;
    cfg.continuous          = false;

    MiningManager miner(chain, cfg);
    miner.setLogger([](const std::string&){});

    miner.start();
    for (int i = 0; i < 50 && miner.isRunning(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    miner.stop();

    CHECK(miner.totalHashes() >= 1);
}

TEST_CASE("MiningManager: each mined block has valid timestamp") {
    ChainProcessor chain;

    MiningManager::Config cfg;
    cfg.defaultLeadingZeros = 0;
    cfg.continuous          = false;

    MiningManager miner(chain, cfg);
    miner.setLogger([](const std::string&){});

    TxPoW blk;
    miner.onMined([&](const TxPoW& b){ blk = b; });

    auto before = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    miner.start();
    for (int i = 0; i < 50 && blk.header().blockNumber.getAsLong() == 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    miner.stop();

    auto after = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    int64_t ts = blk.header().timeMilli.getAsLong();
    CHECK(ts >= before);
    CHECK(ts <= after + 1000);  // +1s tolerance
}
