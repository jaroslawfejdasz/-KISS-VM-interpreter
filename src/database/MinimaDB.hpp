#pragma once
/**
 * MinimaDB — centralny "God object" bazy danych Minima.
 * Java ref: src/org/minima/database/MinimaDB.java
 */
#include "../chain/TxPowTree.hpp"
#include "../chain/BlockStore.hpp"
#include "../chain/ChainState.hpp"
#include "../mmr/MMRSet.hpp"
#include "../mempool/Mempool.hpp"
#include "../wallet/Wallet.hpp"
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include <memory>
#include <optional>
#include <vector>

using minima::mempool::Mempool;

namespace minima {

class MinimaDB {
public:
    MinimaDB()
        : m_txPowTree(std::make_unique<chain::TxPowTree>())
        , m_blockStore(std::make_unique<chain::BlockStore>())
        , m_chainState(std::make_unique<chain::ChainState>())
        , m_mmrSet(std::make_unique<MMRSet>())
        , m_mempool(std::make_unique<Mempool>())
        , m_wallet(std::make_unique<Wallet>())
    {}

    chain::TxPowTree&   txPowTree()   { return *m_txPowTree; }
    chain::BlockStore&  blockStore()  { return *m_blockStore; }
    chain::ChainState&  chainState()  { return *m_chainState; }
    MMRSet&             mmrSet()      { return *m_mmrSet; }
    Mempool&            mempool()     { return *m_mempool; }
    Wallet&             wallet()      { return *m_wallet; }

    bool addBlock(const TxPoW& txpow) {
        MiniData id = txpow.computeID();
        m_blockStore->add(id, txpow);
        bool added = m_txPowTree->addTxPoW(txpow);
        if (!added) return false;
        auto* tip = m_txPowTree->tip();
        if (tip) m_chainState->setTip(tip->txPoWID(), tip->blockNumber());
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

    void reset() {
        m_txPowTree->clear();
        m_chainState = std::make_unique<chain::ChainState>();
        m_mmrSet     = std::make_unique<MMRSet>();
    }

private:
    std::unique_ptr<chain::TxPowTree>  m_txPowTree;
    std::unique_ptr<chain::BlockStore> m_blockStore;
    std::unique_ptr<chain::ChainState> m_chainState;
    std::unique_ptr<MMRSet>            m_mmrSet;
    std::unique_ptr<Mempool>           m_mempool;
    std::unique_ptr<Wallet>            m_wallet;
};

} // namespace minima
