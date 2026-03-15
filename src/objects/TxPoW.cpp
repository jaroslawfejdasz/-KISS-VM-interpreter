#include "TxPoW.hpp"
#include "../crypto/Hash.hpp"
#include "../types/MiniNumber.hpp"

namespace minima {

// TxPoW ID = SHA3(SHA3(header_bytes)) — double hash
MiniData TxPoW::computeID() const {
    auto headerBytes = m_header.serialise();
    return crypto::Hash::sha3_256_double(headerBytes);
}

// PoW score: Minima compares the SHA2 hash of the header (as a large integer)
// against the difficulty. Lower hash = more PoW.
// We return the hash as raw MiniData — callers use isBlock()/isTransaction()
// which compare via MiniNumber deserialized from the hash bytes.
MiniNumber TxPoW::getPoWScore() const {
    auto headerBytes = m_header.serialise();
    auto hash = crypto::Hash::sha2_256(headerBytes);
    // Interpret 32-byte hash as a big decimal number string (hex notation)
    // Minima Java uses new BigDecimal(new BigInteger(hash)) for PoW comparison
    return MiniNumber::fromBytes(hash.bytes());
}

bool TxPoW::isBlock() const {
    MiniNumber score = getPoWScore();
    // Block if hash <= blockDifficulty (lower hash = more work done)
    return score.isLessEqual(m_header.blockDifficulty);
}

bool TxPoW::isTransaction() const {
    MiniNumber score = getPoWScore();
    return score.isLessEqual(m_header.txnDifficulty);
}

void TxPoW::incrementNonce() { m_header.nonce++; }
void TxPoW::setNonce(uint64_t nonce) { m_header.nonce = nonce; }

// Wire format:
//   header : TxHeader (serialised)
//   body   : TxBody   (serialised)

std::vector<uint8_t> TxPoW::serialise() const {
    std::vector<uint8_t> out;
    auto hb = m_header.serialise();
    auto bb = m_body.serialise();
    out.insert(out.end(), hb.begin(), hb.end());
    out.insert(out.end(), bb.begin(), bb.end());
    return out;
}

TxPoW TxPoW::deserialise(const uint8_t* data, size_t& offset) {
    TxPoW t;
    t.m_header = TxHeader::deserialise(data, offset);
    t.m_body   = TxBody::deserialise(data, offset);
    return t;
}

} // namespace minima
