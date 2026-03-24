#include "MMRProof.hpp"

namespace minima {

MMRData MMRProof::calculateLocalPeak() const {
    MMRData current = m_data;
    uint64_t entry  = m_entry;
    for (const auto& sibling : m_proof) {
        if (entry % 2 == 0)
            current = MMRData::combine(current, sibling);
        else
            current = MMRData::combine(sibling, current);
        entry /= 2;
    }
    return current;
}

MMRData MMRProof::calculateProof() const {
    // 1. Compute local peak
    MMRData localPeak = calculateLocalPeak();

    // 2. Bag: left peaks (higher row) are combined first (left side),
    //         then localPeak,
    //         then right peaks (lower row, right side)
    // Order: combine(leftPeaks[0], combine(leftPeaks[1], ... combine(localPeak, rightPeaks...) ...))
    // Actually Minima bags highest first:
    //   result = peaks[0] (highest)
    //   result = combine(result, peaks[1])
    //   ...
    // leftPeaks are stored highest-first, rightPeaks lowest-first

    MMRData result;
    bool hasResult = false;

    // Start with left peaks (highest row first)
    for (const auto& lp : m_leftPeaks) {
        if (!hasResult) { result = lp; hasResult = true; }
        else result = MMRData::combine(result, lp);
    }

    // Then our local peak
    if (!hasResult) { result = localPeak; hasResult = true; }
    else result = MMRData::combine(result, localPeak);

    // Then right peaks
    for (const auto& rp : m_rightPeaks) {
        result = MMRData::combine(result, rp);
    }

    return result;
}


MMRData MMRProof::calculateProof(const MMRData& startData) const {
    // Temporarily use startData as leaf, then calculate
    MMRData current = startData;
    uint64_t entry  = m_entry;
    for (const auto& sibling : m_proof) {
        if (entry % 2 == 0)
            current = MMRData::combine(current, sibling);
        else
            current = MMRData::combine(sibling, current);
        entry /= 2;
    }
    MMRData localPeak = current;

    // Bag peaks (same as calculateProof())
    MMRData result;
    bool hasResult = false;
    for (const auto& lp : m_leftPeaks) {
        if (!hasResult) { result = lp; hasResult = true; }
        else result = MMRData::combine(result, lp);
    }
    if (!hasResult) { result = localPeak; hasResult = true; }
    else result = MMRData::combine(result, localPeak);
    for (const auto& rp : m_rightPeaks) {
        result = MMRData::combine(result, rp);
    }
    return result;
}

bool MMRProof::verifyProof(const MMRData& root) const {
    MMRData computed = calculateProof();
    return computed.getData() == root.getData();
}

void MMRProof::serialise(DataStream& ds) const {
    ds.writeUInt64(m_blocktime);
    ds.writeUInt64(m_entry);
    m_data.serialise(ds);
    ds.writeUInt32(static_cast<uint32_t>(m_proof.size()));
    for (const auto& c : m_proof) c.serialise(ds);
    ds.writeUInt32(static_cast<uint32_t>(m_leftPeaks.size()));
    for (const auto& c : m_leftPeaks) c.serialise(ds);
    ds.writeUInt32(static_cast<uint32_t>(m_rightPeaks.size()));
    for (const auto& c : m_rightPeaks) c.serialise(ds);
}

void MMRProof::deserialise(DataStream& ds) {
    m_blocktime = ds.readUInt64();
    m_entry     = ds.readUInt64();
    m_data.deserialise(ds);
    auto readVec = [&](std::vector<MMRData>& v) {
        uint32_t n = ds.readUInt32(); v.clear(); v.reserve(n);
        for (uint32_t i = 0; i < n; ++i) { MMRData d; d.deserialise(ds); v.push_back(std::move(d)); }
    };
    readVec(m_proof);
    readVec(m_leftPeaks);
    readVec(m_rightPeaks);
}

// ── Convenience wrappers ─────────────────────────────────────────────────
std::vector<uint8_t> MMRProof::serialise() const {
    DataStream ds;
    serialise(ds);
    return ds.buffer();
}

MMRProof MMRProof::deserialise(const uint8_t* data, size_t& offset, size_t total_size) {
    // Use remaining bytes from offset to end of buffer — never read past end.
    size_t avail = (total_size > offset) ? (total_size - offset) : 0;
    DataStream ds(data + offset, avail);
    MMRProof proof;
    proof.deserialise(ds);
    offset += ds.position();
    return proof;
}

// Backward-compat overload — caller doesn't know total size; use generous cap.
MMRProof MMRProof::deserialise(const uint8_t* data, size_t& offset) {
    // We don't know the true buffer size here. Use MAX_PROOF as a safe upper
    // bound; real bounds checking happens inside DataStream::readBytes().
    constexpr size_t MAX_PROOF = 64 * 1024;
    DataStream ds(data + offset, MAX_PROOF);
    MMRProof proof;
    proof.deserialise(ds);
    offset += ds.position();
    return proof;
}

} // namespace minima
