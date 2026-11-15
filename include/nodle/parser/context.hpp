
#pragma once

#include "nodle/lexer/token.hpp"
#include <memory>

namespace nodle::parser {

struct ParserContext {
private:
  std::vector<lexer::Token> tokens_;
  uint32_t offset = 0;

public:
  ParserContext(std::vector<lexer::Token> tokens);

  bool remaining() const;

  lexer::Token peek() const;
  void pop();
};

using ParserContextPtr = std::shared_ptr<ParserContext>;

} // namespace nodle::parser
