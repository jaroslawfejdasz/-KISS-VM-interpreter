#include "Parser.hpp"
#include <algorithm>
#include <stdexcept>

namespace minima::kissvm {

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool tokenIsBlockEnd(const Token& t) {
    if (t.type == TokenType::KEYWORD) {
        const std::string& v = t.value;
        return v == "ENDIF" || v == "ELSE" || v == "ELSEIF" || v == "ENDWHILE";
    }
    return t.type == TokenType::END_OF_SCRIPT;
}

Parser::Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {
    m_tokens.push_back({TokenType::END_OF_SCRIPT, "<EOF>", 0, 0});
}

Token& Parser::peek(int offset) {
    size_t idx = m_pos + (size_t)offset;
    if (idx >= m_tokens.size()) return m_tokens.back();
    return m_tokens[idx];
}

Token Parser::consume() {
    if (m_pos < m_tokens.size()) return m_tokens[m_pos++];
    return m_tokens.back();
}

Token Parser::expect(TokenType type, const char* what) {
    if (!check(type))
        throw ParseError(std::string("Expected ") + what + ", got '" + peek().value + "'",
                         peek().line, peek().col);
    return consume();
}

bool Parser::check(TokenType type) const {
    if (m_pos >= m_tokens.size()) return false;
    return m_tokens[m_pos].type == type;
}

bool Parser::checkKeyword(const char* kw) const {
    if (m_pos >= m_tokens.size()) return false;
    const Token& t = m_tokens[m_pos];
    return t.type == TokenType::KEYWORD && t.value == kw;
}

bool Parser::match(TokenType type) {
    if (check(type)) { consume(); return true; }
    return false;
}

bool Parser::matchKeyword(const char* kw) {
    if (checkKeyword(kw)) { consume(); return true; }
    return false;
}

bool Parser::atEnd() const {
    return m_pos >= m_tokens.size() || m_tokens[m_pos].type == TokenType::END_OF_SCRIPT;
}

std::unique_ptr<Node> Parser::makeNode(NodeKind k, Token t) {
    return std::make_unique<Node>(k, std::move(t));
}

// ── Top-level ─────────────────────────────────────────────────────────────────

std::unique_ptr<Node> Parser::parse() {
    auto prog = makeNode(NodeKind::PROGRAM, peek());
    while (!atEnd() && !tokenIsBlockEnd(peek()))
        prog->children.push_back(parseStatement());
    return prog;
}

// ── Statements ────────────────────────────────────────────────────────────────

std::unique_ptr<Node> Parser::parseStatement() {
    if (check(TokenType::KEYWORD)) {
        const std::string& kw = peek().value;
        if (kw == "LET")    return parseLet();
        if (kw == "IF")     return parseIf();
        if (kw == "WHILE")  return parseWhile();
        if (kw == "RETURN") return parseReturn();
        if (kw == "ASSERT") return parseAssert();
        if (kw == "EXEC")   return parseExec();
        if (kw == "MAST")   return parseMast();
    }
    return parseExprStmt();
}

std::unique_ptr<Node> Parser::parseLet() {
    Token tok = consume();  // eat LET
    Token name = expect(TokenType::IDENTIFIER, "variable name");
    expect(TokenType::ASSIGN, "=");
    auto expr = parseExpr();
    auto node = makeNode(NodeKind::LET, tok);
    node->name = name.value;
    node->children.push_back(std::move(expr));
    return node;
}

std::unique_ptr<Node> Parser::parseIf() {
    Token tok = consume();  // eat IF
    auto node = makeNode(NodeKind::IF, tok);

    // Condition
    node->children.push_back(parseExpr());
    expect(TokenType::KEYWORD, "THEN");

    // Then-block
    auto thenBlock = makeNode(NodeKind::PROGRAM, peek());
    while (!atEnd() && !tokenIsBlockEnd(peek()))
        thenBlock->children.push_back(parseStatement());
    node->children.push_back(std::move(thenBlock));

    // ELSEIF / ELSE chains
    while (checkKeyword("ELSEIF")) {
        consume(); // eat ELSEIF
        // elseif condition
        node->children.push_back(parseExpr());
        expect(TokenType::KEYWORD, "THEN");
        auto elseifBlock = makeNode(NodeKind::PROGRAM, peek());
        while (!atEnd() && !tokenIsBlockEnd(peek()))
            elseifBlock->children.push_back(parseStatement());
        node->children.push_back(std::move(elseifBlock));
    }

    if (checkKeyword("ELSE")) {
        consume(); // eat ELSE
        auto elseBlock = makeNode(NodeKind::PROGRAM, peek());
        while (!atEnd() && !tokenIsBlockEnd(peek()))
            elseBlock->children.push_back(parseStatement());
        node->children.push_back(std::move(elseBlock));
    }

    expect(TokenType::KEYWORD, "ENDIF");
    return node;
}

std::unique_ptr<Node> Parser::parseWhile() {
    Token tok = consume();  // eat WHILE
    auto node = makeNode(NodeKind::WHILE, tok);
    node->children.push_back(parseExpr());
    expect(TokenType::KEYWORD, "DO");
    while (!atEnd() && !checkKeyword("ENDWHILE"))
        node->children.push_back(parseStatement());
    expect(TokenType::KEYWORD, "ENDWHILE");
    return node;
}

std::unique_ptr<Node> Parser::parseReturn() {
    Token tok = consume();  // eat RETURN
    auto node = makeNode(NodeKind::RETURN, tok);
    node->children.push_back(parseExpr());
    return node;
}

std::unique_ptr<Node> Parser::parseAssert() {
    Token tok = consume();  // eat ASSERT
    auto node = makeNode(NodeKind::ASSERT, tok);
    node->children.push_back(parseExpr());
    return node;
}

std::unique_ptr<Node> Parser::parseExec() {
    Token tok = consume();  // eat EXEC
    auto node = makeNode(NodeKind::EXEC, tok);
    node->children.push_back(parseExpr());
    return node;
}

std::unique_ptr<Node> Parser::parseMast() {
    Token tok = consume();  // eat MAST
    auto node = makeNode(NodeKind::MAST, tok);
    node->children.push_back(parseExpr());
    return node;
}

std::unique_ptr<Node> Parser::parseExprStmt() {
    Token tok = peek();
    auto expr = parseExpr();
    auto node = makeNode(NodeKind::EXPR_STMT, tok);
    node->children.push_back(std::move(expr));
    return node;
}

// ── Expressions ───────────────────────────────────────────────────────────────

std::unique_ptr<Node> Parser::parseExpr()  { return parseOr(); }

std::unique_ptr<Node> Parser::parseOr() {
    auto left = parseAnd();
    while (check(TokenType::OR) || check(TokenType::NOR) || check(TokenType::XOR)) {
        Token op = consume();
        auto right = parseAnd();
        auto node = makeNode(NodeKind::BINARY, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<Node> Parser::parseAnd() {
    auto left = parseNot();
    while (check(TokenType::AND) || check(TokenType::NAND)) {
        Token op = consume();
        auto right = parseNot();
        auto node = makeNode(NodeKind::BINARY, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<Node> Parser::parseNot() {
    if (check(TokenType::NOT)) {
        Token op = consume();
        auto node = makeNode(NodeKind::UNARY, op);
        node->children.push_back(parseNot());
        return node;
    }
    return parseCmp();
}

std::unique_ptr<Node> Parser::parseCmp() {
    auto left = parseAdd();
    while (check(TokenType::EQ) || check(TokenType::NEQ) ||
           check(TokenType::LT) || check(TokenType::GT)  ||
           check(TokenType::LTE)|| check(TokenType::GTE)) {
        Token op = consume();
        auto right = parseAdd();
        auto node = makeNode(NodeKind::BINARY, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<Node> Parser::parseAdd() {
    auto left = parseMul();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        Token op = consume();
        auto right = parseMul();
        auto node = makeNode(NodeKind::BINARY, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<Node> Parser::parseMul() {
    auto left = parseUnary();
    while (check(TokenType::MULTIPLY) || check(TokenType::DIVIDE) || check(TokenType::MODULO)) {
        Token op = consume();
        auto right = parseUnary();
        auto node = makeNode(NodeKind::BINARY, op);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        left = std::move(node);
    }
    return left;
}

std::unique_ptr<Node> Parser::parseUnary() {
    if (check(TokenType::MINUS)) {
        Token op = consume();
        auto node = makeNode(NodeKind::UNARY, op);
        node->children.push_back(parsePrimary());
        return node;
    }
    return parsePrimary();
}

std::unique_ptr<Node> Parser::parsePrimary() {
    Token t = peek();

    // Boolean literal
    if (t.type == TokenType::BOOLEAN_LITERAL) {
        consume();
        auto node = makeNode(NodeKind::BOOL_LIT, t);
        node->bval = (t.value == "TRUE");
        return node;
    }

    // Number literal
    if (t.type == TokenType::NUMBER_LITERAL) {
        consume();
        return makeNode(NodeKind::NUM_LIT, t);
    }

    // Hex literal
    if (t.type == TokenType::HEX_LITERAL) {
        consume();
        return makeNode(NodeKind::HEX_LIT, t);
    }

    // String literal
    if (t.type == TokenType::STRING_LITERAL) {
        consume();
        return makeNode(NodeKind::STR_LIT, t);
    }

    // Identifier — variable or function call
    if (t.type == TokenType::IDENTIFIER) {
        consume();
        // @ variables are always env lookups
        if (t.value[0] == '@') {
            auto node = makeNode(NodeKind::VAR, t);
            node->name = t.value;
            return node;
        }
        // Function call: IDENT(
        if (check(TokenType::LPAREN)) {
            return parseFuncall(t);
        }
        auto node = makeNode(NodeKind::VAR, t);
        node->name = t.value;
        return node;
    }

    // Parenthesized expression
    if (t.type == TokenType::LPAREN) {
        consume();
        auto expr = parseExpr();
        expect(TokenType::RPAREN, ")");
        return expr;
    }

    throw ParseError("Unexpected token '" + t.value + "'", t.line, t.col);
}

std::unique_ptr<Node> Parser::parseFuncall(Token nameToken) {
    auto node = makeNode(NodeKind::CALL, nameToken);
    std::string upper = nameToken.value;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    node->name = upper;

    expect(TokenType::LPAREN, "(");
    if (!check(TokenType::RPAREN)) {
        node->children.push_back(parseExpr());
        // Support both comma-separated and space-separated arguments.
        // Minima KISS VM uses space-separated args (e.g. PROOF(data proof root)).
        // After each arg: skip optional comma, then loop if next is NOT RPAREN.
        while (true) {
            match(TokenType::COMMA); // consume optional comma
            if (check(TokenType::RPAREN)) break;
            node->children.push_back(parseExpr());
        }
    }
    expect(TokenType::RPAREN, ")");
    return node;
}

} // namespace minima::kissvm
