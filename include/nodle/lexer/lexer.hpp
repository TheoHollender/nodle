
#pragma once

#include <utility>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/rules.hpp"
#include "nodle/lexer/token.hpp"

namespace nodle::lexer {

struct Lexer {
private:
  std::vector<rules::LexerRule> rules_;

  /**
   * Lexes the longest token starting at `location`.
   *
   * @return the token, and whether the matching rule asked for it to be
   * skipped (e.g. a comment).
   */
  std::pair<Token, bool> lex_token(nodle::io::SourceLocation location);

public:
  Lexer();
  Lexer(std::vector<rules::LexerRule> rules);

  std::vector<Token> lex(nodle::io::SourceFile::Ptr file);
};

}; // namespace nodle::lexer
