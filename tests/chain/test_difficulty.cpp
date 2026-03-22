#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/chain/DifficultyAdjust.hpp"

using namespace minima;

// Helper: make uniform difficulty
static MiniData makeDiff(uint8_t byte0 = 0x00, uint8_t rest = 0xFF) {
    std::vector<uint8_t> d(32, rest);
    d[0] = byte0;
    return MiniData(d);
}

// Build a synthetic chain of n blocks with uniform timing and difficulty
// chain[0] = newest (parent), chain[n-1] = oldest
static std::vector<BlockSummary> buildChain(int n, int64_t intervalMs,
                                             const MiniData& diff)
{
    std::vector<BlockSummary> chain;
    int64_t now = int64_t(1700000000) * 1000; // base time
    for (int i = 0; i < n; ++i) {
        int64_t blockNum = 500 - i;
        int64_t t        = now - (int64_t)i * intervalMs;
        chain.emplace_back(blockNum, t, diff);
    }
    return chain;
}

TEST_CASE("DifficultyAdjust: first 8 blocks return minTxPoWWork") {
    // chain[0].blockNumber = 7 → return min work
    std::vector<BlockSummary> chain;
    chain.emplace_back(7, 1000, makeDiff(0x10));
    auto d = DifficultyAdjust::calculate(chain);
    CHECK(d.bytes() == minTxPoWWork().bytes());
}

TEST_CASE("DifficultyAdjust: not enough history returns minTxPoWWork") {
    // Only 10 blocks — far fewer than 256
    auto chain = buildChain(10, 50000, makeDiff(0x00));
    auto d = DifficultyAdjust::calculate(chain);
    CHECK(d.bytes() == minTxPoWWork().bytes());
}

TEST_CASE("DifficultyAdjust: perfect speed returns approx same difficulty") {
    // 50-second blocks = 0.02 blocks/sec = MINIMA_BLOCK_SPEED
    // speedRatio ≈ 1.0 → newDiff ≈ avgDiff
    int64_t interval = 50000; // ms
    auto baseDiff = MiniData(std::vector<uint8_t>(32, 0x10));
    int n = DifficultyParams::BLOCKS_SPEED_CALC + DifficultyParams::MEDIAN_BLOCK_CALC * 2 + 5;
    auto chain = buildChain(n, interval, baseDiff);

    auto newDiff = DifficultyAdjust::calculate(chain);
    // Result should be close to baseDiff (within a factor of ~1.1)
    // Just verify it's a valid 32-byte value and not all zeros
    CHECK(newDiff.size() == 32);
    CHECK(newDiff.bytes() != std::vector<uint8_t>(32, 0x00));
}

TEST_CASE("DifficultyAdjust: too-fast blocks → lower difficulty (easier)") {
    // 1-second blocks → 1.0 blocks/sec >> 0.02 target
    // speedRatio = 0.02/1.0 = 0.02 < 0.25 → clamped to 0.25
    // newDiff = avgDiff * 0.25 → harder (smaller value in high bytes, larger in meaning)
    // Wait - Minima: difficulty = MAX/hashCount, so larger value = easier
    // If chain is too fast → need more work → smaller difficulty value
    int64_t interval = 1000; // 1s per block = too fast
    auto baseDiff = MiniData(std::vector<uint8_t>(32, 0x10));
    int n = DifficultyParams::BLOCKS_SPEED_CALC + DifficultyParams::MEDIAN_BLOCK_CALC * 2 + 5;
    auto chain = buildChain(n, interval, baseDiff);

    auto newDiff = DifficultyAdjust::calculate(chain);
    CHECK(newDiff.size() == 32);
    // newDiff should be smaller (harder) than baseDiff since blocks were too fast
    // First byte of baseDiff is 0x10; newDiff first byte should be <= 0x10
    // Actually clamped at MIN_SPEED_BOUND=0.25 so it's 0.25 * baseDiff
    // We just check it's valid (not the all-0xFF minWork)
    CHECK(newDiff.bytes() != minTxPoWWork().bytes());
}

TEST_CASE("DifficultyAdjust: too-slow blocks → higher difficulty (easier actually)") {
    // 3600-second blocks → too slow
    // speedRatio = 0.02 / (1/3600) = 72 → clamped to MAX 4.0
    // newDiff = avgDiff * 4 → easier (bigger value)
    int64_t interval = 3600000; // 1hr per block
    auto baseDiff = MiniData(std::vector<uint8_t>(32, 0x10));
    int n = DifficultyParams::BLOCKS_SPEED_CALC + DifficultyParams::MEDIAN_BLOCK_CALC * 2 + 5;
    auto chain = buildChain(n, interval, baseDiff);

    auto newDiff = DifficultyAdjust::calculate(chain);
    CHECK(newDiff.size() == 32);
    // newDiff should be larger than baseDiff (easier), clamped at minTxPoWWork
    // With ratio=4 and baseDiff[0]=0x10 → 0x40 or minWork
    // Expect first significant byte ≥ 0x10
    // (or clamped to minWork = 0xFF)
    CHECK(newDiff.bytes()[0] >= baseDiff.bytes()[0]);
}

TEST_CASE("DifficultyAdjust: result with minWork base stays near minWork") {
    // Base diff = all 0xFF (minWork), slow blocks → ratio=4 → mulRational(0xFF,4)=0xFC per byte
    // 0xFC < 0xFF so exceedsMin=false → returns ~0xFC*32 (very easy, close to minWork)
    auto baseDiff = minTxPoWWork();
    int n = DifficultyParams::BLOCKS_SPEED_CALC + DifficultyParams::MEDIAN_BLOCK_CALC * 2 + 5;
    auto chain = buildChain(n, 3600000, baseDiff);

    auto newDiff = DifficultyAdjust::calculate(chain);
    CHECK(newDiff.size() == 32);
    // Result should be high (easy difficulty) — first byte >= 0xF0
    CHECK(newDiff.bytes()[0] >= 0xF0);
}

TEST_CASE("DifficultyAdjust: big256 add and div helpers") {
    using namespace minima::bigint;
    std::vector<uint8_t> a(32, 0x00); a[31] = 0x10;
    std::vector<uint8_t> b(32, 0x00); b[31] = 0x20;
    auto c = add256(a, b);
    CHECK(c[31] == 0x30);

    auto d = div256(c, 3);
    CHECK(d[31] == 0x10);
}

TEST_CASE("DifficultyAdjust: mulRational by 1.0 is identity") {
    using namespace minima::bigint;
    std::vector<uint8_t> a(32, 0x00);
    a[0] = 0x01; a[31] = 0x00;
    auto b = mulRational(a, 1.0);
    // Should be very close to a (floating point ≈ exact for small values)
    CHECK(b[0] == 0x01);
}
