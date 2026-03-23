#include "TxBody.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"
#include <random>
#include <chrono>

namespace minima {

TxBody::TxBody() {
    resetPRNG();
    txnDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF)); // max = easiest
}

void TxBody::resetPRNG() {
    std::vector<uint8_t> rnd(32);
    std::mt19937_64 rng(
        (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    for (auto& b : rnd) b = dist(rng);
    prng = MiniData(rnd);
}

bool TxBody::isEmptyTxn() const {
    return txn.inputs().empty() && txn.outputs().empty();
}

MiniData TxBody::computeHash() const {
    return crypto::Hash::sha3_256(serialise());
}

std::vector<uint8_t> TxBody::serialise() const {
    DataStream ds;
    ds.writeBytes(prng.serialise());
    ds.writeBytes(txnDifficulty.serialise());
    ds.writeBytes(txn.serialise());
    ds.writeBytes(witness.serialise());
    ds.writeBytes(burnTxn.serialise());
    ds.writeBytes(burnWitness.serialise());
    MiniNumber count(int64_t(txnList.size()));
    ds.writeBytes(count.serialise());
    for (const auto& id : txnList) ds.writeBytes(id.serialise());
    return ds.buffer();
}

TxBody TxBody::deserialise(const uint8_t* data, size_t& offset) {
    TxBody b;
    b.prng          = MiniData::deserialise(data, offset);
    b.txnDifficulty = MiniData::deserialise(data, offset);
    b.txn           = Transaction::deserialise(data, offset);
    b.witness       = Witness::deserialise(data, offset);
    b.burnTxn       = Transaction::deserialise(data, offset);
    b.burnWitness   = Witness::deserialise(data, offset);
    int64_t cnt = MiniNumber::deserialise(data, offset).getAsLong();
    b.txnList.reserve((size_t)cnt);
    for (int64_t i = 0; i < cnt; ++i)
        b.txnList.push_back(MiniData::deserialise(data, offset));
    return b;
}

} // namespace minima
