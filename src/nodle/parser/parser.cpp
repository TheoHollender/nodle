
#include "nodle/parser/parser.hpp"
#include "nodle/ast/ast.hpp"
#include <string>

using namespace nodle::io;
using namespace nodle::ast;
using namespace nodle::lexer;
using namespace nodle::parser;

namespace {
auto make_term_rule() {
  return (rules::TokenRule{L_INT}.map<std::unique_ptr<Expression>>(
             [](Token tok) {
               return std::make_unique<EInteger>(
                   tok, std::atoi(std::string(tok.text()).c_str()));
             })) |
         (rules::TokenRule{L_FLOAT}.map<std::unique_ptr<Expression>>(
             [](Token tok) {
               return std::make_unique<EFloat>(
                   tok, std::atof(std::string(tok.text()).c_str()));
             })) |
         (rules::TokenRule{L_TRUE}.map<std::unique_ptr<Expression>>(
             [](Token tok) { return std::make_unique<EBool>(tok, true); })) |
         (rules::TokenRule{L_FALSE}.map<std::unique_ptr<Expression>>(
             [](Token tok) { return std::make_unique<EBool>(tok, false); })) |
         (rules::TokenRule{K_SELF}.map<std::unique_ptr<Expression>>(
             [](Token tok) {
               return std::make_unique<Self>(tok);
             })); // TODO var, not, tuple, field access, call, method call,
                  // instantiation
}

static const auto term_rule =
    make_term_rule().map<std::unique_ptr<Expression>>( // std::variant<T> => T
        [](auto x) {
          return std::visit(
              [](auto &&alt) -> std::unique_ptr<Expression> {
                return std::move(alt);
              },
              std::move(x));
        });

// TODO comprendre pourquoi ce truc rate
rules::ParseResult<std::unique_ptr<Expression>> term(ParserContextPtr ctx) {
  return term_rule.parse(ctx);
}
}; // namespace
