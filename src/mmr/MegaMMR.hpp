#pragma once
/**
 * MegaMMR — Global UTxO state as a single Merkle Mountain Range.
 * Port of com.minima.database.MegaMMR (Minima Java).
 *
 * Purpose:
 *  - Tracks ALL unspent coins as leaves in one global MMR.
 *  - Fast-sync: new nodes receive a MegaMMR snapshot instead of full history.
 *  - Re-org support: checkpoint/rollback at block boundaries.
 *  - Coin proof verification against block's mmrRoot field.
 *
 * Java ref: com.minima.database.MegaMMR
 *
 * Lifecycle per block:
 *  1. processBlock(spentIDs, newCoins)  — update state
 *  2. verifyRoot(block.header.mmrRoot)  — assert our root == block root
 *  3. checkpoint(blockNum)              — save snapshot for re-org
 *  4. pruneCheckpoints(blockNum, 64)    — discard old snapshots
 *
 * Fast-sync wire format (getSyncPacket / applySyncPacket):
 *   [MiniNumber leafCount]
 *   for each leaf: [uint8 spent] [MiniData hash (serialised)]
 *   [MiniNumber indexCount]
 *   for each index: [MiniData coinID] [MiniNumber leafEntry]
 */

#include "MMRSet.hpp"
#include "MMRData.hpp"
#include "MMREntry.hpp"
#include "MMRProof.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../crypto/Hash.hpp"
#include "../serialization/DataStream.hpp"
#include "../objects/Coin.hpp"

#include <unordered_map>
#include <map>
#include <vector>
#include <optional>
#include <stdexcept>

