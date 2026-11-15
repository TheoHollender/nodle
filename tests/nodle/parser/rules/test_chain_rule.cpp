#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"
#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"
#include "nodle/parser/rules/chain.hpp"
#include "nodle/parser/rules/token.hpp"

using nodle::io::SourceFile;
using nodle::io::SourceLocation;
using nodle::lexer::Token;
using nodle::lexer::TokenType;
using nodle::parser::ParserContext;
using nodle::parser::ParserContextPtr;
using nodle::parser::rules::BasicRule;
using nodle::parser::rules::BasicRuleTrait;
using nodle::parser::rules::ChainRule;
using nodle::parser::rules::ParseResult;
using nodle::parser::rules::TokenRule;
using enum nodle::lexer::TokenType;

namespace {

// The tokens of "val x = 42;"  ->  K_VAL, I_IDENTIFIER, D_ASSIGN, L_INT,
// D_SEMICOLON
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

// ---- small rules with controllable behaviour
// -------------------------------------------

// Matches without consuming anything.
template <typename T> struct ZeroRule : BasicRuleTrait<ZeroRule<T>, T> {
  T value;
  explicit ZeroRule(T v) : value(std::move(v)) {}
  ParseResult<T> parse(ParserContextPtr) {
    return ParseResult<T>::Match(value, false);
  }
};

// Never matches.
template <typename T> struct FailRule : BasicRuleTrait<FailRule<T>, T> {
  ParseResult<T> parse(ParserContextPtr) { return ParseResult<T>::NoMatch(); }
};

// Consumes one token of any type and yields `value`.
template <typename T> struct EatRule : BasicRuleTrait<EatRule<T>, T> {
  T value;
  explicit EatRule(T v) : value(std::move(v)) {}
  ParseResult<T> parse(ParserContextPtr ctx) {
    if (!ctx->remaining()) {
      return ParseResult<T>::NoMatch();
    }
    ctx->pop();
    return ParseResult<T>::Match(value, true);
  }
};

// Consumes one token and yields a move-only value.
struct UniqueRule : BasicRuleTrait<UniqueRule, std::unique_ptr<int>> {
  int value;
  explicit UniqueRule(int v) : value(v) {}
  ParseResult<std::unique_ptr<int>> parse(ParserContextPtr ctx) {
    if (!ctx->remaining()) {
      return ParseResult<std::unique_ptr<int>>::NoMatch();
    }
    ctx->pop();
    return ParseResult<std::unique_ptr<int>>::Match(
        std::make_unique<int>(value), true);
  }
};

// Records that it was run, then delegates.
template <BasicRule R>
struct LoggedRule : BasicRuleTrait<LoggedRule<R>, typename R::return_type> {
  R inner;
  std::string name;
  std::shared_ptr<std::vector<std::string>> log;
  LoggedRule(R r, std::string n, std::shared_ptr<std::vector<std::string>> l)
      : inner(std::move(r)), name(std::move(n)), log(std::move(l)) {}
  ParseResult<typename R::return_type> parse(ParserContextPtr ctx) {
    log->push_back(name);
    return inner.parse(ctx);
  }
};

// Number of tokens left in the context (consumes it).
std::size_t drain(const ParserContextPtr &ctx) {
  std::size_t n = 0;
  while (ctx->remaining()) {
    ctx->pop();
    ++n;
  }
  return n;
}

} // namespace

// NOTE: write `ChainRule chain{a, b};` with braces. With parentheses,
// `ChainRule chain(TokenRule(K_VAL), TokenRule(K_VAL));` is parsed as a
// function declaration (the "most vexing parse").

// ------------------------------------------------------------------ the rule's
// type

using TwoTokens = ChainRule<TokenRule, TokenRule>;

static_assert(BasicRule<TwoTokens>);
static_assert(std::is_same_v<TwoTokens::return_type, std::tuple<Token, Token>>);
static_assert(std::is_same_v<ChainRule<TwoTokens, TokenRule>::return_type,
                             std::tuple<Token, Token, Token>>);
