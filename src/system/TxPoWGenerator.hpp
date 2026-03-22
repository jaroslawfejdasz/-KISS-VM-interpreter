#pragma once
/**
 * TxPoWGenerator — składa nowy TxPoW do wyminowania.
 * Java ref: src/org/minima/system/brains/TxPoWGenerator.java
 *
 * Wybiera transakcje z mempoola, ustawia superParents,
 * tworzy gotowy TxPoW który miner próbuje wyminować.
 */
#include "../objects/TxPoW.hpp"
#include "../objects/Transaction.hpp"
#include "../objects/Witness.hpp"
#include "../chain/TxPowTree.hpp"
#include "../mempool/Mempool.hpp"
#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"
#include "../crypto/Hash.hpp"
#include <optional>
#include <vector>
#include <chrono>

namespace minima::system {

class TxPoWGenerator {
public:
    /// Maksymalna liczba transakcji w jednym bloku
    static constexpr size_t MAX_TXN_IN_BLOCK = 50;

    TxPoWGenerator(chain::TxPowTree& tree, mempool::Mempool& mempool)
        : m_tree(tree), m_mempool(mempool) {}

    /**
     * Wygeneruj nowy TxPoW gotowy do wyminowania.
     * Jeśli miner_txn jest ustawiony — to "główna" transakcja użytkownika.
     */
    TxPoW generateTxPoW(
        std::optional<Transaction> miner_txn  = std::nullopt,
        std::optional<Witness>     miner_wit  = std::nullopt
    ) const {
        TxPoW txp;

        // ── Header ────────────────────────────────────────────────────────
        auto* tip = m_tree.tip();
        int64_t nextBlock = tip ? (tip->blockNumber() + 1) : 0;
        txp.header().blockNumber = MiniNumber(nextBlock);
        txp.header().timeMilli   = currentTimeMillis();
        txp.header().nonce       = MiniNumber(int64_t(0)); // miner fills this

        // superParents: poziom 0 = bezpośredni rodzic
        if (tip) {
            txp.header().superParents[0] = tip->txPoWID();
            // Poziomy 1-31: logarithmiczne superparents (jak Java)
            fillSuperParents(txp, tip);
        }

        // ── Główna transakcja ─────────────────────────────────────────────
        if (miner_txn.has_value()) {
            txp.body().txn = *miner_txn;
            if (miner_wit.has_value()) txp.body().witness = *miner_wit;
        }

        // ── Transakcje z mempoola ─────────────────────────────────────────
        auto pending = m_mempool.getPending(MAX_TXN_IN_BLOCK);

        // Dodaj TxIDs z mempoola jako "superTxPoW" (transakcje w bloku)
        // W Minima blok zawiera listę TxPoW ID które są potwierdzane
        for (const auto& memTxp : pending) {
            txp.body().txnList.push_back(memTxp.computeID());
        }

        // ── Magic (consensus params) ─────────────────────────────────────
        // Dziedziczymy od rodzica lub ustawiamy defaults
        // (pełna implementacja wymaga MinimaDB — tu uproszczone)

        return txp;
    }

    /**
     * Uproszczona wersja: genesis block (blok #0).
     */
    TxPoW generateGenesis() const {
        TxPoW txp;
        txp.header().blockNumber = MiniNumber(int64_t(0));
        txp.header().timeMilli   = currentTimeMillis();
        txp.header().nonce       = MiniNumber(int64_t(0));
        // superParents dla genesis = wszystkie 0x00...
        for (int i = 0; i < CASCADE_LEVELS; ++i)
            txp.header().superParents[i] = MiniData(std::vector<uint8_t>(32, 0x00));
        return txp;
    }

private:
    /**
     * Wypełnij superParents na wyższych poziomach.
     * Java: co 2^level bloków wstecz, superParents[level] = ID bloku.
     * Uproszczone — wypełniamy na podstawie dostępnych węzłów w drzewie.
     */
    void fillSuperParents(TxPoW& txp, chain::TxPoWTreeNode* tip) const {
        // Level 0 już ustawiony
        chain::TxPoWTreeNode* cur = tip;
        for (int level = 1; level < CASCADE_LEVELS && cur != nullptr; ++level) {
            // Idź 2^(level-1) bloków wstecz
            int steps = (1 << (level - 1));
            chain::TxPoWTreeNode* ancestor = cur;
            for (int s = 0; s < steps && ancestor->parent() != nullptr; ++s)
                ancestor = ancestor->parent();
            txp.header().superParents[level] = ancestor->txPoWID();
            cur = cur->parent();
        }
    }

    static MiniNumber currentTimeMillis() {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return MiniNumber(ms);
    }

    chain::TxPowTree&   m_tree;
    mempool::Mempool&   m_mempool;
};

} // namespace minima::system
