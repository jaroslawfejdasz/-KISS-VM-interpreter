#pragma once
/**
 * CoinProof — Coin + MMR proof (used in TxBlock / IBD).
 * Java ref: src/org/minima/objects/CoinProof.java
 */
#include "Coin.hpp"
#include "../mmr/MMRProof.hpp"
#include "../serialization/DataStream.hpp"
#include <vector>
#include <cstdint>

namespace minima {

class CoinProof {
public:
    CoinProof() = default;
    CoinProof(const Coin& coin, const MMRProof& proof)
        : m_coin(coin), m_proof(proof) {}

    const Coin&     coin()  const { return m_coin; }
    const MMRProof& proof() const { return m_proof; }

    std::vector<uint8_t> serialise() const {
        auto out = m_coin.serialise();
        DataStream ds;
        m_proof.serialise(ds);
        const auto& pb = ds.buffer();
        out.insert(out.end(), pb.begin(), pb.end());
        return out;
    }

    static CoinProof deserialise(const uint8_t* data, size_t& offset,
                                  size_t totalLen) {
        CoinProof cp;
        cp.m_coin = Coin::deserialise(data, offset);
        DataStream ds(data + offset, totalLen - offset);
        cp.m_proof.deserialise(ds);
        offset += ds.position();
        return cp;
    }

private:
    Coin     m_coin;
    MMRProof m_proof;
};

} // namespace minima
