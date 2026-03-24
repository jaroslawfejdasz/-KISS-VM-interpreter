#include "Interpreter.hpp"
#include "Tokenizer.hpp"
#include "Parser.hpp"
#include "Contract.hpp"
#include <algorithm>
#include <stdexcept>

namespace minima::kissvm {

Interpreter::Interpreter(Environment& env, Contract& ctx)
    : m_env(env), m_ctx(ctx) {}

// ── Public entry point ────────────────────────────────────────────────────────

Value Interpreter::execute(const Node& program) {
    Value last = Value::boolean(false);
    try {
        for (const auto& stmt : program.children) {
            tick();
            switch (stmt->kind) {
                case NodeKind::EXPR_STMT:
                    last = evalNode(*stmt->children[0]);
                    break;
                default:
                    execStatement(*stmt);
                    break;
            }
        }
    } catch (ReturnSignal& rs) {
        return rs.value;
    }
    return last;
}

void Interpreter::tick() {
    if (++m_instrCount > MAX_INSTRUCTIONS)
        throw ContractException("Instruction limit exceeded (" + std::to_string(m_instrCount) + ")");
}

// ── Statement dispatch ────────────────────────────────────────────────────────

void Interpreter::execStatement(const Node& stmt) {
    tick();
    switch (stmt.kind) {
        case NodeKind::LET:       execLet(stmt);    break;
        case NodeKind::IF:        execIf(stmt);     break;
        case NodeKind::WHILE:     execWhile(stmt);  break;
        case NodeKind::RETURN:    execReturn(stmt); break;
        case NodeKind::ASSERT:    execAssert(stmt); break;
        case NodeKind::EXEC:      execExec(stmt);   break;
        case NodeKind::MAST:      execMast(stmt);   break;
        case NodeKind::EXPR_STMT: evalNode(*stmt.children[0]); break;
        case NodeKind::PROGRAM:
            for (auto& child : stmt.children) execStatement(*child);
            break;
        default:
            throw ContractException("Unknown statement node kind");
    }
}

// ── Expression evaluation ─────────────────────────────────────────────────────

Value Interpreter::evalNode(const Node& node) {
    tick();
    switch (node.kind) {
        case NodeKind::BOOL_LIT:  return evalBoolLit(node);
        case NodeKind::NUM_LIT:   return evalNumLit(node);
        case NodeKind::HEX_LIT:   return evalHexLit(node);
        case NodeKind::STR_LIT:   return evalStrLit(node);
        case NodeKind::VAR:       return evalVar(node);
        case NodeKind::BINARY:    return evalBinary(node);
        case NodeKind::UNARY:     return evalUnary(node);
        case NodeKind::CALL:      return evalCall(node);
        case NodeKind::EXPR_STMT: return evalNode(*node.children[0]);
        default:
            throw ContractException("Cannot evaluate node kind " + std::to_string((int)node.kind));
    }
}

Value Interpreter::evalBoolLit(const Node& n) {
    return Value::boolean(n.bval);
}

Value Interpreter::evalNumLit(const Node& n) {
    return Value::number(MiniNumber(n.tok.value));
}

Value Interpreter::evalHexLit(const Node& n) {
    // tok.value is "0xFF..." — strip "0x" prefix
    std::string hex = n.tok.value.substr(2);
    // Convert hex string to bytes
    if (hex.size() % 2 != 0) hex = "0" + hex;
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t byte = (uint8_t)std::stoul(hex.substr(i, 2), nullptr, 16);
        bytes.push_back(byte);
    }
    return Value::hex(MiniData(std::move(bytes)));
}

Value Interpreter::evalStrLit(const Node& n) {
    return Value::script(n.tok.value);
}

Value Interpreter::evalVar(const Node& n) {
    auto v = m_env.get(n.name);
    if (!v) {
        // Unknown variable = 0 (matches Minima Java behavior)
        return Value::number(MiniNumber("0"));
    }
    return *v;
}

