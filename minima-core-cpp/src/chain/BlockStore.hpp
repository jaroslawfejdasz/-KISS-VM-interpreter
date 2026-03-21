#pragma once
/**
 * BlockStore — ordered store of accepted block TxPoWs.
 *
 * Indexed by:
 *   - txpowID (hash)  → O(1) lookup
 *   - blockNumber     → O(1) lookup of canonical chain
 */
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include "UTxOSet.hpp"   // reuses MiniDataHash / MiniDataEqual
#include <unordered_map>
#include <map>
#include <vector>
#include <optional>

namespace minima::chain {

class BlockStore {
public:
    void store(const TxPoW& block) {
        uint64_t bn = static_cast<uint64_t>(
            block.header().blockNumber.getAsLong());
        m_byHash[block.computeID()]   = block;
        m_byNumber[bn]                        = block.computeID();
    }

    bool has(const MiniData& hashID) const {
        return m_byHash.count(hashID) > 0;
    }

    std::optional<TxPoW> getByHash(const MiniData& hashID) const {
        auto it = m_byHash.find(hashID);
        if (it == m_byHash.end()) return std::nullopt;
        return it->second;
    }

    std::optional<TxPoW> getByNumber(uint64_t blockNum) const {
        auto it = m_byNumber.find(blockNum);
        if (it == m_byNumber.end()) return std::nullopt;
        return getByHash(it->second);
    }

    size_t size() const { return m_byHash.size(); }

    // Walk ancestors of `hashID` up to `count` blocks (inclusive), newest first
    bool getAncestors(const MiniData& hashID, size_t count,
                      std::vector<TxPoW>& out) const {
        MiniData cur = hashID;
        for (size_t i = 0; i < count; ++i) {
            auto opt = getByHash(cur);
            if (!opt) return false;
            out.push_back(*opt);
            if (opt->header().blockNumber.getAsLong() == 0) break; // genesis
            cur = opt->header().parentID;
        }
        return true;
    }

private:
    std::unordered_map<MiniData, TxPoW,    MiniDataHash, MiniDataEqual> m_byHash;
    std::map<uint64_t,           MiniData>                               m_byNumber;
};

} // namespace minima::chain
