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
#include <vector>

namespace minima::chain {

class ChainProcessor {
public:
    ChainProcessor() = default;

    /**
     * Process a TxPoW block.
     * Returns false if: duplicate, or invalid.
     */
    bool processBlock(const TxPoW& txpow) {
        MiniData id = txpow.computeID();

        // Reject duplicates
        if (m_store.has(id)) return false;

        int64_t bn = txpow.header().blockNumber.getAsLong();

        // Store it
        m_store.add(id, txpow);

        // Update tip if this extends the chain
        if (bn >= m_state.getHeight()) {
            m_state.setTip(id, bn);
        }

        // Apply coins: add outputs, remove inputs
        applyCoins(txpow);

        return true;
    }

    int64_t       getHeight()   const { return m_state.getHeight(); }
    MiniData      getTip()      const { return m_state.getTip(); }
    BlockStore&   blockStore()        { return m_store; }
    UTxOSet&      utxoSet()           { return m_utxo; }

private:
    BlockStore  m_store;
    ChainState  m_state;
    UTxOSet     m_utxo;

    void applyCoins(const TxPoW& txpow) {
        // Spend inputs
        for (const auto& coin : txpow.body().txn.inputs())
            m_utxo.spendCoin(coin.coinID());
        // Add outputs
        for (const auto& coin : txpow.body().txn.outputs())
            m_utxo.addCoin(coin);
    }
};

} // namespace minima::chain
