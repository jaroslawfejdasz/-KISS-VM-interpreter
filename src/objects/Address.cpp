#include "Address.hpp"
#include "../crypto/Hash.hpp"

namespace minima {

Address::Address(const MiniData& hash) : m_hash(hash) {}

Address Address::fromScript(const std::string& script) {
    MiniData scriptBytes(std::vector<uint8_t>(script.begin(), script.end()));
    return Address(crypto::Hash::sha3_256(scriptBytes));
}

// Wire format: MiniData.serialise() (4-byte len + raw bytes)
std::vector<uint8_t> Address::serialise() const {
    return m_hash.serialise();
}

Address Address::deserialise(const uint8_t* data, size_t& offset) {
    auto hash = MiniData::deserialise(data, offset);
    return Address(hash);
}

} // namespace minima
