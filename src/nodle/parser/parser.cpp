
#include "nodle/parser/parser.hpp"
#include "nodle/ast/ast.hpp"
#include <string>

using namespace nodle::io;
using namespace nodle::ast;
using namespace nodle::lexer;
using namespace nodle::parser;

namespace {
auto make_object_rule() {}

auto make_term_rule() {
  return (rules::TokenRule{L_INT}.map<std::unique_ptr<Expression>>(
             [](Token tok) {
               return std::make_unique<EInteger>(
                   tok.source_location(),
                   std::atoi(std::string(tok.text()).c_str()));
             })) |
         (rules::TokenRule{L_FLOAT}.map<std::unique_ptr<Expression>>(
             [](Token tok) {
               return std::make_unique<EFloat>(
                   tok.source_location(),
                   std::atof(std::string(tok.text()).c_str()));
             })) |
         (rules::TokenRule{L_TRUE}.map<std::unique_ptr<Expression>>(
             [](Token tok) {
               return std::make_unique<EBool>(tok.source_location(), true);
             })) |
         (rules::TokenRule{L_FALSE}.map<std::unique_ptr<Expression>>(
             [](Token tok) {
               return std::make_unique<EBool>(tok.source_location(), false);
             })) |
         (rules::TokenRule{K_SELF}.map<std::unique_ptr<Expression>>(
             [](Token tok) {
               return std::make_unique<Self>(tok.source_location());
             }));
  // TODO var, not, tuple, field access, call, method call,
  // instantiation
}

static const auto term_rule = make_term_rule();

rules::ParseResult<std::unique_ptr<Expression>> term(ParserContextPtr ctx) {
  return term_rule.parse(ctx);
}
}; // namespace
