
#pragma once

#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"

namespace nodle::parser::rules {

struct TokenRule : public BasicRuleTrait<TokenRule, lexer::Token> {
private:
  lexer::TokenType token_type_;

public:
  using return_type = lexer::Token;

  TokenRule(lexer::TokenType token_type) : token_type_(token_type) {}

  ParseResult<lexer::Token> parse(ParserContextPtr ctx) const {
    if (ctx->remaining() && ctx->peek().token_type() == token_type_) {
      lexer::Token token = ctx->peek();
      ctx->pop();

      return ParseResult<lexer::Token>::Match(std::move(token), true);
    }
    return ParseResult<lexer::Token>::NoMatch();
  }
};

} // namespace nodle::parser::rules
