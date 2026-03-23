#pragma once
/**
 * MMRSet — the full Merkle Mountain Range state.
 *
 * In Minima Java: com.minima.objects.base.MMRSet
 *
 * An MMR is a list of perfect binary trees (peaks). Leaves are added
 * left-to-right. When two trees of equal height exist, they are merged.
 *
 * Key operations:
 *   - addLeaf(data)   → returns MMREntry (row=0, entry=leafCount)
 *   - getProof(entry) → MMRProof for leaf at entry
 *   - getRoot()       → single root hash (bagging the peaks)
 *   - updateLeaf(entry, data) → mark coin spent etc.
 */
#include "MMRData.hpp"
#include "MMREntry.hpp"
#include "MMRProof.hpp"
#include <map>
#include <vector>
#include <optional>

namespace minima {

class MMRSet {
public:
    MMRSet() = default;

    /**
     * Add a new leaf. Returns its MMREntry (row=0).
     */
    MMREntry addLeaf(MMRData data);

    /**
     * Update an existing leaf (e.g. mark as spent).
     */
    void updateLeaf(uint64_t entry, MMRData data);

    /**
     * Get MMREntry at given row + entry index.
     */
    std::optional<MMREntry> getEntry(int row, uint64_t entry) const;

    /**
     * Build inclusion proof for leaf at entry.
     */
    MMRProof getProof(uint64_t entry) const;

    /**
     * Compute the single MMR root by bagging all peaks.
     * root = hash(peak_highest || ... || peak_lowest)
     */
    MMRData getRoot() const;

    /**
     * Total number of leaves.
     */
    uint64_t getLeafCount() const { return m_leafCount; }

    /**
     * Height of the MMR (max row index).
     */
    int getMaxRow() const;

    void serialise(DataStream& ds) const;
    void deserialise(DataStream& ds);

private:
    // nodes[row][entry] = MMRData
    std::map<int, std::map<uint64_t, MMRData>> m_nodes;
    uint64_t m_leafCount{0};

    void setNode(int row, uint64_t entry, MMRData data);
    std::optional<MMRData> getNode(int row, uint64_t entry) const;

    // After adding a leaf, propagate upward merging sibling pairs
    void propagate(int row, uint64_t entry);
};

} // namespace minima
