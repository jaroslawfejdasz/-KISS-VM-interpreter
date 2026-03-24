#pragma once
/**
 * Token — custom token on Minima.
 *
 * Java ref: src/org/minima/objects/Token.java
 *
 * Wire format (writeDataStream):
 *   coinID          (MiniData — writeHashToStream = 32 raw bytes)
 *   tokenScript     (MiniString — 2-byte len + UTF-8)
 *   tokenScale      (MiniNumber — len-prefixed decimal)
 *   tokenMinimaAmt  (MiniNumber)
 *   tokenName       (MiniString)
 *   tokenCreated    (MiniNumber — block number)
 *
 * TokenID = SHA3(serialised bytes)
 */
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../types/MiniString.hpp"
#include "../crypto/Hash.hpp"
#include <vector>
#include <cstdint>
#include <cstring>

namespace minima {

class Token {
public:
    // ── Special token IDs ──────────────────────────────────────────────────
    static MiniData TOKENID_MINIMA() { return MiniData(std::vector<uint8_t>{0x00}); }
    static MiniData TOKENID_CREATE() { return MiniData(std::vector<uint8_t>{0xFF}); }

    // ── Constructors ───────────────────────────────────────────────────────
    Token() = default;

    Token(const MiniData&   coinID,
          const MiniNumber& scale,
          const MiniNumber& minimaAmount,
          const MiniString& name,
          const MiniString& script,
          const MiniNumber& created = MiniNumber(int64_t(0)))
        : m_coinID(coinID)
        , m_tokenScale(scale)
        , m_tokenMinimaAmount(minimaAmount)
        , m_tokenName(name)
        , m_tokenScript(script)
        , m_tokenCreated(created)
    {
        calculateTokenID();
    }

    // ── Getters ────────────────────────────────────────────────────────────
    const MiniData&   coinID()       const { return m_coinID; }
    const MiniData&   tokenID()      const { return m_tokenID; }
    const MiniNumber& scale()        const { return m_tokenScale; }
    const MiniNumber& minimaAmount() const { return m_tokenMinimaAmount; }
    const MiniString& name()         const { return m_tokenName; }
    const MiniString& script()       const { return m_tokenScript; }
    const MiniNumber& createdBlock() const { return m_tokenCreated; }

    // ── Scale helpers ──────────────────────────────────────────────────────
    MiniNumber getScaledTokenAmount(const MiniNumber& minimaAmt) const {
        int64_t sc = m_tokenScale.getAsLong();
        MiniNumber cur = minimaAmt;
        for (int64_t i = 0; i < sc; ++i) cur = cur * MiniNumber(int64_t(10));
        return cur;
    }
    MiniNumber getScaledMinimaAmount(const MiniNumber& tokenAmt) const {
        int64_t sc = m_tokenScale.getAsLong();
        MiniNumber cur = tokenAmt;
        for (int64_t i = 0; i < sc; ++i) cur = cur / MiniNumber(int64_t(10));
        return cur;
    }
    MiniNumber totalTokens() const { return getScaledTokenAmount(m_tokenMinimaAmount); }

    // ── Serialisation (wire-exact Java order) ─────────────────────────────
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto append = [&](const std::vector<uint8_t>& v) {
            out.insert(out.end(), v.begin(), v.end());
        };
        // Java: mCoinID.writeHashToStream → 32 raw bytes
        auto coinBytes = m_coinID.bytes();
        coinBytes.resize(32, 0x00);
        out.insert(out.end(), coinBytes.begin(), coinBytes.end());
        append(m_tokenScript.serialise());
        append(m_tokenScale.serialise());
        append(m_tokenMinimaAmount.serialise());
        append(m_tokenName.serialise());
        append(m_tokenCreated.serialise());
        return out;
    }

    static Token deserialise(const uint8_t* data, size_t& offset) {
        Token t;
        // coinID: 32 raw bytes
        std::vector<uint8_t> coinBytes(data + offset, data + offset + 32);
        t.m_coinID = MiniData(coinBytes);
        offset += 32;
        t.m_tokenScript       = MiniString::deserialise(data, offset);
        t.m_tokenScale        = MiniNumber::deserialise(data, offset);
        t.m_tokenMinimaAmount = MiniNumber::deserialise(data, offset);
        t.m_tokenName         = MiniString::deserialise(data, offset);
        t.m_tokenCreated      = MiniNumber::deserialise(data, offset);
        t.calculateTokenID();
        return t;
    }

    bool operator==(const Token& rhs) const {
        return m_tokenID.bytes() == rhs.m_tokenID.bytes();
    }
    bool operator!=(const Token& rhs) const { return !(*this == rhs); }

private:
    MiniData   m_coinID;
    MiniNumber m_tokenScale   {int64_t(0)};
    MiniNumber m_tokenMinimaAmount{int64_t(0)};
    MiniString m_tokenName;
    MiniString m_tokenScript;
    MiniNumber m_tokenCreated {int64_t(0)};
    MiniData   m_tokenID;   // computed, not serialised

    void calculateTokenID() {
        auto raw = serialise();
        m_tokenID = MiniData(crypto::Hash::sha3_256(raw));
    }
};

} // namespace minima
