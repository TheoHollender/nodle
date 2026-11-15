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
using nodle::parser::rules::ChainRule;
using nodle::parser::rules::OptionRule;
using nodle::parser::rules::OrRule;
using nodle::parser::rules::ParseResult;
using nodle::parser::rules::TokenRule;
using nodle::parser::rules::option_utils::collapse_into_option;
using nodle::parser::rules::option_utils::optional_collapsed_t;
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
  ParseResult<T> parse(ParserContextPtr) const {
    return ParseResult<T>::Match(value, false);
  }
};

// Never matches.
template <typename T> struct FailRule : BasicRuleTrait<FailRule<T>, T> {
  ParseResult<T> parse(ParserContextPtr) const { return ParseResult<T>::NoMatch(); }
};

// Consumes one token of any type and yields `value`.
template <typename T> struct EatRule : BasicRuleTrait<EatRule<T>, T> {
  T value;
  explicit EatRule(T v) : value(std::move(v)) {}
  ParseResult<T> parse(ParserContextPtr ctx) const {
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
  ParseResult<std::unique_ptr<int>> parse(ParserContextPtr ctx) const {
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
  ParseResult<typename R::return_type> parse(ParserContextPtr ctx) const {
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

struct X {
  bool operator==(const X &) const = default;
};

} // namespace

// NOTE: write `OptionRule rule{a};` with braces (see the ChainRule tests).
// `OptionRule rule{OptionRule{a}};` deduces the copy constructor
// (OptionRule<A>), not OptionRule<OptionRule<A>>. The nesting tests spell the
// type out so that they really test an option of an option.

// ------------------------------------------------------- optional_collapsed_t

static_assert(std::is_same_v<optional_collapsed_t<int>, std::optional<int>>);
static_assert(std::is_same_v<optional_collapsed_t<std::optional<int>>,
                             std::optional<int>>);
static_assert(
    std::is_same_v<optional_collapsed_t<std::optional<std::optional<int>>>,
                   std::optional<int>>);
static_assert(std::is_same_v<optional_collapsed_t<std::optional<
                                 std::optional<std::optional<std::string>>>>,
                             std::optional<std::string>>);
// Only optionals are collapsed.
static_assert(std::is_same_v<optional_collapsed_t<std::variant<int, X>>,
                             std::optional<std::variant<int, X>>>);
static_assert(std::is_same_v<optional_collapsed_t<std::tuple<int, X>>,
                             std::optional<std::tuple<int, X>>>);
static_assert(
    std::is_same_v<optional_collapsed_t<std::variant<std::optional<int>, X>>,
                   std::optional<std::variant<std::optional<int>, X>>>);

// -----------------------------------------------------------
// collapse_into_option

TEST(CollapseIntoOption, WrapsAPlainValue) {
  auto r = collapse_into_option(5);
  static_assert(std::is_same_v<decltype(r), std::optional<int>>);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 5);
}

TEST(CollapseIntoOption, KeepsAnEngagedOptionalAsIs) {
  auto r = collapse_into_option(std::optional<int>(5));
  static_assert(std::is_same_v<decltype(r), std::optional<int>>);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 5);
}

TEST(CollapseIntoOption, KeepsAnEmptyOptionalEmpty) {
  auto r = collapse_into_option(std::optional<int>());
  static_assert(std::is_same_v<decltype(r), std::optional<int>>);
  EXPECT_FALSE(r.has_value());
}

TEST(CollapseIntoOption, CollapsesTwoLevels) {
  auto r = collapse_into_option(
      std::optional<std::optional<int>>(std::optional<int>(5)));
  static_assert(std::is_same_v<decltype(r), std::optional<int>>);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, 5);
}

TEST(CollapseIntoOption, AnEmptyInnerOptionalGivesAnEmptyResult) {
  auto r = collapse_into_option(
      std::optional<std::optional<int>>(std::optional<int>()));
  static_assert(std::is_same_v<decltype(r), std::optional<int>>);
  EXPECT_FALSE(r.has_value());
}

TEST(CollapseIntoOption, AnEmptyOuterOptionalGivesAnEmptyResult) {
  auto r = collapse_into_option(std::optional<std::optional<int>>());
  static_assert(std::is_same_v<decltype(r), std::optional<int>>);
  EXPECT_FALSE(r.has_value());
}

TEST(CollapseIntoOption, CollapsesThreeLevels) {
  using Deep = std::optional<std::optional<std::optional<std::string>>>;
  auto r = collapse_into_option(Deep(std::optional<std::optional<std::string>>(
      std::optional<std::string>("deep"))));
  static_assert(std::is_same_v<decltype(r), std::optional<std::string>>);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(*r, "deep");
}

