#include "Witness.hpp"
#include "../serialization/DataStream.hpp"
#include "../crypto/Hash.hpp"

namespace minima {

// ── ScriptProof::address() ────────────────────────────────────────────────
MiniData ScriptProof::address() const {
    // Java: Crypto.getInstance().hashData(script.getBytes())
    // = SHA3-256 of the raw UTF-8 bytes of the script string
    const std::string& s = script.str();
    return crypto::Hash::sha3_256(
        reinterpret_cast<const uint8_t*>(s.data()),
        s.size());
}

// ── Witness serialise ─────────────────────────────────────────────────────
std::vector<uint8_t> Witness::serialise() const {
    DataStream ds;

    // Signatures
    ds.writeUInt8(static_cast<uint8_t>(m_signatures.size()));
    for (const auto& sig : m_signatures) {
        ds.writeBytes(sig.serialise());
    }

    // ScriptProofs
    ds.writeUInt8(static_cast<uint8_t>(m_scripts.size()));
    for (const auto& sp : m_scripts) {
        ds.writeBytes(sp.serialise());
    }

    return ds.buffer();
}

// ── Witness deserialise ───────────────────────────────────────────────────
Witness Witness::deserialise(const uint8_t* data, size_t& offset) {
    Witness w;

    // Signatures
    uint8_t sigCount = data[offset++];
    w.m_signatures.reserve(sigCount);
    for (uint8_t i = 0; i < sigCount; ++i)
        w.m_signatures.push_back(Signature::deserialise(data, offset));

    // ScriptProofs
    uint8_t spCount = data[offset++];
    w.m_scripts.reserve(spCount);
    for (uint8_t i = 0; i < spCount; ++i)
        w.m_scripts.push_back(ScriptProof::deserialise(data, offset));

    return w;
}

// ── scriptForAddress ──────────────────────────────────────────────────────
std::optional<MiniString> Witness::scriptForAddress(const MiniData& address) const {
    for (const auto& sp : m_scripts) {
        if (sp.address() == address)
            return sp.script;
    }
    return std::nullopt;
}

} // namespace minima
