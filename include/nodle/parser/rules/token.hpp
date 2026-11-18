
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
  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    int start = graph.allocate_node(rule);
    int end = graph.allocate_node(rule);

    graph.add_edge(graph::Edge::token(start, end, token_type_));

    return graph::Subgraph(start, end);
  }
};

} // namespace nodle::parser::rules
