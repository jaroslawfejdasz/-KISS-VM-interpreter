#include "Token.hpp"

namespace minima::kissvm {

const char* tokenTypeName(TokenType t) {
    switch(t) {
        case TokenType::NUMBER_LITERAL:  return "NUMBER_LITERAL";
        case TokenType::HEX_LITERAL:     return "HEX_LITERAL";
        case TokenType::STRING_LITERAL:  return "STRING_LITERAL";
        case TokenType::BOOLEAN_LITERAL: return "BOOLEAN_LITERAL";
        case TokenType::IDENTIFIER:      return "IDENTIFIER";
        case TokenType::KEYWORD:         return "KEYWORD";
        case TokenType::PLUS:            return "PLUS";
        case TokenType::MINUS:           return "MINUS";
        case TokenType::MULTIPLY:        return "MULTIPLY";
        case TokenType::DIVIDE:          return "DIVIDE";
        case TokenType::MODULO:          return "MODULO";
        case TokenType::EQ:              return "EQ";
        case TokenType::NEQ:             return "NEQ";
        case TokenType::LT:              return "LT";
        case TokenType::GT:              return "GT";
        case TokenType::LTE:             return "LTE";
        case TokenType::GTE:             return "GTE";
        case TokenType::AND:             return "AND";
        case TokenType::OR:              return "OR";
        case TokenType::NOT:             return "NOT";
        case TokenType::XOR:             return "XOR";
        case TokenType::NAND:            return "NAND";
        case TokenType::NOR:             return "NOR";
        case TokenType::LSHIFT:          return "LSHIFT";
        case TokenType::RSHIFT:          return "RSHIFT";
        case TokenType::LPAREN:          return "LPAREN";
        case TokenType::RPAREN:          return "RPAREN";
        case TokenType::COMMA:           return "COMMA";
        case TokenType::ASSIGN:          return "ASSIGN";
        case TokenType::END_OF_SCRIPT:   return "END_OF_SCRIPT";
        default:                         return "UNKNOWN";
    }
}

} // namespace minima::kissvm
