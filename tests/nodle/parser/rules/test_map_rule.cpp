#include <gtest/gtest.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"
#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"
#include "nodle/parser/rules/call.hpp"
#include "nodle/parser/rules/chain.hpp"
#include "nodle/parser/rules/option.hpp"
#include "nodle/parser/rules/or.hpp"
#include "nodle/parser/rules/token.hpp"

using nodle::io::SourceFile;
using nodle::io::SourceLocation;
using nodle::lexer::Token;
using nodle::lexer::TokenType;
using nodle::parser::ParserContext;
using nodle::parser::ParserContextPtr;
using nodle::parser::rules::BasicRule;
using nodle::parser::rules::BasicRuleTrait;
using nodle::parser::rules::CallRule;
using nodle::parser::rules::ChainRule;
using nodle::parser::rules::OptionRule;
using nodle::parser::rules::OrRule;
using nodle::parser::rules::ParseResult;
using nodle::parser::rules::TokenRule;
using nodle::parser::rules::option_utils::collapse_into_option;
using nodle::parser::rules::option_utils::optional_collapsed_t;
using enum nodle::lexer::TokenType;

namespace {

nodle::parser::rules::ParseResult<int>
empty_rule_function(nodle::parser::ParserContextPtr ctx) {
  (void)ctx;
  return nodle::parser::rules::ParseResult<int>::NoMatch();
}
nodle::parser::rules::ParseResult<int>
zero_no_token_rule_function(nodle::parser::ParserContextPtr ctx) {
  (void)ctx;
  return nodle::parser::rules::ParseResult<int>::Match(0, false);
}
nodle::parser::rules::ParseResult<int>
zero_one_token_rule_function(nodle::parser::ParserContextPtr ctx) {
  (void)ctx;
  return nodle::parser::rules::ParseResult<int>::Match(0, true);
}

ParserContextPtr sample_context() {
  auto file = SourceFile::from_string("t.nodle", "val x = 42;");
  return std::make_shared<ParserContext>(std::vector<Token>{
      Token(K_VAL, SourceLocation(file, 0, 3)),
      Token(I_IDENTIFIER, SourceLocation(file, 4, 1)),
      Token(D_ASSIGN, SourceLocation(file, 6, 1)),
      Token(L_INT, SourceLocation(file, 8, 2)),
      Token(D_SEMICOLON, SourceLocation(file, 10, 1)),
  });
}

}; // namespace

TEST(MapRule, EmptyStaysEmpty) {
  auto rule = CallRule<int, empty_rule_function>{}.map<int>(
      [](int x) { return x + 1; });

  auto res = rule.parse(sample_context());
  EXPECT_FALSE(res.has_value());
}
TEST(MapRule, ZeroNoTokenStaysWithMapped) {
  auto rule = CallRule<int, zero_no_token_rule_function>{}.map<int>(
      [](int x) { return x + 1; });

  auto res = rule.parse(sample_context());
  EXPECT_TRUE(res.has_value());
  EXPECT_FALSE(res.is_match_consumed());
  EXPECT_EQ((*res), 1);
}
TEST(MapRule, ZeroWithTokenStaysWithMapped) {
  auto rule = CallRule<int, zero_one_token_rule_function>{}.map<int>(
      [](int x) { return x + 1; });

  auto res = rule.parse(sample_context());
  EXPECT_TRUE(res.has_value());
  EXPECT_TRUE(res.is_match_consumed());
  EXPECT_EQ((*res), 1);
}
