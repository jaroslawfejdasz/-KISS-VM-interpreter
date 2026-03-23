#pragma once
/**
 * Parser — recursive descent parser for KISS VM scripts.
 *
 * Grammar (simplified):
 *   program     := statement* EOF
 *   statement   := let_stmt | if_stmt | while_stmt | return_stmt
 *                | assert_stmt | exec_stmt | mast_stmt | expr_stmt
 *   let_stmt    := LET IDENT = expr
 *   if_stmt     := IF expr THEN statement* (ELSEIF expr THEN statement*)* (ELSE statement*)? ENDIF
 *   while_stmt  := WHILE expr DO statement* ENDWHILE
 *   return_stmt := RETURN expr
 *   assert_stmt := ASSERT expr
 *   exec_stmt   := EXEC expr
 *   mast_stmt   := MAST expr
 *   expr_stmt   := expr  (bare expression — evaluated and discarded or used as final result)
 *   expr        := or_expr
 *   or_expr     := and_expr ((OR | NOR | XOR) and_expr)*
 *   and_expr    := not_expr ((AND | NAND) not_expr)*
 *   not_expr    := NOT not_expr | cmp_expr
 *   cmp_expr    := add_expr ((EQ|NEQ|LT|GT|LTE|GTE) add_expr)*
 *   add_expr    := mul_expr ((+|-) mul_expr)*
 *   mul_expr    := unary   ((*|/|%) unary)*
 *   unary       := - primary | primary
 *   primary     := BOOL_LIT | NUM_LIT | HEX_LIT | STRING_LIT | IDENT
 *                | funcall | ( expr )
 *   funcall     := IDENT ( args )
 *   args        := expr (, expr)*
 */

#include "Token.hpp"
#include <vector>
#include <memory>
#include <string>

namespace minima::kissvm {

// ── AST node types ────────────────────────────────────────────────────────────

enum class NodeKind {
    // Statements
    PROGRAM, LET, IF, WHILE, RETURN, ASSERT, EXEC, MAST, EXPR_STMT,
    // Expressions
    BOOL_LIT, NUM_LIT, HEX_LIT, STR_LIT, VAR,
    BINARY, UNARY, CALL
};

struct Node {
    NodeKind kind;
    Token    tok;  // originating token (for errors)

    // Children — used differently per node kind
    std::vector<std::unique_ptr<Node>> children;

    // Extra payload
    std::string name;   // variable/function name
    bool        bval{false};

    Node() = default;
    Node(NodeKind k, Token t) : kind(k), tok(std::move(t)) {}
};

// ── Parser ────────────────────────────────────────────────────────────────────

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg, int line = 0, int col = 0)
        : std::runtime_error(msg), line(line), col(col) {}
    int line, col;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<Node> parse();  // returns PROGRAM node

private:
    std::vector<Token> m_tokens;
    size_t             m_pos{0};

    Token& peek(int offset = 0);
    Token  consume();
    Token  expect(TokenType type, const char* what);
    bool   check(TokenType type) const;
    bool   checkKeyword(const char* kw) const;
    bool   match(TokenType type);
    bool   matchKeyword(const char* kw);
    bool   atEnd() const;

    std::unique_ptr<Node> parseStatement();
    std::unique_ptr<Node> parseLet();
    std::unique_ptr<Node> parseIf();
    std::unique_ptr<Node> parseWhile();
    std::unique_ptr<Node> parseReturn();
    std::unique_ptr<Node> parseAssert();
    std::unique_ptr<Node> parseExec();
    std::unique_ptr<Node> parseMast();
    std::unique_ptr<Node> parseExprStmt();

    std::unique_ptr<Node> parseExpr();
    std::unique_ptr<Node> parseOr();
    std::unique_ptr<Node> parseAnd();
    std::unique_ptr<Node> parseNot();
    std::unique_ptr<Node> parseCmp();
    std::unique_ptr<Node> parseAdd();
    std::unique_ptr<Node> parseMul();
    std::unique_ptr<Node> parseUnary();
    std::unique_ptr<Node> parsePrimary();
    std::unique_ptr<Node> parseFuncall(Token nameToken);

    std::unique_ptr<Node> makeNode(NodeKind k, Token t);
};

} // namespace minima::kissvm
