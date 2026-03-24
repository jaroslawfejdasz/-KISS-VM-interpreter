#pragma once
/**
 * ChainState — current best chain tip.
 */
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include <vector>
#include <cstdint>

namespace minima::chain {

class ChainState {
public:
    ChainState() : m_height(0), m_tip(MiniData(std::vector<uint8_t>(32, 0x00))) {}

    int64_t   getHeight() const { return m_height; }
    MiniData  getTip()    const { return m_tip; }

    void setTip(const MiniData& id, int64_t height) {
        m_tip    = id;
        m_height = height;
    }

    bool isGenesis() const { return m_height == 0; }

private:
    int64_t  m_height;
    MiniData m_tip;
};

} // namespace minima::chain
