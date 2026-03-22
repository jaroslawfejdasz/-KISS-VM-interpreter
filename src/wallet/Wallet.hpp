#pragma once
/**
 * Wallet — zarządzanie kluczami WOTS (Winternitz OTS).
 * Java ref: src/org/minima/database/wallet/Wallet.java
 *
 * Każdy klucz WOTS może być użyty tylko RAZ (one-time signature).
 */
#include "../crypto/Winternitz.hpp"
#include "../crypto/Hash.hpp"
#include "../types/MiniData.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>
#include <random>

namespace minima {

struct WalletKey {
    MiniData seed;
    MiniData publicKey;
    MiniData address;
    bool     used{false};
};

class Wallet {
public:
    Wallet() = default;

    MiniData createNewKey() {
        auto seedVec = randomSeedVec(32);
        MiniData seed(seedVec);
        auto pubKeyVec = crypto::Winternitz::generatePublicKey(seedVec);
        MiniData pubKey(pubKeyVec);
        MiniData address = crypto::Hash::sha3_256(pubKey);

        WalletKey key{ seed, pubKey, address, false };
        m_keys[addrKey(address)] = key;
        m_keyList.push_back(address);
        return address;
    }

    bool hasKey(const MiniData& address) const {
        return m_keys.count(addrKey(address)) > 0;
    }

    std::optional<WalletKey> getKey(const MiniData& address) const {
        auto it = m_keys.find(addrKey(address));
        if (it == m_keys.end()) return std::nullopt;
        return it->second;
    }

    std::optional<MiniData> sign(const MiniData& address, const MiniData& data) {
        auto it = m_keys.find(addrKey(address));
        if (it == m_keys.end()) return std::nullopt;
        if (it->second.used) return std::nullopt;
        auto sigVec = crypto::Winternitz::sign(it->second.seed.bytes(), data.bytes());
        it->second.used = true;
        return MiniData(sigVec);
    }

    const std::vector<MiniData>& addresses() const { return m_keyList; }
    size_t size()        const { return m_keys.size(); }
    size_t unusedCount() const {
        size_t n = 0;
        for (auto& [k,v] : m_keys) if (!v.used) ++n;
        return n;
    }

    std::optional<MiniData> nextUnusedAddress() const {
        for (const auto& addr : m_keyList) {
            auto it = m_keys.find(addrKey(addr));
            if (it != m_keys.end() && !it->second.used) return addr;
        }
        return std::nullopt;
    }

private:
    static std::string addrKey(const MiniData& d) {
        const auto& b = d.bytes();
        return std::string(b.begin(), b.end());
    }
    static std::vector<uint8_t> randomSeedVec(size_t len) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        std::vector<uint8_t> buf(len);
        for (auto& byte : buf) byte = dist(gen);
        return buf;
    }

    std::unordered_map<std::string, WalletKey> m_keys;
    std::vector<MiniData>                      m_keyList;
};

} // namespace minima