static_assert(std::is_same_v<ChainRule<TokenRule, TwoTokens>::return_type,
                             std::tuple<Token, Token, Token>>);
static_assert(std::is_same_v<ChainRule<TwoTokens, TwoTokens>::return_type,
                             std::tuple<Token, Token, Token, Token>>);
static_assert(std::is_same_v<ChainRule<EatRule<int>, TokenRule>::return_type,
                             std::tuple<int, Token>>);
static_assert(std::is_same_v<ChainRule<EatRule<std::tuple<int, std::string>>,
                                       TokenRule>::return_type,
                             std::tuple<int, std::string, Token>>);

TEST(ChainRule, DeducesItsTemplateArgumentsFromTheSubRules) {
  ChainRule chain{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  static_assert(std::is_same_v<decltype(chain), TwoTokens>);
  SUCCEED();
}

// ------------------------------------------------------------------------
// matching

TEST(ChainRule, MatchesTwoConsecutiveTokens) {
  ChainRule chain{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(std::get<0>(*result).text(), "val");
  EXPECT_EQ(std::get<1>(*result).text(), "x");
  EXPECT_EQ(std::get<0>(*result).token_type(), K_VAL);
  EXPECT_EQ(std::get<1>(*result).token_type(), I_IDENTIFIER);
}

TEST(ChainRule, ConsumesTheTokensOfBothRules) {
  ChainRule chain{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  chain.parse(ctx);
  ASSERT_TRUE(ctx->remaining());
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
  EXPECT_EQ(drain(ctx), 3u);
}

TEST(ChainRule, MatchesAtTheCurrentPosition) {
  ChainRule chain{TokenRule(D_ASSIGN), TokenRule(L_INT)};
  auto ctx = sample_context();
  ctx->pop();
  ctx->pop();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(*result).text(), "=");
  EXPECT_EQ(std::get<1>(*result).text(), "42");
  EXPECT_EQ(ctx->peek().token_type(), D_SEMICOLON);
}

TEST(ChainRule, ASequenceOfTheWholeInput) {
  ChainRule chain{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  ChainRule rest{TokenRule(D_ASSIGN), TokenRule(L_INT)};
  auto ctx = sample_context();
  ASSERT_TRUE(chain.parse(ctx).is_match_consumed());
  ASSERT_TRUE(rest.parse(ctx).is_match_consumed());
  EXPECT_EQ(ctx->peek().token_type(), D_SEMICOLON);
}

TEST(ChainRule, ReturnedTokensKeepTheirSourceLocation) {
  ChainRule chain{TokenRule(D_ASSIGN), TokenRule(L_INT)};
  auto ctx = sample_context();
  ctx->pop();
  ctx->pop();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(*result).source_location().offset(), 6u);
  EXPECT_EQ(std::get<1>(*result).source_location().offset(), 8u);
  EXPECT_EQ(std::get<1>(*result).source_location().length(), 2u);
}

// ------------------------------------------------------------------- not
// matching

TEST(ChainRule, NoMatchWhenTheFirstRuleFails) {
  ChainRule chain{TokenRule(D_SEMICOLON), TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  EXPECT_TRUE(result.is_no_match());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL); // nothing consumed
}

TEST(ChainRule, FirstRuleFailingDoesNotRunTheSecondOne) {
  auto log = std::make_shared<std::vector<std::string>>();
  ChainRule chain{LoggedRule(FailRule<int>{}, "first", log),
                  LoggedRule(EatRule<int>(1), "second", log)};
  auto ctx = sample_context();
  EXPECT_TRUE(chain.parse(ctx).is_no_match());
  EXPECT_EQ(*log, (std::vector<std::string>{"first"}));
}

TEST(ChainRule, RunsTheFirstRuleBeforeTheSecond) {
  auto log = std::make_shared<std::vector<std::string>>();
  ChainRule chain{LoggedRule(EatRule<int>(1), "first", log),
                  LoggedRule(EatRule<int>(2), "second", log)};
  auto ctx = sample_context();
  chain.parse(ctx);
  EXPECT_EQ(*log, (std::vector<std::string>{"first", "second"}));
}

TEST(ChainRule, EmptyInputGivesNoMatch) {
  ChainRule chain{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  auto ctx = std::make_shared<ParserContext>(std::vector<Token>{});
  EXPECT_TRUE(chain.parse(ctx).is_no_match());
}

TEST(ChainRule, NoMatchWhenTheFirstTokenIsWrongEvenIfTheSecondWouldMatch) {
  ChainRule chain{TokenRule(I_IDENTIFIER), TokenRule(D_ASSIGN)};
  auto ctx = sample_context(); // starts with K_VAL
  EXPECT_TRUE(chain.parse(ctx).is_no_match());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

// ---------------------------------------- the first rule matched, the second
// did not

TEST(ChainRule, ThrowsWhenTheSecondRuleFailsAfterTheFirstConsumedTokens) {
  ChainRule chain{TokenRule(K_VAL),
                  TokenRule(D_SEMICOLON)}; // "val" then "x", not ';'
  auto ctx = sample_context();
  EXPECT_THROW(chain.parse(ctx), std::runtime_error);
}

TEST(ChainRule, ThrowsWhenTheInputEndsInTheMiddleOfAChain) {
  ChainRule chain{TokenRule(D_SEMICOLON), TokenRule(K_VAL)};
  auto ctx = sample_context();
  for (int i = 0; i < 4; ++i) {
    ctx->pop();
  }
  ASSERT_EQ(ctx->peek().token_type(), D_SEMICOLON); // the last token
  EXPECT_THROW(chain.parse(ctx), std::runtime_error);
}

TEST(ChainRule,
     NoMatchWhenTheFirstRuleMatchedWithoutConsumingAndTheSecondFails) {
  // Nothing was consumed, so there is nothing to commit to: this is a plain
  // no-match.
  ChainRule chain{ZeroRule<int>(1), FailRule<int>{}};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  EXPECT_TRUE(result.is_no_match());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

// ---------------------------------------------- consumed vs zero-length
// combinations

TEST(ChainRule, ConsumedIfBothConsumed) {
  ChainRule chain{EatRule<int>(1), EatRule<int>(2)};
  EXPECT_TRUE(chain.parse(sample_context()).is_match_consumed());
}
TEST(ChainRule, ConsumedIfBothConsumedWithOperand) {
  auto chain = EatRule<int>(1) >> EatRule<int>(2);
  EXPECT_TRUE(chain.parse(sample_context()).is_match_consumed());
}

TEST(ChainRule, ConsumedIfOnlyTheFirstConsumed) {
  ChainRule chain{EatRule<int>(1), ZeroRule<int>(2)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(ChainRule, ConsumedIfOnlyTheSecondConsumed) {
  ChainRule chain{ZeroRule<int>(1), EatRule<int>(2)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(ChainRule, ZeroLengthIfNeitherConsumed) {
  ChainRule chain{ZeroRule<int>(1), ZeroRule<int>(2)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result.is_match_consumed());
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(drain(ctx), 5u); // nothing consumed
}

TEST(ChainRule, ZeroLengthMatchesStillCarryTheirValues) {
  ChainRule chain{ZeroRule<int>(1), ZeroRule<std::string>("two")};
  auto result = chain.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(*result), 1);
  EXPECT_EQ(std::get<1>(*result), "two");
}

// --------------------------------------------------------------------- values

TEST(ChainRule, ValuesOfDifferentTypesAreKeptInOrder) {
  ChainRule chain{EatRule<int>(7), EatRule<std::string>("seven")};
  auto result = chain.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::tuple<int, std::string>>);
  EXPECT_EQ(std::get<0>(*result), 7);
  EXPECT_EQ(std::get<1>(*result), "seven");
}

TEST(ChainRule, MoveOnlyValuesAreMovedNotCopied) {
  ChainRule chain{UniqueRule(1), UniqueRule(2)};
  auto result = chain.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*std::get<0>(*result), 1);
  EXPECT_EQ(*std::get<1>(*result), 2);
}

TEST(ChainRule, ATupleValuedSubRuleIsMergedIntoTheChain) {
  ChainRule chain{EatRule<std::tuple<int, std::string>>({1, "a"}),
                  EatRule<double>(2.5)};
  auto result = chain.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::tuple<int, std::string, double>>);
  EXPECT_EQ(std::get<0>(*result), 1);
  EXPECT_EQ(std::get<1>(*result), "a");
  EXPECT_EQ(std::get<2>(*result), 2.5);
}

// ------------------------------------------------------------------------
// nesting

TEST(ChainRule, ThreeRulesChainedOnTheLeftGiveAFlatTuple) {
  ChainRule chain{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
                  TokenRule(D_ASSIGN)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(*result)>> == 3);
  EXPECT_EQ(std::get<0>(*result).text(), "val");
  EXPECT_EQ(std::get<1>(*result).text(), "x");
  EXPECT_EQ(std::get<2>(*result).text(), "=");
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(ChainRule, ThreeRulesChainedOnTheRightGiveTheSameFlatTuple) {
  ChainRule chain{TokenRule(K_VAL),
                  ChainRule{TokenRule(I_IDENTIFIER), TokenRule(D_ASSIGN)}};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(*result)>> == 3);
  EXPECT_EQ(std::get<0>(*result).text(), "val");
  EXPECT_EQ(std::get<1>(*result).text(), "x");
  EXPECT_EQ(std::get<2>(*result).text(), "=");
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(ChainRule, FourRulesFromTwoChains) {
  ChainRule left{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  ChainRule right{TokenRule(D_ASSIGN), TokenRule(L_INT)};
  ChainRule chain{left, right};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(*result)>> == 4);
  EXPECT_EQ(std::get<3>(*result).text(), "42");
  EXPECT_EQ(ctx->peek().token_type(), D_SEMICOLON);
}

TEST(ChainRule,
     AFailureDeepInsideANestedChainStillThrowsOnceTokensWereConsumed) {
  // val x ';'  -> the inner chain matches "val x", the outer second rule fails
  // on '='.
  ChainRule chain{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
                  TokenRule(D_SEMICOLON)};
  EXPECT_THROW(chain.parse(sample_context()), std::runtime_error);
}

TEST(ChainRule, ANestedChainThatDoesNotStartMatchingIsAPlainNoMatch) {
  ChainRule chain{ChainRule{TokenRule(D_SEMICOLON), TokenRule(I_IDENTIFIER)},
                  TokenRule(D_ASSIGN)};
  auto ctx = sample_context();
  EXPECT_TRUE(chain.parse(ctx).is_no_match());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

TEST(ChainRule, MixedZeroLengthAndConsumingRulesInANestedChain) {
  ChainRule chain{ChainRule{ZeroRule<int>(1), ZeroRule<int>(2)},
                  EatRule<int>(3)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(*result, (std::tuple<int, int, int>{1, 2, 3}));
}

// -------------------------------------------------------------------------
// reuse

TEST(ChainRule, OneRuleObjectCanBeUsedRepeatedlyAndOnDifferentContexts) {
  ChainRule chain{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  auto first = sample_context();
  auto second = sample_context();
  EXPECT_TRUE(chain.parse(first).is_match_consumed());
  EXPECT_TRUE(chain.parse(second).is_match_consumed());
  EXPECT_TRUE(chain.parse(first).is_no_match()); // `first` now starts at '='
  EXPECT_EQ(first->peek().token_type(), D_ASSIGN);
}