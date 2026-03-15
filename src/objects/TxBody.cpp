#include "TxBody.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"

namespace minima {

bool TxBody::isEmptyTxn() const {
    return txn.inputs().empty() && txn.outputs().empty();
}

MiniData TxBody::computeHash() const {
    auto bytes = serialise();
    return crypto::Hash::sha3_256(bytes);
}

// Wire format:
//   txn          : Transaction
//   witness      : Witness
//   burnTxns_count : uint8
//   burnTxns[]   : MiniData × count
//   txnList_count : uint8
//   txnList[]    : MiniData × count

std::vector<uint8_t> TxBody::serialise() const {
    DataStream ds;
    ds.writeBytes(txn.serialise());
    ds.writeBytes(witness.serialise());

    ds.writeUInt8((uint8_t)burnTxns.size());
    for (auto& id : burnTxns) ds.writeBytes(id.serialise());

    ds.writeUInt8((uint8_t)txnList.size());
    for (auto& id : txnList) ds.writeBytes(id.serialise());

    return ds.buffer();
}

TxBody TxBody::deserialise(const uint8_t* data, size_t& offset) {
    TxBody b;
    b.txn     = Transaction::deserialise(data, offset);
    b.witness = Witness::deserialise(data, offset);

    uint8_t burnCount = data[offset++];
    for (uint8_t i = 0; i < burnCount; ++i)
        b.burnTxns.push_back(MiniData::deserialise(data, offset));

    uint8_t txnCount = data[offset++];
    for (uint8_t i = 0; i < txnCount; ++i)
        b.txnList.push_back(MiniData::deserialise(data, offset));

    return b;
}

} // namespace minima
