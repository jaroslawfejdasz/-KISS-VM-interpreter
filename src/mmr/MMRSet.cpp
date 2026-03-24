#include "MMRSet.hpp"
#include <algorithm>
#include <stdexcept>

namespace minima {

// ── public API ───────────────────────────────────────────────────────────────

MMREntry MMRSet::addLeaf(MMRData data) {
    uint64_t entry = m_leafCount++;
    setNode(0, entry, data);
    propagate(0, entry);
    return MMREntry(0, entry, std::move(data));
}

void MMRSet::updateLeaf(uint64_t entry, MMRData data) {
    if (!getNode(0, entry).has_value())
        throw std::out_of_range("MMRSet::updateLeaf — entry does not exist");
    setNode(0, entry, data);
    propagate(0, entry);
}

std::optional<MMREntry> MMRSet::getEntry(int row, uint64_t entry) const {
    auto node = getNode(row, entry);
    if (!node.has_value()) return std::nullopt;
    return MMREntry(row, entry, *node);
}

MMRProof MMRSet::getProof(uint64_t leafEntry) const {
    MMRProof proof;
    proof.setEntry(leafEntry);
    auto leaf = getNode(0, leafEntry);
    if (!leaf.has_value())
        throw std::out_of_range("MMRSet::getProof — leaf does not exist");
    proof.setData(*leaf);

    // Build sibling chain up to local peak
    int      row   = 0;
    uint64_t entry = leafEntry;
    while (true) {
        uint64_t sibling = (entry % 2 == 0) ? entry + 1 : entry - 1;
        auto sibNode = getNode(row, sibling);
        if (!sibNode.has_value()) break;
        proof.addProofChunk(*sibNode);
        entry /= 2;
        row++;
    }
    // Now (row, entry) is our local peak

    // Collect all peaks and classify as left/right relative to our peak
    std::vector<std::pair<int,uint64_t>> allPeaks;
    for (const auto& [r, entries] : m_nodes) {
        for (const auto& [e, data] : entries) {
            int      pRow   = r + 1;
            uint64_t pEntry = e / 2;
            bool hasParent  = false;
            auto it = m_nodes.find(pRow);
            if (it != m_nodes.end())
                hasParent = it->second.count(pEntry) > 0;
            if (!hasParent)
                allPeaks.emplace_back(r, e);
        }
    }
    // Sort highest row first, then highest entry first
    std::sort(allPeaks.begin(), allPeaks.end(), [](const auto& a, const auto& b){
        if (a.first != b.first) return a.first > b.first;
        return a.second > b.second;
    });

    // Classify: peaks before ours (higher row or same row lower entry) → left
    //           peaks after ours → right
    bool foundOurs = false;
    for (const auto& [pr, pe] : allPeaks) {
        if (pr == row && pe == entry) { foundOurs = true; continue; }
        if (!foundOurs)
            proof.addLeftPeak(*getNode(pr, pe));
        else
            proof.addRightPeak(*getNode(pr, pe));
    }
    return proof;
}

MMRData MMRSet::getRoot() const {
    if (m_leafCount == 0) return MMRData(MiniData());

    // Collect all peaks: nodes that have no parent node
    std::vector<std::pair<int,uint64_t>> peaks;
    for (const auto& [row, entries] : m_nodes) {
        for (const auto& [entry, data] : entries) {
            int      pRow   = row + 1;
            uint64_t pEntry = entry / 2;
            bool hasParent  = false;
            auto it = m_nodes.find(pRow);
            if (it != m_nodes.end())
                hasParent = it->second.count(pEntry) > 0;
            if (!hasParent)
                peaks.emplace_back(row, entry);
        }
    }

    if (peaks.empty()) return MMRData(MiniData());
    if (peaks.size() == 1)
        return *getNode(peaks[0].first, peaks[0].second);

    // Sort: highest row first, within same row highest entry first
    std::sort(peaks.begin(), peaks.end(), [](const auto& a, const auto& b){
        if (a.first != b.first) return a.first > b.first;
        return a.second > b.second;
    });

    // Bag peaks: combine(highest, second), then result with third, etc.
    MMRData result = *getNode(peaks[0].first, peaks[0].second);
    for (size_t i = 1; i < peaks.size(); ++i) {
        MMRData peak = *getNode(peaks[i].first, peaks[i].second);
        result = MMRData::combine(result, peak);
    }
    return result;
}

int MMRSet::getMaxRow() const {
    if (m_nodes.empty()) return 0;
    return m_nodes.rbegin()->first;
}

// ── private ──────────────────────────────────────────────────────────────────

void MMRSet::setNode(int row, uint64_t entry, MMRData data) {
    m_nodes[row][entry] = std::move(data);
}

std::optional<MMRData> MMRSet::getNode(int row, uint64_t entry) const {
    auto rowIt = m_nodes.find(row);
    if (rowIt == m_nodes.end()) return std::nullopt;
    auto entIt = rowIt->second.find(entry);
    if (entIt == rowIt->second.end()) return std::nullopt;
    return entIt->second;
}

void MMRSet::propagate(int row, uint64_t entry) {
    while (true) {
        uint64_t sibling = (entry % 2 == 0) ? entry + 1 : entry - 1;
        auto sibNode = getNode(row, sibling);
        if (!sibNode.has_value()) break;

        uint64_t parentEntry = entry / 2;
        MMRData left  = (entry % 2 == 0) ? *getNode(row, entry) : *sibNode;
        MMRData right = (entry % 2 == 0) ? *sibNode             : *getNode(row, entry);
        setNode(row + 1, parentEntry, MMRData::combine(left, right));
        row++;
        entry = parentEntry;
    }
}

void MMRSet::serialise(DataStream& ds) const {
    ds.writeUInt64(m_leafCount);
    ds.writeUInt32(static_cast<uint32_t>(m_nodes.size()));
    for (const auto& [row, entries] : m_nodes) {
        ds.writeUInt32(static_cast<uint32_t>(row));
        ds.writeUInt32(static_cast<uint32_t>(entries.size()));
        for (const auto& [entry, data] : entries) {
            ds.writeUInt64(entry);
            data.serialise(ds);
        }
    }
}

void MMRSet::deserialise(DataStream& ds) {
    m_leafCount = ds.readUInt64();
    uint32_t rowCount = ds.readUInt32();
    m_nodes.clear();
    for (uint32_t r = 0; r < rowCount; ++r) {
        int      row        = static_cast<int>(ds.readUInt32());
        uint32_t entryCount = ds.readUInt32();
        for (uint32_t e = 0; e < entryCount; ++e) {
            uint64_t entry = ds.readUInt64();
            MMRData  data;
            data.deserialise(ds);
            m_nodes[row][entry] = std::move(data);
        }
    }
}

} // namespace minima
