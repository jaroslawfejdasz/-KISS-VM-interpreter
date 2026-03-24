#pragma once
/**
 * MinimaDB — centralny "God object" bazy danych Minima.
 * Java ref: src/org/minima/database/MinimaDB.java
 *
 * Stan 2026-03-23:
 *   - TxPowTree, BlockStore, ChainState, Mempool, Wallet               ✅
 *   - SQLite persistence (openPersistence, persistBlock, bootstrapFromDB) ✅
 *   - MMRSet (per-block MMR)                                            ✅
 *   - Cascade integration (auto-trim old history)                       ✅ NEW
 *   - MegaMMR integration (global UTxO MMR, checkpoint/rollback)        ✅ NEW
 *
 * Block processing flow (addBlock):
 *   1. txPowTree.addTxPoW()          — add to chain tree
 *   2. chainState.setTip()           — update tip pointer
 *   3. applyBlockToMMR()             — per-block MMR leaf update
 *   4. megaMMR.processBlock()        — global UTxO MMR update
 *   5. megaMMR.checkpoint(blockNum)  — save snapshot for re-org
 *   6. megaMMR.pruneCheckpoints()    — discard old snapshots
 *   7. cascade.addToTip()            — add to cascade
 *   8. cascade.cascadeChain()        — prune old history
 *   (9. txPowTree.pruneBelow()      — deferred to TxPoWProcessor.trimTree)
 *  10. persistBlock()                — optional SQLite write
 */

#include "../chain/TxPowTree.hpp"
#include "../chain/BlockStore.hpp"
#include "../chain/ChainState.hpp"
#include "../chain/cascade/Cascade.hpp"
#include "../mmr/MMRSet.hpp"
#include "../mmr/MegaMMR.hpp"
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
#include <unordered_map>

using minima::mempool::Mempool;

namespace minima {

// How many block checkpoints MegaMMR keeps (for re-org recovery).
static constexpr int MEGAMMR_CHECKPOINT_DEPTH = 64;

class MinimaDB {
public:
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Construction
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    MinimaDB()
        : m_txPowTree(std::make_unique<chain::TxPowTree>())
        , m_blockStore(std::make_unique<chain::BlockStore>())
        , m_chainState(std::make_unique<chain::ChainState>())
        , m_mmrSet(std::make_unique<MMRSet>())
        , m_megaMMR(std::make_unique<MegaMMR>())
        , m_cascade(std::make_unique<cascade::Cascade>())
        , m_mempool(std::make_unique<Mempool>())
        , m_wallet(std::make_unique<Wallet>())
        , m_persistEnabled(false)
    {}

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Persistence API
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /**
     * Open SQLite database and restore state from disk.
     * Call once at node startup.
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
     * Replay TxPowTree from SQLite on node restart.
     * Java ref: MinimaDB.loadDB() + TxPoWProcessor.reBuildTransactionDB()
     */
    void bootstrapFromDB() {
        if (!m_persistEnabled || !m_pbs) return;
        if (m_pbs->count() == 0) return;

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
        // Rebuild both MMRs after replay
        rebuildMMR();
        rebuildMegaMMR();
        // Rebuild cascade from canonical chain
        rebuildCascade();
    }

