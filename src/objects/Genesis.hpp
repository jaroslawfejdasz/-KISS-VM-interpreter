#pragma once
/**
 * Genesis — Bootstrap objects for block #0.
 *
 * Wire-compatible port of Minima Java genesis construction:
 *   - makeGenesisCoin()   → 1-billion-Minima initial UTxO
 *   - makeGenesisMMR()    → MMRSet with genesis coin as leaf #0
 *   - makeGenesisTxPoW()  → deterministic block #0
 *   - isGenesisBlock()    → predicate
 *
 * Java refs: GeneralParams, TxPoWMiner.createGenesisBlock(), MinimaDB.loadDB()
 *
 * Key constants:
 *   MINIMA_TOTAL_AMOUNT = "1000000000"
 *   GENESIS_SCRIPT      = "RETURN TRUE"
 *   coinID              = 0x00...00 (32 bytes)
 *   address             = SHA3-256("RETURN TRUE")
 */
#include "Coin.hpp"
#include "TxPoW.hpp"
#include "Address.hpp"
#include "../mmr/MMRSet.hpp"
#include "../mmr/MMRData.hpp"
#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniString.hpp"
#include "../crypto/Hash.hpp"
#include <string>

namespace minima {

inline const std::string& genesisScript() {
    static const std::string s = "RETURN TRUE";
    return s;
}

// ── Genesis Coin ─────────────────────────────────────────────────────────

/**
 * Build the genesis coin.
 *   coinID  = 0x00...00 (32 bytes)
 *   address = SHA3-256("RETURN TRUE")
 *   amount  = 1,000,000,000
 */
inline Coin makeGenesisCoin() {
    MiniData coinID(std::vector<uint8_t>(32, 0x00));

    // Java: new Address(new MiniString("RETURN TRUE"))
    //       → SHA3(MiniString.writeDataStream()) = SHA3(2-byte-len + utf8-bytes)
    MiniString ms(genesisScript());
    auto msBytes = ms.serialise();
    MiniData addrHash = crypto::Hash::sha3_256(msBytes.data(), msBytes.size());
    Address addr(addrHash);

    Coin c;
    c.setCoinID(coinID)
     .setAddress(addr)
     .setAmount(MiniNumber("1000000000"));
    return c;
}

// ── Genesis MMR ──────────────────────────────────────────────────────────

/**
 * Build genesis MMR: one leaf = hash(genesis coin serialisation).
 */
inline MMRSet makeGenesisMMR(const Coin& gc) {
    MMRSet mmr;
    auto coinBytes = gc.serialise();
    MiniData leafHash = crypto::Hash::sha3_256(coinBytes.data(), coinBytes.size());
    MMRData leaf;
    leaf.setData(leafHash);
    leaf.setSpent(false);
    mmr.addLeaf(leaf);
    return mmr;
}

// ── Genesis TxPoW ────────────────────────────────────────────────────────

/**
 * Build deterministic genesis TxPoW (block #0).
 *   blockNumber = 0, timeMilli = 0, nonce = 0
 *   superParents[all] = 0x00...00
 *   mmrRoot = root of genesis MMR
 *   blockDifficulty = 0xFF...FF (easiest)
 */
inline TxPoW makeGenesisTxPoW() {
    Coin    gc  = makeGenesisCoin();
    MMRSet  mmr = makeGenesisMMR(gc);
    MMRData root = mmr.getRoot();

    TxPoW txp;
    TxHeader& h = txp.header();
    h.blockNumber    = MiniNumber(int64_t(0));
    h.timeMilli      = MiniNumber(int64_t(0));
    h.nonce          = MiniNumber(int64_t(0));
    h.blockDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
    h.mmrRoot         = root.getData();

    const MiniData zero32(std::vector<uint8_t>(32, 0x00));
    for (int i = 0; i < CASCADE_LEVELS; ++i)
        h.superParents[i] = zero32;

    return txp;
}

// ── Predicate ────────────────────────────────────────────────────────────

inline bool isGenesisBlock(const TxPoW& txp) {
    if (!(txp.header().blockNumber == MiniNumber(int64_t(0)))) return false;
    const MiniData zero32(std::vector<uint8_t>(32, 0x00));
    for (int i = 0; i < CASCADE_LEVELS; ++i)
        if (!(txp.header().superParents[i] == zero32)) return false;
    return true;
}

} // namespace minima
