#include "Coin.hpp"
#include "Token.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"
#include <stdexcept>

namespace minima {

// ── Builders ──────────────────────────────────────────────────────────────────

Coin& Coin::addStateVar(const StateVariable& sv) {
    m_stateVars.erase(
        std::remove_if(m_stateVars.begin(), m_stateVars.end(),
            [&](const StateVariable& s){ return s.port() == sv.port(); }),
        m_stateVars.end()
    );
    m_stateVars.push_back(sv);
    return *this;
}

std::optional<StateVariable> Coin::stateVar(uint8_t port) const {
    for (auto& sv : m_stateVars)
        if (sv.port() == sv.port()) return sv;
    return std::nullopt;
}

// ── Serialisation ─────────────────────────────────────────────────────────────
// Wire format — EXACT Java Coin.writeDataStream order:
//   address         : MiniData
//   amount          : MiniNumber
//   tokenID         : MiniData
//   token_flag      : 1 byte (bool) — is Token object present?
//   token           : Token (if flag == 1)
//   storeState      : 1 byte (bool) — are state vars stored?
//   stateVars[]     : MiniNumber(count) + StateVariable × count (if storeState)
//   spent           : 1 byte (bool)
//   mmrEntry        : MiniNumber
//   created         : MiniNumber

std::vector<uint8_t> Coin::serialise() const {
    DataStream ds;
    ds.writeBytes(m_address.hash().serialise());    // address MiniData
    ds.writeBytes(m_amount.serialise());               // amount  MiniNumber
    ds.writeBytes(m_tokenID.serialise());              // tokenID MiniData
    // Token object (optional)
    bool hasToken = m_token.has_value();
    ds.writeUInt8(hasToken ? 1 : 0);
    if (hasToken) {
        ds.writeBytes(m_token->serialise());
    }
    // storeState + state variables
    bool hasState = !m_stateVars.empty();
    ds.writeUInt8(hasState ? 1 : 0);
    if (hasState) {
        ds.writeBytes(MiniNumber(int64_t(m_stateVars.size())).serialise());
        for (auto& sv : m_stateVars) ds.writeBytes(sv.serialise());
    }
    ds.writeUInt8(m_spent ? 1 : 0);
    ds.writeBytes(m_mmrEntry.serialise());
    ds.writeBytes(m_created.serialise());
    return ds.buffer();
}

Coin Coin::deserialise(const uint8_t* data, size_t& offset) {
    Coin c;
    // address
    c.m_address = Address(MiniData::deserialise(data, offset));
    // amount
    c.m_amount = MiniNumber::deserialise(data, offset);
    // tokenID
    c.m_tokenID = MiniData::deserialise(data, offset);
    // token (optional)
    bool hasToken = (data[offset++] != 0);
    if (hasToken) {
        c.m_token = Token::deserialise(data, offset);
    }
    // storeState
    bool storeState = (data[offset++] != 0);
    if (storeState) {
        int64_t cnt = MiniNumber::deserialise(data, offset).getAsLong();
        if (cnt < 0 || cnt > 256) throw std::runtime_error("Coin: stateVars count invalid");
        for (int64_t i = 0; i < cnt; ++i)
            c.m_stateVars.push_back(StateVariable::deserialise(data, offset));
    }
    // spent
    c.m_spent = (data[offset++] != 0);
    // mmrEntry
    c.m_mmrEntry = MiniNumber::deserialise(data, offset);
    // created
    c.m_created = MiniNumber::deserialise(data, offset);
    return c;
}

} // namespace minima
