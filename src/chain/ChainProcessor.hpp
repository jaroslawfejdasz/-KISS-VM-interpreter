#pragma once
/**
 * ChainProcessor — ties together UTxOSet, BlockStore, and validation.
 *
 * Responsibilities:
 *   1. Accept new TxPoW blocks
 *   2. Validate via TxPoWValidator (stateless) + UTxO check (stateful)
 *   3. Apply to UTxO set and BlockStore
 *   4. Maintain tip (ChainState)
 *   5. Basic reorg detection (longest chain wins)
 */
#include "ChainState.hpp"
#include "BlockStore.hpp"
#include "UTxOSet.hpp"
#include "../objects/TxPoW.hpp"
#include "../validation/TxPoWValidator.hpp"
#include <string>
#include <optional>

namespace minima::chain {

struct ProcessResult {
    bool        accepted{false};
    std::string reason;   // empty on success
    static ProcessResult ok()                       { return {true,  ""}; }
    static ProcessResult fail(const std::string& r) { return {false, r}; }
};

class ChainProcessor {
public:
    ChainProcessor() = default;

    // ── Accessors ─────────────────────────────────────────────────────────────
    const ChainState& tip()        const { return m_tip; }
    const UTxOSet&    utxos()      const { return m_utxos; }
    const BlockStore& blockStore() const { return m_blocks; }

    // ── Genesis ───────────────────────────────────────────────────────────────
    ProcessResult initGenesis(const TxPoW& genesis) {
        if (!m_tip.isGenesis())
            return ProcessResult::fail("Chain already initialised");

        // Apply genesis outputs directly (no inputs to spend)
        for (const auto& out : genesis.body().txn.outputs())
            m_utxos.add(out);

        m_blocks.store(genesis);
        m_tip.blockNumber = static_cast<uint64_t>(
            genesis.header().blockNumber.getAsLong());
        m_tip.timeMilli   = genesis.header().timestamp;
        return ProcessResult::ok();
    }

    // ── Process a new block ───────────────────────────────────────────────────
    ProcessResult processBlock(const TxPoW& block) {
        // Must qualify as a block
        if (!block.isBlock())
            return ProcessResult::fail("TxPoW does not qualify as a block");

        uint64_t bn = static_cast<uint64_t>(
            block.header().blockNumber.getAsLong());

        // Already known?
        if (m_blocks.has(block.computeID()))
            return ProcessResult::fail("Block already known");

        // Parent must be known (except genesis)
        if (bn > 0 && !m_blocks.has(block.header().parentID))
            return ProcessResult::fail("Unknown parent");

        // Stateful coin validation (are inputs in UTxO?)
        for (const auto& in : block.body().txn.inputs()) {
            if (!m_utxos.contains(in.coinID()))
                return ProcessResult::fail(
                    "Input coin not found in UTxO set: " +
                    in.coinID().toHexString());
        }

        // Stateless validation (PoW, scripts, balance, etc.)
        auto coinLookup = [this](const MiniData& coinID) -> const Coin* {
            auto opt = m_utxos.get(coinID);
            if (!opt) return nullptr;
            // Store locally so we can return a pointer — valid for this call
            m_lookupCache = *opt;
            return &m_lookupCache;
        };

        minima::validation::TxPoWValidator validator(coinLookup);
        auto vr = validator.validate(block);
        if (!vr.valid)
            return ProcessResult::fail("Validation failed: " + vr.error);

        // Apply to UTxO set
        bool applied = m_utxos.applyBlock(block.body().txn);
        if (!applied)
            return ProcessResult::fail("UTxO apply failed (race condition?)");

        // Store block
        m_blocks.store(block);

        // Update tip if this extends the canonical chain
        ChainState candidate;
        candidate.blockNumber = bn;
        candidate.timeMilli   = block.header().timestamp;
        if (m_tip.isBetterThan(candidate))
            m_tip = candidate;

        return ProcessResult::ok();
    }

private:
    ChainState  m_tip;
    UTxOSet     m_utxos;
    BlockStore  m_blocks;
    mutable Coin m_lookupCache;  // scratch space for coinLookup lambda
};

} // namespace minima::chain
