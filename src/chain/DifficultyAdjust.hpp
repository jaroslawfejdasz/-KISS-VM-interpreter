#pragma once
/**
 * DifficultyAdjust — Minima block difficulty retarget.
 *
 * Java ref: TxPoWGenerator.getBlockDifficulty()
 *
 * Algorithm:
 *   1. Look back MINIMA_BLOCKS_SPEED_CALC (256) blocks
 *   2. Compute median-time block at each end (5-block window)
 *   3. Actual speed (blocks/sec) = blockDiff / timeDiff
 *   4. speedRatio = TARGET_SPEED / actualSpeed   (clamped [0.25, 4.0])
 *   5. averageDifficulty = mean of difficulty values over those 256 blocks
 *   6. newDifficulty = averageDifficulty * speedRatio
 *   7. Clamp: newDifficulty <= MIN_TXPOW_WORK  (= all 0xFF = easiest)
 *
 * For standalone use without a full tree, simplified interface provided:
 *   DifficultyAdjust::calculate(blockHeaders, targetBlocksPerSec)
 *
 * Note: In Minima "difficulty" is an inverted value — HIGHER bytes = EASIER.
 * (difficulty = MAX_VAL / hashCount, so more hashes → smaller value → harder)
 */
#include "../objects/TxHeader.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include <vector>
#include <algorithm>
#include <cstdint>
#include <numeric>

// ── big-integer helpers (256-bit as 32 bytes big-endian) ──────────────────────
namespace minima { namespace bigint {

// Add two 32-byte big-endian values
inline std::vector<uint8_t> add256(const std::vector<uint8_t>& a,
                                    const std::vector<uint8_t>& b)
{
    std::vector<uint8_t> out(32, 0);
    int carry = 0;
    for (int i = 31; i >= 0; --i) {
        int s = (int)(a.size() > 32 ? 0 : a[i]) + (int)(b.size() > 32 ? 0 : b[i]) + carry;
        out[i] = (uint8_t)(s & 0xFF);
        carry  = s >> 8;
    }
    return out;
}

// Divide 32-byte big-endian value by integer n
inline std::vector<uint8_t> div256(const std::vector<uint8_t>& a, uint64_t n)
{
    if (n == 0) return std::vector<uint8_t>(32, 0xFF);
    std::vector<uint8_t> out(32, 0);
    uint64_t rem = 0;
    for (int i = 0; i < 32; ++i) {
        uint64_t cur = (rem << 8) | a[i];
        out[i] = (uint8_t)(cur / n);
        rem    = cur % n;
    }
    return out;
}

// Divide 33-byte big-endian value by integer n, return bottom 32 bytes
inline std::vector<uint8_t> div256_33(const std::vector<uint8_t>& a, uint64_t n)
{
    if (n == 0) return std::vector<uint8_t>(32, 0xFF);
    // a has 33 bytes; result is 33 bytes, we return last 32
    std::vector<uint8_t> out(33, 0);
    uint64_t rem = 0;
    for (int i = 0; i < 33; ++i) {
        uint64_t cur = (rem << 8) | a[i];
        out[i] = (uint8_t)(cur / n);
        rem    = cur % n;
    }
    // Return bottom 32 bytes (drop leading overflow byte)
    return std::vector<uint8_t>(out.begin() + 1, out.end());
}

// Multiply 32-byte big-endian by rational (num/den), result clipped to 32 bytes
inline std::vector<uint8_t> mulRational(const std::vector<uint8_t>& a,
                                         double ratio)
{
    if (ratio <= 0.0) return std::vector<uint8_t>(32, 0);
    // Work with 64-bit scaled to avoid big-int: use double approximation
    // For the purposes of difficulty adjustment, double precision is sufficient
    // (we only use top ~64 bits of difficulty)
    std::vector<uint8_t> out(32, 0);
    double carry = 0.0;
    for (int i = 31; i >= 0; --i) {
        double v = a[i] * ratio + carry;
        uint64_t lo  = (uint64_t)v;
        carry        = (v - lo) * 256.0;
        out[i]       = (uint8_t)(lo & 0xFF);
    }
    return out;
}

}} // namespace minima::bigint

namespace minima {

// ── Constants (from GlobalParams) ─────────────────────────────────────────────
struct DifficultyParams {
    static constexpr double BLOCK_SPEED          = 0.02;   // blocks/sec (50s target)
    static constexpr int    BLOCKS_SPEED_CALC    = 256;    // look-back window
    static constexpr int    MEDIAN_BLOCK_CALC    = 5;      // median window
    static constexpr double MAX_SPEED_BOUND      = 4.0;    // clamp ratio max
    static constexpr double MIN_SPEED_BOUND      = 0.25;   // clamp ratio min
};

// All-0xFF = minimum work required = easiest possible
inline MiniData minTxPoWWork() {
    return MiniData(std::vector<uint8_t>(32, 0xFF));
}

// ── Input: a lightweight block summary ────────────────────────────────────────
struct BlockSummary {
    int64_t  blockNumber;
    int64_t  timeMilli;
    MiniData blockDifficulty; // 32-byte big-endian

