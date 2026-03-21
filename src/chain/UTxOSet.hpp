#pragma once
/**
 * UTxOSet — in-memory set of unspent transaction outputs.
 * Java ref: src/org/minima/database/minidbd/MinimaDB.java (coinDB)
 */
#include "../objects/Coin.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include <unordered_map>
#include <vector>

namespace minima {
struct MiniDataHash;
struct MiniDataEqual;
} // forward

namespace minima::chain {

class UTxOSet {
public:
    void addCoin(const Coin& c) {
        m_coins[c.coinID()] = c;
    }

    void spendCoin(const MiniData& id) {
        m_coins.erase(id);
    }

    bool hasCoin(const MiniData& id) const {
        return m_coins.count(id) > 0;
    }

    const Coin* getCoin(const MiniData& id) const {
        auto it = m_coins.find(id);
        if (it == m_coins.end()) return nullptr;
        return &it->second;
    }

    MiniNumber getBalance(const MiniData& address, const MiniData& tokenID) const {
        MiniNumber total(int64_t(0));
        for (const auto& [id, c] : m_coins) {
            bool addrMatch = (c.address().hash().bytes() == address.bytes());
            bool tokMatch  = (!c.hasToken() && tokenID.bytes() == std::vector<uint8_t>{0x00}) ||
                             (c.hasToken() && c.tokenID().bytes() == tokenID.bytes());
            if (addrMatch && tokMatch)
                total = total + c.amount();
        }
        return total;
    }

    size_t size() const { return m_coins.size(); }

private:
    struct Hash {
        size_t operator()(const MiniData& d) const {
            size_t h = 0;
            for (uint8_t b : d.bytes()) h = h * 31 + b;
            return h;
        }
    };
    struct Eq {
        bool operator()(const MiniData& a, const MiniData& b) const {
            return a.bytes() == b.bytes();
        }
    };
    std::unordered_map<MiniData, Coin, Hash, Eq> m_coins;
};

} // namespace minima::chain
