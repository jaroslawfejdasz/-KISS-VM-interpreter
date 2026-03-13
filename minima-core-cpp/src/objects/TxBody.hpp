#pragma once
/**
 * TxBody — the transaction payload inside a TxPoW unit.
 *
 * Contains the actual transaction, witness proofs, and (if this TxPoW
 * is a block) the list of included transactions.
 */

#include "Transaction.hpp"
#include "Witness.hpp"
#include "../types/MiniData.hpp"
#include <vector>

namespace minima {

class TxBody {
public:
    Transaction              txn;
    Witness                  witness;
    std::vector<MiniData>    burnTxns;     // TxPoW IDs burned to raise PoW
    std::vector<MiniData>    txnList;      // TxPoW IDs included (if block)

    bool isEmptyTxn() const;  // no inputs/outputs — pure PoW pulse

    MiniData             computeHash() const;  // SHA3(serialise())
    std::vector<uint8_t> serialise()   const;
    static TxBody        deserialise(const uint8_t* data, size_t& offset);
};

} // namespace minima
