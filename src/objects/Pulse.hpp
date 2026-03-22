#pragma once
/**
 * Pulse — P2P keepalive / chain-tip sync message.
 *
 * Port of org.minima.objects.Pulse.
 *
 * Sent every ~60s between peers. Contains up to 60 recent block IDs.
 * Receiver compares IDs with its chain tip → triggers IBD if mismatch.
 *
 * Wire format:
 *   [uint8]  count  (0..60)
 *   count × [MiniData.serialise()]
 */
#include "../types/MiniData.hpp"
#include <vector>
#include <stdexcept>

namespace minima {

class Pulse {
public:
    static constexpr int MAX_PULSE_BLOCKS = 60;

    Pulse() = default;

    void addTxPoWID(const MiniData& id) {
        if (static_cast<int>(m_ids.size()) >= MAX_PULSE_BLOCKS)
            throw std::runtime_error("Pulse: max 60 IDs");
        m_ids.push_back(id);
    }

    void clear() { m_ids.clear(); }

    const std::vector<MiniData>& txPoWIDs() const { return m_ids; }
    size_t size()    const { return m_ids.size(); }
    bool   isEmpty() const { return m_ids.empty(); }

    bool contains(const MiniData& id) const {
        for (const auto& x : m_ids) if (x == id) return true;
        return false;
    }

    // ── Wire ──────────────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        out.push_back(static_cast<uint8_t>(m_ids.size()));
        for (const auto& id : m_ids) {
            auto b = id.serialise();
            out.insert(out.end(), b.begin(), b.end());
        }
        return out;
    }

    static Pulse deserialise(const uint8_t* data, size_t& offset) {
        Pulse p;
        uint8_t count = data[offset++];
        if (count > MAX_PULSE_BLOCKS)
            throw std::runtime_error("Pulse: count > 60");
        p.m_ids.reserve(count);
        for (uint8_t i = 0; i < count; ++i)
            p.m_ids.push_back(MiniData::deserialise(data, offset));
        return p;
    }

private:
    std::vector<MiniData> m_ids;
};

} // namespace minima
