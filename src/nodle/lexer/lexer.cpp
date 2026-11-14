
#include "nodle/lexer/lexer.hpp"

using namespace nodle::lexer;

Lexer::Lexer() : rules_(rules::create_all_rules()) {}
Lexer::Lexer(std::vector<rules::LexerRule> rules) : rules_(std::move(rules)) {}

Token Lexer::lex_token(nodle::io::SourceLocation location) {
  uint32_t max_length = 0;
  TokenType token_type;
  for (const rules::LexerRule &rule : rules_) {
    auto opt_result = rule(location);

    if (!opt_result.has_value()) {
      continue;
    }

    auto result = opt_result.value();
    if (result.length <= max_length) {
      continue;
    }

    max_length = result.length;
    token_type = result.type;
  }

  if (max_length == 0) {
    // TODO throw a nicely formatted exception
    //  => column and line
    throw std::runtime_error("could not lex token at position " +
                             std::to_string(location.offset()));
  }

  return Token(token_type, nodle::io::SourceLocation(
                               location.file(), location.offset(), max_length));
}
std::vector<Token> Lexer::lex(nodle::io::SourceFile::Ptr file) {
  uint32_t offset = 0;
  std::string_view content = file->content();

  std::vector<Token> tokens;

  while (offset < content.size()) {
    if (std::isspace(content[offset])) {
      offset++;
      continue;
    }

    Token token = lex_token(nodle::io::SourceLocation(
        file, offset, static_cast<uint32_t>(content.size() - offset)));

    tokens.push_back(std::move(token));
    offset += token.source_location().length();
  }

  return tokens;
}
