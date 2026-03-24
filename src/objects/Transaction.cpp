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
    std::vector<uint8_t> buf = txID.bytes();
    buf.push_back((outputIndex >> 24) & 0xff);
    buf.push_back((outputIndex >> 16) & 0xff);
    buf.push_back((outputIndex >>  8) & 0xff);
    buf.push_back( outputIndex        & 0xff);
    return crypto::Hash::sha3_256(buf);
}

// Wire format — EXACT Java Transaction.writeDataStream:
//   inputs_count    : MiniNumber
//   inputs[]        : Coin × count
//   outputs_count   : MiniNumber
//   outputs[]       : Coin × count
//   stateVars_count : MiniNumber
//   stateVars[]     : StateVariable × count
//   mLinkHash       : MiniData (writeHashToStream = 4-byte len + bytes)

std::vector<uint8_t> Transaction::serialise() const {
    DataStream ds;
    ds.writeBytes(MiniNumber(int64_t(m_inputs.size())).serialise());
    for (auto& c : m_inputs)  ds.writeBytes(c.serialise());
    ds.writeBytes(MiniNumber(int64_t(m_outputs.size())).serialise());
    for (auto& c : m_outputs) ds.writeBytes(c.serialise());
    ds.writeBytes(MiniNumber(int64_t(m_stateVars.size())).serialise());
    for (auto& sv : m_stateVars) ds.writeBytes(sv.serialise());
    // mLinkHash: writeHashToStream = int32 len + bytes
    uint32_t hlen = (uint32_t)m_linkHash.bytes().size();
    ds.writeUInt8((hlen >> 24) & 0xff);
    ds.writeUInt8((hlen >> 16) & 0xff);
    ds.writeUInt8((hlen >>  8) & 0xff);
    ds.writeUInt8( hlen        & 0xff);
    ds.writeBytes(m_linkHash.bytes());
    return ds.buffer();
}

Transaction Transaction::deserialise(const uint8_t* data, size_t& offset) {
    Transaction t;

    auto readHash = [&]() -> MiniData {
        uint32_t len = ((uint32_t)data[offset] << 24) | ((uint32_t)data[offset+1] << 16)
                     | ((uint32_t)data[offset+2] << 8) | (uint32_t)data[offset+3];
        offset += 4;
        if (len > 65536) throw std::runtime_error("MiniData: too large");
        MiniData d(std::vector<uint8_t>(data + offset, data + offset + len));
        offset += len;
        return d;
    };

    int64_t inCount = MiniNumber::deserialise(data, offset).getAsLong();
    for (int64_t i = 0; i < inCount; ++i) t.m_inputs.push_back(Coin::deserialise(data, offset));

    int64_t outCount = MiniNumber::deserialise(data, offset).getAsLong();
    for (int64_t i = 0; i < outCount; ++i) t.m_outputs.push_back(Coin::deserialise(data, offset));

    int64_t svCount = MiniNumber::deserialise(data, offset).getAsLong();
    for (int64_t i = 0; i < svCount; ++i) t.m_stateVars.push_back(StateVariable::deserialise(data, offset));

    t.m_linkHash = readHash();
    return t;
}

} // namespace minima
