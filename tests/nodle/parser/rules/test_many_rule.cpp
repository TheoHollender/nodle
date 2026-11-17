#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
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
using nodle::parser::rules::ManyRule;
using nodle::parser::rules::OptionRule;
using nodle::parser::rules::OrRule;
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

// A context made of the given token types (the source text is irrelevant).
ParserContextPtr context_of(std::initializer_list<TokenType> types) {
  auto file = SourceFile::from_string("t.nodle", std::string(128, ' '));
  std::vector<Token> tokens;
  std::uint32_t offset = 0;
  for (TokenType type : types) {
    tokens.emplace_back(type, SourceLocation(file, offset++, 1));
  }
  return std::make_shared<ParserContext>(std::move(tokens));
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
  ParseResult<T> parse(ParserContextPtr) const {
    return ParseResult<T>::NoMatch();
  }
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

// Stateful: the first `left` calls consume a token and yield their index, the
// next ones match without consuming anything (and still yield a value).
struct ThenZeroRule : BasicRuleTrait<ThenZeroRule, int> {
  // parse() is const, the state of this rule is mutable.
  mutable int left;
  mutable int calls = 0;
  explicit ThenZeroRule(int n) : left(n) {}
  ParseResult<int> parse(ParserContextPtr ctx) const {
    int index = calls++;
    if (left > 0 && ctx->remaining()) {
      --left;
      ctx->pop();
      return ParseResult<int>::Match(index, true);
    }
    return ParseResult<int>::Match(-1, false);
  }
};

// Matches one token of type TYPE.
template <TokenType TYPE> ParseResult<Token> fn_token(ParserContextPtr ctx) {
  if (ctx->remaining() && ctx->peek().token_type() == TYPE) {
    Token token = ctx->peek();
    ctx->pop();
    return ParseResult<Token>::Match(std::move(token), true);
  }
  return ParseResult<Token>::NoMatch();
}

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

// NOTE: write `ManyRule rule{a};` with braces (see the ChainRule tests), or
// use `a.many()`. `ManyRule rule{ManyRule{a}};` would deduce the copy
// constructor, so nested ManyRules below are written with `.many().many()`.

// ------------------------------------------------------------------ the rule's
// type

using ManyTokens = ManyRule<TokenRule>;

static_assert(BasicRule<ManyTokens>);
static_assert(std::is_same_v<ManyTokens::return_type, std::vector<Token>>);
static_assert(
    std::is_same_v<ManyRule<EatRule<int>>::return_type, std::vector<int>>);
static_assert(
    std::is_same_v<ManyRule<ChainRule<TokenRule, TokenRule>>::return_type,
                   std::vector<std::tuple<Token, Token>>>);
static_assert(std::is_same_v<
              ManyRule<OrRule<EatRule<int>, EatRule<std::string>>>::return_type,
              std::vector<std::variant<int, std::string>>>);
static_assert(std::is_same_v<ManyRule<OptionRule<TokenRule>>::return_type,
                             std::vector<std::optional<Token>>>);
static_assert(std::is_same_v<ManyRule<ManyRule<TokenRule>>::return_type,
                             std::vector<std::vector<Token>>>);
static_assert(std::is_same_v<ManyRule<UniqueRule>::return_type,
                             std::vector<std::unique_ptr<int>>>);
static_assert(
    std::is_same_v<ManyRule<CallRule<Token, &fn_token<K_VAL>>>::return_type,
                   std::vector<Token>>);

// A ManyRule is the element of a chain, an or, an option...
static_assert(std::is_same_v<ChainRule<TokenRule, ManyTokens>::return_type,
                             std::tuple<Token, std::vector<Token>>>);
static_assert(
    std::is_same_v<ChainRule<ManyTokens, ManyTokens>::return_type,
                   std::tuple<std::vector<Token>, std::vector<Token>>>);
static_assert(std::is_same_v<OrRule<ManyTokens, EatRule<int>>::return_type,
                             std::variant<std::vector<Token>, int>>);
static_assert(std::is_same_v<OptionRule<ManyTokens>::return_type,
                             std::optional<std::vector<Token>>>);

TEST(ManyRule, DeducesItsTemplateArgumentFromTheSubRule) {
  ManyRule rule{TokenRule(K_VAL)};
  static_assert(std::is_same_v<decltype(rule), ManyTokens>);
  SUCCEED();
}

// ----------------------------------------------------------- the many() method

TEST(ManyMethod, ReturnsAManyRuleOfTheRule) {
  auto rule = TokenRule(K_VAL).many();
  static_assert(std::is_same_v<decltype(rule), ManyTokens>);
  SUCCEED();
}

TEST(ManyMethod, WorksOnACustomRule) {
  auto rule = EatRule<int>(3).many();
  static_assert(std::is_same_v<decltype(rule), ManyRule<EatRule<int>>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, (std::vector<int>{3, 3, 3, 3, 3}));
}

TEST(ManyMethod, WorksOnAnObjectAndOnATemporary) {
  TokenRule named(K_VAL);
  auto from_object = named.many();
  auto from_temporary = TokenRule(K_VAL).many();
  EXPECT_EQ(from_object.parse(sample_context())->size(), 1u);
  EXPECT_EQ(from_temporary.parse(sample_context())->size(), 1u);
}

TEST(ManyMethod, WorksOnAConstRule) {
  const TokenRule rule(K_VAL);
  auto many = rule.many();
  static_assert(std::is_same_v<decltype(many), ManyTokens>);
  EXPECT_EQ(many.parse(sample_context())->size(), 1u);
}

TEST(ManyRule, CanBeParsedThroughAConstReference) {
  const ManyRule rule{TokenRule(K_VAL)};
  auto ctx = context_of({K_VAL, K_VAL, L_INT});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);
}

