#include <gtest/gtest.h>

#include <memory>
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
using nodle::parser::rules::OrRule;
using nodle::parser::rules::ParseResult;
using nodle::parser::rules::TokenRule;
using nodle::parser::rules::or_util::unpack_or;
using nodle::parser::rules::or_util::unpack_or_t;
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

struct X {
  bool operator==(const X &) const = default;
};
struct Y {
  bool operator==(const Y &) const = default;
};
struct Z {
  bool operator==(const Z &) const = default;
};

} // namespace

// NOTE: write `OrRule rule{a, b};` with braces. With parentheses,
// `OrRule rule(TokenRule(K_VAL), TokenRule(K_VAL));` is parsed as a
// function declaration (the "most vexing parse").

// ------------------------------------------------------------------
// unpack_or_t

static_assert(std::is_same_v<unpack_or_t<X, Y>, std::variant<X, Y>>);
static_assert(
    std::is_same_v<unpack_or_t<std::variant<X, Y>, Z>, std::variant<X, Y, Z>>);
static_assert(
    std::is_same_v<unpack_or_t<X, std::variant<Y, Z>>, std::variant<X, Y, Z>>);
static_assert(std::is_same_v<unpack_or_t<std::variant<X, Y>, std::variant<Z>>,
                             std::variant<X, Y, Z>>);
static_assert(std::is_same_v<unpack_or_t<std::variant<X>, std::variant<Y, Z>>,
                             std::variant<X, Y, Z>>);
static_assert(std::is_same_v<
              unpack_or_t<std::variant<X, Y>, std::variant<Z, std::string>>,
              std::variant<X, Y, Z, std::string>>);

// Only the outermost variants are flattened.
static_assert(std::is_same_v<unpack_or_t<std::variant<std::variant<X, Y>>, Z>,
                             std::variant<std::variant<X, Y>, Z>>);

// A tuple is a single alternative, not something to unpack.
static_assert(std::is_same_v<unpack_or_t<std::tuple<X, Y>, Z>,
                             std::variant<std::tuple<X, Y>, Z>>);

// Duplicated alternatives are removed.
static_assert(std::is_same_v<unpack_or_t<X, X>, std::variant<X>>);
static_assert(
    std::is_same_v<unpack_or_t<std::variant<X, Y>, X>, std::variant<X, Y>>);
static_assert(
    std::is_same_v<unpack_or_t<X, std::variant<X, Y>>, std::variant<X, Y>>);
static_assert(
    std::is_same_v<unpack_or_t<std::variant<X, Y>, std::variant<Y, Z>>,
                   std::variant<X, Y, Z>>);

// References and cv-qualifiers are stripped.
static_assert(std::is_same_v<unpack_or_t<const X &, Y &&>, std::variant<X, Y>>);

// ---------------------------------------------------------------- unpack_or

TEST(UnpackOr, PutsAPlainValueIntoTheVariant) {
  using R = unpack_or_t<X, Y>;
  R r = unpack_or<R>(Y{});
  EXPECT_EQ(r.index(), 1u);
}

TEST(UnpackOr, KeepsTheValueOfAPlainAlternative) {
  using R = unpack_or_t<int, std::string>;
  R r = unpack_or<R>(std::string("hello"));
  ASSERT_TRUE(std::holds_alternative<std::string>(r));
  EXPECT_EQ(std::get<std::string>(r), "hello");
}

TEST(UnpackOr, MovesAVariantIntoTheLeftOfAFlatVariant) {
  using R = unpack_or_t<std::variant<X, Y>, Z>;
  R r = unpack_or<R>(std::variant<X, Y>(Y{}));
  EXPECT_EQ(r.index(), 1u);
}

TEST(UnpackOr, MovesAVariantIntoTheRightOfAFlatVariant) {
  using R = unpack_or_t<X, std::variant<Y, Z>>;
  R r = unpack_or<R>(std::variant<Y, Z>(Z{}));
  EXPECT_EQ(r.index(), 2u);
}

TEST(UnpackOr, EveryAlternativeOfAVariantKeepsItsValue) {
  using Inner = std::variant<int, std::string>;
  using R = unpack_or_t<Inner, double>;
  R a = unpack_or<R>(Inner(3));
  R b = unpack_or<R>(Inner(std::string("s")));
  EXPECT_EQ(std::get<int>(a), 3);
  EXPECT_EQ(std::get<std::string>(b), "s");
}

