#pragma once
/**
 * TxPoW — Transaction Proof of Work unit.
 *
 * TxPoW is Minima's fundamental unit. Every user-submitted transaction IS a
 * TxPoW — it contains a transaction AND a proof-of-work header. Blocks are
 * simply TxPoW units with sufficient PoW to cross the block threshold.
 *
 * This unification (transaction == potential block) is what allows every
 * node to participate in consensus without dedicated miners.
 *
 * Minima reference: src/org/minima/objects/TxPoW.java
 *
 * Structure:
 *   TxPoW
 *   ├── TxHeader   (PoW header — what gets hashed)
 *   │   ├── block number
 *   │   ├── timestamp
 *   │   ├── parent TxPoW ID
 *   │   ├── nonce (varies during mining)
 *   │   ├── mmr root (coin accumulator)
 *   │   └── body hash
 *   ├── TxBody     (transaction data)
 *   │   ├── Transaction (inputs/outputs)
 *   │   ├── Witness     (signatures/scripts)
 *   │   └── list of TxPoW IDs in this block (if block-level unit)
 *   └── TxBlock    (block-level metadata, only set if this is a block)
 */

#include "TxHeader.hpp"
#include "TxBody.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include <memory>

namespace minima {

class TxPoW {
public:
    TxPoW() = default;

    // ── Identity ─────────────────────────────────────────────────────────────
    // TxPoW ID = SHA3(SHA3(header_bytes))  (double hash)
    MiniData computeID() const;

    // ── Header / Body ─────────────────────────────────────────────────────────
    TxHeader&       header()       { return m_header; }
    const TxHeader& header() const { return m_header; }
    TxBody&         body()         { return m_body; }
    const TxBody&   body()   const { return m_body; }

    // ── PoW checks ────────────────────────────────────────────────────────────
    MiniNumber  getPoWScore()  const;   // numeric value of PoW
    bool        isBlock()      const;   // PoW >= block difficulty
    bool        isTransaction()const;   // always true if valid TxPoW

    // ── Mining ────────────────────────────────────────────────────────────────
    // Increment nonce and recheck — call in a loop until isBlock() or tx threshold met
    void        incrementNonce();
    void        setNonce(uint64_t nonce);

    // ── Serialisation ─────────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const;
    static TxPoW         deserialise(const uint8_t* data, size_t& offset);

private:
    TxHeader m_header;
    TxBody   m_body;
};

} // namespace minima
