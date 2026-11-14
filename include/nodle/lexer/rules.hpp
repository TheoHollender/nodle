
#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"

namespace nodle::lexer::rules {

struct LexerResult {
  uint32_t length;
  TokenType type;
};

using LexerRule = std::function<std::optional<LexerResult>(io::SourceLocation)>;

LexerRule create_number_rule();
LexerRule create_string_rule(std::string keyword, TokenType type);
LexerRule create_identifier_rule(TokenType type);

std::vector<LexerRule> create_all_rules();

} // namespace nodle::lexer::rules
