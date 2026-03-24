#pragma once
/**
 * Witness — proof data for transaction inputs.
 *
 * Wire-exact port of org.minima.objects.Witness (Java Minima Core).
 *
 * Java structure (Witness.java):
 *   Witness {
 *       ArrayList<Signature>   mSignatureProofs;  // WOTS tree signatures
 *       ArrayList<CoinProof>   mCoinProofs;        // MMR proofs for input coins
 *       ArrayList<ScriptProof> mScriptProofs;      // Script + MMR address proof
 *   }
 *
 * Wire format (MiniNumber size prefix for each list):
 *   MiniNumber(num_signatures)
 *   for each Signature:
 *       MiniNumber(num_sig_proofs)
 *       for each SignatureProof:
 *           publicKey (MiniData)    ← WOTS leaf pubkey
 *           signature (MiniData)    ← WOTS signature
 *           proof (MMRProof)        ← Merkle path to TreeKey root
 *   MiniNumber(num_coin_proofs)
 *   for each CoinProof:
 *       coin (Coin)
 *       proof (MMRProof)
 *   MiniNumber(num_script_proofs)
 *   for each ScriptProof:
 *       script (MiniString)
 *       proof (MMRProof)
 */

#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../types/MiniString.hpp"
#include "../mmr/MMRProof.hpp"
#include "../mmr/MMRData.hpp"
#include "Coin.hpp"
#include "CoinProof.hpp"
#include <optional>
#include <vector>

namespace minima {

// ── SignatureProof ─────────────────────────────────────────────────────────
// Java: org.minima.objects.keys.SignatureProof
// One WOTS leaf signature with its Merkle proof path to the TreeKey root.
struct SignatureProof {
    MiniData mPublicKey;    // WOTS leaf public key
    MiniData mSignature;    // WOTS signature
    MMRProof mProof;        // Merkle proof → TreeKey root

    // Compute the root public key: proof.calculateProof(MMRData(publicKey)).getData()
    // Java: getRootPublicKey() = mProof.calculateProof(MMRData.CreateMMRDataLeafNode(mPublicKey, ZERO))
    MiniData getRootPublicKey() const;

    bool isValid() const {
        return !mPublicKey.empty() && !mSignature.empty();
    }

    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto append = [&](const std::vector<uint8_t>& v) {
            out.insert(out.end(), v.begin(), v.end());
        };
        append(mPublicKey.serialise());
        append(mSignature.serialise());
        append(mProof.serialise());
        return out;
    }

    static SignatureProof deserialise(const uint8_t* data, size_t& off) {
        SignatureProof sp;
        sp.mPublicKey = MiniData::deserialise(data, off);
        sp.mSignature = MiniData::deserialise(data, off);
        sp.mProof     = MMRProof::deserialise(data, off);
        return sp;
    }
};

// ── Signature ─────────────────────────────────────────────────────────────
// Java: org.minima.objects.keys.Signature
// A list of SignatureProof objects (usually 1 for single-key signing).
// rootPublicKey is derived from the first proof's calculateProof().
struct Signature {
    std::vector<SignatureProof> mSignatures;

    void addSignatureProof(const SignatureProof& sp) {
        mSignatures.push_back(sp);
    }

    // Root public key = first proof's root (all should have same root)
    MiniData getRootPublicKey() const {
        if (mSignatures.empty()) return MiniData();
        return mSignatures[0].getRootPublicKey();
    }

    bool empty() const { return mSignatures.empty(); }

    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto append = [&](const std::vector<uint8_t>& v) {
            out.insert(out.end(), v.begin(), v.end());
        };
        // MiniNumber(count)
        append(MiniNumber(int64_t(mSignatures.size())).serialise());
        for (const auto& sp : mSignatures)
            append(sp.serialise());
        return out;
    }

    static Signature deserialise(const uint8_t* data, size_t& off) {
        Signature s;
        int64_t cnt = MiniNumber::deserialise(data, off).getAsLong();
        s.mSignatures.reserve(static_cast<size_t>(cnt));
        for (int64_t i = 0; i < cnt; ++i)
            s.mSignatures.push_back(SignatureProof::deserialise(data, off));
        return s;
    }
};

// CoinProof is defined in CoinProof.hpp
// (Coin + MMRProof, wire-exact port of org.minima.objects.CoinProof)

// ── ScriptProof ────────────────────────────────────────────────────────────
// Java: org.minima.objects.ScriptProof
// A KISS VM script plus its MMR proof that script→address is valid.
struct ScriptProof {
    MiniString mScript;    // plaintext KISS VM script
    MMRProof   mProof;     // MMR proof (root = address of script)

    // Compute the address this script resolves to (= MMR root derived from proof)
    // Java: mProof.calculateProof(MMRData.CreateMMRDataLeafNode(mScript, ZERO)).getData()
    MiniData address() const;

    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto append = [&](const std::vector<uint8_t>& v) {
            out.insert(out.end(), v.begin(), v.end());
        };
        append(mScript.serialise());
        append(mProof.serialise());
        return out;
    }

    static ScriptProof deserialise(const uint8_t* data, size_t& off) {
        ScriptProof sp;
        sp.mScript = MiniString::deserialise(data, off);
        sp.mProof  = MMRProof::deserialise(data, off);
        return sp;
    }
};

// ── Witness ────────────────────────────────────────────────────────────────
class Witness {
public:
    Witness() = default;

    // ── Mutation ──────────────────────────────────────────────────────────
    void addSignature  (const Signature&   sig) { m_signatures.push_back(sig); }
    void addCoinProof  (const CoinProof&   cp)  { m_coinProofs.push_back(cp); }
    void addScriptProof(const ScriptProof& sp)  { m_scripts.push_back(sp); }

    // ── Query ─────────────────────────────────────────────────────────────
    const std::vector<Signature>&   signatures()  const { return m_signatures; }
    const std::vector<CoinProof>&   coinProofs()  const { return m_coinProofs; }
    const std::vector<ScriptProof>& scriptProofs() const { return m_scripts; }

    // Lookup script for a given address
    std::optional<MiniString> scriptForAddress(const MiniData& address) const;

    // Check if signed by given root public key
    bool isSignedBy(const MiniData& rootPubKey) const;

    // ── Wire format ───────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const;
    static Witness       deserialise(const uint8_t* data, size_t& offset);
    static Witness       deserialise(const uint8_t* data, size_t& offset, size_t total_size);

    bool isEmpty() const {
        return m_signatures.empty() && m_coinProofs.empty() && m_scripts.empty();
    }

private:
    std::vector<Signature>   m_signatures;
    std::vector<CoinProof>   m_coinProofs;
    std::vector<ScriptProof> m_scripts;
};

} // namespace minima
