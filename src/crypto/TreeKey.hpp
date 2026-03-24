#pragma once
/**
 * TreeKey — Merkle tree of Winternitz OTS keys
 * Java-exact port of org.minima.objects.keys.TreeKey
 * 
 * A TreeKey has:
 *   - 2^depth leaves (keys)
 *   - each leaf = a WOTS keypair (derived from master seed + leaf index)
 *   - root = Merkle root of all leaf public keys (SHA3-256 tree)
 *   - sign(data): picks the next unused leaf, signs, produces SignatureProof
 *   - verify: checks WOTS sig + Merkle proof
 * 
 * Default: depth=12 → 4096 one-time signatures per TreeKey
 */

#include "Winternitz.hpp"
#include "Hash.hpp"
#include "../types/MiniData.hpp"
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace minima::crypto {

// ── Merkle authentication path ────────────────────────────────────────────
struct MerklePath {
    std::vector<std::vector<uint8_t>> nodes;  // sibling hashes (32 bytes each)

    bool empty() const { return nodes.empty(); }
    size_t size() const { return nodes.size(); }
};

// ── Result of a single WOTS sign + Merkle proof ──────────────────────────
struct SignatureProof {
    MiniData rootPublicKey;   // 32-byte Merkle root
    int      leafIndex{0};    // which leaf was used
    MiniData publicKey;       // leaf WOTS public key (2880 bytes)
    MiniData signature;       // WOTS signature (2880 bytes)
    MerklePath proof;         // Merkle auth path

    bool isValid() const {
        return !rootPublicKey.empty() &&
               !publicKey.empty() &&
               !signature.empty();
    }
};

// ── TreeKey ───────────────────────────────────────────────────────────────
class TreeKey {
public:
    static constexpr int DEFAULT_DEPTH = 12;  // 2^12 = 4096 keys

    TreeKey() = default;

    /**
     * Create a TreeKey from a 32-byte master seed.
     */
    explicit TreeKey(const MiniData& masterSeed, int depth = DEFAULT_DEPTH)
        : mDepth(depth), mMasterSeed(masterSeed), mKeyUsed(0) {
        buildMerkleTree();
    }

    // ── Public API ───────────────────────────────────────────────────────

    MiniData getPublicKey() const { return mRoot; }

    int depth()    const { return mDepth; }
    int keyUsed()  const { return mKeyUsed; }
    int keysLeft() const { return (1 << mDepth) - mKeyUsed; }
    bool canSign() const { return keysLeft() > 0; }

    /**
     * Sign data with the next available leaf.
     */
    SignatureProof sign(const MiniData& data) {
        if (!canSign()) throw std::runtime_error("TreeKey exhausted");

        int leaf = mKeyUsed++;

        auto leafSeed = deriveLeafSeed(mMasterSeed.bytes(), leaf);

        // WOTS sign
        Winternitz::SigData wotsig = Winternitz::sign(leafSeed, data.bytes());
        Winternitz::PubKey  wotpub = Winternitz::generatePublicKey(leafSeed);

        // Merkle auth path
        MerklePath path = buildAuthPath(leaf);

        SignatureProof sp;
        sp.rootPublicKey = mRoot;
        sp.leafIndex     = leaf;
        sp.publicKey     = MiniData(wotpub);
        sp.signature     = MiniData(wotsig);
        sp.proof         = path;
        return sp;
    }

    // ── Verification (static) ────────────────────────────────────────────

    /**
     * Verify a SignatureProof against a root public key.
     * 1. WOTS verify
     * 2. Merkle path verify
     */
    static bool verify(const MiniData& rootPubKey,
                       const MiniData& data,
                       const SignatureProof& proof) {
        // Step 1: WOTS verify
        bool wotsOk = Winternitz::verify(
            proof.publicKey.bytes(),
            data.bytes(),
            proof.signature.bytes());
        if (!wotsOk) return false;

        // Step 2: Merkle path verify
        // Start: hash of leaf pubkey
        auto current = sha3_32(proof.publicKey.bytes());

        int idx = proof.leafIndex;
        for (const auto& sibling : proof.proof.nodes) {
            std::vector<uint8_t> combined;
            combined.reserve(64);
            if (idx & 1) {
                // current is right child → sibling || current
                combined.insert(combined.end(), sibling.begin(), sibling.end());
                combined.insert(combined.end(), current.begin(), current.end());
            } else {
                // current is left child → current || sibling
                combined.insert(combined.end(), current.begin(), current.end());
                combined.insert(combined.end(), sibling.begin(), sibling.end());
            }
            current = sha3_32(combined);
            idx >>= 1;
        }

        return current == rootPubKey.bytes();
    }

private:
    int      mDepth{DEFAULT_DEPTH};
    MiniData mMasterSeed;
    int      mKeyUsed{0};
    MiniData mRoot;

    // Flattened Merkle tree stored as raw 32-byte vectors
    // Index 1 = root, leaves at [numLeaves..2*numLeaves-1]
    std::vector<std::vector<uint8_t>> mTree;

    void buildMerkleTree() {
        int numLeaves = 1 << mDepth;
        mTree.resize(2 * numLeaves);

        // Fill leaves: SHA3-256(WOTS pubkey)
        for (int i = 0; i < numLeaves; ++i) {
            auto seed = deriveLeafSeed(mMasterSeed.bytes(), i);
            auto pub  = Winternitz::generatePublicKey(seed);
            mTree[numLeaves + i] = sha3_32(pub);
        }

        // Build internal nodes bottom-up
        for (int i = numLeaves - 1; i >= 1; --i) {
            std::vector<uint8_t> combined;
            combined.reserve(64);
            combined.insert(combined.end(),
                mTree[2*i].begin(),   mTree[2*i].end());
            combined.insert(combined.end(),
                mTree[2*i+1].begin(), mTree[2*i+1].end());
            mTree[i] = sha3_32(combined);
        }

        mRoot = MiniData(mTree[1]);
    }

    MerklePath buildAuthPath(int leaf) const {
        int numLeaves = 1 << mDepth;
        int idx = numLeaves + leaf;
        MerklePath path;
        while (idx > 1) {
            int sibling = (idx & 1) ? idx - 1 : idx + 1;
            path.nodes.push_back(mTree[sibling]);
            idx >>= 1;
        }
        return path;
    }

    static std::vector<uint8_t> deriveLeafSeed(
            const std::vector<uint8_t>& master, int index) {
        std::vector<uint8_t> data;
        data.reserve(master.size() + 4);
        data.insert(data.end(), master.begin(), master.end());
        data.push_back(static_cast<uint8_t>((index >> 24) & 0xFF));
        data.push_back(static_cast<uint8_t>((index >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((index >>  8) & 0xFF));
        data.push_back(static_cast<uint8_t>( index        & 0xFF));
        return sha3_32(data);
    }

    // Internal SHA3-256 helper → returns std::vector<uint8_t> (32 bytes)
    static std::vector<uint8_t> sha3_32(const std::vector<uint8_t>& data) {
        return Hash::sha3_256(data.data(), data.size()).bytes();
    }
};

} // namespace minima::crypto
