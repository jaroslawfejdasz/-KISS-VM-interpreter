#pragma once
/**
 * Interpreter — tree-walking evaluator for KISS VM AST.
 *
 * Walks the AST produced by Parser and evaluates each node,
 * maintaining an Environment for variable state.
 * Calls FunctionRegistry for built-in function dispatch.
 *
 * Special signals (implemented via C++ exceptions to keep the
 * tree-walker clean, matching Java's design):
 *   ReturnSignal  — carries the return value up the call stack
 *   AssertFailed  — thrown on ASSERT with falsy expression
 *   InstrLimit    — thrown when MAX_INSTRUCTIONS exceeded
 */

#include "Parser.hpp"
#include "Environment.hpp"
#include "Value.hpp"
#include "functions/Functions.hpp"
#include <stdexcept>
#include <string>

namespace minima::kissvm {

// ── Control-flow signals ──────────────────────────────────────────────────────

struct ReturnSignal {
    Value value;
    explicit ReturnSignal(Value v) : value(std::move(v)) {}
};

// ── Interpreter ───────────────────────────────────────────────────────────────

class Interpreter {
public:
    static constexpr int MAX_INSTRUCTIONS = 1024;

    explicit Interpreter(Environment& env, Contract& ctx);

    // Execute a PROGRAM node. Returns the last expression value
    // (or the value carried by a RETURN statement).
    Value execute(const Node& program);

    int  instrCount() const { return m_instrCount; }

private:
    Environment& m_env;
    Contract&    m_ctx;
    int          m_instrCount{0};

    void  tick();

    Value evalNode(const Node& node);
    void  execStatement(const Node& stmt);

    Value evalBoolLit (const Node& n);
    Value evalNumLit  (const Node& n);
    Value evalHexLit  (const Node& n);
    Value evalStrLit  (const Node& n);
    Value evalVar     (const Node& n);
    Value evalBinary  (const Node& n);
    Value evalUnary   (const Node& n);
    Value evalCall    (const Node& n);

    void execLet    (const Node& n);
    void execIf     (const Node& n);
    void execWhile  (const Node& n);
    void execReturn (const Node& n);
    void execAssert (const Node& n);
    void execExec   (const Node& n);
    void execMast   (const Node& n);
};

} // namespace minima::kissvm
