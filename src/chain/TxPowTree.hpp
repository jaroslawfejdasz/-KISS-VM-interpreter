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
#include <unordered_set>
#include <queue>

namespace minima::chain {

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

    /**
     * pruneBelow — usuń węzły głębiej niż `depth` (od korzenia).
     * Uwaga: nie naprawia wskaźników parent/children — używaj trimTree() zamiast tego.
     */
    void pruneBelow(int64_t depth) {
        std::vector<MiniData> toRemove;
        for (auto& [id, node] : m_nodes)
            if (node->depth() < depth) toRemove.push_back(id);
        for (const auto& id : toRemove) { delete m_nodes[id]; m_nodes.erase(id); }
    }

    /**
     * trimTree — bezpieczne przycinanie drzewa.
     * Java ref: TxPoWTree.trimTree(int keepDepth)
     *
     * Zachowuje `keepDepth` poziomów od aktualnego tipa w górę.
     * Wyznacza nowy korzeń (keepDepth poziomów powyżej tipa),
     * usuwa wszystkie węzły poza zachowywaną gałęzią kanoniczną + jej fork-dzieci,
     * naprawia wskaźnik parent nowego korzenia (nullptr) i odcina stary root.
     *
     * Algorytm:
     *   1. Wyznacz newRoot = tip przesunięty w górę o keepDepth kroków
     *   2. Zbierz wszystkie węzły w poddrzewie newRoot (BFS)
     *   3. Usuń wszystkie węzły spoza poddrzewa
     *   4. Odetnij rodzica newRoot
     *   5. Zaktualizuj m_root, przenumeruj depth od 0
     */
    void trimTree(int64_t keepDepth = 64) {
        if (!m_tip || !m_root) return;

        // 1. Znajdź newRoot — keepDepth kroków w górę od tipa
        TxPoWTreeNode* newRoot = m_tip;
        for (int64_t i = 0; i < keepDepth; ++i) {
            if (!newRoot->parent()) break;
            newRoot = newRoot->parent();
        }

        if (newRoot == m_root) return; // nic do przycięcia

        // 2. Zbierz wszystkie węzły w poddrzewie newRoot (BFS)
        std::unordered_set<TxPoWTreeNode*> keep;
        {
            std::queue<TxPoWTreeNode*> bfs;
            bfs.push(newRoot);
            while (!bfs.empty()) {
                auto* cur = bfs.front(); bfs.pop();
                keep.insert(cur);
                for (auto* child : cur->children()) bfs.push(child);
            }
        }

        // 3. Usuń węzły spoza poddrzewa
        std::vector<MiniData> toRemove;
        for (auto& [id, node] : m_nodes)
            if (!keep.count(node)) toRemove.push_back(id);
        for (const auto& id : toRemove) {
            delete m_nodes[id];
            m_nodes.erase(id);
        }

        // 4. Odetnij rodzica newRoot (staje się nowym korzeniem)
        newRoot->detachParent();
        m_root = newRoot;

        // 5. Przenumeruj depth od 0 (BFS)
        {
            std::queue<std::pair<TxPoWTreeNode*, int64_t>> bfs;
            bfs.push({m_root, 0});
            while (!bfs.empty()) {
                auto [node, d] = bfs.front(); bfs.pop();
                node->setDepth(d);
                for (auto* child : node->children()) bfs.push({child, d + 1});
            }
        }
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
    std::unordered_map<MiniData, TxPoWTreeNode*, minima::MiniDataHash, minima::MiniDataEqual> m_nodes;
};

} // namespace minima::chain