TEST(CollapseIntoOption, MovesAMoveOnlyValue) {
  auto r = collapse_into_option(std::make_unique<int>(4));
  static_assert(
      std::is_same_v<decltype(r), std::optional<std::unique_ptr<int>>>);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(**r, 4);
}

TEST(CollapseIntoOption, MovesAMoveOnlyValueOutOfAnOptional) {
  auto r = collapse_into_option(
      std::optional<std::unique_ptr<int>>(std::make_unique<int>(4)));
  static_assert(
      std::is_same_v<decltype(r), std::optional<std::unique_ptr<int>>>);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(**r, 4);
}

TEST(CollapseIntoOption, DoesNotUnpackOtherWrappers) {
  auto r =
      collapse_into_option(std::variant<int, std::string>(std::string("s")));
  static_assert(std::is_same_v<decltype(r),
                               std::optional<std::variant<int, std::string>>>);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(std::get<std::string>(*r), "s");
}

// ------------------------------------------------------------------ the rule's
// type

using OptionalToken = OptionRule<TokenRule>;

static_assert(BasicRule<OptionalToken>);
static_assert(std::is_same_v<OptionalToken::return_type, std::optional<Token>>);
static_assert(
    std::is_same_v<OptionRule<EatRule<int>>::return_type, std::optional<int>>);
static_assert(
    std::is_same_v<OptionRule<ChainRule<TokenRule, TokenRule>>::return_type,
                   std::optional<std::tuple<Token, Token>>>);
static_assert(
    std::is_same_v<
        OptionRule<OrRule<EatRule<int>, EatRule<std::string>>>::return_type,
        std::optional<std::variant<int, std::string>>>);
// Optionals are collapsed: an option never returns an optional of an optional.
static_assert(std::is_same_v<OptionRule<OptionRule<EatRule<int>>>::return_type,
                             std::optional<int>>);
static_assert(
    std::is_same_v<OptionRule<EatRule<std::optional<int>>>::return_type,
                   std::optional<int>>);
static_assert(
    std::is_same_v<
        OptionRule<EatRule<std::optional<std::optional<int>>>>::return_type,
        std::optional<int>>);
static_assert(
    std::is_same_v<OptionRule<OptionRule<OptionRule<TokenRule>>>::return_type,
                   std::optional<Token>>);

TEST(OptionRule, DeducesItsTemplateArgumentFromTheSubRule) {
  OptionRule rule{TokenRule(K_VAL)};
  static_assert(std::is_same_v<decltype(rule), OptionalToken>);
  SUCCEED();
}

// ------------------------------------------------------------------------
// matching

TEST(OptionRule, HoldsTheValueWhenTheSubRuleMatches) {
  OptionRule rule{TokenRule(K_VAL)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ((*result)->text(), "val");
  EXPECT_EQ((*result)->token_type(), K_VAL);
}

TEST(OptionRule, ConsumesTheTokensOfTheSubRuleWhenItMatches) {
  OptionRule rule{TokenRule(K_VAL)};
  auto ctx = sample_context();
  rule.parse(ctx);
  ASSERT_TRUE(ctx->remaining());
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OptionRule, MatchesAtTheCurrentPosition) {
  OptionRule rule{TokenRule(D_ASSIGN)};
  auto ctx = sample_context();
  ctx->pop();
  ctx->pop();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ((*result)->text(), "=");
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(OptionRule, ReturnedTokenKeepsItsSourceLocation) {
  OptionRule rule{TokenRule(L_INT)};
  auto ctx = sample_context();
  for (int i = 0; i < 3; ++i) {
    ctx->pop();
  }
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ((*result)->source_location().offset(), 8u);
  EXPECT_EQ((*result)->source_location().length(), 2u);
}

// ----------------------------------------------- the sub rule does not match

TEST(OptionRule, StillMatchesWhenTheSubRuleDoesNot) {
  OptionRule rule{TokenRule(D_SEMICOLON)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.is_no_match());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->has_value());
}

TEST(OptionRule, AnAbsentValueIsAZeroLengthMatch) {
  OptionRule rule{TokenRule(D_SEMICOLON)};
  auto result = rule.parse(sample_context());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result.is_match_consumed());
}

