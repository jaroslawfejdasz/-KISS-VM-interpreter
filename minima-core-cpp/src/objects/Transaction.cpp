#include "Transaction.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"

namespace minima {

void Transaction::addInput(const Coin& coin)  { m_inputs.push_back(coin); }
void Transaction::addOutput(const Coin& coin) { m_outputs.push_back(coin); }

void Transaction::addStateVar(const StateVariable& sv) {
    m_stateVars.erase(
        std::remove_if(m_stateVars.begin(), m_stateVars.end(),
            [&](const StateVariable& s){ return s.port() == sv.port(); }),
        m_stateVars.end());
    m_stateVars.push_back(sv);
}

std::optional<StateVariable> Transaction::stateVar(uint8_t port) const {
    for (auto& sv : m_stateVars)
        if (sv.port() == port) return sv;
    return std::nullopt;
}

bool Transaction::isValid() const {
    // Basic sanity: at least 1 output, no duplicate input coinIDs
    if (m_outputs.empty()) return false;
    for (size_t i = 0; i < m_inputs.size(); ++i)
        for (size_t j = i + 1; j < m_inputs.size(); ++j)
            if (m_inputs[i].coinID() == m_inputs[j].coinID()) return false;
    return true;
}

bool Transaction::inputsBalance() const {
    MiniNumber sumIn, sumOut;
    for (auto& c : m_inputs)  sumIn  = sumIn.add(c.amount());
    for (auto& c : m_outputs) sumOut = sumOut.add(c.amount());
    return sumIn.isEqual(sumOut);
}

MiniData Transaction::computeCoinID(const MiniData& txID, uint32_t outputIndex) {
    // CoinID = SHA3(txID_bytes + big-endian uint32 output index)
    std::vector<uint8_t> buf = txID.bytes();
    buf.push_back((outputIndex >> 24) & 0xff);
    buf.push_back((outputIndex >> 16) & 0xff);
    buf.push_back((outputIndex >>  8) & 0xff);
    buf.push_back( outputIndex        & 0xff);
    return crypto::Hash::sha3_256(buf);
}

// Wire format:
//   inputs_count  : uint8
//   inputs[]      : Coin × count
//   outputs_count : uint8
//   outputs[]     : Coin × count
//   stateVars_count : uint8
//   stateVars[]   : StateVariable × count

std::vector<uint8_t> Transaction::serialise() const {
    DataStream ds;
    ds.writeUInt8((uint8_t)m_inputs.size());
    for (auto& c : m_inputs)  ds.writeBytes(c.serialise());
    ds.writeUInt8((uint8_t)m_outputs.size());
    for (auto& c : m_outputs) ds.writeBytes(c.serialise());
    ds.writeUInt8((uint8_t)m_stateVars.size());
    for (auto& sv : m_stateVars) ds.writeBytes(sv.serialise());
    return ds.buffer();
}

Transaction Transaction::deserialise(const uint8_t* data, size_t& offset) {
    Transaction t;
    uint8_t inCount = data[offset++];
    for (uint8_t i = 0; i < inCount; ++i) t.m_inputs.push_back(Coin::deserialise(data, offset));
    uint8_t outCount = data[offset++];
    for (uint8_t i = 0; i < outCount; ++i) t.m_outputs.push_back(Coin::deserialise(data, offset));
    uint8_t svCount = data[offset++];
    for (uint8_t i = 0; i < svCount; ++i) t.m_stateVars.push_back(StateVariable::deserialise(data, offset));
    return t;
}

} // namespace minima
