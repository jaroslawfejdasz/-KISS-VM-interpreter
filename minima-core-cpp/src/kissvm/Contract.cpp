#include "Contract.hpp"
#include "Tokenizer.hpp"
#include "Parser.hpp"
#include "Interpreter.hpp"
#include "../crypto/Hash.hpp"

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
    // Initialize function registry once
    FunctionRegistry::instance().registerAll();
    setupEnvironment();
}

void Contract::setupEnvironment() {
    using namespace minima;

    const auto& inputs  = m_txn.inputs();
    const auto& outputs = m_txn.outputs();

    // @AMOUNT, @ADDRESS for the coin being spent
    if (m_inputIdx < inputs.size()) {
        const auto& coin = inputs[m_inputIdx];
        m_env.setAmount(coin.amount());
        m_env.setAddress(coin.address().hash());
    }

    m_env.setScript(m_script);

    // @TOTIN — sum of all input amounts
    MiniNumber totIn("0");
    for (const auto& coin : inputs) totIn = totIn + coin.amount();
    m_env.setTotIn(totIn);

    // @TOTOUT — sum of all output amounts
    MiniNumber totOut("0");
    for (const auto& coin : outputs) totOut = totOut + coin.amount();
    m_env.setTotOut(totOut);

    // @INPUT — number of inputs
    m_env.set("@INPUT", Value::number(MiniNumber((int64_t)inputs.size())));

    // @OUTPUT — number of outputs
    m_env.set("@OUTPUT", Value::number(MiniNumber((int64_t)outputs.size())));

    // State variables: @PREVSTATE[0-255] and @STATE[0-255] from witness
    // These would be set by the validator before contract execution.
    // Here we set defaults.
}

Value Contract::execute() {
    m_instrCount = 0;
    m_result     = false;
    m_trace.clear();

    try {
        // 1. Tokenize
        Tokenizer tok(m_script);
        auto tokens = tok.tokenize();

        // 2. Parse
        Parser parser(std::move(tokens));
        auto ast = parser.parse();

        // 3. Interpret
        Interpreter interp(m_env, *this);
        Value result = interp.execute(*ast);
        m_instrCount = interp.instrCount();

        // Contract passes if final value is TRUE (boolean) or truthy
        m_result = result.isTrue();
        return result;

    } catch (const ReturnSignal&) {
        // Should not escape — Interpreter catches it. Just in case:
        m_result = false;
        return Value::boolean(false);

    } catch (const ContractException& e) {
        m_trace = std::string("ContractException: ") + e.what();
        m_result = false;
        return Value::boolean(false);

    } catch (const ParseError& e) {
        m_trace = std::string("ParseError: ") + e.what();
        m_result = false;
        return Value::boolean(false);

    } catch (const TokenizerError& e) {
        m_trace = std::string("TokenizerError: ") + e.what();
        m_result = false;
        return Value::boolean(false);

    } catch (const std::exception& e) {
        m_trace = std::string("Exception: ") + e.what();
        m_result = false;
        return Value::boolean(false);
    }
}

bool Contract::isTrue() const { return m_result; }

// Called by Interpreter (friend) to tick
void Contract::tick() {
    if (++m_instrCount > MAX_INSTRUCTIONS)
        throw ContractException("Instruction limit exceeded: " + std::to_string(m_instrCount));
}

Value Contract::evalExpression(const std::string& expr) {
    try {
        Tokenizer tok(expr);
        auto tokens = tok.tokenize();
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        Interpreter interp(m_env, *this);
        return interp.execute(*ast);
    } catch (...) {
        return Value::boolean(false);
    }
}

// ── Accessors used by built-in functions ──────────────────────────────────────

const Transaction& Contract::txn()     const { return m_txn; }
const Witness&     Contract::witness() const { return m_witness; }
size_t             Contract::inputIdx()const { return m_inputIdx; }
Environment&       Contract::env()           { return m_env; }

} // namespace minima::kissvm