    BlockSummary(int64_t bn, int64_t tm, MiniData d)
        : blockNumber(bn), timeMilli(tm), blockDifficulty(std::move(d)) {}
};

class DifficultyAdjust {
public:
    /**
     * Calculate the next block difficulty.
     *
     * @param chain   Recent blocks, NEWEST FIRST (index 0 = parent block)
     * @return        New blockDifficulty for the next block
     *
     * Requirements:
     *   chain.size() >= BLOCKS_SPEED_CALC + MEDIAN_BLOCK_CALC*2
     *   For the first 8 blocks: returns minTxPoWWork()
     */
    static MiniData calculate(const std::vector<BlockSummary>& chain,
                               const DifficultyParams& params = {})
    {
        // First 8 blocks: minimum work
        if (chain.empty() || chain[0].blockNumber < 8)
            return minTxPoWWork();

        int lookback = DifficultyParams::BLOCKS_SPEED_CALC;
        if ((int)chain.size() < lookback + DifficultyParams::MEDIAN_BLOCK_CALC * 2)
            return minTxPoWWork(); // Not enough history yet

        // Get median-time block at tip
        int startIdx = getMedianTimeBlockIndex(chain, 0, DifficultyParams::MEDIAN_BLOCK_CALC);
        // Get median-time block lookback blocks back
        int endIdx   = getMedianTimeBlockIndex(chain, lookback, DifficultyParams::MEDIAN_BLOCK_CALC);

        const BlockSummary& startBlock = chain[startIdx];
        const BlockSummary& endBlock   = chain[endIdx];

        // Same block (edge case at root)
        if (startBlock.blockNumber == endBlock.blockNumber)
            return MiniData(chain[0].blockDifficulty);

        int64_t blockDiff = startBlock.blockNumber - endBlock.blockNumber;
        int64_t timeDiff  = startBlock.timeMilli   - endBlock.timeMilli;

        // Guard against zero/negative time
        if (timeDiff <= 0) return MiniData(chain[0].blockDifficulty);

        // Actual speed in blocks/sec
        double speed = (double)blockDiff / ((double)timeDiff / 1000.0);

        // Speed ratio (target / actual), clamped
        double speedRatio = DifficultyParams::BLOCK_SPEED / speed;
        speedRatio = std::max(DifficultyParams::MIN_SPEED_BOUND,
                     std::min(DifficultyParams::MAX_SPEED_BOUND, speedRatio));

        // Average difficulty over [endIdx, startIdx)
        auto avgDiff = getAverageDifficulty(chain, endIdx, blockDiff);

        // New difficulty = average * speedRatio
        auto newDiffBytes = bigint::mulRational(avgDiff, speedRatio);
        MiniData newDiff(newDiffBytes);

        // Clamp: cannot be harder (smaller value) than MIN_TXPOW_WORK (all 0xFF)
        // In Minima: "isMore" for MiniData = numerically larger = easier work
        // We only allow newDiff <= minTxPoWWork (easier or equal), never harder
        // minTxPoWWork = 0xFF...FF, harder target = 0x00...01
        // If newDiff > minTxPoWWork (all 0xFF) → clamp to minTxPoWWork
        // (In practice newDiff should never exceed 0xFF×32 since we multiply avg)
        const auto& minWork = minTxPoWWork().bytes();
        bool exceedsMin = false;
        for (int i = 0; i < 32; ++i) {
            if (newDiffBytes[i] > minWork[i]) { exceedsMin = true; break; }
            if (newDiffBytes[i] < minWork[i]) break;
        }
        if (exceedsMin) return minTxPoWWork();

        return newDiff;
    }

    /**
     * Simpler interface: given a vector of (blockNumber, timeMilli, difficulty)
     * triples, compute the next difficulty.
     */
    static MiniData calculateFromArrays(
        const std::vector<int64_t>& blockNumbers,
        const std::vector<int64_t>& blockTimes,
        const std::vector<MiniData>& difficulties)
    {
        size_t n = std::min({blockNumbers.size(), blockTimes.size(), difficulties.size()});
        std::vector<BlockSummary> chain;
        chain.reserve(n);
        for (size_t i = 0; i < n; ++i)
            chain.emplace_back(blockNumbers[i], blockTimes[i], difficulties[i]);
        return calculate(chain);
    }

private:
    // Pick the index with median timeMilli in window [start, start+window)
    static int getMedianTimeBlockIndex(const std::vector<BlockSummary>& chain,
                                        int start, int window)
    {
        int available = std::min(window, (int)chain.size() - start);
        if (available <= 0) return start;

        std::vector<std::pair<int64_t,int>> times; // (time, index)
        for (int i = start; i < start + available; ++i)
            times.emplace_back(chain[i].timeMilli, i);
        std::sort(times.begin(), times.end());
        return times[times.size() / 2].second;
    }

    // Average of blockDifficulty over chain[endIdx..endIdx+count)
    // Uses 33-byte accumulator to avoid overflow during summation
    static std::vector<uint8_t> getAverageDifficulty(
        const std::vector<BlockSummary>& chain,
        int startIdx, int64_t count)
    {
        if (count <= 0) return std::vector<uint8_t>(32, 0xFF);

        // Use 33-byte total to handle carry from 32-byte sum
        std::vector<uint8_t> total(33, 0);
        int n = 0;
        for (int64_t i = 0; i < count && (startIdx + i) < (int64_t)chain.size(); ++i) {
            const auto& db = chain[startIdx + (int)i].blockDifficulty.bytes();
            // Pad to 32 bytes
            std::vector<uint8_t> padded(32, 0);
            size_t off = (db.size() < 32) ? 32 - db.size() : 0;
            for (size_t j = 0; j < db.size() && j + off < 32; ++j)
                padded[off + j] = db[j];
            // Add padded (32 bytes) to total (33 bytes), aligned to LSB
            int carry = 0;
            for (int j = 32; j >= 0; --j) {
                int a = total[j];
                int b = (j > 0) ? padded[j-1] : 0; // padded[0..31] → total[1..32]
                int s = a + b + carry;
                total[j] = (uint8_t)(s & 0xFF);
                carry = s >> 8;
            }
            ++n;
        }
        if (n == 0) return std::vector<uint8_t>(32, 0xFF);
        // Divide 33-byte total by n, return bottom 32 bytes
        std::vector<uint8_t> result = bigint::div256_33(total, (uint64_t)n);
        return result;
    }
};

} // namespace minima