TEST(UnpackOr, AnLvalueIsCopiedAndLeftIntact) {
  using R = unpack_or_t<int, std::string>;
  std::string s = "keep";
  R r = unpack_or<R>(s);
  EXPECT_EQ(std::get<std::string>(r), "keep");
  EXPECT_EQ(s, "keep");
}

TEST(UnpackOr, AMoveOnlyValueIsMoved) {
  using R = unpack_or_t<std::unique_ptr<int>, X>;
  R r = unpack_or<R>(std::make_unique<int>(5));
  ASSERT_TRUE(std::holds_alternative<std::unique_ptr<int>>(r));
  EXPECT_EQ(*std::get<std::unique_ptr<int>>(r), 5);
}

TEST(UnpackOr, AMoveOnlyValueInsideAVariantIsMoved) {
  using Inner = std::variant<std::unique_ptr<int>, X>;
  using R = unpack_or_t<Inner, Y>;
  R r = unpack_or<R>(Inner(std::make_unique<int>(6)));
  ASSERT_TRUE(std::holds_alternative<std::unique_ptr<int>>(r));
  EXPECT_EQ(*std::get<std::unique_ptr<int>>(r), 6);
}

TEST(UnpackOr, ADuplicatedAlternativeEndsUpOnce) {
  using R = unpack_or_t<std::variant<X, Y>, X>;
  static_assert(std::variant_size_v<R> == 2);
  R r = unpack_or<R>(X{});
  EXPECT_EQ(r.index(), 0u);
}

// ------------------------------------------------------------------ the rule's
// type

using TokenOrToken = OrRule<TokenRule, TokenRule>;

static_assert(BasicRule<TokenOrToken>);
static_assert(std::is_same_v<TokenOrToken::return_type, std::variant<Token>>);
static_assert(std::is_same_v<OrRule<EatRule<int>, TokenRule>::return_type,
                             std::variant<int, Token>>);
static_assert(
    std::is_same_v<OrRule<EatRule<int>, EatRule<std::string>>::return_type,
                   std::variant<int, std::string>>);
static_assert(std::is_same_v<OrRule<EatRule<int>, EatRule<int>>::return_type,
                             std::variant<int>>);
static_assert(std::is_same_v<OrRule<OrRule<EatRule<int>, EatRule<std::string>>,
                                    EatRule<double>>::return_type,
                             std::variant<int, std::string, double>>);
static_assert(std::is_same_v<
              OrRule<EatRule<double>,
                     OrRule<EatRule<int>, EatRule<std::string>>>::return_type,
              std::variant<double, int, std::string>>);
static_assert(
    std::is_same_v<OrRule<OrRule<EatRule<int>, EatRule<std::string>>,
                          OrRule<EatRule<double>, EatRule<int>>>::return_type,
                   std::variant<int, std::string, double>>);
static_assert(std::is_same_v<OrRule<EatRule<std::variant<int, std::string>>,
                                    EatRule<double>>::return_type,
                             std::variant<int, std::string, double>>);
static_assert(std::is_same_v<OrRule<ChainRule<TokenRule, TokenRule>,
                                    EatRule<int>>::return_type,
                             std::variant<std::tuple<Token, Token>, int>>);

TEST(OrRule, DeducesItsTemplateArgumentsFromTheSubRules) {
  OrRule rule{EatRule<int>(1), TokenRule(I_IDENTIFIER)};
  static_assert(
      std::is_same_v<decltype(rule), OrRule<EatRule<int>, TokenRule>>);
  SUCCEED();
}

// ------------------------------------------------------------------------
// matching

TEST(OrRule, MatchesTheFirstAlternative) {
  OrRule rule{TokenRule(K_VAL), EatRule<int>(7)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<Token>(*result).text(), "val");
  EXPECT_EQ(std::get<Token>(*result).token_type(), K_VAL);
}

