#pragma once
#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"
#include "Address.hpp"
#include "StateVariable.hpp"
#include "Token.hpp"
#include <vector>
#include <optional>

namespace minima {

/**
 * Coin — Unspent Transaction Output (UTxO)
 *
 * Wire format (Java Coin.writeDataStream):
 *   address    : MiniData
 *   amount     : MiniNumber
 *   tokenID    : MiniData  (0x00=native, 32B=custom)
 *   token_flag : uint8 (1 = Token object follows)
 *   token      : Token (if flag)
 *   storeState : uint8 (1 = state vars follow)
 *   count      : MiniNumber (if storeState)
 *   stateVars  : StateVariable[] (if storeState)
 *   spent      : uint8
 *   mmrEntry   : MiniNumber
 *   created    : MiniNumber
 */
class Coin {
public:
    Coin() = default;

    // ── Getters ──────────────────────────────────────────────────────────────
    const Address&    address()  const { return m_address; }
    const MiniNumber& amount()   const { return m_amount; }
    const MiniData&   tokenID()  const { return m_tokenID; }
    bool  isSpent()              const { return m_spent; }
    bool  hasToken()             const { return m_token.has_value(); }
    const Token& token()         const { return m_token.value(); }
    const std::vector<StateVariable>& stateVars() const { return m_stateVars; }
    std::optional<StateVariable> stateVar(uint8_t port) const;
    const MiniNumber& mmrEntry() const { return m_mmrEntry; }
    const MiniNumber& created()  const { return m_created; }
    const MiniData&   coinID()   const { return m_coinID; }

    // ── Fluent setters ────────────────────────────────────────────────────────
    Coin& setAddress (const Address& a)    { m_address = a;   return *this; }
    Coin& setAmount  (const MiniNumber& n) { m_amount  = n;   return *this; }
    Coin& setTokenID (const MiniData& tid) { m_tokenID = tid; return *this; }
    Coin& setToken   (const Token& t)      { m_token   = t;   return *this; }
    Coin& setSpent   (bool s)              { m_spent   = s;   return *this; }
    Coin& setMmrEntry(const MiniNumber& n) { m_mmrEntry = n;  return *this; }
    Coin& setCreated (const MiniNumber& n) { m_created  = n;  return *this; }
    Coin& setCoinID  (const MiniData& id)  { m_coinID   = id; return *this; }
    Coin& addStateVar(const StateVariable& sv);

    // ── Serialisation ─────────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const;
    static Coin deserialise(const uint8_t* data, size_t& offset);

private:
    Address    m_address;
    MiniNumber m_amount;
    MiniData   m_tokenID;
    std::optional<Token> m_token;
    bool       m_spent{false};
    std::vector<StateVariable> m_stateVars;
    MiniNumber m_mmrEntry;
    MiniNumber m_created;
    MiniData   m_coinID;  // cached (not on-wire)
};

} // namespace minima
