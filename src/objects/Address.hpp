#pragma once
/**
 * Address — Minima coin address.
 *
 * A Minima address is the SHA3 hash of a KISS VM script.
 * The "default" address (P2PK) is SHA3(RETURN SIGNEDBY(pubkey)).
 */

#include "../types/MiniData.hpp"
#include <string>

namespace minima {

class Address {
public:
    Address() = default;
    explicit Address(const MiniData& hash);

    // Derive address from script string
    static Address fromScript(const std::string& script);

    const MiniData& hash()   const { return m_hash; }
    std::string     toHex()  const { return m_hash.toHexString(); }
    bool            isValid()const { return m_hash.size() == 32; }

    bool operator==(const Address& rhs) const { return m_hash == rhs.m_hash; }
    bool operator!=(const Address& rhs) const { return !(*this == rhs); }

    std::vector<uint8_t> serialise() const;
    static Address       deserialise(const uint8_t* data, size_t& offset);

private:
    MiniData m_hash;  // 32-byte SHA3 of the script
};

} // namespace minima