Value Interpreter::evalBinary(const Node& n) {
    const TokenType op = n.tok.type;

    // Short-circuit AND/NAND
    if (op == TokenType::AND || op == TokenType::NAND) {
        Value left = evalNode(*n.children[0]);
        if (!left.isTrue()) return Value::boolean(op == TokenType::NAND ? true : false);
        Value right = evalNode(*n.children[1]);
        bool result = left.isTrue() && right.isTrue();
        return Value::boolean(op == TokenType::NAND ? !result : result);
    }

    // Short-circuit OR/NOR
    if (op == TokenType::OR || op == TokenType::NOR) {
        Value left = evalNode(*n.children[0]);
        if (left.isTrue()) return Value::boolean(op == TokenType::NOR ? false : true);
        Value right = evalNode(*n.children[1]);
        bool result = left.isTrue() || right.isTrue();
        return Value::boolean(op == TokenType::NOR ? !result : result);
    }

    // All others: evaluate both sides
    Value left  = evalNode(*n.children[0]);
    Value right = evalNode(*n.children[1]);

    switch (op) {
        // Boolean
        case TokenType::XOR:
            return Value::boolean(left.isTrue() != right.isTrue());

        // Comparison
        case TokenType::EQ:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::boolean(left.asNumber() == right.asNumber());
            if (left.type() == ValueType::HEX && right.type() == ValueType::HEX)
                return Value::boolean(left.asHex() == right.asHex());
            if (left.type() == ValueType::BOOLEAN && right.type() == ValueType::BOOLEAN)
                return Value::boolean(left.asBoolean() == right.asBoolean());
            if (left.type() == ValueType::SCRIPT && right.type() == ValueType::SCRIPT)
                return Value::boolean(left.asScript().str() == right.asScript().str());
            return Value::boolean(left.toString() == right.toString());

        case TokenType::NEQ: {
            auto eq = evalBinary(Node()); // we re-evaluate below
            // Inline: NEQ = NOT EQ
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::boolean(!(left.asNumber() == right.asNumber()));
            return Value::boolean(left.toString() != right.toString());
        }

        case TokenType::LT:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::boolean(left.asNumber() < right.asNumber());
            throw ContractException("LT requires NUMBER operands");

        case TokenType::GT:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::boolean(left.asNumber() > right.asNumber());
            throw ContractException("GT requires NUMBER operands");

        case TokenType::LTE:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::boolean(left.asNumber() <= right.asNumber());
            throw ContractException("LTE requires NUMBER operands");

        case TokenType::GTE:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::boolean(left.asNumber() >= right.asNumber());
            throw ContractException("GTE requires NUMBER operands");

        // Arithmetic
        case TokenType::PLUS:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::number(left.asNumber() + right.asNumber());
            // String concat via + is not KISS VM — use CONCAT() function
            throw ContractException("PLUS requires NUMBER operands");

        case TokenType::MINUS:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::number(left.asNumber() - right.asNumber());
            throw ContractException("MINUS requires NUMBER operands");

        case TokenType::MULTIPLY:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::number(left.asNumber() * right.asNumber());
            throw ContractException("MULTIPLY requires NUMBER operands");

        case TokenType::DIVIDE:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::number(left.asNumber() / right.asNumber());
            throw ContractException("DIVIDE requires NUMBER operands");

        case TokenType::MODULO:
            if (left.type() == ValueType::NUMBER && right.type() == ValueType::NUMBER)
                return Value::number(left.asNumber() % right.asNumber());
            throw ContractException("MODULO requires NUMBER operands");

        default:
            throw ContractException("Unknown binary operator: " + n.tok.value);
    }
}

Value Interpreter::evalUnary(const Node& n) {
    Value operand = evalNode(*n.children[0]);
    switch (n.tok.type) {
        case TokenType::NOT:
            return Value::boolean(!operand.isTrue());
        case TokenType::MINUS:
            if (operand.type() == ValueType::NUMBER)
                return Value::number(MiniNumber("0") - operand.asNumber());
            throw ContractException("Unary minus requires NUMBER");
        default:
            throw ContractException("Unknown unary operator: " + n.tok.value);
    }
}

