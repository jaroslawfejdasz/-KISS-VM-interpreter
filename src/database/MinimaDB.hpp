#pragma once
/**
 * MinimaDB — centralny "God object" bazy danych Minima.
 * Java ref: src/org/minima/database/MinimaDB.java
 *
 * Nowe funkcje (2026-03-23):
 *   - openPersistence(path) — otwiera SQLite i przywraca stan
 *   - bootstrapFromDB()     — replay TxPowTree z SQLite po restarcie
 *   - persistBlock(txpow)   — zapisuje blok do SQLite
 *   - rebuildMMR()          — przebudowuje MMR na bazie canonicalChain
 */
#include "../chain/TxPowTree.hpp"
#include "../chain/BlockStore.hpp"
#include "../chain/ChainState.hpp"
#include "../mmr/MMRSet.hpp"
#include "../mempool/Mempool.hpp"
#include "../wallet/Wallet.hpp"
#include "../objects/TxPoW.hpp"
#include "../objects/TxBlock.hpp"
#include "../objects/Coin.hpp"
#include "../objects/Genesis.hpp"
#include "../types/MiniData.hpp"
#include "../persistence/Database.hpp"
#include "../persistence/BlockStore.hpp"
#include "../persistence/UTxOStore.hpp"
#include <memory>
#include <optional>
#include <vector>
#include <string>

using minima::mempool::Mempool;

namespace minima {

class MinimaDB {
public:
    // ── Constructors ──────────────────────────────────────────────────────

    MinimaDB()
        : m_txPowTree(std::make_unique<chain::TxPowTree>())
        , m_blockStore(std::make_unique<chain::BlockStore>())
        , m_chainState(std::make_unique<chain::ChainState>())
        , m_mmrSet(std::make_unique<MMRSet>())
        , m_mempool(std::make_unique<Mempool>())
        , m_wallet(std::make_unique<Wallet>())
        , m_persistEnabled(false)
    {}

    // ── Persistence API ───────────────────────────────────────────────────

    /**
     * Otwórz bazę SQLite i przywróć stan z dysku.
     * Wywołaj raz na starcie węzła.
     */
    bool openPersistence(const std::string& dbPath) {
        try {
            m_db   = std::make_unique<persistence::Database>(dbPath);
            m_pbs  = std::make_unique<persistence::BlockStore>(*m_db);
            m_utxo = std::make_unique<persistence::UTxOStore>(*m_db);
            m_persistEnabled = true;
            bootstrapFromDB();
            return true;
        } catch (const std::exception&) {
            m_persistEnabled = false;
            return false;
        }
    }

    bool isPersistenceEnabled() const { return m_persistEnabled; }

    /**
     * Replay TxPowTree z SQLite.
     * Java ref: MinimaDB.loadDB() + TxPoWProcessor.reBuildTransactionDB()
     */
    void bootstrapFromDB() {
        if (!m_persistEnabled || !m_pbs) return;
        if (m_pbs->count() == 0) return;

        // Wczytaj wszystkie bloki rosnąco po block_number
        auto blocks = m_pbs->getRange(0, INT64_MAX);

        for (const auto& tb : blocks) {
            const TxPoW& txp = tb.txpow();
            bool added = m_txPowTree->addTxPoW(txp);
            if (added) {
                m_blockStore->add(txp.computeID(), txp);
                auto* tip = m_txPowTree->tip();
                if (tip) m_chainState->setTip(tip->txPoWID(), tip->blockNumber());
            }
        }
        // Przebuduj MMR po replaying
        rebuildMMR();
    }

    /**
     * Zapisz blok do SQLite (persistence).
     */
    void persistBlock(const TxPoW& txpow,
                      const std::vector<Coin>& newCoins = {},
                      const std::vector<MiniData>& spentCoinIDs = {}) {
        if (!m_persistEnabled || !m_pbs) return;

        TxBlock tb(txpow);
        for (const auto& c : newCoins) tb.addNewCoin(c);

        m_db->beginTransaction();
        try {
            m_pbs->put(tb);
            if (m_utxo) {
                int64_t blockNum = txpow.header().blockNumber.getAsLong();
                for (const auto& coin : newCoins) {
                    m_utxo->put(coin, blockNum);
                }
                for (const auto& coinID : spentCoinIDs) {
                    m_utxo->markSpent(coinID.toHexString(false), blockNum);
                }
            }
            m_db->commit();
        } catch (...) {
            m_db->rollback();
            throw;
        }
    }