TEST(ManyMethod, WorksOnAChain) {
  auto rule = ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}.many();
  static_assert(std::is_same_v<decltype(rule),
                               ManyRule<ChainRule<TokenRule, TokenRule>>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1u);
}

TEST(ManyMethod, CanBeCalledOnAManyRule) {
  auto rule = TokenRule(K_VAL).many().many();
  static_assert(std::is_same_v<decltype(rule), ManyRule<ManyTokens>>);
  SUCCEED();
}

TEST(ManyMethod, TheRuleReturnedIsACopyOfTheOriginal) {
  ThenZeroRule original(2);
  auto many = original.many();
  auto result = many.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);
  EXPECT_EQ(original.left, 2); // the original did not run
  EXPECT_EQ(original.calls, 0);
}

TEST(ManyMethod, GivesTheSameResultAsTheConstructor) {
  auto from_method = TokenRule(K_VAL).many();
  ManyRule from_constructor{TokenRule(K_VAL)};
  auto a = from_method.parse(sample_context());
  auto b = from_constructor.parse(sample_context());
  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(a->size(), b->size());
  EXPECT_EQ(a.status(), b.status());
}

// ------------------------------------------------------------------------
// matching

TEST(ManyRule, MatchesASingleToken) {
  ManyRule rule{TokenRule(K_VAL)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].text(), "val");
  EXPECT_EQ((*result)[0].token_type(), K_VAL);
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
}

TEST(ManyRule, MatchesSeveralConsecutiveTokens) {
  ManyRule rule{TokenRule(K_VAL)};
  auto ctx = context_of({K_VAL, K_VAL, K_VAL, L_INT});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  ASSERT_EQ(result->size(), 3u);
  for (const Token &token : *result) {
    EXPECT_EQ(token.token_type(), K_VAL);
  }
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
  EXPECT_EQ(drain(ctx), 1u);
}

TEST(ManyRule, KeepsTheValuesInTheOrderOfTheInput) {
  ManyRule rule{OrRule{TokenRule(K_VAL),
                       OrRule{TokenRule(I_IDENTIFIER), TokenRule(D_ASSIGN)}}};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 3u);
  EXPECT_EQ((*result)[0].text(), "val");
  EXPECT_EQ((*result)[1].text(), "x");
  EXPECT_EQ((*result)[2].text(), "=");
}

