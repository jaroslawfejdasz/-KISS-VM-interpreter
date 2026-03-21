#pragma once
/**
 * Mempool — pending transaction pool.
 *
 * Tracks unconfirmed TxPoW units that have passed basic validation but
 * are not yet included in a block.
 *
 * Features:
 *  - Duplicate rejection (same txpowID)
 *  - Double-spend detection (same input coinID in >1 pending tx)
 *  - Capacity cap with FIFO eviction
 *  - onBlockAccepted: removes txns whose inputs were spent by block
 */
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <vector>
#include <optional>
#include <functional>

namespace minima::mempool {

// ── Hashers for MiniData ──────────────────────────────────────────────────────
struct MiniDataHashM {
    size_t operator()(const MiniData& d) const {
        size_t h = 0;
        for (auto b : d.bytes()) h = h * 31 + b;
        return h;
    }
};
struct MiniDataEqualM {
    bool operator()(const MiniData& a, const MiniData& b) const {
        return a.bytes() == b.bytes();
    }
};

class Mempool {
public:
    static constexpr size_t kDefaultCapacity = 10000;

    explicit Mempool(size_t capacity = kDefaultCapacity)
        : m_capacity(capacity) {}

    // ── Add ───────────────────────────────────────────────────────────────────
    // Returns false if duplicate or double-spend detected
    bool add(const TxPoW& tx) {
        MiniData id = tx.computeID();

        // Duplicate check
        if (m_byID.count(id)) return false;

        // Double-spend check (any input already in pool)
        for (const auto& in : tx.body().txn.inputs())
            if (m_spentInPool.count(in.coinID())) return false;

        // Capacity eviction (FIFO — evict oldest)
        if (m_order.size() >= m_capacity) {
            const MiniData& oldest = m_order.front();
            _evict(oldest);
        }

        // Insert
        m_byID.emplace(id, tx);
        m_order.push_back(id);
        for (const auto& in : tx.body().txn.inputs())
            m_spentInPool.emplace(in.coinID());

        return true;
    }

    // ── Remove ────────────────────────────────────────────────────────────────
    bool remove(const MiniData& txID) {
        if (!m_byID.count(txID)) return false;
        _evict(txID);
        return true;
    }

    // ── Query ─────────────────────────────────────────────────────────────────
    bool contains(const MiniData& txID) const {
        return m_byID.count(txID) > 0;
    }

    size_t size() const { return m_byID.size(); }

    // Return up to `limit` pending txns (oldest first)
    std::vector<TxPoW> getPending(size_t limit) const {
        std::vector<TxPoW> result;
        for (const auto& id : m_order) {
            if (result.size() >= limit) break;
            auto it = m_byID.find(id);
            if (it != m_byID.end())
                result.push_back(it->second);
        }
        return result;
    }

    // ── Block acceptance ──────────────────────────────────────────────────────
    // Remove any pending tx that conflicts with the accepted block
    void onBlockAccepted(const TxPoW& block) {
        // Collect coinIDs spent by block
        std::vector<MiniData> spentNow;
        for (const auto& in : block.body().txn.inputs())
            spentNow.push_back(in.coinID());

        // Find and evict any pending tx that spends same coins
        std::vector<MiniData> toRemove;
        for (const auto& [id, tx] : m_byID) {
            for (const auto& in : tx.body().txn.inputs()) {
                bool conflict = false;
                for (const auto& s : spentNow)
                    if (s.bytes() == in.coinID().bytes()) { conflict = true; break; }
                if (conflict) { toRemove.push_back(id); break; }
            }
        }
        for (const auto& id : toRemove) _evict(id);
    }

private:
    size_t m_capacity;

    std::unordered_map<MiniData, TxPoW,  MiniDataHashM, MiniDataEqualM> m_byID;
    std::unordered_set<MiniData,          MiniDataHashM, MiniDataEqualM> m_spentInPool;
    std::deque<MiniData>                                                  m_order;

    void _evict(const MiniData& id) {
        auto it = m_byID.find(id);
        if (it == m_byID.end()) return;

        // Free spent-coin tracking for this tx
        for (const auto& in : it->second.body().txn.inputs())
            m_spentInPool.erase(in.coinID());

        m_byID.erase(it);

        // Remove from ordered queue
        for (auto qit = m_order.begin(); qit != m_order.end(); ++qit) {
            if (qit->bytes() == id.bytes()) {
                m_order.erase(qit);
                break;
            }
        }
    }
};

} // namespace minima::mempool