    // ── Core DB API ───────────────────────────────────────────────────────

    chain::TxPowTree&   txPowTree()   { return *m_txPowTree; }
    chain::BlockStore&  blockStore()  { return *m_blockStore; }
    chain::ChainState&  chainState()  { return *m_chainState; }
    MMRSet&             mmrSet()      { return *m_mmrSet; }
    Mempool&            mempool()     { return *m_mempool; }
    Wallet&             wallet()      { return *m_wallet; }

    bool addBlock(const TxPoW& txpow) {
        MiniData id = txpow.computeID();
        bool added = m_txPowTree->addTxPoW(txpow);
        if (!added) return false;
        m_blockStore->add(id, txpow);
        auto* tip = m_txPowTree->tip();
        if (tip) m_chainState->setTip(tip->txPoWID(), tip->blockNumber());
        return true;
    }

    /**
     * Dodaj blok + opcjonalnie persystuj do SQLite.
     */
    bool addBlockWithPersist(const TxPoW& txpow,
                             const std::vector<Coin>& newCoins = {},
                             const std::vector<MiniData>& spentCoinIDs = {}) {
        if (!addBlock(txpow)) return false;
        if (m_persistEnabled) {
            persistBlock(txpow, newCoins, spentCoinIDs);
        }
        return true;
    }

    std::optional<TxPoW> currentTip() const {
        auto* node = m_txPowTree->tip();
        if (!node) return std::nullopt;
        return node->txpow();
    }

    int64_t currentHeight() const { return m_chainState->getHeight(); }

    std::vector<TxPoW> canonicalChain() const {
        auto nodes = m_txPowTree->canonicalChain();
        std::vector<TxPoW> chain;
        chain.reserve(nodes.size());
        for (auto* n : nodes) chain.push_back(n->txpow());
        return chain;
    }

    bool hasBlock(const MiniData& id) const { return m_blockStore->has(id); }
    std::optional<TxPoW> getBlock(const MiniData& id) const { return m_blockStore->get(id); }

    /**
     * Przebuduj MMR na bazie canonicalChain.
     * Java ref: MinimaDB.buildNewMMR()
     */
    void rebuildMMR() {
        m_mmrSet = std::make_unique<MMRSet>();
        auto chain = m_txPowTree->canonicalChain();
        for (const auto* node : chain) {
            applyBlockToMMR(node->txpow());
        }
    }

    void reset() {
        m_txPowTree->clear();
        m_chainState = std::make_unique<chain::ChainState>();
        m_mmrSet     = std::make_unique<MMRSet>();
    }

    // ── Persistence accessors ─────────────────────────────────────────────

    persistence::BlockStore* persistentBlockStore() {
        return m_persistEnabled ? m_pbs.get() : nullptr;
    }

    persistence::UTxOStore* utxoStore() {
        return m_persistEnabled ? m_utxo.get() : nullptr;
    }

    // ── Genesis ───────────────────────────────────────────────────────────

    bool initGenesis() {
        if (m_txPowTree->size() > 0) return false;
        TxPoW genesis = makeGenesisTxPoW();
        bool ok = addBlock(genesis);
        if (ok && m_persistEnabled) {
            persistBlock(genesis);
        }
        return ok;
    }

private:
    void applyBlockToMMR(const TxPoW& txpow) {
        MiniData blockID = txpow.computeID();
        MMRData data(blockID);
        m_mmrSet->addLeaf(data);
    }

    // ── In-memory state ───────────────────────────────────────────────────
    std::unique_ptr<chain::TxPowTree>  m_txPowTree;
    std::unique_ptr<chain::BlockStore> m_blockStore;
    std::unique_ptr<chain::ChainState> m_chainState;
    std::unique_ptr<MMRSet>            m_mmrSet;
    std::unique_ptr<Mempool>           m_mempool;
    std::unique_ptr<Wallet>            m_wallet;

    // ── Persistent state ─────────────────────────────────────────────────
    bool m_persistEnabled;
    std::unique_ptr<persistence::Database>   m_db;
    std::unique_ptr<persistence::BlockStore> m_pbs;
    std::unique_ptr<persistence::UTxOStore>  m_utxo;
};

} // namespace minima
