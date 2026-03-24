#pragma once
/**
 * Cascade — Minima chain cascade / pruning structure.
 *
 * Java ref: org.minima.database.cascade.Cascade
 *
 * The cascade keeps a singly-linked list of CascadeNodes (tip → root).
 * As the chain grows, older blocks are promoted to higher cascade levels
 * (each level doubles the weight). This allows nodes to verify chain weight
 * without storing the entire history.
 *
 * Key parameters (from Java GlobalParams):
 *   MINIMA_CASCADE_LEVELS      = 32  (max levels)
 *   MINIMA_CASCADE_LEVEL_NODES = 1   (nodes per level before promoting)
 *
 * Algorithm (cascadeChain):
 *   1. Start at tip (level 0)
 *   2. Walk toward root; if node.superLevel >= currentCascadeLevel → keep it
 *   3. After LEVEL_NODES nodes at a level → increment cascadeLevel
 *   4. Prune nodes whose superLevel < currentCascadeLevel
 *
 * Wire format:
 *   [MiniNumber numNodes][CascadeNode × numNodes from tip to root]
 */

#include "CascadeNode.hpp"
#include "../../types/MiniNumber.hpp"
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <sstream>

namespace minima {
namespace cascade {

// Java GlobalParams — match exactly
static constexpr int CASCADE_LEVELS_MAX  = 32;
static constexpr int CASCADE_LEVEL_NODES = 1;   // nodes per level

class Cascade {
public:
    Cascade() = default;

    // ── Add new block to tip ──────────────────────────────────────────────
    void addToTip(const TxPoW& txpow) {
        auto node = std::make_unique<CascadeNode>(txpow);

        if (!m_nodes.empty()) {
            // new tip's parent is the old tip
            node->setParent(m_nodes.front().get());
        }

        m_nodes.insert(m_nodes.begin(), std::move(node));
        rebuildParentLinks();
        recalcWeight();
    }

    // ── Cascade / prune ───────────────────────────────────────────────────
    // Java-exact: walk from tip toward root, assign levels,
    // drop nodes whose superLevel < current cascade level.
    void cascadeChain() {
        if (m_nodes.empty()) return;

        std::vector<std::unique_ptr<CascadeNode>> kept;

        int cascLevel  = 0;
        int totLevel   = 1;
        m_totalWeight  = 0.0;

        // First node (tip) always kept at level 0
        m_nodes.front()->setLevel(0);
        m_totalWeight += m_nodes.front()->weight();
        kept.push_back(std::move(m_nodes.front()));

        for (size_t i = 1; i < m_nodes.size(); ++i) {
            CascadeNode& cur = *m_nodes[i];
            int superLev = cur.superLevel();

            if (superLev >= cascLevel) {
                cur.setLevel(cascLevel);
                m_totalWeight += cur.weight();
                kept.push_back(std::move(m_nodes[i]));

                totLevel++;
                if (totLevel >= CASCADE_LEVEL_NODES) {
                    if (cascLevel < CASCADE_LEVELS_MAX - 1) {
                        cascLevel++;
                        totLevel = 0;
                    }
                }
            }
            // else: node pruned (superLevel too low for this cascade level)
        }

        m_nodes = std::move(kept);
        rebuildParentLinks();
    }

    // ── Accessors ─────────────────────────────────────────────────────────
    CascadeNode* tip()  {
        return m_nodes.empty() ? nullptr : m_nodes.front().get();
    }
    const CascadeNode* tip() const {
        return m_nodes.empty() ? nullptr : m_nodes.front().get();
    }

    CascadeNode* root() {
        return m_nodes.empty() ? nullptr : m_nodes.back().get();
    }
    const CascadeNode* root() const {
        return m_nodes.empty() ? nullptr : m_nodes.back().get();
    }

    double totalWeight()  const { return m_totalWeight; }
    int    length()       const { return static_cast<int>(m_nodes.size()); }
    bool   empty()        const { return m_nodes.empty(); }

    int64_t tipBlock() const {
        return tip() ? tip()->blockNumber() : -1;
    }
    int64_t rootBlock() const {
        return root() ? root()->blockNumber() : -1;
    }

    // ── Validate cascade integrity (Java: checkCascadeCorrect) ────────────
    bool isValid(std::string* errOut = nullptr) const {
        if (m_nodes.empty()) return true;

        if (m_nodes.front()->level() != 0) {
            if (errOut) *errOut = "Cascade tip not at level 0";
            return false;
        }

        int prevLevel = 0;
        int counter   = 0;

        for (size_t i = 0; i < m_nodes.size(); ++i) {
            int clevel = m_nodes[i]->level();

            if (clevel != prevLevel) {
                if (clevel != prevLevel + 1) {
                    if (errOut) *errOut = "Non-sequential level jump at node " +
                                         std::to_string(i);
                    return false;
                }
                counter   = 0;
                prevLevel = clevel;
            }
            counter++;
        }
        return true;
    }

    // ── Serialise ─────────────────────────────────────────────────────────
    // [MiniNumber len][node0 (tip)][node1]...[nodeN-1 (root)]
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;

        auto lenBytes = MiniNumber(static_cast<int64_t>(m_nodes.size())).serialise();
        out.insert(out.end(), lenBytes.begin(), lenBytes.end());

        for (const auto& n : m_nodes) {
            auto nb = n->serialise();
            out.insert(out.end(), nb.begin(), nb.end());
        }
        return out;
    }

    static Cascade deserialise(const std::vector<uint8_t>& data, size_t& offset) {
        Cascade c;
        int len = static_cast<int>(MiniNumber::deserialise(data, offset).getAsLong());

        for (int i = 0; i < len; ++i) {
            auto node = std::make_unique<CascadeNode>(
                CascadeNode::deserialise(data, offset));
            c.m_nodes.push_back(std::move(node));
        }

        c.rebuildParentLinks();
        c.recalcWeight();
        return c;
    }

    // ── Debug ─────────────────────────────────────────────────────────────
    std::string toString() const {
        std::ostringstream ss;
        ss << "Cascade[len=" << m_nodes.size()
           << " tip=" << tipBlock()
           << " root=" << rootBlock()
           << " weight=" << m_totalWeight << "]\n";
        for (const auto& n : m_nodes) {
            ss << "  " << n->toString() << "\n";
        }
        return ss.str();
    }

private:
    // tip first, root last
    std::vector<std::unique_ptr<CascadeNode>> m_nodes;
    double m_totalWeight = 0.0;

    void rebuildParentLinks() {
        for (size_t i = 0; i + 1 < m_nodes.size(); ++i)
            m_nodes[i]->setParent(m_nodes[i + 1].get());
        if (!m_nodes.empty())
            m_nodes.back()->setParent(nullptr);
    }

    void recalcWeight() {
        m_totalWeight = 0.0;
        for (const auto& n : m_nodes)
            m_totalWeight += n->weight();
    }
};

} // namespace cascade
} // namespace minima
