#pragma once
/**
 * Tokenizer — lexer for KISS VM scripts.
 *
 * Converts a raw script string into a flat list of Tokens.
 * Handles:
 *  - Case-insensitive keywords and function names
 *  - 0x-prefixed hex literals
 *  - Quoted string literals
 *  - Single-line comments (/* ... */)
 *  - Numeric literals (decimal, may be negative)
 */

#include "Token.hpp"
#include <vector>
#include <string>
#include <stdexcept>

namespace minima::kissvm {

class TokenizerError : public std::runtime_error {
public:
    explicit TokenizerError(const std::string& msg, int line, int col)
        : std::runtime_error(msg), line(line), col(col) {}
    int line, col;
};

class Tokenizer {
public:
    explicit Tokenizer(const std::string& script);

    std::vector<Token> tokenize();

private:
    std::string m_src;
    size_t      m_pos{0};
    int         m_line{1};
    int         m_col{1};

    char   peek(int offset = 0) const;
    char   consume();
    void   skipWhitespace();
    void   skipComment();
    Token  readHexLiteral();
    Token  readNumberLiteral();
    Token  readStringLiteral();
    Token  readIdentifierOrKeyword();
    bool   isKeyword(const std::string& upper) const;
    Token  tokenFromOperatorWord(const std::string& upper, int line, int col);
};

} // namespace minima::kissvm
