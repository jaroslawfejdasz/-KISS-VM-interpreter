#include "Coin.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"
#include <stdexcept>

namespace minima {

// ── Builders ──────────────────────────────────────────────────────────────────

Coin& Coin::addStateVar(const StateVariable& sv) {
    // Remove existing var at same port (overwrite semantics)
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
        if (sv.port() == port) return sv;
    return std::nullopt;
}

// ── Serialisation ─────────────────────────────────────────────────────────────
// Wire format (mirrors Minima Java Coin.writeDataStream):
//   coinID          : MiniData
//   address         : MiniData (address hash)
//   amount          : MiniNumber
//   tokenID_present : bool
//   tokenID         : MiniData   (only if present)
//   floating        : bool
//   spent           : bool
//   stateVars_count : uint8
//   stateVars[]     : StateVariable × count

std::vector<uint8_t> Coin::serialise() const {
    DataStream ds;

    // Write coinID
    auto coinIdBytes = m_coinID.serialise();
    ds.writeBytes(coinIdBytes);

    // Write address hash
    auto addrBytes = m_address.serialise();
    ds.writeBytes(addrBytes);

    // Write amount
    auto amtBytes = m_amount.serialise();
    ds.writeBytes(amtBytes);

    // Write tokenID
    ds.writeBool(m_tokenID.has_value());
    if (m_tokenID.has_value()) {
        auto tidBytes = m_tokenID->serialise();
        ds.writeBytes(tidBytes);
    }

    ds.writeBool(m_floating);
    ds.writeBool(m_spent);

    // State variables
    ds.writeUInt8((uint8_t)m_stateVars.size());
    for (auto& sv : m_stateVars) {
        auto svBytes = sv.serialise();
        ds.writeBytes(svBytes);
    }

    return ds.buffer();
}

Coin Coin::deserialise(const uint8_t* data, size_t& offset) {
    Coin c;
    c.m_coinID  = MiniData::deserialise(data, offset);

    // address is stored as MiniData (the 32-byte hash)
    auto addrHash = MiniData::deserialise(data, offset);
    c.m_address = Address(addrHash);

    c.m_amount = MiniNumber::deserialise(data, offset);

    bool hasToken = data[offset++];
    if (hasToken) {
        c.m_tokenID = MiniData::deserialise(data, offset);
    }

    c.m_floating = data[offset++];
    c.m_spent    = data[offset++];

    uint8_t svCount = data[offset++];
    for (uint8_t i = 0; i < svCount; ++i) {
        c.m_stateVars.push_back(StateVariable::deserialise(data, offset));
    }

    return c;
}

} // namespace minima