TEST(OrRule, MatchesTheSecondAlternativeWhenTheFirstDoesNot) {
  OrRule rule{TokenRule(D_SEMICOLON), TokenRule(K_VAL)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(std::get<0>(*result).text(), "val");
}

TEST(OrRule, ReportsWhichAlternativeMatched) {
  OrRule rule{FailRule<int>{}, TokenRule(K_VAL)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  using R = decltype(rule)::return_type;
  static_assert(std::is_same_v<R, std::variant<int, Token>>);
  EXPECT_EQ(result->index(), 1u);
  EXPECT_TRUE(std::holds_alternative<Token>(*result));
}

TEST(OrRule, ConsumesTheTokensOfTheFirstAlternative) {
  OrRule rule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  rule.parse(ctx);
  ASSERT_TRUE(ctx->remaining());
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OrRule, ConsumesTheTokensOfTheSecondAlternativeOnly) {
  OrRule rule{TokenRule(D_SEMICOLON), TokenRule(K_VAL)};
  auto ctx = sample_context();
  rule.parse(ctx);
  ASSERT_TRUE(ctx->remaining());
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OrRule, MatchesAtTheCurrentPosition) {
  OrRule rule{TokenRule(K_VAL), TokenRule(D_ASSIGN)};
  auto ctx = sample_context();
  ctx->pop();
  ctx->pop();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(*result).text(), "=");
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(OrRule, ASequenceOfTheWholeInput) {
  OrRule first{TokenRule(I_IDENTIFIER), TokenRule(K_VAL)};
  OrRule second{TokenRule(D_ASSIGN), TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  ASSERT_TRUE(first.parse(ctx).is_match_consumed());
  ASSERT_TRUE(second.parse(ctx).is_match_consumed());
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(OrRule, ReturnedTokensKeepTheirSourceLocation) {
  OrRule rule{TokenRule(L_INT), TokenRule(D_ASSIGN)};
  auto ctx = sample_context();
  ctx->pop();
  ctx->pop();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(*result).source_location().offset(), 6u);
  EXPECT_EQ(std::get<0>(*result).source_location().length(), 1u);
}

TEST(OrRule, TheFirstMatchingAlternativeWinsWhenBothWouldMatch) {
  OrRule rule{EatRule<int>(1), EatRule<std::string>("two")};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<int>(*result), 1);
  EXPECT_EQ(drain(ctx), 4u); // the second alternative did not eat anything
}

// ------------------------------------------------------------------- not
// matching

TEST(OrRule, NoMatchWhenNoAlternativeMatches) {
  OrRule rule{TokenRule(D_SEMICOLON), TokenRule(L_INT)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  EXPECT_TRUE(result.is_no_match());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL); // nothing consumed
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(OrRule, EmptyInputGivesNoMatch) {
  OrRule rule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  auto ctx = std::make_shared<ParserContext>(std::vector<Token>{});
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
}

TEST(OrRule, NoMatchWhenBothAlternativesFailWithDifferentTypes) {
  OrRule rule{FailRule<int>{}, FailRule<std::string>{}};
  EXPECT_TRUE(rule.parse(sample_context()).is_no_match());
}

TEST(OrRule, ASecondAlternativeThatWouldMatchLaterInTheInputIsNotUsed) {
  // The alternatives are tried at the current position only.
  OrRule rule{TokenRule(I_IDENTIFIER), TokenRule(D_ASSIGN)};
  auto ctx = sample_context(); // starts with K_VAL
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

// ---------------------------------------------------- order of evaluation
// (laziness)

TEST(OrRule, RunsTheFirstAlternativeBeforeTheSecond) {
  auto log = std::make_shared<std::vector<std::string>>();
  OrRule rule{LoggedRule(FailRule<int>{}, "first", log),
              LoggedRule(FailRule<int>{}, "second", log)};
  rule.parse(sample_context());
  EXPECT_EQ(*log, (std::vector<std::string>{"first", "second"}));
}

TEST(OrRule, FirstAlternativeMatchingDoesNotRunTheSecondOne) {
  auto log = std::make_shared<std::vector<std::string>>();
  OrRule rule{LoggedRule(EatRule<int>(1), "first", log),
              LoggedRule(EatRule<int>(2), "second", log)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*log, (std::vector<std::string>{"first"}));
}

TEST(OrRule, SecondAlternativeIsRunWhenTheFirstDoesNotMatch) {
  auto log = std::make_shared<std::vector<std::string>>();
  OrRule rule{LoggedRule(FailRule<int>{}, "first", log),
              LoggedRule(EatRule<int>(2), "second", log)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*log, (std::vector<std::string>{"first", "second"}));
}

// ---------------------------------------------- consumed vs zero-length
// combinations

TEST(OrRule, ConsumedIfTheFirstAlternativeConsumed) {
  OrRule rule{EatRule<int>(1), ZeroRule<std::string>("z")};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OrRule, ConsumedIfTheSecondAlternativeConsumed) {
  OrRule rule{FailRule<int>{}, EatRule<std::string>("s")};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OrRule, ZeroLengthIfTheFirstAlternativeMatchedZeroLength) {
  OrRule rule{ZeroRule<int>(1), EatRule<std::string>("s")};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result.is_match_consumed());
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(drain(ctx), 5u); // nothing consumed
}

TEST(OrRule, ZeroLengthIfTheSecondAlternativeMatchedZeroLength) {
  OrRule rule{FailRule<int>{}, ZeroRule<std::string>("z")};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(OrRule, ZeroLengthIfBothAlternativesMatchedZeroLength) {
  OrRule rule{ZeroRule<int>(1), ZeroRule<std::string>("z")};
  auto result = rule.parse(sample_context());
  EXPECT_TRUE(result.is_match_zero());
}

TEST(OrRule, ZeroLengthMatchesStillCarryTheirValues) {
  OrRule rule{FailRule<int>{}, ZeroRule<std::string>("two")};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<std::string>(*result), "two");
}

// A first alternative that can match zero tokens always wins, so the second
// one is unreachable (ordered choice, like a PEG's `a? / b`).
TEST(OrRule, AFirstAlternativeThatMatchesZeroLengthShadowsTheSecond) {
  auto log = std::make_shared<std::vector<std::string>>();
  OrRule rule{LoggedRule(ZeroRule<int>(1), "first", log),
              LoggedRule(EatRule<std::string>("s"), "second", log)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<int>(*result), 1);
  EXPECT_EQ(*log, (std::vector<std::string>{"first"}));
  EXPECT_EQ(drain(ctx), 5u);
}

// --------------------------------------------------------------------- values

TEST(OrRule, ValuesOfDifferentTypesLandInTheRightAlternative) {
  OrRule first{EatRule<int>(7), EatRule<std::string>("seven")};
  OrRule second{FailRule<int>{}, EatRule<std::string>("seven")};
  auto a = first.parse(sample_context());
  auto b = second.parse(sample_context());
  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*a)>,
                               std::variant<int, std::string>>);
  EXPECT_EQ(std::get<int>(*a), 7);
  EXPECT_EQ(std::get<std::string>(*b), "seven");
}

TEST(OrRule, MoveOnlyValuesAreMovedNotCopied) {
  OrRule rule{FailRule<int>{}, UniqueRule(2)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(std::holds_alternative<std::unique_ptr<int>>(*result));
  EXPECT_EQ(*std::get<std::unique_ptr<int>>(*result), 2);
}

TEST(OrRule, AMoveOnlyFirstAlternativeIsMoved) {
  OrRule rule{UniqueRule(1), EatRule<int>(2)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(*std::get<0>(*result), 1);
}

TEST(OrRule, AVariantValuedSubRuleIsMergedIntoTheResult) {
  using V = std::variant<int, std::string>;
  OrRule first{EatRule<V>(V(std::string("a"))), EatRule<double>(2.5)};
  auto a = first.parse(sample_context());
  ASSERT_TRUE(a.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*a)>,
                               std::variant<int, std::string, double>>);
  EXPECT_EQ(a->index(), 1u);
  EXPECT_EQ(std::get<std::string>(*a), "a");

  OrRule second{FailRule<V>{}, EatRule<double>(2.5)};
  auto b = second.parse(sample_context());
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(b->index(), 2u);
  EXPECT_EQ(std::get<double>(*b), 2.5);
}

TEST(OrRule, AVariantValuedSecondSubRuleIsMergedIntoTheResult) {
  using V = std::variant<std::string, double>;
  OrRule rule{FailRule<int>{}, EatRule<V>(V(2.5))};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::variant<int, std::string, double>>);
  EXPECT_EQ(result->index(), 2u);
  EXPECT_EQ(std::get<double>(*result), 2.5);
}

TEST(OrRule, TwoAlternativesOfTheSameTypeGiveASingleAlternative) {
  OrRule rule{FailRule<int>{}, EatRule<int>(4)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::variant<int>>);
  EXPECT_EQ(std::get<int>(*result), 4);
}

// ------------------------------------------------------------------------
// nesting

TEST(OrRule, ThreeRulesOredOnTheLeftGiveAFlatVariant) {
  OrRule rule{OrRule{TokenRule(D_SEMICOLON), EatRule<int>(1)},
              EatRule<std::string>("s")};
  static_assert(std::is_same_v<decltype(rule)::return_type,
                               std::variant<Token, int, std::string>>);
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 1u);
  EXPECT_EQ(std::get<int>(*result), 1);
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OrRule, ThreeRulesOredOnTheRightGiveAFlatVariant) {
  OrRule rule{TokenRule(D_SEMICOLON),
              OrRule{FailRule<int>{}, EatRule<std::string>("s")}};
  static_assert(std::is_same_v<decltype(rule)::return_type,
                               std::variant<Token, int, std::string>>);
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 2u);
  EXPECT_EQ(std::get<std::string>(*result), "s");
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(OrRule, EachAlternativeOfAThreeWayChoiceCanBeReached) {
  auto make = [](TokenType a, TokenType b, TokenType c) {
    return OrRule{OrRule{TokenRule(a), TokenRule(b)}, TokenRule(c)};
  };
  // "val" is the first token: it matches whichever position K_VAL is given.
  EXPECT_TRUE(make(K_VAL, D_ASSIGN, L_INT).parse(sample_context()).has_value());
  EXPECT_TRUE(make(D_ASSIGN, K_VAL, L_INT).parse(sample_context()).has_value());
  EXPECT_TRUE(make(D_ASSIGN, L_INT, K_VAL).parse(sample_context()).has_value());
  EXPECT_TRUE(
      make(D_ASSIGN, L_INT, D_SEMICOLON).parse(sample_context()).is_no_match());
}

