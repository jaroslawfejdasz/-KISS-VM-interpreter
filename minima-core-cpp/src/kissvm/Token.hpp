#pragma once
/**
 * Token — lexical token produced by the KISS VM tokenizer.
 *
 * Minima reference: src/org/minima/kissvm/tokens/
 */

#include <string>

namespace minima::kissvm {

enum class TokenType {
    // Literals
    NUMBER_LITERAL,
    HEX_LITERAL,
    STRING_LITERAL,
    BOOLEAN_LITERAL,

    // Identifiers / keywords
    IDENTIFIER,     // variable names, function names
    KEYWORD,        // LET IF THEN ELSE ELSEIF ENDIF WHILE DO ENDWHILE RETURN ASSERT EXEC MAST

    // Operators
    PLUS, MINUS, MULTIPLY, DIVIDE, MODULO,
    EQ, NEQ, LT, GT, LTE, GTE,
    AND, OR, NOT, XOR, NAND, NOR,
    LSHIFT, RSHIFT, AMP, PIPE, CARET, TILDE,

    // Structural
    LPAREN, RPAREN,
    COMMA,
    COLON,
    ASSIGN,  // =

    // Control
    END_OF_SCRIPT,
    UNKNOWN
};

struct Token {
    TokenType   type;
    std::string value;    // raw text from source
    int         line{0};  // for error reporting
    int         col{0};
};

const char* tokenTypeName(TokenType t);

} // namespace minima::kissvm
