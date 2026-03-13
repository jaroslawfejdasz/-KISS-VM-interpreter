#pragma once
/**
 * Contract — the top-level KISS VM execution context.
 *
 * To validate a coin spend:
 *  1. Create a Contract with the coin's script
 *  2. Inject transaction context (block number, coin age, signatures, state vars...)
 *  3. Call execute()
 *  4. Check isTrue() — if false, the spend is invalid and the transaction is rejected
 *
 * Minima reference: src/org/minima/kissvm/Contract.java
 *
 * LIMITS:
 *  - Max 1024 instructions executed (throws ContractException if exceeded)
 *  - Max 64 stack depth (throws ContractException if exceeded)
 */

#include "Environment.hpp"
#include "Value.hpp"
#include "../objects/Transaction.hpp"
#include "../objects/Witness.hpp"
#include "../objects/Coin.hpp"
#include <string>
#include <vector>
#include <stdexcept>
#include <functional>

namespace minima::kissvm {

class ContractException : public std::runtime_error {
public:
    explicit ContractException(const std::string& msg) : std::runtime_error(msg) {}
};

class Contract {
public:
    static constexpr int MAX_INSTRUCTIONS = 1024;
    static constexpr int MAX_STACK_DEPTH  = 64;

    /**
     * @param script    The KISS VM script text (from coin address / witness)
     * @param txn       The spending transaction
     * @param witness   Witness data (signatures, script proofs)
     * @param inputIdx  Which input coin is being validated (0-based)
     */
    Contract(const std::string&  script,
             const Transaction&  txn,
             const Witness&      witness,
             size_t              inputIdx);

    // Execute the contract. Returns final stack top (should be TRUE).
    // Throws ContractException on:
    //   - instruction limit exceeded
    //   - stack overflow
    //   - type errors
    //   - ASSERT failure
    Value execute();

    bool   isTrue()  const;
    bool   isFalse() const { return !isTrue(); }

    // Inspection
    const std::string& script()      const { return m_script; }
    int                instructions()const { return m_instrCount; }
    const std::string& traceLog()    const { return m_trace; }

private:
    std::string     m_script;
    Environment     m_env;
    const Transaction& m_txn;
    const Witness&     m_witness;
    size_t          m_inputIdx;
    int             m_instrCount{0};
    bool            m_result{false};
    std::string     m_trace;

    void  setupEnvironment();
    Value evalExpression(const std::string& expr);
    void  tick();  // increment + check instruction counter
};

} // namespace minima::kissvm