namespace minima {

class MegaMMR {
public:
    MegaMMR() = default;

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Coin ↔ MMR leaf hashing
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /**
     * Compute the MMRData for a Coin.
     * leaf_hash = SHA3_256(coinID_bytes || address_bytes || amount_string)
     * Matches Java: MMRSet.getMMRData(Coin).
     */
    static MMRData coinToMMRData(const Coin& coin) {
        std::vector<uint8_t> buf;
        const auto& cid  = coin.coinID().bytes();
        const auto& addr = coin.address().hash().bytes();
        std::string amt  = coin.amount().toString();
        buf.insert(buf.end(), cid.begin(),  cid.end());
        buf.insert(buf.end(), addr.begin(), addr.end());
        buf.insert(buf.end(), amt.begin(),  amt.end());
        MiniData h = crypto::Hash::sha3_256(buf.data(), buf.size());
        return MMRData(h, coin.amount(), false);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Mutation
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /** Add an unspent coin. Returns leaf entry index. */
    uint64_t addCoin(const Coin& coin) {
        MMRData  d   = coinToMMRData(coin);
        MMREntry e   = m_mmr.addLeaf(d);
        uint64_t idx = e.getEntry();
        m_idx[coin.coinID()] = idx;
        return idx;
    }

    /**
     * Mark a coin as spent.
     * Leaf stays in MMR (spent is metadata), coin remains in index for auditing.
     * Returns false if coinID not tracked.
     */
    bool spendCoin(const MiniData& coinID) {
        auto it = m_idx.find(coinID);
        if (it == m_idx.end()) return false;
        auto optE = m_mmr.getEntry(0, it->second);
        if (!optE) return false;
        MMRData d = optE->getData();
        d.setSpent(true);
        m_mmr.updateLeaf(it->second, d);
        return true;
    }

    /**
     * Apply a block's state change: spend inputs, add outputs.
     * Java ref: MegaMMR.processBlock(TxBlock).
     */
    void processBlock(const std::vector<MiniData>& spentIDs,
                      const std::vector<Coin>&     newCoins) {
        for (const auto& id : spentIDs)  spendCoin(id);
        for (const auto& c  : newCoins)  addCoin(c);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Checkpoint / Rollback  (re-org support)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /**
     * Save the current MMR state at blockNum.
     * Call AFTER verifyRoot() succeeds for this block.
     * Java ref: MegaMMR.saveState(MiniNumber).
     */
    void checkpoint(const MiniNumber& blockNum) {
        m_checkpoints[blockNum.getAsLong()] = CheckpointData{m_mmr, m_idx};
    }

    /**
     * Roll back to a previously saved checkpoint.
     * Used during chain re-org to discard orphaned blocks.
     * Java ref: MegaMMR.loadState(MiniNumber).
     * Returns true if the checkpoint existed, false otherwise.
     */
    bool rollback(const MiniNumber& blockNum) {
        auto it = m_checkpoints.find(blockNum.getAsLong());
        if (it == m_checkpoints.end()) return false;
        m_mmr = it->second.mmr;
        m_idx = it->second.idx;
        return true;
    }

    /**
     * Discard checkpoints older than (currentBlock - keepDepth).
     * Default depth 64 matches Java reference.
     */
    void pruneCheckpoints(const MiniNumber& currentBlock, int keepDepth = 64) {
        int64_t cutoff = currentBlock.getAsLong() - keepDepth;
        for (auto it = m_checkpoints.begin(); it != m_checkpoints.end(); ) {
            if (it->first < cutoff)
                it = m_checkpoints.erase(it);
            else
                ++it;
        }
    }

    size_t checkpointCount() const { return m_checkpoints.size(); }

    bool hasCheckpoint(const MiniNumber& blockNum) const {
        return m_checkpoints.count(blockNum.getAsLong()) > 0;
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Root verification
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /**
     * Verify our computed root matches an expected root.
     * Used by block validator: MegaMMR.verifyRoot(block.header.mmrRoot).
     * Java ref: MegaMMR.checkRoot(MMRData).
     */
    bool verifyRoot(const MiniData& expectedRoot) const {
        return m_mmr.getRoot().getData() == expectedRoot;
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Query
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    MMRData  getRoot()   const { return m_mmr.getRoot(); }
    uint64_t leafCount() const { return m_mmr.getLeafCount(); }

    bool hasCoin(const MiniData& id) const { return m_idx.count(id) > 0; }

    std::optional<uint64_t> getLeafEntry(const MiniData& id) const {
        auto it = m_idx.find(id);
        if (it == m_idx.end()) return std::nullopt;
        return it->second;
    }

    std::optional<MMRProof> getCoinProof(const MiniData& id) const {
        auto it = m_idx.find(id);
        if (it == m_idx.end()) return std::nullopt;
        return m_mmr.getProof(it->second);
    }

    /**
     * Verify that a Coin is a valid leaf in our MMR.
     * proof must be the MMRProof from Witness.mCoinProofs.
     */
    bool verifyCoinProof(const Coin& coin, MMRProof proof) const {
        proof.setData(coinToMMRData(coin));
        return proof.verifyProof(m_mmr.getRoot());
    }

    void reset() {
        m_mmr = MMRSet();
        m_idx.clear();
        m_checkpoints.clear();
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Fast-Sync wire protocol
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /**
     * Serialise the full MegaMMR state for fast-sync (IBD_MEGAMMR message).
     * New nodes call applySyncPacket() on this data to bootstrap without
     * replaying the full chain history.
     * Java ref: MegaMMR.getSyncPacket().
     */
    std::vector<uint8_t> getSyncPacket() const {
        DataStream ds;
        uint64_t n = m_mmr.getLeafCount();
        ds.writeBytes(MiniNumber(int64_t(n)).serialise());

        for (uint64_t i = 0; i < n; ++i) {
            auto optE = m_mmr.getEntry(0, i);
            if (optE) {
                ds.writeUInt8(optE->getData().isSpent() ? 1 : 0);
                ds.writeBytes(optE->getData().getData().serialise());
            } else {
                // Gap in MMR (shouldn't happen, but guard anyway)
                ds.writeUInt8(0);
                ds.writeBytes(MiniData(std::vector<uint8_t>(32, 0)).serialise());
            }
        }

        ds.writeBytes(MiniNumber(int64_t(m_idx.size())).serialise());
        for (const auto& [cid, entry] : m_idx) {
            ds.writeBytes(cid.serialise());
            ds.writeBytes(MiniNumber(int64_t(entry)).serialise());
        }
        return ds.buffer();
    }

    /**
     * Rebuild MegaMMR from a received sync packet.
     * Java ref: MegaMMR.applySyncPacket(byte[]).
     */
    static MegaMMR applySyncPacket(const uint8_t* data, size_t& offset) {
        MegaMMR mega;

        MiniNumber leafCountN = MiniNumber::deserialise(data, offset);
        uint64_t n = static_cast<uint64_t>(leafCountN.getAsLong());

        for (uint64_t i = 0; i < n; ++i) {
            bool     spent = (data[offset++] != 0);
            MiniData hash  = MiniData::deserialise(data, offset);
            MMRData  d(hash);
            d.setSpent(spent);
            mega.m_mmr.addLeaf(d);
        }

        MiniNumber idxCountN = MiniNumber::deserialise(data, offset);
        uint64_t ic = static_cast<uint64_t>(idxCountN.getAsLong());

        for (uint64_t i = 0; i < ic; ++i) {
            MiniData   cid  = MiniData::deserialise(data, offset);
            MiniNumber ent  = MiniNumber::deserialise(data, offset);
            mega.m_idx[cid] = static_cast<uint64_t>(ent.getAsLong());
        }

        return mega;
    }

    // Aliases for backward compatibility
    std::vector<uint8_t> serialise()    const { return getSyncPacket(); }
    static MegaMMR deserialise(const uint8_t* data, size_t& offset) {
        return applySyncPacket(data, offset);
    }

private:
    // ── Coin index hash helpers ────────────────────────────────────────────
    struct CIDHash {
        size_t operator()(const MiniData& d) const noexcept {
            size_t s = 0;
            for (uint8_t b : d.bytes())
                s ^= std::hash<uint8_t>{}(b) + 0x9e3779b9 + (s<<6) + (s>>2);
            return s;
        }
    };
    struct CIDEq {
        bool operator()(const MiniData& a, const MiniData& b) const noexcept {
            return a == b;
        }
    };

    using CoinIndex = std::unordered_map<MiniData, uint64_t, CIDHash, CIDEq>;

    // ── Checkpoint storage ────────────────────────────────────────────────
    struct CheckpointData {
        MMRSet    mmr;
        CoinIndex idx;
    };

    MMRSet    m_mmr;
    CoinIndex m_idx;

    // Ordered by block number for efficient pruning
    std::map<int64_t, CheckpointData> m_checkpoints;
};

} // namespace minima
