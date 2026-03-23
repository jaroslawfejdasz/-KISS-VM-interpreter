#pragma once
/**
 * TxPoWMiner — increments nonce until TxPoW satisfies difficulty.
 *
 * Mining algorithm (Minima Java ref: TxPoWMiner.java):
 *   1. nonce = 0
 *   2. powHash = SHA2-256(TxHeader bytes)
 *   3. If powHash <= blockDifficulty  → isBlock() == true
 *   4. If powHash <= txnDifficulty    → isTransaction() == true
 *   5. Else: nonce++ and repeat
 */
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include <cstdint>
#include <atomic>
#include <vector>
#include <algorithm>

namespace minima {
namespace mining {

// Compare 32-byte big-endian: return true if a < b
inline bool lessThan(const std::vector<uint8_t>& a,
                     const std::vector<uint8_t>& b)
{
    size_t len = std::min(a.size(), b.size());
    for (size_t i = 0; i < len; ++i) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return false;
}

/**
 * Mine until TxPoW.isBlock() == true.
 * @param txpow     Modified in place
 * @param maxIter   0 = unlimited
 * @param stop      Optional cancel flag (checked every 1024 iters)
 * @return true if valid block nonce found
 */
inline bool mineBlock(TxPoW& txpow,
                      uint64_t maxIter = 0,
                      const std::atomic<bool>* stop = nullptr)
{
    for (uint64_t n = 0; ; ++n) {
        txpow.header().nonce = MiniNumber(static_cast<int64_t>(n));
        if (txpow.isBlock()) return true;
        if (maxIter > 0 && n >= maxIter - 1) return false;
        if (stop && (n & 0x3FF) == 0 && stop->load()) return false;
    }
}

/**
 * Mine until TxPoW.isTransaction() == true (txnDifficulty).
 */
inline bool mineTxn(TxPoW& txpow,
                    uint64_t maxIter = 0,
                    const std::atomic<bool>* stop = nullptr)
{
    for (uint64_t n = 0; ; ++n) {
        txpow.header().nonce = MiniNumber(static_cast<int64_t>(n));
        if (txpow.isTransaction()) return true;
        if (maxIter > 0 && n >= maxIter - 1) return false;
        if (stop && (n & 0x3FF) == 0 && stop->load()) return false;
    }
}

/**
 * Build a difficulty target.
 * leadingZeroBytes == 0 → all 0xFF = trivial (any hash passes)
 * leadingZeroBytes == 32 → impossible
 */
inline MiniData makeDifficulty(int leadingZeroBytes) {
    std::vector<uint8_t> d(32, 0xFF);
    for (int i = 0; i < leadingZeroBytes && i < 32; ++i) d[i] = 0x00;
    return MiniData(d);
}

} // namespace mining
} // namespace minima
