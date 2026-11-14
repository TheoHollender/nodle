
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

LexerRule createNumberRule();
LexerRule createStringRule(std::string keyword, TokenType type);
LexerRule createIdentifierRule(TokenType type);

std::vector<LexerRule> createAllRules();

} // namespace nodle::lexer::rules
