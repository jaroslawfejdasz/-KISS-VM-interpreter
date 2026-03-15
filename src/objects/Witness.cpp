#include "Witness.hpp"
#include "../serialization/DataStream.hpp"

namespace minima {

void Witness::addScript(const ScriptProof& sp)       { m_scripts.push_back(sp); }
void Witness::addSignature(const SignatureProof& sig) { m_signatures.push_back(sig); }

std::optional<MiniString> Witness::scriptForAddress(const MiniData& address) const {
    for (auto& sp : m_scripts)
        if (sp.address == address) return sp.script;
    return std::nullopt;
}

// Wire format:
//   scripts_count    : uint8
//   scripts[]        : { script: MiniString, address: MiniData }
//   sigs_count       : uint8
//   sigs[]           : { pubKey: MiniData, signature: MiniData }

std::vector<uint8_t> Witness::serialise() const {
    DataStream ds;

    ds.writeUInt8((uint8_t)m_scripts.size());
    for (auto& sp : m_scripts) {
        ds.writeBytes(sp.script.serialise());
        ds.writeBytes(sp.address.serialise());
    }

    ds.writeUInt8((uint8_t)m_signatures.size());
    for (auto& sig : m_signatures) {
        ds.writeBytes(sig.pubKey.serialise());
        ds.writeBytes(sig.signature.serialise());
    }

    return ds.buffer();
}

Witness Witness::deserialise(const uint8_t* data, size_t& offset) {
    Witness w;

    uint8_t scriptCount = data[offset++];
    for (uint8_t i = 0; i < scriptCount; ++i) {
        ScriptProof sp;
        sp.script  = MiniString::deserialise(data, offset);
        sp.address = MiniData::deserialise(data, offset);
        w.m_scripts.push_back(std::move(sp));
    }

    uint8_t sigCount = data[offset++];
    for (uint8_t i = 0; i < sigCount; ++i) {
        SignatureProof sig;
        sig.pubKey    = MiniData::deserialise(data, offset);
        sig.signature = MiniData::deserialise(data, offset);
        w.m_signatures.push_back(std::move(sig));
    }

    return w;
}

} // namespace minima
