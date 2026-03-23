#include "MMREntry.hpp"

namespace minima {

void MMREntry::serialise(DataStream& ds) const {
    ds.writeUInt32(static_cast<uint32_t>(m_row));
    ds.writeUInt64(m_entry);
    m_data.serialise(ds);
}

void MMREntry::deserialise(DataStream& ds) {
    m_row   = static_cast<int>(ds.readUInt32());
    m_entry = ds.readUInt64();
    m_data.deserialise(ds);
}

} // namespace minima
