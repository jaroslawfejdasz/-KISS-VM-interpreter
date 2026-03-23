#include "Tokenizer.hpp"
#include <cctype>
#include <algorithm>
#include <set>

namespace minima::kissvm {

static const std::set<std::string> KEYWORDS = {
    "LET","IF","THEN","ELSE","ELSEIF","ENDIF",
    "WHILE","DO","ENDWHILE","RETURN","ASSERT","EXEC","MAST"
};

static const std::set<std::string> BOOLEAN_LITERALS = { "TRUE", "FALSE" };

static const std::set<std::string> OPERATOR_WORDS = {
    "EQ","NEQ","LT","GT","LTE","GTE","AND","OR","NOT","XOR","NAND","NOR",
    "LSHIFT","RSHIFT"
};

Tokenizer::Tokenizer(const std::string& script) : m_src(script) {}

char Tokenizer::peek(int offset) const {
    size_t idx = m_pos + offset;
    return (idx < m_src.size()) ? m_src[idx] : '\0';
}

char Tokenizer::consume() {
    char c = m_src[m_pos++];
    if (c == '\n') { ++m_line; m_col = 1; } else { ++m_col; }
    return c;
}

void Tokenizer::skipWhitespace() {
    while (m_pos < m_src.size() && std::isspace((unsigned char)peek())) consume();
}

void Tokenizer::skipComment() {
    // /* ... */
    consume(); consume(); // eat /*
    while (m_pos + 1 < m_src.size()) {
        if (peek() == '*' && peek(1) == '/') { consume(); consume(); return; }
        consume();
    }
    throw TokenizerError("Unterminated comment", m_line, m_col);
}

Token Tokenizer::readHexLiteral() {
    int line = m_line, col = m_col;
    consume(); consume(); // eat 0x
    std::string hex;
    while (m_pos < m_src.size() && std::isxdigit((unsigned char)peek()))
        hex.push_back(std::toupper((unsigned char)consume()));
    if (hex.empty())
        throw TokenizerError("Empty hex literal after 0x", line, col);
    return {TokenType::HEX_LITERAL, "0x" + hex, line, col};
}

Token Tokenizer::readNumberLiteral() {
    int line = m_line, col = m_col;
    std::string num;
    if (peek() == '-') num.push_back(consume());
    while (m_pos < m_src.size() && (std::isdigit((unsigned char)peek()) || peek() == '.'))
        num.push_back(consume());
    return {TokenType::NUMBER_LITERAL, num, line, col};
}

Token Tokenizer::readStringLiteral() {
    int line = m_line, col = m_col;
    consume(); // eat opening [
    std::string s;
    while (m_pos < m_src.size() && peek() != ']') s.push_back(consume());
    if (m_pos >= m_src.size()) throw TokenizerError("Unterminated string literal", line, col);
    consume(); // eat ]
    return {TokenType::STRING_LITERAL, s, line, col};
}

Token Tokenizer::readIdentifierOrKeyword() {
    int line = m_line, col = m_col;
    std::string word;
    while (m_pos < m_src.size() && (std::isalnum((unsigned char)peek()) || peek() == '_' || peek() == '@'))
        word.push_back(consume());

    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (BOOLEAN_LITERALS.count(upper))    return {TokenType::BOOLEAN_LITERAL, upper, line, col};
    if (KEYWORDS.count(upper))            return {TokenType::KEYWORD, upper, line, col};
    if (OPERATOR_WORDS.count(upper))      return tokenFromOperatorWord(upper, line, col);
    return {TokenType::IDENTIFIER, word, line, col};
}

Token Tokenizer::tokenFromOperatorWord(const std::string& upper, int line, int col) {
    if (upper=="EQ")     return {TokenType::EQ,     upper,line,col};
    if (upper=="NEQ")    return {TokenType::NEQ,    upper,line,col};
    if (upper=="LT")     return {TokenType::LT,     upper,line,col};
    if (upper=="GT")     return {TokenType::GT,     upper,line,col};
    if (upper=="LTE")    return {TokenType::LTE,    upper,line,col};
    if (upper=="GTE")    return {TokenType::GTE,    upper,line,col};
    if (upper=="AND")    return {TokenType::AND,    upper,line,col};
    if (upper=="OR")     return {TokenType::OR,     upper,line,col};
    if (upper=="NOT")    return {TokenType::NOT,    upper,line,col};
    if (upper=="XOR")    return {TokenType::XOR,    upper,line,col};
    if (upper=="NAND")   return {TokenType::NAND,   upper,line,col};
    if (upper=="NOR")    return {TokenType::NOR,    upper,line,col};
    if (upper=="LSHIFT") return {TokenType::LSHIFT, upper,line,col};
    if (upper=="RSHIFT") return {TokenType::RSHIFT, upper,line,col};
    return {TokenType::IDENTIFIER, upper, line, col};
}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespace();
        if (m_pos >= m_src.size()) break;

        int line = m_line, col = m_col;
        char c = peek();

        // Comment
        if (c == '/' && peek(1) == '*') { skipComment(); continue; }

        // Hex literal
        if (c == '0' && (peek(1)=='x'||peek(1)=='X')) {
            tokens.push_back(readHexLiteral());
            continue;
        }

        // String literal [...]
        if (c == '[') { tokens.push_back(readStringLiteral()); continue; }

        // Number literal (digit or negative sign followed by digit)
        if (std::isdigit((unsigned char)c) ||
            (c == '-' && m_pos+1 < m_src.size() && std::isdigit((unsigned char)peek(1)))) {
            tokens.push_back(readNumberLiteral());
            continue;
        }

        // Symbol operators
        if (c == '+') { consume(); tokens.push_back({TokenType::PLUS, "+", line, col}); continue; }
        if (c == '-') { consume(); tokens.push_back({TokenType::MINUS, "-", line, col}); continue; }
        if (c == '*') { consume(); tokens.push_back({TokenType::MULTIPLY, "*", line, col}); continue; }
        if (c == '/') { consume(); tokens.push_back({TokenType::DIVIDE, "/", line, col}); continue; }
        if (c == '%') { consume(); tokens.push_back({TokenType::MODULO, "%", line, col}); continue; }
        if (c == '(') { consume(); tokens.push_back({TokenType::LPAREN, "(", line, col}); continue; }
        if (c == ')') { consume(); tokens.push_back({TokenType::RPAREN, ")", line, col}); continue; }
        if (c == ',') { consume(); tokens.push_back({TokenType::COMMA, ",", line, col}); continue; }
        if (c == '=') { consume(); tokens.push_back({TokenType::ASSIGN, "=", line, col}); continue; }

        // Identifier / keyword
        if (std::isalpha((unsigned char)c) || c=='_' || c=='@') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        throw TokenizerError(std::string("Unexpected character: '") + c + "'", line, col);
    }
    tokens.push_back({TokenType::END_OF_SCRIPT, "", m_line, m_col});
    return tokens;
}

} // namespace minima::kissvm
