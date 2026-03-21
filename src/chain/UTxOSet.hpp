#pragma once
/**
 * UTxOSet — in-memory unspent transaction output set.
 *
 * Keyed by CoinID (MiniData). Thread safety: external.
 */
#include "../objects/Coin.hpp"
#include "../objects/Transaction.hpp"
#include "../types/MiniData.hpp"
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>

namespace minima::chain {

// Hasher / equality for MiniData keys
struct MiniDataHash {
    size_t operator()(const MiniData& d) const {
        size_t h = 0;
        for (auto b : d.bytes())
            h = h * 31 + b;
        return h;
    }
};
struct MiniDataEqual {
    bool operator()(const MiniData& a, const MiniData& b) const {
        return a.bytes() == b.bytes();
    }
};

using CoinMap = std::unordered_map<MiniData, Coin, MiniDataHash, MiniDataEqual>;

class UTxOSet {
public:
    // ── Mutation ─────────────────────────────────────────────────────────────
    void add(const Coin& coin) {
        m_coins[coin.coinID()] = coin;
    }

    bool remove(const MiniData& coinID) {
        return m_coins.erase(coinID) > 0;
    }

    // ── Query ─────────────────────────────────────────────────────────────────
    bool contains(const MiniData& coinID) const {
        return m_coins.count(coinID) > 0;
    }

    std::optional<Coin> get(const MiniData& coinID) const {
        auto it = m_coins.find(coinID);
        if (it == m_coins.end()) return std::nullopt;
        return it->second;
    }

    size_t size() const { return m_coins.size(); }

    // All coins at a given address hash
    std::vector<Coin> coinsAt(const MiniData& addrHash) const {
        std::vector<Coin> result;
        for (auto& [id, c] : m_coins)
            if (c.address().hash().bytes() == addrHash.bytes())
                result.push_back(c);
        return result;
    }

    // ── Block application ─────────────────────────────────────────────────────
    // Returns false (and makes NO changes) if any input is missing
    bool applyBlock(const Transaction& tx) {
        // Check all inputs exist first
        for (const auto& in : tx.inputs())
            if (!contains(in.coinID())) return false;
        // Remove inputs
        for (const auto& in : tx.inputs())
            remove(in.coinID());
        // Add outputs
        for (const auto& out : tx.outputs())
            add(out);
        return true;
    }

    // Undo applyBlock (used during reorg)
    void revertBlock(const Transaction& tx) {
        for (const auto& out : tx.outputs())
            remove(out.coinID());
        for (const auto& in : tx.inputs())
            add(in);
    }

private:
    CoinMap m_coins;
};

} // namespace minima::chain
