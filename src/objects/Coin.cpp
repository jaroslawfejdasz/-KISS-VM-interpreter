#include "Coin.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"
#include <stdexcept>

namespace minima {

Coin& Coin::addStateVar(const StateVariable& sv) {
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

// Wire format — EXACT Java Coin.writeDataStream:
//   mCoinID         : MiniData (writeHashToStream)
//   mAddress        : MiniData (writeHashToStream)
//   mAmount         : MiniNumber
//   mTokenID        : MiniData (writeHashToStream)
//   mStoreState     : MiniByte (1 byte bool)
//   mMMREntryNumber : MiniNumber
//   mSpent          : MiniNumber
//   mBlockCreated   : MiniNumber
//   mState.size()   : MiniNumber
//   mState[]        : StateVariable × count
//   hasToken        : MiniByte (1 byte bool)
//   mToken          : Token (if hasToken)

static void writeHash(DataStream& ds, const MiniData& d) {
    uint32_t len = (uint32_t)d.bytes().size();
    ds.writeUInt8((len >> 24) & 0xff);
    ds.writeUInt8((len >> 16) & 0xff);
    ds.writeUInt8((len >>  8) & 0xff);
    ds.writeUInt8( len        & 0xff);
    ds.writeBytes(d.bytes());
}

static MiniData readHash(const uint8_t* data, size_t& offset) {
    uint32_t len = ((uint32_t)data[offset] << 24) | ((uint32_t)data[offset+1] << 16)
                 | ((uint32_t)data[offset+2] << 8) | (uint32_t)data[offset+3];
    offset += 4;
    if (len > 65536) throw std::runtime_error("MiniData: too large");
    MiniData d(std::vector<uint8_t>(data + offset, data + offset + len));
    offset += len;
    return d;
}

std::vector<uint8_t> Coin::serialise() const {
    DataStream ds;
    writeHash(ds, m_coinID);
    writeHash(ds, m_address.hash());
    ds.writeBytes(m_amount.serialise());
    writeHash(ds, m_tokenID.value_or(MiniData(std::vector<uint8_t>{0x00})));
    ds.writeUInt8(m_storeState ? 1 : 0);
    ds.writeBytes(m_mmrEntry.serialise());
    ds.writeBytes(MiniNumber(int64_t(m_spent ? 1 : 0)).serialise());
    ds.writeBytes(m_blockCreated.serialise());
    ds.writeBytes(MiniNumber(int64_t(m_stateVars.size())).serialise());
    for (auto& sv : m_stateVars) ds.writeBytes(sv.serialise());
    ds.writeUInt8(0); // no Token
    return ds.buffer();
}

Coin Coin::deserialise(const uint8_t* data, size_t& offset) {
    Coin c;
    c.m_coinID  = readHash(data, offset);
    auto addrHash = readHash(data, offset);
    c.m_address = Address(addrHash);
    c.m_amount  = MiniNumber::deserialise(data, offset);
    auto tokenID = readHash(data, offset);
    // tokenID 0x00 = native Minima, others = custom token
    c.m_tokenID = tokenID;

    c.m_storeState  = data[offset++] != 0;
    c.m_mmrEntry    = MiniNumber::deserialise(data, offset);
    // mSpent is MiniNumber in Java
    auto spentNum   = MiniNumber::deserialise(data, offset);
    c.m_spent       = (spentNum.getAsLong() != 0);
    c.m_blockCreated = MiniNumber::deserialise(data, offset);

    int64_t svCount = MiniNumber::deserialise(data, offset).getAsLong();
    for (int64_t i = 0; i < svCount; ++i)
        c.m_stateVars.push_back(StateVariable::deserialise(data, offset));

    uint8_t hasToken = data[offset++];
    if (hasToken) {
        // Token.deserialise — skip for now (read all its fields)
        // Token: mCoinID(hash), mScript(MiniString), mTotalTokens(MiniNumber),
        //        mDecimals(MiniNumber), mName(MiniString), mEngineVersion(MiniNumber)
        readHash(data, offset);  // coinID
        // MiniString: 2-byte len + bytes
        uint16_t slen = ((uint16_t)data[offset] << 8) | data[offset+1]; offset += 2;
        offset += slen; // script
        MiniNumber::deserialise(data, offset); // total tokens
        MiniNumber::deserialise(data, offset); // decimals
        slen = ((uint16_t)data[offset] << 8) | data[offset+1]; offset += 2;
        offset += slen; // name
        MiniNumber::deserialise(data, offset); // engine version
    }

    return c;
}

MiniData Coin::hashValue() const {
    auto bytes = serialise();
    return crypto::Hash::sha3_256(bytes.data(), bytes.size());
}

} // namespace minima
