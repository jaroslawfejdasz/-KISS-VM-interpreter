#pragma once
/**
 * ChainProcessor — validates and integrates blocks into the chain.
 * Java ref: src/org/minima/system/brains/TxPoWProcessor.java
 */
#include "BlockStore.hpp"
#include "ChainState.hpp"
#include "UTxOSet.hpp"
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include "../chain/cascade/Cascade.hpp"
#include <vector>

namespace minima::chain {

class ChainProcessor {
public:
    ChainProcessor() = default;

    /**
     * Process a TxPoW block.
     * Returns false if: duplicate or invalid.
     */
    bool processBlock(const TxPoW& txpow) {
        MiniData id = txpow.computeID();
        if (m_store.has(id)) return false;

        int64_t bn = txpow.header().blockNumber.getAsLong();
        m_store.add(id, txpow);

        if (bn >= m_state.getHeight())
            m_state.setTip(id, bn);

        applyCoins(txpow);
        return true;
    }

    /**
     * Bootstrap chain state from a Cascade received in IBD.
     * Java ref: TxPoWProcessor.processCascade()
     *
     * The cascade provides historical blocks (pruned to headers).
     * We jump the chain height to the cascade tip so IBD continues
     * syncing forward from there.
     */
    void applyCascade(const cascade::Cascade& casc) {
        if (casc.empty()) return;

        const auto* tip  = casc.tip();
        const auto* root = casc.root();
        if (!tip || !root) return;

        int64_t cascadeTip  = tip->blockNumber();
        int64_t cascadeRoot = root->blockNumber();

        // Only bootstrap if cascade is ahead of our current tip
        if (m_state.getHeight() < cascadeTip) {
            // Synthetic TxPoW from cascade tip header for a valid ID
            TxPoW syntheticTip;
            syntheticTip.header() = tip->header();
            MiniData id = syntheticTip.computeID();
            m_store.add(id, syntheticTip);
            m_state.setTip(id, cascadeTip);
            m_cascadeRoot = cascadeRoot;
        }
    }

    int64_t  getHeight()           const { return m_state.getHeight(); }
    MiniData getTip()              const { return m_state.getTip(); }
    int64_t  cascadeRoot()         const { return m_cascadeRoot; }
    bool     hasCascadeBootstrap() const { return m_cascadeRoot >= 0; }

    BlockStore& blockStore() { return m_store; }
    UTxOSet&    utxoSet()    { return m_utxo; }

private:
    BlockStore  m_store;
    ChainState  m_state;
    UTxOSet     m_utxo;
    int64_t     m_cascadeRoot = -1;

    void applyCoins(const TxPoW& txpow) {
        for (const auto& coin : txpow.body().txn.inputs())
            m_utxo.spendCoin(coin.coinID());
        for (const auto& coin : txpow.body().txn.outputs())
            m_utxo.addCoin(coin);
    }
};

} // namespace minima::chain
