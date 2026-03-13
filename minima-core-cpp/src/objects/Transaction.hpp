#pragma once
/**
 * Transaction — the core data structure moved on the Minima network.
 *
 * A Minima transaction is a list of input coins → output coins, with an
 * optional script proof (Witness) that unlocks each input.
 *
 * Minima reference: src/org/minima/objects/Transaction.java
 */

#include "Coin.hpp"
#include <vector>

namespace minima {

class Transaction {
public:
    Transaction() = default;

    // Inputs / Outputs
    void addInput (const Coin& coin);
    void addOutput(const Coin& coin);

    const std::vector<Coin>& inputs()  const { return m_inputs; }
    const std::vector<Coin>& outputs() const { return m_outputs; }

    // State variables on the transaction itself (port 0..255)
    void addStateVar(const StateVariable& sv);
    const std::vector<StateVariable>& stateVars() const { return m_stateVars; }
    std::optional<StateVariable> stateVar(uint8_t port) const;

    // Validity checks
    bool isValid()       const;   // basic sanity (not full chain validation)
    bool inputsBalance() const;   // sum(inputs) == sum(outputs) + fee

    // CoinID computation — SHA3(TxID + output_index)
    static MiniData computeCoinID(const MiniData& txID, uint32_t outputIndex);

    std::vector<uint8_t> serialise() const;
    static Transaction   deserialise(const uint8_t* data, size_t& offset);

private:
    std::vector<Coin>          m_inputs;
    std::vector<Coin>          m_outputs;
    std::vector<StateVariable> m_stateVars;
};

} // namespace minima