TEST(OptionRule, ConsumesNothingWhenTheSubRuleDoesNotMatch) {
  OptionRule rule{TokenRule(D_SEMICOLON)};
  auto ctx = sample_context();
  rule.parse(ctx);
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(OptionRule, EmptyInputGivesAnAbsentValue) {
  OptionRule rule{TokenRule(K_VAL)};
  auto ctx = std::make_shared<ParserContext>(std::vector<Token>{});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result->has_value());
}

TEST(OptionRule, NeverReturnsNoMatch) {
  OptionRule failing{FailRule<int>{}};
  OptionRule matching{EatRule<int>(1)};
  EXPECT_FALSE(failing.parse(sample_context()).is_no_match());
  EXPECT_FALSE(matching.parse(sample_context()).is_no_match());
  auto empty = std::make_shared<ParserContext>(std::vector<Token>{});
  EXPECT_FALSE(failing.parse(empty).is_no_match());
}

TEST(OptionRule, ASecondCallAfterTheTokenWasConsumedGivesAnAbsentValue) {
  OptionRule rule{TokenRule(K_VAL)};
  auto ctx = sample_context();
  auto first = rule.parse(ctx);
  auto second = rule.parse(ctx); // now at "x"
  ASSERT_TRUE(first.has_value());
  EXPECT_TRUE(first->has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_FALSE(second->has_value());
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
}

// ---------------------------------------------- the sub rule's status is kept

TEST(OptionRule, ConsumedWhenTheSubRuleConsumed) {
  OptionRule rule{EatRule<int>(1)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OptionRule, ZeroLengthWithAValueWhenTheSubRuleMatchedZeroLength) {
  OptionRule rule{ZeroRule<int>(5)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  ASSERT_TRUE(result->has_value()); // present, unlike an absent option
  EXPECT_EQ(**result, 5);
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(OptionRule, ThePresenceOfTheValueIsIndependentOfTheMatchStatus) {
  OptionRule zero_present{ZeroRule<int>(1)};
  OptionRule zero_absent{FailRule<int>{}};
  auto a = zero_present.parse(sample_context());
  auto b = zero_absent.parse(sample_context());
  EXPECT_TRUE(a.is_match_zero());
  EXPECT_TRUE(b.is_match_zero());
  EXPECT_TRUE(a->has_value());
  EXPECT_FALSE(b->has_value());
}

// ------------------------------------------------------ running the sub rule

TEST(OptionRule, RunsTheSubRuleExactlyOnceWhenItMatches) {
  auto log = std::make_shared<std::vector<std::string>>();
  OptionRule rule{LoggedRule(EatRule<int>(1), "sub", log)};
  rule.parse(sample_context());
  EXPECT_EQ(*log, (std::vector<std::string>{"sub"}));
}

TEST(OptionRule, RunsTheSubRuleExactlyOnceWhenItDoesNotMatch) {
  auto log = std::make_shared<std::vector<std::string>>();
  OptionRule rule{LoggedRule(FailRule<int>{}, "sub", log)};
  rule.parse(sample_context());
  EXPECT_EQ(*log, (std::vector<std::string>{"sub"}));
}

// --------------------------------------------------------------------- values

TEST(OptionRule, TheValueIsAnOptionalOfTheSubRuleValue) {
  OptionRule rule{EatRule<std::string>("hello")};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::optional<std::string>>);
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(**result, "hello");
}

TEST(OptionRule, AnAbsentValueIsAnEmptyOptional) {
  OptionRule rule{FailRule<std::string>{}};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, std::nullopt);
}

TEST(OptionRule, MoveOnlyValuesAreMovedNotCopied) {
  OptionRule rule{UniqueRule(3)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(***result, 3);
}

TEST(OptionRule, AnAbsentMoveOnlyValueIsEmpty) {
  OptionRule rule{FailRule<std::unique_ptr<int>>{}};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->has_value());
}

TEST(OptionRule, ADefaultValueIsNotConfusedWithAnAbsentOne) {
  OptionRule rule{EatRule<int>(0)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(**result, 0);
}

// ------------------------------------------------------------------------
// nesting

TEST(OptionRule, AnOptionOfAnOptionThatMatchesIsASingleOptional) {
  OptionRule<OptionRule<TokenRule>> rule{OptionRule{TokenRule(K_VAL)}};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::optional<Token>>);
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ((*result)->text(), "val");
}

TEST(OptionRule, AnOptionOfAnOptionThatDoesNotMatchIsAnEmptyOptional) {
  OptionRule<OptionRule<TokenRule>> rule{OptionRule{TokenRule(D_SEMICOLON)}};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result->has_value());
}

TEST(OptionRule, ANestedOptionThatFailsConsumesNothing) {
  OptionRule<OptionRule<TokenRule>> rule{OptionRule{TokenRule(D_SEMICOLON)}};
  auto ctx = sample_context();
  rule.parse(ctx);
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(OptionRule, ThreeNestedOptionsStillGiveASingleOptional) {
  OptionRule<OptionRule<OptionRule<TokenRule>>> rule{
      OptionRule<OptionRule<TokenRule>>{OptionRule{TokenRule(K_VAL)}}};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::optional<Token>>);
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ((*result)->text(), "val");
}

TEST(OptionRule, AnOptionalValuedSubRuleIsNotWrappedAgain) {
  OptionRule rule{EatRule<std::optional<int>>(std::optional<int>(8))};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::optional<int>>);
  EXPECT_TRUE(result.is_match_consumed());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(**result, 8);
}

