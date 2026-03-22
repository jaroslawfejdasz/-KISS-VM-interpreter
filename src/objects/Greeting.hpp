#pragma once
/**
 * Greeting — initial handshake message between Minima nodes.
 *
 * Java ref: src/org/minima/objects/Greeting.java
 *
 * Wire format:
 *   version   (MiniString)
 *   topBlock  (MiniNumber — -1 means fresh install)
 *   chain[]   (MiniNumber count + MiniData[] txpow IDs from tip→root)
 */
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../types/MiniString.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace minima {

// Minima protocol version
static constexpr const char* MINIMA_VERSION = "1.0.45";

class Greeting {
public:
    Greeting()
        : m_version(MINIMA_VERSION)
        , m_topBlock(MiniNumber(int64_t(-1)))
    {}

    // ── Accessors ─────────────────────────────────────────────────────────
    const MiniString&            version()  const { return m_version; }
    const MiniNumber&            topBlock() const { return m_topBlock; }
    MiniNumber                   rootBlock()const {
        if (m_chain.empty()) return MiniNumber(int64_t(-1));
        return m_chain.size() > 0 ? MiniNumber(int64_t(0)) : MiniNumber(int64_t(-1));
    }
    const std::vector<MiniData>& chain()    const { return m_chain; }

    void setTopBlock(const MiniNumber& n) { m_topBlock = n; }
    void addChainID(const MiniData& id)   { m_chain.push_back(id); }

    bool isFreshInstall() const {
        return m_topBlock.getAsLong() < 0;
    }

    // ── Serialisation ─────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto append = [&](const std::vector<uint8_t>& v) {
            out.insert(out.end(), v.begin(), v.end());
        };
        append(m_version.serialise());
        append(m_topBlock.serialise());
        append(MiniNumber(int64_t(m_chain.size())).serialise());
        for (const auto& id : m_chain)
            append(id.serialise());
        return out;
    }

    static Greeting deserialise(const uint8_t* data, size_t& offset) {
        Greeting g;
        g.m_version  = MiniString::deserialise(data, offset);
        g.m_topBlock = MiniNumber::deserialise(data, offset);
        int64_t count = MiniNumber::deserialise(data, offset).getAsLong();
        for (int64_t i = 0; i < count; ++i)
            g.m_chain.push_back(MiniData::deserialise(data, offset));
        return g;
    }

private:
    MiniString            m_version;
    MiniNumber            m_topBlock;
    std::vector<MiniData> m_chain;   // txpow IDs tip → root
};

} // namespace minima
