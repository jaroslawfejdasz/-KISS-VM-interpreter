#pragma once
/**
 * Coin — an Unspent Transaction Output (UTxO).
 *
 * Every coin on Minima is identified by a unique CoinID (hash), carries an amount,
 * belongs to a script address, and optionally holds state variables and a token ID.
 *
 * Minima reference: src/org/minima/objects/Coin.java
 */

#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"
#include "Address.hpp"
#include "StateVariable.hpp"
#include <vector>
#include <optional>

namespace minima {

class Coin {
public:
    Coin() = default;

    // ── Identity ─────────────────────────────────────────────────────────────
    const MiniData&   coinID()   const { return m_coinID; }
    const Address&    address()  const { return m_address; }
    const MiniNumber& amount()   const { return m_amount; }

    // ── Token ─────────────────────────────────────────────────────────────────
    // If tokenID is empty → native Minima. Otherwise → custom token.
    bool hasToken() const { return m_tokenID.has_value(); }
    const MiniData& tokenID() const { return m_tokenID.value(); }

    // ── State variables (0..255) ──────────────────────────────────────────────
    const std::vector<StateVariable>& stateVars() const { return m_stateVars; }
    std::optional<StateVariable>      stateVar(uint8_t port) const;

    // ── Flags ────────────────────────────────────────────────────────────────
    bool isFloating() const { return m_floating; }  // amount not fixed at creation
    bool isSpent()    const { return m_spent; }

    // ── Builders (fluent API) ─────────────────────────────────────────────────
    Coin& setCoinID  (const MiniData& id)   { m_coinID = id;      return *this; }
    Coin& setAddress (const Address& addr)  { m_address = addr;   return *this; }
    Coin& setAmount  (const MiniNumber& amt){ m_amount = amt;     return *this; }
    Coin& setTokenID (const MiniData& tid)  { m_tokenID = tid;    return *this; }
    Coin& setFloating(bool f)               { m_floating = f;     return *this; }
    Coin& addStateVar(const StateVariable& sv);

    // ── Serialisation ─────────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const;
    static Coin          deserialise(const uint8_t* data, size_t& offset);

private:
    MiniData   m_coinID;
    Address    m_address;
    MiniNumber m_amount;
    std::optional<MiniData> m_tokenID;
    std::vector<StateVariable> m_stateVars;
    bool m_floating{false};
    bool m_spent{false};
};

} // namespace minima
