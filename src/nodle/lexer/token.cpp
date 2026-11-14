
#include "nodle/lexer/token.hpp"

using namespace nodle::lexer;

Token::Token(TokenType token_type, nodle::io::SourceLocation source_location)
    : token_type_(token_type), source_location_(std::move(source_location)) {}

TokenType Token::token_type() const { return token_type_; }
nodle::io::SourceLocation Token::source_location() const {
  return source_location_;
}

std::string_view Token::text() const { return source_location_.text(); }
