#pragma once
/**
 * Coin — an Unspent Transaction Output (UTxO).
 *
 * Minima reference: src/org/minima/objects/Coin.java
 *
 * Wire format — EXACT Java Coin.writeDataStream:
 *   mCoinID         : MiniData (writeHashToStream)
 *   mAddress        : MiniData (writeHashToStream)
 *   mAmount         : MiniNumber
 *   mTokenID        : MiniData (writeHashToStream)
 *   mStoreState     : MiniByte (1 byte bool)
 *   mMMREntryNumber : MiniNumber
 *   mSpent          : MiniNumber
 *   mBlockCreated   : MiniNumber
 *   mState.size()   : MiniNumber
 *   mState[]        : StateVariable × count
 *   hasToken        : MiniByte (1 byte bool)
 *   mToken          : Token (if hasToken)
 */

#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"
#include "Address.hpp"
#include "StateVariable.hpp"
#include <vector>
#include <optional>
#include "../crypto/Hash.hpp"

namespace minima {

class Coin {
public:
    Coin() = default;

    const MiniData&   coinID()       const { return m_coinID; }
    const Address&    address()      const { return m_address; }
    const MiniNumber& amount()       const { return m_amount; }
    const MiniNumber& mmrEntry()     const { return m_mmrEntry; }
    const MiniNumber& blockCreated() const { return m_blockCreated; }
    bool  storeState()  const { return m_storeState; }
    bool  isSpent()     const { return m_spent; }

    bool hasToken() const { return m_tokenID.has_value(); }
    const MiniData& tokenID() const { return m_tokenID.value(); }

    const std::vector<StateVariable>& stateVars() const { return m_stateVars; }
    std::optional<StateVariable>      stateVar(uint8_t port) const;

    Coin& setCoinID      (const MiniData& id)     { m_coinID = id;           return *this; }
    Coin& setAddress     (const Address& addr)    { m_address = addr;        return *this; }
    Coin& setAmount      (const MiniNumber& amt)  { m_amount = amt;          return *this; }
    Coin& setTokenID     (const MiniData& tid)    { m_tokenID = tid;         return *this; }
    Coin& setStoreState  (bool s)                 { m_storeState = s;        return *this; }
    Coin& setMmrEntry    (const MiniNumber& e)    { m_mmrEntry = e;          return *this; }
    Coin& setSpent       (bool s)                 { m_spent = s;             return *this; }
    Coin& setBlockCreated(const MiniNumber& b)    { m_blockCreated = b;      return *this; }
    Coin& addStateVar    (const StateVariable& sv);

    bool operator==(const Coin& o) const { return m_coinID == o.m_coinID; }

    std::vector<uint8_t> serialise() const;
    static Coin          deserialise(const uint8_t* data, size_t& offset);

    MiniData hashValue() const;

private:
    MiniData   m_coinID;
    Address    m_address;
    MiniNumber m_amount;
    std::optional<MiniData> m_tokenID;
    bool       m_storeState{false};
    MiniNumber m_mmrEntry;
    bool       m_spent{false};
    MiniNumber m_blockCreated;
    std::vector<StateVariable> m_stateVars;
};

} // namespace minima
