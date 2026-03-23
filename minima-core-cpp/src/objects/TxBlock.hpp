#pragma once
/**
 * TxBlock — a processed block ready for sync / IBD.
 *
 * Java ref: src/org/minima/objects/TxBlock.java
 *
 * Wire format:
 *   txpow          (TxPoW serialise)
 *   previousPeaks  (MiniNumber count + MMREntry[] )
 *   spentCoins     (MiniNumber count + CoinProof[])
 *   newCoins       (MiniNumber count + Coin[]      )
 */
#include "TxPoW.hpp"
#include "Coin.hpp"
#include "CoinProof.hpp"
#include "../mmr/MMREntry.hpp"
#include "../types/MiniNumber.hpp"
#include <vector>
#include <cstdint>

namespace minima {

class TxBlock {
public:
    TxBlock() = default;
    explicit TxBlock(const TxPoW& txpow) : m_txpow(txpow) {}

    // ── Accessors ─────────────────────────────────────────────────────────
    const TxPoW&                   txpow()         const { return m_txpow; }
    TxPoW&                         txpow()               { return m_txpow; }
    const std::vector<MMREntry>&   previousPeaks() const { return m_previousPeaks; }
    const std::vector<CoinProof>&  spentCoins()    const { return m_spentCoins; }
    const std::vector<Coin>&       newCoins()      const { return m_newCoins; }

    void addPreviousPeak(const MMREntry& e)  { m_previousPeaks.push_back(e); }
    void addSpentCoin(const CoinProof& cp)   { m_spentCoins.push_back(cp); }
    void addNewCoin(const Coin& c)           { m_newCoins.push_back(c); }

    // ── Serialisation (wire-exact Java order) ─────────────────────────────
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto append = [&](const std::vector<uint8_t>& v) {
            out.insert(out.end(), v.begin(), v.end());
        };

        // TxPoW
        append(m_txpow.serialise());

        // previousPeaks: count + entries
        append(MiniNumber(int64_t(m_previousPeaks.size())).serialise());
        for (const auto& e : m_previousPeaks) {
            DataStream ds;
            e.serialise(ds);
            const auto& b = ds.buffer();
            out.insert(out.end(), b.begin(), b.end());
        }

        // spentCoins: count + CoinProofs
        append(MiniNumber(int64_t(m_spentCoins.size())).serialise());
        for (const auto& cp : m_spentCoins)
            append(cp.serialise());

        // newCoins: count + Coins
        append(MiniNumber(int64_t(m_newCoins.size())).serialise());
        for (const auto& c : m_newCoins)
            append(c.serialise());

        return out;
    }

    static TxBlock deserialise(const uint8_t* data, size_t& offset, size_t totalLen) {
        TxBlock tb;
        tb.m_txpow = TxPoW::deserialise(data, offset);

        int64_t peakCount = MiniNumber::deserialise(data, offset).getAsLong();
        for (int64_t i = 0; i < peakCount; ++i) {
            DataStream ds(data + offset, totalLen - offset);
            MMREntry e;
            e.deserialise(ds);
            offset += ds.position();
            tb.m_previousPeaks.push_back(e);
        }

        int64_t spentCount = MiniNumber::deserialise(data, offset).getAsLong();
        for (int64_t i = 0; i < spentCount; ++i)
            tb.m_spentCoins.push_back(CoinProof::deserialise(data, offset, totalLen));

        int64_t newCount = MiniNumber::deserialise(data, offset).getAsLong();
        for (int64_t i = 0; i < newCount; ++i)
            tb.m_newCoins.push_back(Coin::deserialise(data, offset));

        return tb;
    }

private:
    TxPoW                 m_txpow;
    std::vector<MMREntry> m_previousPeaks;
    std::vector<CoinProof>m_spentCoins;
    std::vector<Coin>     m_newCoins;
};

} // namespace minima
