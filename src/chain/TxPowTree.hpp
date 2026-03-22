#pragma once
/**
 * TxPowTree — kanoniczna reprezentacja łańcucha jako drzewo.
 * Java ref: src/org/minima/database/txpowtree/TxPowTree.java
 *
 * Parent ID = superParents[0] (poziom 0 = bezpośredni rodzic).
 * TxPoW ID  = computeID() = SHA3(SHA3(header)).
 */
#include "TxPoWTreeNode.hpp"
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace minima::chain {

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

class TxPowTree {
public:
    TxPowTree() : m_root(nullptr), m_tip(nullptr) {}
    ~TxPowTree() { clear(); }

    /// Dodaj TxPoW. false = orphan (brak rodzica). true = ok lub duplikat.
    bool addTxPoW(const TxPoW& txpow) {
        MiniData id = txpow.computeID();
        if (m_nodes.count(id)) return true; // duplikat

        auto* node = new TxPoWTreeNode(txpow);

        if (m_root == nullptr) {
            // Pierwszy węzeł — genesis lub root po pruning
            m_root = node;
            m_tip  = node;
            node->setChainWeight(MiniNumber(int64_t(1)));
            m_nodes[id] = node;
            return true;
        }

        // Parent = superParents[0]
        const MiniData& parentID = txpow.header().superParents[0];
        auto it = m_nodes.find(parentID);
        if (it == m_nodes.end()) {
            delete node;
            return false; // orphan
        }

        TxPoWTreeNode* parent = it->second;
        parent->addChild(node);
        node->setChainWeight(MiniNumber(int64_t(parent->chainWeight().getAsLong() + 1)));
        m_nodes[id] = node;
        updateTip(node);
        return true;
    }

    bool            has(const MiniData& id)     const { return m_nodes.count(id) > 0; }
    TxPoWTreeNode*  getNode(const MiniData& id) const {
        auto it = m_nodes.find(id);
        return (it != m_nodes.end()) ? it->second : nullptr;
    }

    TxPoWTreeNode* tip()  const { return m_tip; }
    TxPoWTreeNode* root() const { return m_root; }
    size_t         size() const { return m_nodes.size(); }

    std::vector<TxPoWTreeNode*> canonicalChain() const {
        std::vector<TxPoWTreeNode*> chain;
        TxPoWTreeNode* cur = m_tip;
        while (cur) { chain.push_back(cur); cur = cur->parent(); }
        std::reverse(chain.begin(), chain.end());
        return chain;
    }

    void pruneBelow(int64_t depth) {
        std::vector<MiniData> toRemove;
        for (auto& [id, node] : m_nodes)
            if (node->depth() < depth) toRemove.push_back(id);
        for (const auto& id : toRemove) { delete m_nodes[id]; m_nodes.erase(id); }
    }

    void clear() {
        for (auto& [id, node] : m_nodes) delete node;
        m_nodes.clear();
        m_root = nullptr;
        m_tip  = nullptr;
    }

private:
    void updateTip(TxPoWTreeNode* c) {
        if (!m_tip || c->chainWeight().isMore(m_tip->chainWeight())) m_tip = c;
    }

    TxPoWTreeNode* m_root;
    TxPoWTreeNode* m_tip;
    std::unordered_map<MiniData, TxPoWTreeNode*, MiniDataHash, MiniDataEqual> m_nodes;
};

} // namespace minima::chain