Value Interpreter::evalCall(const Node& n) {
    std::vector<Value> args;
    args.reserve(n.children.size());
    for (const auto& child : n.children)
        args.push_back(evalNode(*child));

    auto& reg = FunctionRegistry::instance();
    if (!reg.has(n.name))
        throw ContractException("Unknown function: " + n.name);
    return reg.call(n.name, args, m_ctx);
}

// ── Statements ────────────────────────────────────────────────────────────────

void Interpreter::execLet(const Node& n) {
    Value v = evalNode(*n.children[0]);
    m_env.set(n.name, v);
}

void Interpreter::execIf(const Node& n) {
    // children layout:
    //   [0] condition
    //   [1] then-block (PROGRAM)
    //   [2] (optional) elseif-condition OR else-block
    //   [3] (optional) elseif-block
    //   ... pairs of (condition, block) for ELSEIF
    //   last child is else-block if count is even (after then-block)
    // We use: cond, then, (elseif_cond, elseif_block)*, [else_block]?

    size_t idx = 0;
    // Primary IF
    Value cond = evalNode(*n.children[idx++]);
    if (cond.isTrue()) {
        execStatement(*n.children[idx]);
        return;
    }
    idx++; // skip then-block

    // ELSEIF chains: pairs of (cond, block)
    while (idx + 1 < n.children.size()) {
        // peek: is this an elseif condition or the else block?
        // We identify elseif pairs vs else-block by count:
        // total children = 2 + 2*N_elseif + (0 or 1 else)
        // But simpler: we just try to evaluate as condition, then block
        // ELSEIF pairs are always even-numbered remaining
        size_t remaining = n.children.size() - idx;
        if (remaining >= 2) {
            Value elseifCond = evalNode(*n.children[idx++]);
            if (elseifCond.isTrue()) {
                execStatement(*n.children[idx]);
                return;
            }
            idx++; // skip elseif block
        } else {
            break;
        }
    }

    // ELSE block — last child if odd count after initial pair
    if (idx < n.children.size()) {
        execStatement(*n.children[idx]);
    }
}

void Interpreter::execWhile(const Node& n) {
    // children[0] = condition, children[1..] = body statements
    // Actually body is children[1] = PROGRAM node
    static constexpr int MAX_LOOP = 512;
    int loopCount = 0;
    while (true) {
        if (++loopCount > MAX_LOOP)
            throw ContractException("While loop exceeded iteration limit");
        tick();
        Value cond = evalNode(*n.children[0]);
        if (!cond.isTrue()) break;
        try {
            for (size_t i = 1; i < n.children.size(); i++)
                execStatement(*n.children[i]);
        } catch (ReturnSignal&) {
            throw; // propagate
        }
    }
}

void Interpreter::execReturn(const Node& n) {
    Value v = evalNode(*n.children[0]);
    throw ReturnSignal(std::move(v));
}

void Interpreter::execAssert(const Node& n) {
    Value v = evalNode(*n.children[0]);
    if (!v.isTrue())
        throw ContractException("ASSERT failed");
}

void Interpreter::execExec(const Node& n) {
    // EXEC evaluates a script expression and executes it as a sub-script
    Value v = evalNode(*n.children[0]);
    if (v.type() != ValueType::SCRIPT)
        throw ContractException("EXEC requires a SCRIPT value");
    std::string subScript = v.asScript().str();
    // Re-enter: tokenize, parse, interpret sub-script
    // Uses same environment (same m_env) — KISS VM EXEC shares scope
    Tokenizer tok(subScript);
    auto tokens = tok.tokenize();
    Parser parser(std::move(tokens));
    auto ast = parser.parse();
    execute(*ast);  // result discarded — EXEC is for side effects
}

void Interpreter::execMast(const Node& n) {
    // MAST: Merkelized Abstract Syntax Tree
    // Evaluates to a hash of a branch, then executes the matching branch from witness
    // For now: same as EXEC (simplified — full MAST requires witness tree lookup)
    execExec(n);
}

} // namespace minima::kissvm
