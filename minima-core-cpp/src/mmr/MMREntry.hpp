#pragma once
/**
 * MMREntry — position + data for one MMR node.
 *
 * In Minima Java: com.minima.objects.base.MMREntry
 * row=0 → leaf row, row>0 → internal node row.
 */
#include "MMRData.hpp"

namespace minima {

class MMREntry {
public:
    MMREntry() = default;
    MMREntry(int row, uint64_t entry, MMRData data)
        : m_row(row), m_entry(entry), m_data(std::move(data)) {}

    int            getRow()   const { return m_row; }
    uint64_t       getEntry() const { return m_entry; }
    const MMRData& getData()  const { return m_data; }
    MMRData&       getData()        { return m_data; }

    void setData(MMRData d) { m_data = std::move(d); }

    bool isEmpty() const { return m_data.isEmpty(); }

    void serialise(DataStream& ds) const;
    void deserialise(DataStream& ds);

private:
    int      m_row{0};
    uint64_t m_entry{0};
    MMRData  m_data;
};

} // namespace minima
