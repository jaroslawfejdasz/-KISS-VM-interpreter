#pragma once
/**
 * CascadeNode — one node in the Minima chain cascade.
 *
 * Java ref: org.minima.database.cascade.CascadeNode
 *
 * Each node wraps a TxPoW header (body cleared) and tracks:
 *   - m_currentLevel : cascade level (0 = fresh, increases as chain grows)
 *   - m_superLevel   : superLevel of the underlying TxPoW
 *
 * Weight formula (Java-exact):
 *   weight = txpow.weight * 2^currentLevel
 *
 * Wire format:
 *   [TxHeader serialise][MiniNumber currentLevel]
 */

#include "../../objects/TxPoW.hpp"
#include "../../objects/TxHeader.hpp"
#include "../../types/MiniNumber.hpp"
#include "../../types/MiniData.hpp"
#include <cmath>
#include <string>
#include <vector>
#include <cstdint>

namespace minima {
namespace cascade {

class CascadeNode {
public:
    // ── Construct from a live TxPoW ───────────────────────────────────────
    explicit CascadeNode(const TxPoW& txpow)
        : m_currentLevel(0)
        , m_superLevel(txpow.getSuperLevel())
        , m_parent(nullptr)
    {
        // Store header only — body is not persisted in cascade
        m_header = txpow.header();
    }

    // ── For deserialisation ───────────────────────────────────────────────
    CascadeNode()
        : m_currentLevel(0)
        , m_superLevel(0)
        , m_parent(nullptr)
    {}

    // ── Serialise / deserialise ───────────────────────────────────────────
    std::vector<uint8_t> serialise() const {
        auto out = m_header.serialise();
        auto lvl = MiniNumber(static_cast<int64_t>(m_currentLevel)).serialise();
        out.insert(out.end(), lvl.begin(), lvl.end());
        return out;
    }

    static CascadeNode deserialise(const std::vector<uint8_t>& data, size_t& offset) {
        CascadeNode node;
        node.m_header       = TxHeader::deserialise(data, offset);
        auto lvl            = MiniNumber::deserialise(data, offset);
        node.m_currentLevel = static_cast<int>(lvl.getAsLong());
        // Derive superLevel from header (count leading equal superParent slots)
        node.m_superLevel   = deriveSuperLevel(node.m_header);
        return node;
    }

    // ── Accessors ─────────────────────────────────────────────────────────
    const TxHeader& header() const { return m_header; }
    TxHeader&       header()       { return m_header; }

    int  level()       const { return m_currentLevel; }
    void setLevel(int l)     { m_currentLevel = l; }

    int  superLevel()  const { return m_superLevel; }

    CascadeNode*       parent()       { return m_parent; }
    const CascadeNode* parent() const { return m_parent; }
    void setParent(CascadeNode* p)    { m_parent = p; }

    int64_t blockNumber() const {
        return m_header.blockNumber.getAsLong();
    }

    // ── Weight = difficulty_weight * 2^currentLevel ───────────────────────
    double weight() const {
        double base = difficultyToWeight(m_header.blockDifficulty);
        return base * std::pow(2.0, static_cast<double>(m_currentLevel));
    }

    std::string toString() const {
        return "[" + std::to_string(m_currentLevel) + "/" +
               std::to_string(m_superLevel) + "] " +
               "blk#" + std::to_string(blockNumber()) +
               " w=" + std::to_string(weight());
    }

private:
    TxHeader     m_header;
    int          m_currentLevel;
    int          m_superLevel;
    CascadeNode* m_parent;  // raw ptr — owned by Cascade

    // Count how many leading superParent slots are the same (= superLevel)
    static int deriveSuperLevel(const TxHeader& hdr) {
        if (hdr.superParents.empty()) return 0;
        const MiniData& first = hdr.superParents[0];
        int level = 0;
        for (int i = 1; i < CASCADE_LEVELS; ++i) {
            if (hdr.superParents[i].bytes() == first.bytes() &&
                !first.bytes().empty()) {
                level = i;
            } else {
                break;
            }
        }
        return level;
    }

    static double difficultyToWeight(const MiniData& diff) {
        const auto& b = diff.bytes();
        if (b.empty()) return 1.0;
        int zeros = 0;
        for (uint8_t byte : b) {
            if (byte == 0x00) { zeros += 8; continue; }
            uint8_t mask = 0x80;
            while (mask && !(byte & mask)) { zeros++; mask >>= 1; }
            break;
        }
        return std::pow(2.0, static_cast<double>(zeros));
    }
};

} // namespace cascade
} // namespace minima