TEST(ManyRule, StopsAtTheFirstTokenThatDoesNotMatch) {
  // K_VAL K_VAL <L_INT> K_VAL: the last K_VAL is not reached.
  ManyRule rule{TokenRule(K_VAL)};
  auto ctx = context_of({K_VAL, K_VAL, L_INT, K_VAL});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
  EXPECT_EQ(drain(ctx), 2u);
}

TEST(ManyRule, ConsumesExactlyTheMatchedTokens) {
  ManyRule rule{EatRule<int>(1)};
  auto ctx = sample_context();
  rule.parse(ctx);
  EXPECT_FALSE(ctx->remaining());
}

TEST(ManyRule, MatchesTheWholeInputWhenEverythingMatches) {
  ManyRule rule{EatRule<int>(1)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 5u);
}

TEST(ManyRule, MatchesAtTheCurrentPosition) {
  ManyRule rule{TokenRule(D_ASSIGN)};
  auto ctx = sample_context();
  ctx->pop();
  ctx->pop();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].text(), "=");
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(ManyRule, ReturnedTokensKeepTheirSourceLocation) {
  ManyRule rule{TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  ctx->pop();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].source_location().offset(), 4u);
  EXPECT_EQ((*result)[0].source_location().length(), 1u);
}

TEST(ManyRule, ASequenceOfManyRulesWalksThroughTheInput) {
  ManyRule vals{TokenRule(K_VAL)};
  ManyRule idents{TokenRule(I_IDENTIFIER)};
  ManyRule assigns{TokenRule(D_ASSIGN)};
  auto ctx = sample_context();
  EXPECT_TRUE(vals.parse(ctx).is_match_consumed());
  EXPECT_TRUE(idents.parse(ctx).is_match_consumed());
  EXPECT_TRUE(assigns.parse(ctx).is_match_consumed());
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

// ----------------------------------------------- zero matches

TEST(ManyRule, StillMatchesWhenTheSubRuleNeverMatches) {
  ManyRule rule{TokenRule(D_SEMICOLON)};
  auto result = rule.parse(sample_context());
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(result.is_no_match());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(ManyRule, NoMatchesIsAZeroLengthMatch) {
  ManyRule rule{TokenRule(D_SEMICOLON)};
  auto result = rule.parse(sample_context());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_FALSE(result.is_match_consumed());
}

TEST(ManyRule, ConsumesNothingWhenTheSubRuleNeverMatches) {
  ManyRule rule{TokenRule(D_SEMICOLON)};
  auto ctx = sample_context();
  rule.parse(ctx);
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(ManyRule, EmptyInputGivesAnEmptyVector) {
  ManyRule rule{TokenRule(K_VAL)};
  auto ctx = std::make_shared<ParserContext>(std::vector<Token>{});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_TRUE(result->empty());
}

TEST(ManyRule, NeverReturnsNoMatch) {
  ManyRule failing{FailRule<int>{}};
  ManyRule matching{EatRule<int>(1)};
  auto empty = std::make_shared<ParserContext>(std::vector<Token>{});
  EXPECT_FALSE(failing.parse(sample_context()).is_no_match());
  EXPECT_FALSE(matching.parse(sample_context()).is_no_match());
  EXPECT_FALSE(failing.parse(empty).is_no_match());
  EXPECT_FALSE(matching.parse(empty).is_no_match());
}

TEST(ManyRule, ASecondCallAfterEverythingWasConsumedIsEmpty) {
  ManyRule rule{TokenRule(K_VAL)};
  auto ctx = context_of({K_VAL, K_VAL, L_INT});
  auto first = rule.parse(ctx);
  auto second = rule.parse(ctx);
  EXPECT_TRUE(first.is_match_consumed());
  EXPECT_EQ(first->size(), 2u);
  EXPECT_TRUE(second.is_match_zero());
  EXPECT_TRUE(second->empty());
}

// ---------------------------------------------- consumed vs zero-length

TEST(ManyRule, ConsumedAsSoonAsOneIterationMatched) {
  ManyRule rule{TokenRule(K_VAL)};
  auto result = rule.parse(context_of({K_VAL, L_INT}));
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_FALSE(result.is_match_zero());
}

TEST(ManyRule, ConsumedWhenEveryIterationMatched) {
  ManyRule rule{EatRule<int>(1)};
  EXPECT_TRUE(rule.parse(sample_context()).is_match_consumed());
}

// ------------------------------------- a sub rule that matches zero tokens

TEST(ManyRule, StopsAtAZeroLengthMatchOfTheSubRule) {
  // A zero-length sub rule would match forever: it must end the loop.
  ManyRule rule{ZeroRule<int>(1)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_TRUE(result->empty());
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(ManyRule, RunsAZeroLengthSubRuleOnlyOnce) {
  auto log = std::make_shared<std::vector<std::string>>();
  ManyRule rule{LoggedRule(ZeroRule<int>(1), "sub", log)};
  rule.parse(sample_context());
  EXPECT_EQ(*log, (std::vector<std::string>{"sub"}));
}

TEST(ManyRule, TheValueOfTheStoppingZeroLengthMatchIsDropped) {
  // Two consuming matches (0 and 1), then a zero-length one (-1).
  ManyRule rule{ThenZeroRule(2)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(*result, (std::vector<int>{0, 1}));
  EXPECT_EQ(drain(ctx), 3u);
}

TEST(ManyRule, NothingAfterTheFirstZeroLengthMatchIsLookedAt) {
  // One consuming match, then a zero-length one: exactly two calls, even
  // though plenty of tokens are left.
  auto log = std::make_shared<std::vector<std::string>>();
  ManyRule rule{LoggedRule(ThenZeroRule(1), "sub", log)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1u);
  EXPECT_EQ(log->size(), 2u);
  EXPECT_EQ(drain(ctx), 4u);
}

TEST(ManyRule, AZeroLengthMatchAfterTheLastTokenEndsTheLoop) {
  // The input is shorter than the number of consuming matches wanted.
  ManyRule rule{ThenZeroRule(10)};
  auto ctx = context_of({K_VAL, K_VAL});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, (std::vector<int>{0, 1}));
  EXPECT_FALSE(ctx->remaining());
}

// ------------------------------------------------------ running the sub rule

TEST(ManyRule, RunsTheSubRuleOnceMoreThanItMatched) {
  auto log = std::make_shared<std::vector<std::string>>();
  ManyRule rule{LoggedRule(TokenRule(K_VAL), "sub", log)};
  rule.parse(context_of({K_VAL, K_VAL, K_VAL, L_INT}));
  EXPECT_EQ(log->size(), 4u); // three matches and the one that failed
}

TEST(ManyRule, RunsTheSubRuleOnceWhenItDoesNotMatch) {
  auto log = std::make_shared<std::vector<std::string>>();
  ManyRule rule{LoggedRule(FailRule<int>{}, "sub", log)};
  rule.parse(sample_context());
  EXPECT_EQ(*log, (std::vector<std::string>{"sub"}));
}

TEST(ManyRule, RunsTheSubRuleOnceMoreWhenTheInputIsExhausted) {
  auto log = std::make_shared<std::vector<std::string>>();
  ManyRule rule{LoggedRule(EatRule<int>(1), "sub", log)};
  rule.parse(sample_context());
  EXPECT_EQ(log->size(), 6u); // five tokens and the failure at the end
}

TEST(ManyRule, RunsTheSubRuleOnTheContextItWasGiven) {
  ManyRule rule{EatRule<int>(1)};
  auto ctx = sample_context();
  auto other = sample_context();
  rule.parse(ctx);
  EXPECT_FALSE(ctx->remaining());
  EXPECT_EQ(drain(other), 5u); // untouched
}

// --------------------------------------------------------------------- values

TEST(ManyRule, ValuesOfTheSubRuleAreCollectedInAVector) {
  ManyRule rule{EatRule<std::string>("s")};
  auto result = rule.parse(context_of({K_VAL, K_VAL, K_VAL}));
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::vector<std::string>>);
  EXPECT_EQ(*result, (std::vector<std::string>{"s", "s", "s"}));
}

TEST(ManyRule, MoveOnlyValuesAreMovedNotCopied) {
  ManyRule rule{UniqueRule(4)};
  auto result = rule.parse(context_of({K_VAL, K_VAL, K_VAL}));
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 3u);
  for (const auto &value : *result) {
    ASSERT_TRUE(value != nullptr);
    EXPECT_EQ(*value, 4);
  }
}

TEST(ManyRule, MoveOnlyValuesCanBeTakenOutOfTheResult) {
  ManyRule rule{UniqueRule(8)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  auto values = *std::move(result);
  ASSERT_EQ(values.size(), 5u);
  EXPECT_EQ(*values.front(), 8);
}

TEST(ManyRule, ADefaultValueIsNotConfusedWithNoMatch) {
  ManyRule rule{EatRule<int>(0)};
  auto result = rule.parse(context_of({K_VAL, K_VAL}));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, (std::vector<int>{0, 0}));
}

// ------------------------------------------------------------------------
// with the ChainRule

TEST(ManyRule, ManyChainsGiveAVectorOfTuples) {
  ManyRule rule{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  auto ctx = context_of({K_VAL, I_IDENTIFIER, K_VAL, I_IDENTIFIER, D_ASSIGN});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::vector<std::tuple<Token, Token>>>);
  ASSERT_EQ(result->size(), 2u);
  EXPECT_EQ(std::get<0>((*result)[0]).token_type(), K_VAL);
  EXPECT_EQ(std::get<1>((*result)[1]).token_type(), I_IDENTIFIER);
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(ManyRule, ManyChainsStopWhenAChainDoesNotStartMatching) {
  ManyRule rule{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  auto ctx = context_of({K_VAL, I_IDENTIFIER, D_ASSIGN, K_VAL, I_IDENTIFIER});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1u);
  EXPECT_EQ(drain(ctx), 3u);
}

TEST(ManyRule, ManyChainsThrowWhenAChainFailsAfterConsumingTokens) {
  // The second chain consumes "val" and then does not find an identifier.
  ManyRule rule{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  auto ctx = context_of({K_VAL, I_IDENTIFIER, K_VAL, L_INT});
  EXPECT_THROW(rule.parse(ctx), std::runtime_error);
}

TEST(ManyRule, ManyChainsThrowWhenTheInputEndsInTheMiddleOfAChain) {
  ManyRule rule{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  auto ctx = context_of({K_VAL, I_IDENTIFIER, K_VAL});
  EXPECT_THROW(rule.parse(ctx), std::runtime_error);
}

TEST(ManyRule, AManyRuleAsTheSecondRuleOfAChain) {
  ChainRule chain{TokenRule(K_VAL), TokenRule(I_IDENTIFIER).many()};
  auto ctx =
      context_of({K_VAL, I_IDENTIFIER, I_IDENTIFIER, I_IDENTIFIER, D_ASSIGN});
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::tuple<Token, std::vector<Token>>>);
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_EQ(std::get<0>(*result).token_type(), K_VAL);
  EXPECT_EQ(std::get<1>(*result).size(), 3u);
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(ManyRule, AnEmptyManyRuleInAChainDoesNotMakeTheChainFail) {
  ChainRule chain{TokenRule(K_VAL), TokenRule(D_SEMICOLON).many()};
  auto ctx = sample_context();
  auto result = chain.parse(ctx); // no throw: the many rule always matches
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_consumed());
  EXPECT_TRUE(std::get<1>(*result).empty());
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
}

TEST(ManyRule, AManyRuleAsTheFirstRuleOfAChain) {
  ChainRule chain{TokenRule(K_VAL).many(), TokenRule(I_IDENTIFIER)};
  auto ctx = context_of({K_VAL, K_VAL, I_IDENTIFIER, D_ASSIGN});
  auto result = chain.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<0>(*result).size(), 2u);
  EXPECT_EQ(std::get<1>(*result).token_type(), I_IDENTIFIER);
  EXPECT_EQ(ctx->peek().token_type(), D_ASSIGN);
}

TEST(ManyRule, AChainAfterAnEmptyManyRuleIsNoMatchIfItsSecondRuleFails) {
  // The many rule matched zero tokens: there is nothing to commit to.
  ChainRule chain{TokenRule(D_SEMICOLON).many(), TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  EXPECT_TRUE(chain.parse(ctx).is_no_match());
  EXPECT_EQ(drain(ctx), 5u);
}

TEST(ManyRule, ASequenceOfStatements) {
  // program := (val IDENT = INT ;)*
  ManyRule program{
      ChainRule{ChainRule{ChainRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
                          TokenRule(D_ASSIGN)},
                ChainRule{TokenRule(L_INT), TokenRule(D_SEMICOLON)}}};
  auto ctx = context_of({K_VAL, I_IDENTIFIER, D_ASSIGN, L_INT, D_SEMICOLON,
                         K_VAL, I_IDENTIFIER, D_ASSIGN, L_INT, D_SEMICOLON});
  auto result = program.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<
                std::remove_cvref_t<decltype(*result)>,
                std::vector<std::tuple<Token, Token, Token, Token, Token>>>);
  EXPECT_EQ(result->size(), 2u);
  EXPECT_FALSE(ctx->remaining());
}

// ------------------------------------------------------------ with the OrRule

TEST(ManyRule, ManyAlternativesGiveAVectorOfVariants) {
  ManyRule rule{OrRule{TokenRule(K_VAL),
                       ChainRule{TokenRule(D_LPAREN), TokenRule(D_RPAREN)}}};
  auto ctx = context_of({K_VAL, D_LPAREN, D_RPAREN, K_VAL, L_INT});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<
                std::remove_cvref_t<decltype(*result)>,
                std::vector<std::variant<Token, std::tuple<Token, Token>>>>);
  ASSERT_EQ(result->size(), 3u);
  EXPECT_EQ((*result)[0].index(), 0u);
  EXPECT_EQ((*result)[1].index(), 1u);
  EXPECT_EQ((*result)[2].index(), 0u);
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(ManyRule, ManyAlternativesStopWhenNoAlternativeMatches) {
  ManyRule rule{OrRule{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  auto ctx = context_of({K_VAL, I_IDENTIFIER, K_VAL, L_INT});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 3u);
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(ManyRule, AManyRuleAsAnAlternativeOfAnOrIsReachedAndAlwaysMatches) {
  OrRule rule{TokenRule(D_SEMICOLON), TokenRule(K_VAL).many()};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 1u);
  EXPECT_EQ(std::get<1>(*result).size(), 1u);
}

TEST(ManyRule, AManyRuleAsTheFirstAlternativeOfAnOrShadowsTheSecondOne) {
  // A many rule always matches, so the alternatives after it are unreachable.
  auto log = std::make_shared<std::vector<std::string>>();
  OrRule rule{TokenRule(D_SEMICOLON).many(),
              LoggedRule(TokenRule(K_VAL), "second", log)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->index(), 0u);
  EXPECT_TRUE(std::get<0>(*result).empty());
  EXPECT_TRUE(log->empty());
}

// ----------------------------------------------------- with the OptionRule

TEST(ManyRule, ManyOptionsStopWhenTheOptionIsAbsent) {
  // An absent option is a zero-length match, which ends the loop.
  ManyRule rule{OptionRule{TokenRule(K_VAL)}};
  auto ctx = context_of({K_VAL, K_VAL, L_INT, K_VAL});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::vector<std::optional<Token>>>);
  ASSERT_EQ(result->size(), 2u);
  EXPECT_TRUE((*result)[0].has_value());
  EXPECT_TRUE((*result)[1].has_value());
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(ManyRule, ManyOptionsOnNoMatchGiveAnEmptyVector) {
  ManyRule rule{OptionRule{TokenRule(D_SEMICOLON)}};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_TRUE(result->empty());
}

TEST(ManyRule, AnOptionalManyRuleIsAnOptionalVector) {
  OptionRule rule{TokenRule(K_VAL).many()};
  static_assert(std::is_same_v<decltype(rule)::return_type,
                               std::optional<std::vector<Token>>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ((*result)->size(), 1u);
}

// ----------------------------------------------------------- with CallRule

TEST(ManyRule, ManyCalls) {
  ManyRule rule{CallRule<Token, &fn_token<K_VAL>>{}};
  auto ctx = context_of({K_VAL, K_VAL, I_IDENTIFIER});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 2u);
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
}

// ------------------------------------------------------------------ nesting

TEST(ManyRule, AManyRuleOfAManyRuleHoldsASingleInnerVector) {
  // The inner rule eats every K_VAL, then matches zero tokens (which ends the
  // outer loop): the outer vector has exactly one element.
  auto rule = TokenRule(K_VAL).many().many();
  auto ctx = context_of({K_VAL, K_VAL, L_INT});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  static_assert(std::is_same_v<std::remove_cvref_t<decltype(*result)>,
                               std::vector<std::vector<Token>>>);
  EXPECT_TRUE(result.is_match_consumed());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].size(), 2u);
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(ManyRule, AManyRuleOfAManyRuleThatDoesNotMatchIsEmpty) {
  auto rule = TokenRule(D_SEMICOLON).many().many();
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_TRUE(result->empty());
}

TEST(ManyRule, ManyGroupsSeparatedBySomethingTheInnerRuleCannotSkip) {
  // (IDENT*  ',')*  -> groups of identifiers, each followed by a comma
  auto group = ChainRule{TokenRule(I_IDENTIFIER).many(), TokenRule(D_COMMA)};
  auto rule = group.many();
  auto ctx = context_of({I_IDENTIFIER, I_IDENTIFIER, D_COMMA, D_COMMA,
                         I_IDENTIFIER, D_COMMA, L_INT});
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 3u);
  EXPECT_EQ(std::get<0>((*result)[0]).size(), 2u);
  EXPECT_EQ(std::get<0>((*result)[1]).size(), 0u);
  EXPECT_EQ(std::get<0>((*result)[2]).size(), 1u);
  EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

// ------------------------------------------------------------------- reuse

TEST(ManyRule, OneRuleObjectCanBeUsedRepeatedlyAndOnDifferentContexts) {
  ManyRule rule{TokenRule(K_VAL)};
  auto first = context_of({K_VAL, K_VAL, L_INT});
  auto second = context_of({K_VAL, L_INT});
  EXPECT_EQ(rule.parse(first)->size(), 2u);
  EXPECT_EQ(rule.parse(second)->size(), 1u);
  EXPECT_TRUE(rule.parse(first).is_match_zero()); // `first` now starts at L_INT
  EXPECT_EQ(first->peek().token_type(), L_INT);
  EXPECT_EQ(second->peek().token_type(), L_INT);
}

TEST(ManyRule, AStatefulSubRuleKeepsItsStateBetweenParses) {
  // The sub rule inside the ManyRule is stateful: it consumes at most 3 tokens
  // over its whole life.
  ManyRule rule{ThenZeroRule(3)};
  auto ctx = sample_context();
  EXPECT_EQ(*rule.parse(ctx), (std::vector<int>{0, 1, 2}));
  auto second = rule.parse(ctx); // the sub rule is exhausted
  EXPECT_TRUE(second.is_match_zero());
  EXPECT_TRUE(second->empty());
  EXPECT_EQ(drain(ctx), 2u);
}

TEST(ManyRule, ACopyOfTheRuleStartsFromTheStateOfTheOriginal) {
  ManyRule original{ThenZeroRule(2)};
  ManyRule copy = original;
  auto a = original.parse(sample_context());
  auto b = copy.parse(sample_context());
  EXPECT_EQ(*a, (std::vector<int>{0, 1}));
  EXPECT_EQ(*b, (std::vector<int>{0, 1}));
}