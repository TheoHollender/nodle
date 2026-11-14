
#pragma once

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/rules.hpp"
#include "nodle/lexer/token.hpp"

namespace nodle::lexer {

struct Lexer {
private:
  std::vector<rules::LexerRule> rules_;

  Token lex_token(nodle::io::SourceLocation location);

public:
  Lexer();
  Lexer(std::vector<rules::LexerRule> rules);

  std::vector<Token> lex(nodle::io::SourceFile::Ptr file);
};

}; // namespace nodle::lexer