TEST(OptionRule, AnEmptyOptionalFromTheSubRuleGivesAnEmptyOptional) {
  // The sub rule matched (and consumed a token) but its value is empty: the
  // value is empty too, while the match status of the sub rule is kept.
  OptionRule rule{EatRule<std::optional<int>>(std::optional<int>())};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_FALSE(result->has_value());
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OptionRule,
     AnEmptyOptionalFromAZeroLengthSubRuleIsAZeroLengthEmptyOption) {
  OptionRule rule{ZeroRule<std::optional<int>>(std::optional<int>())};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result->has_value());
}

TEST(OptionRule, ADoublyOptionalSubRuleValueIsCollapsed) {
  using Opt2 = std::optional<std::optional<int>>;
  OptionRule rule{EatRule<Opt2>(Opt2(std::optional<int>(3)))};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::optional<int>>);
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(**result, 3);
}

// ------------------------------------------------------ with the ChainRule

TEST(OptionRule, AnOptionalChainIsAnOptionalTuple) {
  OptionRule rule{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::optional<std::tuple<Token, Token>>>);
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(std::get<0>(**result).text(), "val");
  EXPECT_EQ(std::get<1>(**result).text(), "x");
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(OptionRule, AnOptionalChainThatDoesNotStartMatchingIsAbsent) {
  OptionRule rule{ChainRule{TokenRule(D_SEMICOLON), TokenRule(I_IDENTIFIER)}};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result->has_value());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

TEST(OptionRule, AChainFailingAfterConsumingTokensStillThrows) {
  // "val" is consumed, then ';' is expected but 'x' is found: the option does
  // not hide the error.
  OptionRule rule{ChainRule{TokenRule(K_VAL), TokenRule(D_SEMICOLON)}};
  EXPECT_THROW(rule.parse(sample_context()), std::runtime_error);
}

TEST(OptionRule, ATrailingOptionInAChainIsPresentWhenItMatches) {
  ChainRule chain{TokenRule(K_VAL), OptionRule{TokenRule(I_IDENTIFIER)}};
  static_assert(std::is_same_v<decltype(chain)::return_type,
                               std::tuple<Token, std::optional<Token>>>);
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(std::get<0>(*result).text(), "val");
  ASSERT_TRUE(std::get<1>(*result).has_value());
  EXPECT_EQ(std::get<1>(*result)->text(), "x");
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(OptionRule, ATrailingOptionInAChainIsAbsentWhenItDoesNotMatch) {
  ChainRule chain{TokenRule(K_VAL), OptionRule{TokenRule(D_SEMICOLON)}};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value()); // no throw: the option always matches
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(std::get<0>(*result).text(), "val");
  EXPECT_FALSE(std::get<1>(*result).has_value());
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
}

TEST(OptionRule, ALeadingOptionInAChainIsPresentWhenItMatches) {
  ChainRule chain{OptionRule{TokenRule(K_VAL)}, TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::get<0>(*result).has_value());
  EXPECT_EQ(std::get<0>(*result)->text(), "val");
  EXPECT_EQ(std::get<1>(*result).text(), "x");
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(OptionRule, ALeadingOptionInAChainCanBeSkipped) {
  ChainRule chain{OptionRule{TokenRule(D_SEMICOLON)}, TokenRule(K_VAL)};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_FALSE(std::get<0>(*result).has_value());
  EXPECT_EQ(std::get<1>(*result).text(), "val");
}

TEST(OptionRule, AChainAfterAnAbsentOptionIsNoMatchIfItsSecondRuleFails) {
  // The option matched zero tokens, so there is nothing to commit to.
  ChainRule chain{OptionRule{TokenRule(D_SEMICOLON)}, TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  EXPECT_TRUE(chain.parse(ctx).is_no_match());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

TEST(OptionRule, ChainOfTwoOptionsIsZeroLengthWhenBothAreAbsent) {
  ChainRule chain{OptionRule{TokenRule(D_SEMICOLON)},
                  OptionRule{TokenRule(L_INT)}};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(std::get<0>(*result).has_value());
  EXPECT_FALSE(std::get<1>(*result).has_value());
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(OptionRule, ChainOfTwoOptionsKeepsTheOrderOfTheTokens) {
  ChainRule chain{OptionRule{TokenRule(K_VAL)},
                  OptionRule{TokenRule(I_IDENTIFIER)}};
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  ASSERT_TRUE(std::get<0>(*result).has_value());
  ASSERT_TRUE(std::get<1>(*result).has_value());
  EXPECT_EQ(std::get<0>(*result)->text(), "val");
  EXPECT_EQ(std::get<1>(*result)->text(), "x");
}

TEST(OptionRule, ChainOfTwoOptionsInTheWrongOrderOnlyMatchesTheFirstOne) {
  ChainRule chain{OptionRule{TokenRule(I_IDENTIFIER)},
                  OptionRule{TokenRule(K_VAL)}};
  auto ctx = sample_context(); // starts with "val", not an identifier
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(std::get<0>(*result).has_value());
  ASSERT_TRUE(std::get<1>(*result).has_value());
  EXPECT_EQ(std::get<1>(*result)->text(), "val");
}

// --------------------------------------------------------- with the OrRule

TEST(OptionRule, AnOptionalOrIsAnOptionalVariant) {
  OptionRule rule{OrRule{TokenRule(D_SEMICOLON), EatRule<int>(4)}};
  static_assert(std::is_same_v<decltype(rule)::return_type,
                               std::optional<std::variant<Token, int>>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ((*result)->index(), 1u);
  EXPECT_EQ(std::get<int>(**result), 4);
}

TEST(OptionRule, AnOptionalOrWithNoMatchingAlternativeIsAbsent) {
  OptionRule rule{OrRule{TokenRule(D_SEMICOLON), TokenRule(L_INT)}};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result->has_value());
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(OptionRule, AnOptionAsTheSecondAlternativeOfAnOrIsReachedAndPresent) {
  OrRule rule{TokenRule(D_SEMICOLON), OptionRule{TokenRule(K_VAL)}};
  static_assert(std::is_same_v<decltype(rule)::return_type,
                               std::variant<Token, std::optional<Token>>>);
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 1u);
  ASSERT_TRUE(std::get<1>(*result).has_value());
  EXPECT_EQ(std::get<1>(*result)->text(), "val");
}

TEST(OptionRule, AnOptionAsTheFirstAlternativeOfAnOrShadowsTheSecondOne) {
  // An option always matches, so the alternatives after it are unreachable.
  auto log = std::make_shared<std::vector<std::string>>();
  OrRule rule{OptionRule{TokenRule(D_SEMICOLON)},
              LoggedRule(TokenRule(K_VAL), "second", log)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 0u);
  EXPECT_FALSE(std::get<0>(*result).has_value());
  EXPECT_TRUE(log->empty());
  EXPECT_EQ(drain(ctx), 5u);
}

// -------------------------------------------------------------------------
// reuse

TEST(OptionRule, OneRuleObjectCanBeUsedRepeatedlyAndOnDifferentContexts) {
  OptionRule rule{TokenRule(K_VAL)};
  auto first = sample_context();
  auto second = sample_context();
  auto a = rule.parse(first);
  auto b = rule.parse(second);
  auto c = rule.parse(first); // `first` now starts at "x"
  EXPECT_TRUE(a->has_value());
  EXPECT_TRUE(b->has_value());
  EXPECT_FALSE(c->has_value());
  EXPECT_EQ(first->peek().token_type(), I_IDENTIFIER);
  EXPECT_EQ(second->peek().token_type(), I_IDENTIFIER);
}

TEST(OptionRule,
     OneRuleObjectCanBeUsedRepeatedlyAndOnDifferentContextsWithNot) {
  auto rule = ~TokenRule(K_VAL);
  auto first = sample_context();
  auto second = sample_context();
  auto a = rule.parse(first);
  auto b = rule.parse(second);
  auto c = rule.parse(first); // `first` now starts at "x"
  EXPECT_TRUE(a->has_value());
  EXPECT_TRUE(b->has_value());
  EXPECT_FALSE(c->has_value());
  EXPECT_EQ(first->peek().token_type(), I_IDENTIFIER);
  EXPECT_EQ(second->peek().token_type(), I_IDENTIFIER);
}