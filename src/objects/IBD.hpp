#pragma once
/**
 * IBD — Initial Blockchain Download object.
 *
 * Java ref: IBD.java (org.minima.objects)
 *
 * Wire format:
 *   [1 byte: hasCascade (0/1)]
 *   [if cascade: cascade bytes — TODO: Cascade not yet impl, always false]
 *   [MiniNumber: numBlocks]
 *   [for each block: TxBlock bytes]
 */
#include "TxBlock.hpp"
#include "../types/MiniNumber.hpp"
#include "../serialization/DataStream.hpp"
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace minima {

// Max blocks in one IBD (Java ref: IBD.MAX_BLOCKS_FOR_IBD = 34000)
static constexpr int IBD_MAX_BLOCKS = 34000;

class IBD {
public:
    IBD() = default;

    // ── Accessors ─────────────────────────────────────────────────────────────
    const std::vector<TxBlock>& txBlocks() const { return m_txBlocks; }
    std::vector<TxBlock>&       txBlocks()       { return m_txBlocks; }
    bool hasCascade() const { return m_hasCascade; }

    void addBlock(TxBlock b) {
        if ((int)m_txBlocks.size() >= IBD_MAX_BLOCKS)
            throw std::runtime_error("IBD: max blocks exceeded");
        m_txBlocks.push_back(std::move(b));
    }

    MiniNumber treeRoot() const {
        if (m_txBlocks.empty()) return MiniNumber(int64_t(-1));
        return m_txBlocks.front().txpow().header().blockNumber;
    }

    MiniNumber treeTip() const {
        if (m_txBlocks.empty()) return MiniNumber(int64_t(-1));
        return m_txBlocks.back().txpow().header().blockNumber;
    }

    // ── Serialise ─────────────────────────────────────────────────────────────
    // [1 byte hasCascade][MiniNumber numBlocks][blocks...]
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        // hasCascade flag (always false in current implementation)
        out.push_back(m_hasCascade ? 0x01 : 0x00);
        // numBlocks as MiniNumber
        MiniNumber numBlocks(int64_t(m_txBlocks.size()));
        auto nb = numBlocks.serialise();
        out.insert(out.end(), nb.begin(), nb.end());
        // each TxBlock
        for (const auto& tb : m_txBlocks) {
            auto tb_bytes = tb.serialise();
            out.insert(out.end(), tb_bytes.begin(), tb_bytes.end());
        }
        return out;
    }

    static IBD deserialise(const uint8_t* data, size_t& offset, size_t maxSize) {
        if (offset >= maxSize)
            throw std::runtime_error("IBD: empty data");
        IBD ibd;

        // hasCascade
        ibd.m_hasCascade = (data[offset++] != 0x00);
        if (ibd.m_hasCascade) {
            // TODO: Cascade not yet implemented — skip gracefully
            // For now, throw so we know if we ever receive one
            throw std::runtime_error("IBD: Cascade deserialization not yet implemented");
        }

        // numBlocks
        MiniNumber numBlocks = MiniNumber::deserialise(data, offset);
        int count = numBlocks.getAsInt();
        if (count < 0 || count > IBD_MAX_BLOCKS)
            throw std::runtime_error("IBD: invalid block count");

        for (int i = 0; i < count; ++i) {
            TxBlock tb = TxBlock::deserialise(data, offset, maxSize);
            ibd.m_txBlocks.push_back(std::move(tb));
        }
        return ibd;
    }

    static IBD deserialise(const std::vector<uint8_t>& data) {
        size_t off = 0;
        return deserialise(data.data(), off, data.size());
    }

    // ── Weight calculation (for chain selection) ──────────────────────────────
    // Sum of difficulty values (higher = harder chain = more weight)
    // In Minima: difficulty is inverted — smaller value = more work done
    // So "heavier chain" = sum of (1 / difficulty) — but we approximate
    // by counting blocks as a simple weight proxy
    size_t blockCount() const { return m_txBlocks.size(); }

private:
    bool                m_hasCascade = false;
    std::vector<TxBlock> m_txBlocks;
};

} // namespace minima
