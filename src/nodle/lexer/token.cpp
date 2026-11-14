
#include "nodle/lexer/token.hpp"

using namespace nodle::lexer;

Token::Token(TokenType token_type, nodle::io::SourceLocation source_location)
    : token_type_(token_type), source_location_(std::move(source_location)) {}

TokenType Token::token_type() const { return token_type_; }
nodle::io::SourceLocation Token::source_location() const {
  return source_location_;
}

std::string_view Token::text() const { return source_location_.text(); }

std::ostream &operator<<(std::ostream &os, TokenType tt) {
  switch (tt) {
  case L_INT:
    return os << "L_INT";
  case L_FLOAT:
    return os << "L_FLOAT";
  case L_FALSE:
    return os << "L_FALSE";
  case L_TRUE:
    return os << "L_TRUE";
  case I_IDENTIFIER:
    return os << "I_IDENTIFIER";
  case K_INT:
    return os << "K_INT";
  case K_FLOAT:
    return os << "K_FLOAT";
  case K_BOOL:
    return os << "K_BOOL";
  case K_VAL:
    return os << "K_VAL";
  case K_IF:
    return os << "K_IF";
  case K_THEN:
    return os << "K_THEN";
  case K_ELSE:
    return os << "K_ELSE";
  case K_SELF:
    return os << "K_SELF";
  case K_CLASS:
    return os << "K_CLASS";
  case K_GRAPH:
    return os << "K_GRAPH";
  case K_STATE:
    return os << "K_STATE";
  case K_WEIGHT:
    return os << "K_WEIGHT";
  case K_EDGE:
    return os << "K_EDGE";
  case K_PROVE:
    return os << "K_PROVE";
  case K_WITH:
    return os << "K_WITH";
  case K_PROOF:
    return os << "K_PROOF";
  case K_SUCH:
    return os << "K_SUCH";
  case K_THAT:
    return os << "K_THAT";
  case K_FOLLOWS:
    return os << "K_FOLLOWS";
  case K_EQUIVALENCE:
    return os << "K_EQUIVALENCE";
  case K_PROPERTY:
    return os << "K_PROPERTY";
  case K_LEMMA:
    return os << "K_LEMMA";
  case K_USE:
    return os << "K_USE";
  case K_DEF:
    return os << "K_DEF";
  case K_OVERRIDE:
    return os << "K_OVERRIDE";
  case O_NOT:
    return os << "O_NOT";
  case O_PLUS:
    return os << "O_PLUS";
  case O_MINUS:
    return os << "O_MINUS";
  case O_TIMES:
    return os << "O_TIMES";
  case O_DIVIDE:
    return os << "O_DIVIDE";
  case O_MODULO:
    return os << "O_MODULO";
  case O_EQUAL:
    return os << "O_EQUAL";
  case O_DIFFERENT:
    return os << "O_DIFFERENT";
  case O_LESS_THAN:
    return os << "O_LESS_THAN";
  case O_LESS_EQUAL:
    return os << "O_LESS_EQUAL";
  case O_GREATER_THAN:
    return os << "O_GREATER_THAN";
  case O_GREATER_EQUAL:
    return os << "O_GREATER_EQUAL";
  case O_AND:
    return os << "O_AND";
  case O_OR:
    return os << "O_OR";
  case O_XOR:
    return os << "O_XOR";
  case D_LPAREN:
    return os << "D_LPAREN";
  case D_RPAREN:
    return os << "D_RPAREN";
  case D_DOT:
    return os << "D_DOT";
  case D_COMMA:
    return os << "D_COMMA";
  case D_COLON:
    return os << "D_COLON";
  case D_SEMICOLON:
    return os << "D_SEMICOLON";
  case D_ASSIGN:
    return os << "D_ASSIGN";
  case D_EDGE:
    return os << "D_EDGE";
  case D_PIPE:
    return os << "D_PIPE";
  case COMMENT:
    return os << "COMMENT";
  case END_OF_FILE:
    return os << "END_OF_FILE";
  default:
    return os << "???";
  };
}
