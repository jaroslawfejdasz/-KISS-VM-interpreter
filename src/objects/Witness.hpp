#pragma once
/**
 * Witness — proof data for transaction inputs.
 *
 * Wire-exact port of org.minima.objects.Witness (Java Minima Core).
 *
 * Java structure:
 *   Witness {
 *       ArrayList<Signature>   mSignatures;    // list of Signature objects
 *       ArrayList<ScriptProof> mScriptProofs;  // list of ScriptProof objects
 *   }
 *
 *   Signature {
 *       MiniData   mRootPublicKey;  // TreeKey root pubkey (32 bytes)
 *       int        mLeafIndex;      // which WOTS leaf was used
 *       MiniData   mPublicKey;      // WOTS leaf public key (2880 bytes)
 *       MiniData   mSignature;      // WOTS signature (2880 bytes)
 *       MerklePath mProof;          // Merkle auth path (list of 32-byte nodes)
 *   }
 *
 *   ScriptProof {
 *       MiniString mScript;         // plaintext KISS VM script
 *       MMRProof   mMMRProof;       // MMR proof that script→address is in coin MMR
 *   }
 *
 * Wire format (serialise):
 *   [1 byte]  num_signatures
 *   for each Signature:
 *       rootPublicKey.serialise()
 *       leafIndex as MiniNumber.serialise()
 *       publicKey.serialise()
 *       signature.serialise()
 *       [1 byte] merkle_path_length
 *       for each path node: [32 bytes]
 *   [1 byte]  num_script_proofs
 *   for each ScriptProof:
 *       script.serialise()
 *       mmrProof.serialise()
 */

#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../types/MiniString.hpp"
#include "../mmr/MMRProof.hpp"
#include <optional>
#include <vector>

namespace minima {

// ── Merkle authentication path for WOTS / TreeKey ─────────────────────────
struct MerklePath {
    std::vector<MiniData> nodes;   // sibling hashes (32 bytes each)

    bool   empty() const { return nodes.empty(); }
    size_t size()  const { return nodes.size(); }

    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        out.push_back(static_cast<uint8_t>(nodes.size()));
        for (const auto& n : nodes) {
            auto nb = n.serialise();
            out.insert(out.end(), nb.begin(), nb.end());
        }
        return out;
    }

    static MerklePath deserialise(const uint8_t* data, size_t& off) {
        MerklePath mp;
        uint8_t cnt = data[off++];
        mp.nodes.reserve(cnt);
        for (uint8_t i = 0; i < cnt; ++i)
            mp.nodes.push_back(MiniData::deserialise(data, off));
        return mp;
    }
};

// ── Signature — one WOTS TreeKey signature with Merkle auth path ───────────
// Wire-exact port of org.minima.objects.Signature
struct Signature {
    MiniData   rootPublicKey;   // TreeKey Merkle root (32 bytes)
    MiniNumber leafIndex;       // which leaf (used as MiniNumber for wire format)
    MiniData   publicKey;       // WOTS leaf pub key (2880 bytes)
    MiniData   signature;       // WOTS signature (2880 bytes)
    MerklePath proof;           // Merkle auth path

    bool isValid() const {
        return !rootPublicKey.empty() && !publicKey.empty() && !signature.empty();
    }

    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto append = [&](const std::vector<uint8_t>& v) {
            out.insert(out.end(), v.begin(), v.end());
        };
        append(rootPublicKey.serialise());
        append(leafIndex.serialise());
        append(publicKey.serialise());
        append(signature.serialise());
        append(proof.serialise());
        return out;
    }

    static Signature deserialise(const uint8_t* data, size_t& off) {
        Signature s;
        s.rootPublicKey = MiniData::deserialise(data, off);
        s.leafIndex     = MiniNumber::deserialise(data, off);
        s.publicKey     = MiniData::deserialise(data, off);
        s.signature     = MiniData::deserialise(data, off);
        s.proof         = MerklePath::deserialise(data, off);
        return s;
    }
};

// ── ScriptProof — script + MMR proof that script→address is valid ──────────
// Wire-exact port of org.minima.objects.ScriptProof
struct ScriptProof {
    MiniString script;    // plaintext KISS VM script
    MMRProof   mmrProof;  // MMR proof for the address derived from script

    // Helper — compute the address this script resolves to: SHA3(script_bytes)
    // (matches Java: new MiniData(Crypto.getInstance().hashData(script.getBytes())))
    MiniData address() const;

    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto append = [&](const std::vector<uint8_t>& v) {
            out.insert(out.end(), v.begin(), v.end());
        };
        append(script.serialise());
        append(mmrProof.serialise());
        return out;
    }

    static ScriptProof deserialise(const uint8_t* data, size_t& off) {
        ScriptProof sp;
        sp.script   = MiniString::deserialise(data, off);
        sp.mmrProof = MMRProof::deserialise(data, off);
        return sp;
    }
};

// ── Witness ────────────────────────────────────────────────────────────────
class Witness {
public:
    Witness() = default;

    // ── Mutation ──────────────────────────────────────────────────────────
    void addSignature (const Signature&   sig) { m_signatures.push_back(sig); }
    void addScriptProof(const ScriptProof& sp)  { m_scripts.push_back(sp); }

    // ── Query ─────────────────────────────────────────────────────────────
    const std::vector<Signature>&   signatures()  const { return m_signatures; }
    const std::vector<ScriptProof>& scriptProofs() const { return m_scripts; }

    // Convenience: look up script for a given address (SHA3 comparison)
    std::optional<MiniString> scriptForAddress(const MiniData& address) const;

    // ── Wire format ───────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const;
    static Witness       deserialise(const uint8_t* data, size_t& offset);

    // Compactness check (used by validator)
    bool isEmpty() const { return m_signatures.empty() && m_scripts.empty(); }

private:
    std::vector<Signature>   m_signatures;
    std::vector<ScriptProof> m_scripts;
};

} // namespace minima
