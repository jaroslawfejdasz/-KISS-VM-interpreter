#pragma once
/**
 * ChainState — a snapshot of the current best chain tip.
 */
#include <cstdint>

namespace minima::chain {

struct ChainState {
    uint64_t blockNumber{0};
    uint64_t timeMilli{0};

    bool isGenesis() const { return blockNumber == 0; }

    // Returns true if `candidate` is a better chain tip than *this
    bool isBetterThan(const ChainState& candidate) const {
        return candidate.blockNumber > blockNumber;
    }
};

} // namespace minima::chain
