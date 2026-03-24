#pragma once
/**
 * CoinProof — Coin + MMR proof (used in TxBlock / IBD / Witness).
 * Java ref: src/org/minima/objects/CoinProof.java
 *
 * Wire format (Java: CoinProof.writeDataStream):
 *   Coin.writeDataStream(out)
 *   MMRProof.writeDataStream(out)
 */
#include "Coin.hpp"
#include "../mmr/MMRProof.hpp"
#include "../mmr/MMRData.hpp"
#include "../types/MiniNumber.hpp"
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
    Coin&     coin()  { return m_coin; }
    MMRProof& proof() { return m_proof; }

    /**
     * Compute the MMR leaf data for this coin.
     * Java: MMRData.CreateMMRDataLeafNode(getCoin(), getCoin().getAmount())
     *   leaf hash = SHA3(coin.serialise()), value = coin.amount()
     */
    MMRData getMMRData() const;

    // ── Wire format ────────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto cb = m_coin.serialise();
        out.insert(out.end(), cb.begin(), cb.end());
        auto pb = m_proof.serialise();
        out.insert(out.end(), pb.begin(), pb.end());
        return out;
    }

    // 2-arg: MMRProof reads its own length (no bounds check beyond MMRProof internal cap)
    static CoinProof deserialise(const uint8_t* data, size_t& offset) {
        CoinProof cp;
        cp.m_coin  = Coin::deserialise(data, offset);
        cp.m_proof = MMRProof::deserialise(data, offset);
        return cp;
    }

    // 3-arg: pass total buffer size for safe MMRProof deserialization
    static CoinProof deserialise(const uint8_t* data, size_t& offset, size_t totalLen) {
        CoinProof cp;
        cp.m_coin  = Coin::deserialise(data, offset);
        cp.m_proof = MMRProof::deserialise(data, offset, totalLen);
        return cp;
    }

private:
    Coin     m_coin;
    MMRProof m_proof;
};

} // namespace minima
