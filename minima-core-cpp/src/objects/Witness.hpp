#pragma once
#include <optional>
/**
 * Witness — proof data that unlocks transaction inputs.
 *
 * For each input coin, the Witness carries:
 *  - The KISS VM script (plaintext)
 *  - Schnorr signatures (one per SIGNEDBY / MULTISIG requirement)
 *  - MM proofs (Merkle proofs for MAST contracts)
 *
 * Minima reference: src/org/minima/objects/Witness.java
 */

#include "../types/MiniData.hpp"
#include "../types/MiniString.hpp"
#include <vector>

namespace minima {

struct ScriptProof {
    MiniString    script;    // plaintext KISS VM script
    MiniData      address;   // SHA3(script) — must match coin address
};

struct SignatureProof {
    MiniData pubKey;    // 64-byte Schnorr public key
    MiniData signature; // 64-byte Schnorr signature
};

class Witness {
public:
    Witness() = default;

    void addScript   (const ScriptProof& sp);
    void addSignature(const SignatureProof& sig);

    const std::vector<ScriptProof>&    scripts()    const { return m_scripts; }
    const std::vector<SignatureProof>& signatures() const { return m_signatures; }

    // Look up script for a given address
    std::optional<MiniString> scriptForAddress(const MiniData& address) const;

    std::vector<uint8_t> serialise() const;
    static Witness       deserialise(const uint8_t* data, size_t& offset);

private:
    std::vector<ScriptProof>    m_scripts;
    std::vector<SignatureProof> m_signatures;
};

} // namespace minima
