#pragma once
/**
 * TxPoWTreeNode — węzeł drzewa TxPoW.
 * Java ref: src/org/minima/database/txpowtree/TxPoWTreeNode.java
 */
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include <vector>

namespace minima::chain {

class TxPoWTreeNode {
public:
    explicit TxPoWTreeNode(const TxPoW& txpow)
        : m_txpow(txpow)
        , m_id(txpow.computeID())
        , m_parent(nullptr)
        , m_depth(0)
    {}

    const TxPoW&          txpow()   const { return m_txpow; }
    const MiniData&       txPoWID() const { return m_id; }
    TxPoWTreeNode*        parent()  const { return m_parent; }
    int64_t               depth()   const { return m_depth; }
    int64_t               blockNumber() const { return m_txpow.header().blockNumber.getAsLong(); }

    MiniNumber chainWeight() const { return m_chainWeight; }
    void       setChainWeight(const MiniNumber& w) { m_chainWeight = w; }

    void addChild(TxPoWTreeNode* child) {
        child->m_parent = this;
        child->m_depth  = m_depth + 1;
        m_children.push_back(child);
    }

    const std::vector<TxPoWTreeNode*>& children() const { return m_children; }
    bool isLeaf() const { return m_children.empty(); }

    /// Odetnij od rodzica (dla trimTree — nowy korzeń)
    void detachParent() {
        m_parent = nullptr;
        m_depth  = 0;
    }

    /// Ręcznie ustaw depth (dla przenumerowania po trimTree)
    void setDepth(int64_t d) { m_depth = d; }

    TxPoWTreeNode* heaviestChild() const {
        if (m_children.empty()) return nullptr;
        TxPoWTreeNode* best = m_children[0];
        for (auto* c : m_children)
            if (c->m_chainWeight.isMore(best->m_chainWeight))
                best = c;
        return best;
    }

private:
    TxPoW                       m_txpow;
    MiniData                    m_id;
    TxPoWTreeNode*              m_parent;
    std::vector<TxPoWTreeNode*> m_children;
    int64_t                     m_depth;
    MiniNumber                  m_chainWeight;
};

} // namespace minima::chain
