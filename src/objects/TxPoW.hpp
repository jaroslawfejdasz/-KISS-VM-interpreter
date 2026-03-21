#pragma once
/**
 * TxPoW — Transaction Proof of Work unit.
 *
 * Minima Java reference: src/org/minima/objects/TxPoW.java
 *
 * Wire format:
 *   header       : TxHeader (always)
 *   bodyPresent  : uint8 (0x00=false, 0x01=true)
 *   body         : TxBody (only if bodyPresent)
 *
 * TxPoW ID   = SHA3(SHA3(TxHeader bytes))  — double SHA3, header only
 * PoW hash   = SHA2(TxHeader bytes)        — single SHA2
 * isBlock()  = SHA2(header) <= blockDifficulty  (big-endian uint comparison)
 */

#include "TxHeader.hpp"
#include "TxBody.hpp"
#include "../types/MiniData.hpp"

namespace minima {

class TxPoW {
public:
    TxPoW() = default;

    MiniData computeID()    const;   // SHA3(SHA3(header))
    MiniData getPoWHash()   const;   // SHA2(header)
    bool     isBlock()      const;   // powHash <= blockDifficulty
    bool     isTransaction() const;  // powHash <= txnDifficulty
    int      getSuperLevel() const;

    TxHeader&       header()       { return m_header; }
    const TxHeader& header() const { return m_header; }
    TxBody&         body()         { return m_body; }
    const TxBody&   body()   const { return m_body; }

    void mine();  // increment nonce + reset PRNG

    std::vector<uint8_t> serialise(bool includeBody = true) const;
    static TxPoW         deserialise(const uint8_t* data, size_t& offset);
    static TxPoW         deserialise(const std::vector<uint8_t>& data) {
        size_t off = 0; return deserialise(data.data(), off);
    }

private:
    TxHeader m_header;
    TxBody   m_body;
    static bool hashLE(const MiniData& a, const MiniData& b);
};

} // namespace minima
