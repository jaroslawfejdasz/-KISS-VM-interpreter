#include "Witness.hpp"
#include "../crypto/Hash.hpp"

namespace minima {

// ── SignatureProof::getRootPublicKey() ────────────────────────────────────
// Java: mProof.calculateProof(MMRData.CreateMMRDataLeafNode(mPublicKey, MiniNumber.ZERO)).getData()
// "CreateMMRDataLeafNode" hashes: SHA3(MiniNumber(0).bytes() + pubkey.bytes())
// But simpler: MMRData leaf = SHA3(serialise(MiniNumber.ZERO) || serialise(mPublicKey))
// See MMRData::CreateMMRDataLeafNode in Java:
//   public static MMRData CreateMMRDataLeafNode(Streamable zObject, MiniNumber zValue){
//       byte[] data = MiniFormat.encodeMiniData(zObject);
//       MiniData hash = Crypto.getInstance().hashData(data);
//       return new MMRData(hash, zValue);
//   }
MiniData SignatureProof::getRootPublicKey() const {
    // Hash the public key bytes (MiniData.writeDataStream = 4-byte length + bytes)
    auto pkBytes = mPublicKey.serialise();
    MiniData leafHash = crypto::Hash::sha3_256(pkBytes.data(), pkBytes.size());

    // Create leaf MMRData (hash=leafHash, value=0)
    MMRData leafData(leafHash, MiniNumber::ZERO, false);

    // Walk the proof to compute root
    MMRData root = mProof.calculateProof(leafData);
    return root.getData();
}

// ── CoinProof::getMMRData() ───────────────────────────────────────────────
// Java: MMRData.CreateMMRDataLeafNode(getCoin(), getCoin().getAmount())
//   = hash(coin.serialise()), value = coin.amount()
MMRData CoinProof::getMMRData() const {
    auto coinBytes = m_coin.serialise();
    MiniData leafHash = crypto::Hash::sha3_256(coinBytes.data(), coinBytes.size());
    return MMRData(leafHash, m_coin.amount(), false);
}

// ── ScriptProof::address() ────────────────────────────────────────────────
// Java: mProof.calculateProof(MMRData.CreateMMRDataLeafNode(mScript, ZERO)).getData()
//   = root of MMR tree where leaf = hash(script.serialise()), value = 0
MiniData ScriptProof::address() const {
    auto scriptBytes = mScript.serialise();
    MiniData leafHash = crypto::Hash::sha3_256(scriptBytes.data(), scriptBytes.size());
    MMRData  leafData(leafHash, MiniNumber::ZERO, false);
    MMRData  root = mProof.calculateProof(leafData);
    return root.getData();
}

// ── Witness serialise ─────────────────────────────────────────────────────
// Wire format (Java Witness.writeDataStream):
//   MiniNumber(num_signatures)
//   for each Signature: Signature.writeDataStream()
//   MiniNumber(num_coin_proofs)
//   for each CoinProof: CoinProof.writeDataStream()
//   MiniNumber(num_script_proofs)
//   for each ScriptProof: ScriptProof.writeDataStream()
std::vector<uint8_t> Witness::serialise() const {
    std::vector<uint8_t> out;
    auto append = [&](const std::vector<uint8_t>& v) {
        out.insert(out.end(), v.begin(), v.end());
    };

    // Signatures
    append(MiniNumber(int64_t(m_signatures.size())).serialise());
    for (const auto& sig : m_signatures)
        append(sig.serialise());

    // CoinProofs
    append(MiniNumber(int64_t(m_coinProofs.size())).serialise());
    for (const auto& cp : m_coinProofs)
        append(cp.serialise());

    // ScriptProofs
    append(MiniNumber(int64_t(m_scripts.size())).serialise());
    for (const auto& sp : m_scripts)
        append(sp.serialise());

    return out;
}

// ── Witness deserialise ───────────────────────────────────────────────────
Witness Witness::deserialise(const uint8_t* data, size_t& offset, size_t total_size) {
    Witness w;

    // Signatures
    int64_t sigCount = MiniNumber::deserialise(data, offset).getAsLong();
    w.m_signatures.reserve(static_cast<size_t>(sigCount));
    for (int64_t i = 0; i < sigCount; ++i)
        w.m_signatures.push_back(Signature::deserialise(data, offset));

    // CoinProofs — pass total_size so MMRProof can do safe bounds-checked read
    int64_t cpCount = MiniNumber::deserialise(data, offset).getAsLong();
    w.m_coinProofs.reserve(static_cast<size_t>(cpCount));
    for (int64_t i = 0; i < cpCount; ++i)
        w.m_coinProofs.push_back(CoinProof::deserialise(data, offset, total_size));

    // ScriptProofs
    int64_t spCount = MiniNumber::deserialise(data, offset).getAsLong();
    w.m_scripts.reserve(static_cast<size_t>(spCount));
    for (int64_t i = 0; i < spCount; ++i)
        w.m_scripts.push_back(ScriptProof::deserialise(data, offset));

    return w;
}

// backward compat — no total_size
Witness Witness::deserialise(const uint8_t* data, size_t& offset) {
    return deserialise(data, offset, SIZE_MAX);
}

// ── scriptForAddress ──────────────────────────────────────────────────────
std::optional<MiniString> Witness::scriptForAddress(const MiniData& address) const {
    for (const auto& sp : m_scripts) {
        if (sp.address() == address)
            return sp.mScript;
    }
    return std::nullopt;
}

// ── isSignedBy ────────────────────────────────────────────────────────────
bool Witness::isSignedBy(const MiniData& rootPubKey) const {
    for (const auto& sig : m_signatures) {
        if (sig.getRootPublicKey() == rootPubKey)
            return true;
    }
    return false;
}

} // namespace minima
