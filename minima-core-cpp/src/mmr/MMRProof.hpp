#pragma once
/**
 * MMRProof — inclusion proof for one leaf.
 *
 * Proof structure:
 *   1. Sibling chain from leaf up to a local MMR peak
 *   2. Left peaks (higher-row peaks to the left of this peak)
 *   3. Right peaks (lower-row peaks to the right of this peak)
 *
 * Verification: compute local peak → bag with left/right peaks → compare to root.
 */
#include "MMRData.hpp"
#include <vector>

namespace minima {

class MMRProof {
public:
    MMRProof() = default;

    uint64_t              getBlocktime() const { return m_blocktime; }
    uint64_t              getEntry()     const { return m_entry; }
    const MMRData&        getData()      const { return m_data; }
    const std::vector<MMRData>& getProof()       const { return m_proof; }
    const std::vector<MMRData>& getLeftPeaks()   const { return m_leftPeaks; }
    const std::vector<MMRData>& getRightPeaks()  const { return m_rightPeaks; }

    void setBlocktime(uint64_t bt)    { m_blocktime = bt; }
    void setEntry(uint64_t e)         { m_entry = e; }
    void setData(MMRData d)           { m_data = std::move(d); }
    void addProofChunk(MMRData d)     { m_proof.push_back(std::move(d)); }
    void addLeftPeak(MMRData d)       { m_leftPeaks.push_back(std::move(d)); }
    void addRightPeak(MMRData d)      { m_rightPeaks.push_back(std::move(d)); }

    int proofLength() const { return static_cast<int>(m_proof.size()); }

    /** Compute the local peak hash from leaf + sibling chain. */
    MMRData calculateLocalPeak() const;

    /** Compute the full MMR root (bag all peaks). */
    MMRData calculateProof() const;

    /** Verify this proof against a known MMR root hash. */
    bool verifyProof(const MMRData& root) const;

    void serialise(DataStream& ds) const;
    void deserialise(DataStream& ds);

private:
    uint64_t             m_blocktime{0};
    uint64_t             m_entry{0};
    MMRData              m_data;
    std::vector<MMRData> m_proof;       // sibling chain to local peak
    std::vector<MMRData> m_leftPeaks;   // peaks with higher row (left side in bag)
    std::vector<MMRData> m_rightPeaks;  // peaks with lower row (right side in bag)
};

} // namespace minima
