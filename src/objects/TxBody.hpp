#pragma once
/**
 * TxBody — transaction payload inside TxPoW.
 *
 * Minima Java reference: src/org/minima/objects/TxBody.java
 *
 * Wire format — EXACT Java writeDataStream order:
 *   prng            : MiniData   (32 random bytes)
 *   txnDifficulty   : MiniData   (32-byte difficulty target)
 *   transaction     : Transaction
 *   witness         : Witness
 *   burnTransaction : Transaction
 *   burnWitness     : Witness
 *   txnList count   : MiniNumber
 *   txnList[]       : MiniData × count
 */

#include "Transaction.hpp"
#include "Witness.hpp"
#include "../types/MiniData.hpp"
#include <vector>
#include <cstdint>

namespace minima {

class TxBody {
public:
    MiniData   prng;             // 32 random bytes — re-rolled each mine()
    MiniData   txnDifficulty;    // 32-byte target for txn acceptance
    Transaction txn;
    Witness     witness;
    Transaction burnTxn;         // optional burn (increases PoW)
    Witness     burnWitness;
    std::vector<MiniData> txnList; // TxPoW IDs in this block

    TxBody();

    bool     isEmptyTxn() const;
    MiniData computeHash() const;
    void     resetPRNG();

    std::vector<uint8_t> serialise() const;
    static TxBody        deserialise(const uint8_t* data, size_t& offset);
    static TxBody        deserialise(const std::vector<uint8_t>& data, size_t& offset) {
        size_t off = 0; return deserialise(data.data(), off);
    }
};

} // namespace minima