TEST(OrRule, FourRulesFromTwoOrs) {
  OrRule left{EatRule<int>(1), EatRule<std::string>("a")};
  OrRule right{EatRule<double>(2.5), EatRule<X>(X{})};
  OrRule rule{left, right};
  static_assert(std::is_same_v<decltype(rule)::return_type,
                               std::variant<int, std::string, double, X>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 0u);
}

TEST(OrRule, FourRulesFromTwoOrsReachingTheLastAlternative) {
  OrRule left{FailRule<int>{}, FailRule<std::string>{}};
  OrRule right{FailRule<double>{}, EatRule<X>(X{})};
  OrRule rule{left, right};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 3u);
  EXPECT_TRUE(std::holds_alternative<X>(*result));
}

TEST(OrRule, ANestedOrWithoutAMatchFallsThroughToTheOuterSecondRule) {
  OrRule rule{OrRule{TokenRule(D_SEMICOLON), TokenRule(L_INT)},
              EatRule<int>(9)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 1u);
  EXPECT_EQ(std::get<int>(*result), 9);
}

TEST(OrRule, NoMatchWhenEveryRuleOfANestedOrFails) {
  OrRule rule{OrRule{TokenRule(D_SEMICOLON), TokenRule(L_INT)},
              TokenRule(D_ASSIGN)};
  auto ctx = sample_context();
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

// ------------------------------------------------------ with the ChainRule

TEST(OrRule, ChainsAsAlternativesAreKeptAsTuples) {
  OrRule rule{ChainRule{TokenRule(D_SEMICOLON), TokenRule(K_VAL)},
              ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  static_assert(
      std::is_same_v<decltype(rule)::return_type,
                     std::variant<std::tuple<Token, Token>>>); // same type
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(std::get<0>(*result)).text(), "val");
  EXPECT_EQ(std::get<1>(std::get<0>(*result)).text(), "x");
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(OrRule, ChainAndPlainRuleGiveATupleAlternativeAndAPlainOne) {
  OrRule rule{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
              EatRule<int>(3)};
  static_assert(std::is_same_v<decltype(rule)::return_type,
                               std::variant<std::tuple<Token, Token>, int>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<1>(std::get<0>(*result)).text(), "x");
}

TEST(OrRule, SecondAlternativeIsUsedWhenTheFirstChainDoesNotStartMatching) {
  OrRule rule{ChainRule{TokenRule(D_SEMICOLON), TokenRule(I_IDENTIFIER)},
              EatRule<int>(3)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 1u);
  EXPECT_EQ(std::get<int>(*result), 3);
}

TEST(OrRule, AChainFailingAfterConsumingTokensStillThrows) {
  // "val" is consumed by the chain, then ';' is expected but 'x' is found.
  // The second alternative is not tried: the chain has committed.
  auto log = std::make_shared<std::vector<std::string>>();
  OrRule rule{ChainRule{TokenRule(K_VAL), TokenRule(D_SEMICOLON)},
              LoggedRule(EatRule<int>(3), "second", log)};
  EXPECT_THROW(rule.parse(sample_context()), std::runtime_error);
  EXPECT_TRUE(log->empty());
}

TEST(OrRule, AnOrInsideAChainIsOneElementOfTheTuple) {
  ChainRule chain{TokenRule(K_VAL),
                  OrRule{TokenRule(D_SEMICOLON), TokenRule(I_IDENTIFIER)}};
  static_assert(std::is_same_v<decltype(chain)::return_type,
                               std::tuple<Token, std::variant<Token>>>);
  auto ctx = sample_context();
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(*result).text(), "val");
  EXPECT_EQ(std::get<0>(std::get<1>(*result)).text(), "x");
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(OrRule, AnOrInsideAChainThatFailsAfterConsumingTokensThrows) {
  ChainRule chain{TokenRule(K_VAL),
                  OrRule{TokenRule(D_SEMICOLON), TokenRule(L_INT)}};
  EXPECT_THROW(chain.parse(sample_context()), std::runtime_error);
}

TEST(OrRule, ChoosesBetweenTwoStatementsLikeAParser) {
  // statement := "val" ident  |  ident "=" int
  OrRule statement{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
                   ChainRule{TokenRule(I_IDENTIFIER), TokenRule(D_ASSIGN)}};
  auto ctx = sample_context();
  auto result = statement.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(std::get<0>(*result)).token_type(), K_VAL);
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);

  // The same rule on the input "x = ...".
  auto ctx2 = sample_context();
  ctx2->pop(); // skip "val"
  auto result2 = statement.parse(ctx2);
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(std::get<0>(std::get<0>(*result2)).token_type(), I_IDENTIFIER);
  EXPECT_EQ(ctx2->peek().token_type(), L_INT);
}