    /**
     * Persist a block to SQLite.
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
                for (const auto& coin : newCoins)
                    m_utxo->put(coin, blockNum);
                for (const auto& coinID : spentCoinIDs)
                    m_utxo->markSpent(coinID.toHexString(false), blockNum);
            }
            m_db->commit();
        } catch (...) {
            m_db->rollback();
            throw;
        }
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Core block processing
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /**
     * Add a block to the chain. Runs the full processing pipeline:
     *   TxPowTree → MMRSet → MegaMMR (checkpoint) → Cascade (prune) → TxPowTree prune
     *
     * Returns false if the block was already known or rejected.
     */
    bool addBlock(const TxPoW& txpow) {
        // 1. Chain tree
        bool added = m_txPowTree->addTxPoW(txpow);
        if (!added) return false;

        MiniData id      = txpow.computeID();
        int64_t  blockN  = txpow.header().blockNumber.getAsLong();
        m_blockStore->add(id, txpow);

        // 2. Chain state tip
        auto* tip = m_txPowTree->tip();
        if (tip) m_chainState->setTip(tip->txPoWID(), tip->blockNumber());

        // 3. Per-block MMRSet (legacy / script verification context)
        applyBlockToMMR(txpow);

        // 4. MegaMMR: update global UTxO state
        {
            std::vector<MiniData> spentIDs;
            std::vector<Coin>     newCoins;
            for (const auto& c : txpow.body().txn.inputs())  spentIDs.push_back(c.coinID());
            for (const auto& c : txpow.body().txn.outputs()) newCoins.push_back(c);
            m_megaMMR->processBlock(spentIDs, newCoins);
        }

        // 5. MegaMMR checkpoint (enables rollback on re-org)
        m_megaMMR->checkpoint(MiniNumber(blockN));

        // 6. MegaMMR prune old checkpoints
        m_megaMMR->pruneCheckpoints(MiniNumber(blockN), MEGAMMR_CHECKPOINT_DEPTH);

        // 7. Cascade: add block to tip, then prune old history
        m_cascade->addToTip(txpow);
        m_cascade->cascadeChain();

        // Note: TxPowTree pruning (pruneBelow) is intentionally NOT called here.
        // Java ref: TxPoWProcessor.trimTree() handles this carefully after
        // confirming re-org window has passed. Calling pruneBelow() immediately
        // after addBlock() would invalidate parent pointers in the tree and
        // cause dangling pointer access in canonicalChain().
        // The Cascade already tracks which history is prunable at the wire level.

        return true;
    }

