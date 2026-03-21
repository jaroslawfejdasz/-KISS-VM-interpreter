#pragma once
/**
 * BlockStore — indexed store of validated block TxPoWs.
 * Java ref: src/org/minima/database/txpowtree/TxPoWTreeNode.java
 */
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include <unordered_map>
#include <map>
#include <optional>
#include <vector>

namespace minima {

struct MiniDataHash {
    size_t operator()(const MiniData& d) const {
        size_t h = 0;
        for (uint8_t b : d.bytes()) h = h * 31 + b;
        return h;
    }
};
struct MiniDataEqual {
    bool operator()(const MiniData& a, const MiniData& b) const {
        return a.bytes() == b.bytes();
    }
};

} // namespace minima

namespace minima::chain {

class BlockStore {
public:
    void add(const MiniData& id, const TxPoW& block) {
        uint64_t bn = (uint64_t)block.header().blockNumber.getAsLong();
        m_byHash[id]    = block;
        m_byNumber[bn]  = id;
    }

    bool has(const MiniData& id) const { return m_byHash.count(id) > 0; }

    std::optional<TxPoW> get(const MiniData& id) const {
        auto it = m_byHash.find(id);
        if (it == m_byHash.end()) return std::nullopt;
        return it->second;
    }

    std::optional<TxPoW> getByNumber(uint64_t n) const {
        auto it = m_byNumber.find(n);
        if (it == m_byNumber.end()) return std::nullopt;
        return get(it->second);
    }

    size_t size() const { return m_byHash.size(); }

    bool getAncestors(const MiniData& id, size_t count, std::vector<TxPoW>& out) const {
        MiniData cur = id;
        for (size_t i = 0; i < count; ++i) {
            auto opt = get(cur);
            if (!opt) return false;
            out.push_back(*opt);
            if (opt->header().blockNumber.getAsLong() == 0) break;
            cur = opt->header().superParents[0]; // parent = superParents[0]
        }
        return true;
    }

private:
    std::unordered_map<MiniData, TxPoW, MiniDataHash, MiniDataEqual> m_byHash;
    std::map<uint64_t, MiniData> m_byNumber;
};

} // namespace minima::chain
