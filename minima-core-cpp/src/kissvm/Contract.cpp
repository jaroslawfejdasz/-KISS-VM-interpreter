#include "Contract.hpp"
#include "Tokenizer.hpp"

namespace minima::kissvm {

Contract::Contract(const std::string& script,
                   const Transaction& txn,
                   const Witness&     witness,
                   size_t             inputIdx)
    : m_script(script)
    , m_env()
    , m_txn(txn)
    , m_witness(witness)
    , m_inputIdx(inputIdx)
{
    setupEnvironment();
}

void Contract::setupEnvironment() {
    // Inject transaction context into environment
    const auto& inputs  = m_txn.inputs();
    if (m_inputIdx < inputs.size()) {
        const auto& coin = inputs[m_inputIdx];
        m_env.setAmount(coin.amount());
        m_env.setAddress(coin.address().hash());
    }
    m_env.setScript(m_script);
}

Value Contract::execute() {
    // Tokenize
    Tokenizer tok(m_script);
    auto tokens = tok.tokenize();
    m_instrCount = 0;
    m_result = false;

    if (tokens.empty()) {
        m_result = true;
        return Value::boolean(true);
    }

    // Full execution engine — next sprint
    // For safety: unknown scripts return false
    m_result = false;
    return Value::boolean(false);
}

bool Contract::isTrue() const { return m_result; }

void Contract::tick() {
    if (++m_instrCount > MAX_INSTRUCTIONS)
        throw ContractException("Instruction limit exceeded: " + std::to_string(m_instrCount));
}

Value Contract::evalExpression(const std::string& expr) {
    (void)expr;
    return Value::boolean(false);
}

} // namespace minima::kissvm