    /**
     * Add block + optionally persist to SQLite.
     */
    bool addBlockWithPersist(const TxPoW& txpow,
                             const std::vector<Coin>& newCoins = {},
                             const std::vector<MiniData>& spentCoinIDs = {}) {
        if (!addBlock(txpow)) return false;
        if (m_persistEnabled) persistBlock(txpow, newCoins, spentCoinIDs);
        return true;
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Re-org support
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /**
     * Roll back MegaMMR to a saved checkpoint at blockNum.
     * Used when the canonical chain changes (re-org).
     * Returns true if the checkpoint existed.
     */
    bool rollbackMegaMMR(int64_t blockNum) {
        return m_megaMMR->rollback(MiniNumber(blockNum));
    }

    /**
     * Verify our MegaMMR root matches the block's declared mmrRoot.
     * Java ref: TxPoWChecker.checkBlockMMR(TxPoW, MegaMMR).
     */
    bool verifyMegaMMRRoot(const MiniData& expectedRoot) const {
        return m_megaMMR->verifyRoot(expectedRoot);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Accessors
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    chain::TxPowTree&         txPowTree()   { return *m_txPowTree; }
    chain::BlockStore&        blockStore()  { return *m_blockStore; }
    chain::ChainState&        chainState()  { return *m_chainState; }
    MMRSet&                   mmrSet()      { return *m_mmrSet; }
    MegaMMR&                  megaMMR()     { return *m_megaMMR; }
    cascade::Cascade&         cascade()     { return *m_cascade; }
    Mempool&                  mempool()     { return *m_mempool; }
    Wallet&                   wallet()      { return *m_wallet; }

    const MegaMMR&        megaMMR()  const { return *m_megaMMR; }
    const cascade::Cascade& cascade() const { return *m_cascade; }

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

    persistence::BlockStore* persistentBlockStore() {
        return m_persistEnabled ? m_pbs.get() : nullptr;
    }
    persistence::UTxOStore* utxoStore() {
        return m_persistEnabled ? m_utxo.get() : nullptr;
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Rebuild helpers
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /** Rebuild per-block MMRSet from canonical chain. */
    void rebuildMMR() {
        m_mmrSet = std::make_unique<MMRSet>();
        m_coinToMMREntry.clear();
        for (const auto* node : m_txPowTree->canonicalChain())
            applyBlockToMMR(node->txpow());
    }

    /** Rebuild MegaMMR from canonical chain (called after bootstrap or re-org). */
    void rebuildMegaMMR() {
        m_megaMMR->reset();
        for (const auto* node : m_txPowTree->canonicalChain()) {
            const TxPoW& txp = node->txpow();
            int64_t blockN = txp.header().blockNumber.getAsLong();

            std::vector<MiniData> spentIDs;
            std::vector<Coin>     newCoins;
            for (const auto& c : txp.body().txn.inputs())  spentIDs.push_back(c.coinID());
            for (const auto& c : txp.body().txn.outputs()) newCoins.push_back(c);

            m_megaMMR->processBlock(spentIDs, newCoins);
            m_megaMMR->checkpoint(MiniNumber(blockN));
        }
        int64_t tip = currentHeight();
        if (tip >= 0)
            m_megaMMR->pruneCheckpoints(MiniNumber(tip), MEGAMMR_CHECKPOINT_DEPTH);
    }

    /** Rebuild Cascade from canonical chain (called after bootstrap). */
    void rebuildCascade() {
        m_cascade = std::make_unique<cascade::Cascade>();
        for (const auto* node : m_txPowTree->canonicalChain())
            m_cascade->addToTip(node->txpow());
        m_cascade->cascadeChain();
    }

    /** Full reset — clears all in-memory state. */
    void reset() {
        m_txPowTree->clear();
        m_chainState  = std::make_unique<chain::ChainState>();
        m_mmrSet      = std::make_unique<MMRSet>();
        m_megaMMR     = std::make_unique<MegaMMR>();
        m_cascade     = std::make_unique<cascade::Cascade>();
        m_coinToMMREntry.clear();
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Genesis
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    bool initGenesis() {
        if (m_txPowTree->size() > 0) return false;
        TxPoW genesis = makeGenesisTxPoW();
        bool ok = addBlock(genesis);
        if (ok && m_persistEnabled) persistBlock(genesis);
        return ok;
    }

private:
    // ── Per-block MMRSet update ────────────────────────────────────────────
    void applyBlockToMMR(const TxPoW& txpow) {
        // Java ref: MinimaDB.updateMMR(TxPoW)
        const auto& txn = txpow.body().txn;

        for (const auto& coin : txn.outputs()) {
            MiniData coinHash = coin.hashValue();
            MMRData  leaf(coinHash, coin.amount(), false);
            MMREntry entry = m_mmrSet->addLeaf(leaf);
            m_coinToMMREntry[coin.coinID().toHexString(false)] = entry.getEntry();
        }

        for (const auto& coin : txn.inputs()) {
            auto it = m_coinToMMREntry.find(coin.coinID().toHexString(false));
            if (it != m_coinToMMREntry.end()) {
                MiniData coinHash = coin.hashValue();
                MMRData  spent(coinHash, coin.amount(), true);
                m_mmrSet->updateLeaf(it->second, spent);
            }
        }
    }

    // ── In-memory state ────────────────────────────────────────────────────
    std::unordered_map<std::string, uint64_t> m_coinToMMREntry;

    std::unique_ptr<chain::TxPowTree>    m_txPowTree;
    std::unique_ptr<chain::BlockStore>   m_blockStore;
    std::unique_ptr<chain::ChainState>   m_chainState;
    std::unique_ptr<MMRSet>              m_mmrSet;
    std::unique_ptr<MegaMMR>             m_megaMMR;
    std::unique_ptr<cascade::Cascade>    m_cascade;
    std::unique_ptr<Mempool>             m_mempool;
    std::unique_ptr<Wallet>              m_wallet;

    // ── Persistent state ────────────────────────────────────────────────────
    bool m_persistEnabled;
    std::unique_ptr<persistence::Database>   m_db;
    std::unique_ptr<persistence::BlockStore> m_pbs;
    std::unique_ptr<persistence::UTxOStore>  m_utxo;
};

} // namespace minima
