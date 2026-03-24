#pragma once
/**
 * IBD — Initial Blockchain Download object.
 *
 * Java ref: IBD.java (org.minima.objects)
 *
 * Wire format:
 *   [1 byte: hasCascade (0/1)]
 *   [if cascade: Cascade wire bytes — [MiniNumber numNodes][CascadeNode × N]]
 *   [MiniNumber: numBlocks]
 *   [for each block: TxBlock bytes]
 */
#include "TxBlock.hpp"
#include "../types/MiniNumber.hpp"
#include "../serialization/DataStream.hpp"
#include "../chain/cascade/Cascade.hpp"
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <optional>

namespace minima {

// Max blocks in one IBD (Java ref: IBD.MAX_BLOCKS_FOR_IBD = 34000)
static constexpr int IBD_MAX_BLOCKS = 34000;

class IBD {
public:
    IBD() = default;

    // ── Accessors ─────────────────────────────────────────────────────────
    const std::vector<TxBlock>& txBlocks() const { return m_txBlocks; }
    std::vector<TxBlock>&       txBlocks()       { return m_txBlocks; }

    bool hasCascade() const { return m_hasCascade; }

    const cascade::Cascade* cascade() const {
        return m_cascade.has_value() ? &m_cascade.value() : nullptr;
    }
    cascade::Cascade* cascade() {
        return m_cascade.has_value() ? &m_cascade.value() : nullptr;
    }

    void setCascade(cascade::Cascade c) {
        m_cascade    = std::move(c);
        m_hasCascade = true;
    }
    void clearCascade() {
        m_cascade.reset();
        m_hasCascade = false;
    }

    void addBlock(TxBlock b) {
        if (static_cast<int>(m_txBlocks.size()) >= IBD_MAX_BLOCKS)
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

    // ── Serialise ─────────────────────────────────────────────────────────
    // [1 byte hasCascade]
    // [if cascade: Cascade bytes]
    // [MiniNumber numBlocks]
    // [TxBlock × numBlocks]
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;

        // hasCascade flag
        out.push_back(m_hasCascade ? 0x01 : 0x00);

        // Cascade bytes (only if present)
        if (m_hasCascade && m_cascade.has_value()) {
            auto cb = m_cascade->serialise();
            out.insert(out.end(), cb.begin(), cb.end());
        }

        // numBlocks as MiniNumber
        auto nb = MiniNumber(static_cast<int64_t>(m_txBlocks.size())).serialise();
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

        // hasCascade byte
        ibd.m_hasCascade = (data[offset++] != 0x00);

        if (ibd.m_hasCascade) {
            // Deserialise the Cascade
            // Cascade wire: [MiniNumber numNodes][CascadeNode × numNodes]
            std::vector<uint8_t> buf(data + offset, data + maxSize);
            size_t localOff = 0;
            ibd.m_cascade = cascade::Cascade::deserialise(buf, localOff);
            offset += localOff;
        }

        // numBlocks
        MiniNumber numBlocks = MiniNumber::deserialise(data, offset);
        int count = static_cast<int>(numBlocks.getAsLong());
        if (count < 0 || count > IBD_MAX_BLOCKS)
            throw std::runtime_error("IBD: invalid block count " + std::to_string(count));

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

    // ── Weight (for chain selection) ──────────────────────────────────────
    size_t blockCount() const { return m_txBlocks.size(); }

    // Total chain weight = cascade weight + block count
    double totalWeight() const {
        double w = static_cast<double>(m_txBlocks.size());
        if (m_hasCascade && m_cascade.has_value())
            w += m_cascade->totalWeight();
        return w;
    }

private:
    bool                           m_hasCascade = false;
    std::optional<cascade::Cascade> m_cascade;
    std::vector<TxBlock>           m_txBlocks;
};

} // namespace minima