// -------------------------------------------------------------------------
// reuse

TEST(OrRule, OneRuleObjectCanBeUsedRepeatedlyAndOnDifferentContexts) {
  OrRule rule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)};
  auto first = sample_context();
  auto second = sample_context();
  EXPECT_TRUE(rule.parse(first).is_match_consumed());
  EXPECT_TRUE(rule.parse(second).is_match_consumed());
  EXPECT_TRUE(rule.parse(first).is_match_consumed()); // now matches "x"
  EXPECT_TRUE(rule.parse(first).is_no_match());       // now starts at '='
  EXPECT_EQ(first->peek().token_type(), D_ASSIGN);
  EXPECT_EQ(second->peek().token_type(), I_IDENTIFIER);
}

TEST(OrRule, OneRuleObjectCanBeUsedRepeatedlyAndOnDifferentContextsWithOr) {
  auto rule = TokenRule(K_VAL) | TokenRule(I_IDENTIFIER);
  auto first = sample_context();
  auto second = sample_context();
  EXPECT_TRUE(rule.parse(first).is_match_consumed());
  EXPECT_TRUE(rule.parse(second).is_match_consumed());
  EXPECT_TRUE(rule.parse(first).is_match_consumed()); // now matches "x"
  EXPECT_TRUE(rule.parse(first).is_no_match());       // now starts at '='
  EXPECT_EQ(first->peek().token_type(), D_ASSIGN);
  EXPECT_EQ(second->peek().token_type(), I_IDENTIFIER);
}