#include "Hash.hpp"
#include "impl/sha256.h"
#include "impl/sha3.h"

namespace minima::crypto {

// ── SHA-2 ─────────────────────────────────────────────────────────────────────

MiniData Hash::sha2_256(const uint8_t* data, size_t len) {
    uint8_t digest[32];
    impl::sha256(data, len, digest);
    return MiniData(std::vector<uint8_t>(digest, digest + 32));
}

MiniData Hash::sha2_256(const std::vector<uint8_t>& data) {
    return sha2_256(data.data(), data.size());
}

MiniData Hash::sha2_256(const MiniData& data) {
    return sha2_256(data.bytes().data(), data.bytes().size());
}

// ── SHA-3 ─────────────────────────────────────────────────────────────────────

MiniData Hash::sha3_256(const uint8_t* data, size_t len) {
    uint8_t digest[32];
    impl::sha3(data, len, digest, 32);
    return MiniData(std::vector<uint8_t>(digest, digest + 32));
}

MiniData Hash::sha3_256(const std::vector<uint8_t>& data) {
    return sha3_256(data.data(), data.size());
}

MiniData Hash::sha3_256(const MiniData& data) {
    return sha3_256(data.bytes().data(), data.bytes().size());
}

// ── Double hash (TxPoW ID) ────────────────────────────────────────────────────

MiniData Hash::sha3_256_double(const std::vector<uint8_t>& data) {
    auto first = sha3_256(data);
    return sha3_256(first);
}

} // namespace minima::crypto
