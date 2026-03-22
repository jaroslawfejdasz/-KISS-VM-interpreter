#pragma once
/**
 * TxPoWSearcher — przeszukuje TxPowTree.
 * Java ref: src/org/minima/system/brains/TxPoWSearcher.java
 *
 * Odpowiada na pytania:
 * - "Czy ta transakcja jest już w łańcuchu?"
 * - "Znajdź TxPoW po ID"
 * - "Czy ten coin jest wydany?"
 * - "Historia łańcucha od tip do depth N"
 */
#include "../chain/TxPowTree.hpp"
#include "../chain/BlockStore.hpp"
#include "../objects/TxPoW.hpp"
#include "../objects/Coin.hpp"
#include "../types/MiniData.hpp"
#include <optional>
#include <vector>

namespace minima::system {

class TxPoWSearcher {
public:
    TxPoWSearcher(chain::TxPowTree& tree, chain::BlockStore& store)
        : m_tree(tree), m_store(store) {}

    /// Czy TxPoW o danym ID jest w łańcuchu?
    bool isInChain(const MiniData& txpowID) const {
        if (!m_tree.has(txpowID)) return false;
        // Sprawdź czy jest na ścieżce canonical chain
        auto* node = m_tree.getNode(txpowID);
        if (!node) return false;
        // Idź od tip do root — O(depth) ale wystarczy
        auto chain = m_tree.canonicalChain();
        for (auto* n : chain)
            if (n->txPoWID().bytes() == txpowID.bytes()) return true;
        return false;
    }

    /// Pobierz TxPoW po ID (z BlockStore)
    std::optional<TxPoW> getTxPoW(const MiniData& id) const {
        return m_store.get(id);
    }

    /// Tip łańcucha
    std::optional<TxPoW> getTip() const {
        auto* node = m_tree.tip();
        if (!node) return std::nullopt;
        return node->txpow();
    }

    int64_t getHeight() const {
        auto* node = m_tree.tip();
        if (!node) return 0;
        return node->blockNumber();
    }

    /// Pobierz N ostatnich bloków (od tip w tył)
    std::vector<TxPoW> getLastBlocks(size_t count) const {
        auto chain = m_tree.canonicalChain();
        std::vector<TxPoW> result;
        size_t start = (chain.size() > count) ? chain.size() - count : 0;
        for (size_t i = start; i < chain.size(); ++i)
            result.push_back(chain[i]->txpow());
        return result;
    }

    /// Sprawdź czy coin (wg coinID) jest wydany w łańcuchu
    bool isCoinSpent(const MiniData& coinID) const {
        auto chain = m_tree.canonicalChain();
        for (auto* node : chain) {
            for (const auto& coin : node->txpow().body().txn.inputs()) {
                if (coin.coinID().bytes() == coinID.bytes()) return true;
            }
        }
        return false;
    }

    /// Sprawdź czy coin jest w UTXO (output łańcucha, nie wydany)
    bool isCoinUnspent(const MiniData& coinID) const {
        auto chain = m_tree.canonicalChain();
        // Zbierz wszystkie outputs
        std::vector<MiniData> outputs;
        for (auto* node : chain)
            for (const auto& coin : node->txpow().body().txn.outputs())
                outputs.push_back(coin.coinID());
        // Zbierz wszystkie inputs (spendowane)
        std::vector<MiniData> inputs;
        for (auto* node : chain)
            for (const auto& coin : node->txpow().body().txn.inputs())
                inputs.push_back(coin.coinID());
        // coinID musi być w outputs ale nie w inputs
        bool inOutputs = false;
        for (const auto& id : outputs)
            if (id.bytes() == coinID.bytes()) { inOutputs = true; break; }
        if (!inOutputs) return false;
        for (const auto& id : inputs)
            if (id.bytes() == coinID.bytes()) return false;
        return true;
    }

    /// Znajdź blok po wysokości
    std::optional<TxPoW> getBlockAtHeight(int64_t height) const {
        auto chain = m_tree.canonicalChain();
        for (auto* node : chain)
            if (node->blockNumber() == height) return node->txpow();
        return std::nullopt;
    }

private:
    chain::TxPowTree&   m_tree;
    chain::BlockStore&  m_store;
};

} // namespace minima::system
