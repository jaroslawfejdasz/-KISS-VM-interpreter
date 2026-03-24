#pragma once
/**
 * MMRData — a single leaf/node value in the MMR.
 *
 * In Minima Java: com.minima.objects.base.MMRData
 * Stores: data (MiniData hash), value (MiniNumber amount), spent flag.
 * Nodes store hash only (value=0, spent=false).
 */
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../serialization/DataStream.hpp"

namespace minima {

class MMRData {
public:
    MMRData() = default;

    // Leaf constructor
    MMRData(MiniData data, MiniNumber value, bool spent)
        : m_data(std::move(data))
        , m_value(std::move(value))
        , m_spent(spent)
    {}

    // Node constructor (hash only)
    explicit MMRData(MiniData data)
        : m_data(std::move(data))
        , m_value(MiniNumber::ZERO)
        , m_spent(false)
    {}

    const MiniData&   getData()    const { return m_data; }
    const MiniNumber& getValue()   const { return m_value; }
    bool              isSpent()    const { return m_spent; }

    void setSpent(bool s) { m_spent = s; }
    void setData(MiniData d) { m_data = std::move(d); }

    // Combine two child nodes into a parent node hash
    // parent.hash = SHA3_256(left.hash || right.hash)
    static MMRData combine(const MMRData& left, const MMRData& right);

    bool isEmpty() const { return m_data.bytes().empty(); }

    void serialise(DataStream& ds) const;
    void deserialise(DataStream& ds);

    bool operator==(const MMRData& o) const {
        return m_data == o.m_data && m_spent == o.m_spent;
    }

private:
    MiniData   m_data;
    MiniNumber m_value{MiniNumber::ZERO};
    bool       m_spent{false};
};

} // namespace minima
