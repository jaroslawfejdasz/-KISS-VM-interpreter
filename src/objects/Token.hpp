#pragma once
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../types/MiniString.hpp"
#include "../crypto/Hash.hpp"
#include <vector>
#include <string>

namespace minima {

/**
 * Token — custom token on Minima.
 *
 * Wire format (Java Token.writeDataStream):
 *   tokenID     : MiniData
 *   coinID      : MiniData
 *   totalSupply : MiniNumber
 *   scale       : MiniNumber
 *   name        : MiniString
 *   description : MiniString
 *   script      : MiniString  (KISS VM script text)
 */
class Token {
public:
    static MiniData TOKENID_MINIMA() { return MiniData(std::vector<uint8_t>{0x00}); }
    static MiniData TOKENID_CREATE() { return MiniData(std::vector<uint8_t>{0xFF}); }

    Token() = default;

    // Convenience constructor (coinID, scale, minimaAmount, name, script, [created])
    Token(const MiniData&   coinID_,
          const MiniNumber& scale_,
          const MiniNumber& minimaAmount_,
          const MiniString& name_,
          const MiniString& script_,
          const MiniNumber& created_ = MiniNumber(int64_t(0)))
        : m_coinID(coinID_)
        , m_scale(scale_)
        , m_totalSupply(minimaAmount_)
        , m_name(name_)
        , m_description(MiniString(""))
        , m_script(script_)
        , m_created(created_)
    {
        computeTokenID();
    }

    // ── Getters ────────────────────────────────────────────────────────────
    const MiniData&   tokenID()      const { return m_tokenID; }
    const MiniData&   coinID()       const { return m_coinID; }
    const MiniNumber& totalSupply()  const { return m_totalSupply; }
    const MiniNumber& minimaAmount() const { return m_totalSupply; }
    const MiniNumber& scale()        const { return m_scale; }
    const MiniString& name()         const { return m_name; }
    const MiniString& description()  const { return m_description; }
    const MiniString& script()       const { return m_script; }
    const MiniNumber& createdBlock() const { return m_created; }

    // ── Setters ────────────────────────────────────────────────────────────
    Token& setTokenID    (const MiniData&   v) { m_tokenID     = v; return *this; }
    Token& setCoinID     (const MiniData&   v) { m_coinID      = v; return *this; }
    Token& setTotalSupply(const MiniNumber& v) { m_totalSupply = v; return *this; }
    Token& setScale      (const MiniNumber& v) { m_scale       = v; return *this; }
    Token& setName       (const MiniString& v) { m_name        = v; return *this; }
    Token& setDescription(const MiniString& v) { m_description = v; return *this; }
    Token& setScript     (const MiniString& v) { m_script      = v; computeTokenID(); return *this; }
    Token& setCreated    (const MiniNumber& v) { m_created     = v; return *this; }

    // ── Derived ────────────────────────────────────────────────────────────
    MiniNumber totalTokens() const {
        int64_t sc = m_scale.getAsLong();
        MiniNumber cur = m_totalSupply;
        for (int64_t i = 0; i < sc; ++i) cur = cur * MiniNumber(int64_t(10));
        return cur;
    }

    // ── Serialisation ─────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out;
        auto app = [&](const std::vector<uint8_t>& v){
            out.insert(out.end(), v.begin(), v.end());
        };
        app(m_tokenID.serialise());
        app(m_coinID.serialise());
        app(m_totalSupply.serialise());
        app(m_scale.serialise());
        app(m_name.serialise());
        app(m_description.serialise());
        app(m_script.serialise());
        return out;
    }

    static Token deserialise(const uint8_t* data, size_t& offset) {
        Token t;
        t.m_tokenID     = MiniData::deserialise(data, offset);
        t.m_coinID      = MiniData::deserialise(data, offset);
        t.m_totalSupply = MiniNumber::deserialise(data, offset);
        t.m_scale       = MiniNumber::deserialise(data, offset);
        t.m_name        = MiniString::deserialise(data, offset);
        t.m_description = MiniString::deserialise(data, offset);
        t.m_script      = MiniString::deserialise(data, offset);
        return t;
    }

    bool operator==(const Token& o) const {
        return m_tokenID.bytes() == o.m_tokenID.bytes();
    }
    bool operator!=(const Token& o) const { return !(*this == o); }

private:
    MiniData   m_tokenID;
    MiniData   m_coinID;
    MiniNumber m_totalSupply;
    MiniNumber m_scale;
    MiniString m_name;
    MiniString m_description;
    MiniString m_script;
    MiniNumber m_created;

    void computeTokenID() {
        auto raw = serialise();
        // tokenID = SHA3 of everything except the first field (tokenID itself)
        // First field is MiniData m_tokenID: 4 bytes len + m_tokenID.size() bytes
        size_t skip = 4 + m_tokenID.size();
        if (raw.size() > skip)
            m_tokenID = crypto::Hash::sha3_256(raw.data() + skip, raw.size() - skip);
    }
};

} // namespace minima
