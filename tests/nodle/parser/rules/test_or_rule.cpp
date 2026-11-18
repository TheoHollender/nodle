#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "nodle/parser/rules/or.hpp"

#include "nodle/parser/graph/graph.hpp"
using namespace nodle::parser;

using nodle::parser::rules::or_util::unpack_or;
using nodle::parser::rules::or_util::unpack_or_t;
using nodle::parser::rules::or_util::detail::is_std_variant_v;
using nodle::parser::rules::or_util::detail::singleton_to_type;

using std::string;
using std::tuple;
using std::unique_ptr;
using std::variant;

// ------------------------------------------------------------------
// unpack_or_t

// two plain types -> a variant of both
static_assert(std::is_same_v<unpack_or_t<int, char>, variant<int, char>>);
static_assert(std::is_same_v<unpack_or_t<char, int>, variant<char, int>>);

// a variant on either side is flattened
static_assert(std::is_same_v<unpack_or_t<variant<int, char>, double>,
                             variant<int, char, double>>);
static_assert(std::is_same_v<unpack_or_t<int, variant<char, double>>,
                             variant<int, char, double>>);
static_assert(
    std::is_same_v<unpack_or_t<variant<int, char>, variant<double, string>>,
                   variant<int, char, double, string>>);

// duplicates are removed, the first occurrence decides the position
static_assert(
    std::is_same_v<unpack_or_t<variant<int, char>, variant<char, double>>,
                   variant<int, char, double>>);
static_assert(
    std::is_same_v<unpack_or_t<variant<int, char>, int>, variant<int, char>>);
static_assert(
    std::is_same_v<unpack_or_t<int, variant<int, char>>, variant<int, char>>);
static_assert(
    std::is_same_v<unpack_or_t<variant<int, char>, variant<char, int>>,
                   variant<int, char>>);
static_assert(
    std::is_same_v<unpack_or_t<variant<char, int>, variant<int, char>>,
                   variant<char, int>>);

// a result with a single alternative collapses to that alternative: variant<X>
// => X
static_assert(std::is_same_v<unpack_or_t<int, int>, int>);
static_assert(std::is_same_v<unpack_or_t<variant<int>, int>, int>);
static_assert(std::is_same_v<unpack_or_t<int, variant<int>>, int>);
static_assert(std::is_same_v<unpack_or_t<variant<int>, variant<int>>, int>);
static_assert(std::is_same_v<unpack_or_t<string, string>, string>);
static_assert(std::is_same_v<unpack_or_t<tuple<int, char>, tuple<int, char>>,
                             tuple<int, char>>);

// ... but a one-element variant next to another type is just flattened
static_assert(
    std::is_same_v<unpack_or_t<variant<int>, char>, variant<int, char>>);
static_assert(
    std::is_same_v<unpack_or_t<char, variant<int>>, variant<char, int>>);

// types are decayed (const / references are dropped)
static_assert(std::is_same_v<unpack_or_t<const int, int>, int>);
static_assert(std::is_same_v<unpack_or_t<int &, const int &>, int>);
static_assert(
    std::is_same_v<unpack_or_t<const int &, char &&>, variant<int, char>>);

// tuples (and every other non-variant type) are alternatives, not flattened
static_assert(std::is_same_v<unpack_or_t<tuple<int, char>, double>,
                             variant<tuple<int, char>, double>>);
static_assert(
    std::is_same_v<unpack_or_t<tuple<int>, int>, variant<tuple<int>, int>>);

// only ONE level of variant is flattened
static_assert(std::is_same_v<unpack_or_t<variant<variant<int, char>>, double>,
                             variant<variant<int, char>, double>>);

// the result never contains a duplicated alternative: it is always buildable
static_assert(
    std::is_same_v<unpack_or_t<unpack_or_t<int, char>, unpack_or_t<char, int>>,
                   variant<int, char>>);

// building up three alternatives, left or right nested, gives the same variant
static_assert(std::is_same_v<unpack_or_t<unpack_or_t<int, char>, double>,
                             unpack_or_t<int, unpack_or_t<char, double>>>);
static_assert(std::is_same_v<unpack_or_t<unpack_or_t<int, char>, int>,
                             unpack_or_t<int, unpack_or_t<char, int>>>);

// ------------------------------------------------------------------ helpers

static_assert(is_std_variant_v<variant<int>>);
static_assert(is_std_variant_v<variant<int, char> &>);
static_assert(is_std_variant_v<const variant<int, char> &>);
static_assert(!is_std_variant_v<int>);
static_assert(!is_std_variant_v<tuple<int, char>>);
static_assert(!is_std_variant_v<string>);

static_assert(std::is_same_v<singleton_to_type<variant<int>>::type, int>);
static_assert(std::is_same_v<singleton_to_type<variant<int, char>>::type,
                             variant<int, char>>);
static_assert(std::is_same_v<singleton_to_type<int>::type, int>);
static_assert(std::is_same_v<singleton_to_type<variant<>>::type, variant<>>);

// the function is usable in constant expressions
static_assert(unpack_or<variant<int, char>>(3).index() == 0);
static_assert(unpack_or<variant<int, char>>('c').index() == 1);
static_assert(std::get<1>(unpack_or<variant<int, char>>('c')) == 'c');

// ------------------------------------------------------------------ unpack_or:
// plain value

TEST(UnpackOr, PlainValueFirstAlternative) {
  auto v = unpack_or<variant<int, string>>(5);
  ASSERT_EQ(v.index(), 0u);
  EXPECT_EQ(std::get<0>(v), 5);
}

TEST(UnpackOr, PlainValueSecondAlternative) {
  auto v = unpack_or<variant<int, string>>(string("hello"));
  ASSERT_EQ(v.index(), 1u);
  EXPECT_EQ(std::get<1>(v), "hello");
}

TEST(UnpackOr, PlainValueKeepsItsExactType) {
  // The alternative is chosen by type, never by implicit conversion: an int
  // must land in the `int` slot even when a `double` slot comes first.
  auto v = unpack_or<variant<double, int>>(5);
  ASSERT_EQ(v.index(), 1u);
  EXPECT_EQ(std::get<1>(v), 5);

  auto w = unpack_or<variant<int, double>>(2.5);
  ASSERT_EQ(w.index(), 1u);
  EXPECT_EQ(std::get<1>(w), 2.5);
}

TEST(UnpackOr, PlainValueDoesNotConvertBoolAndInt) {
  auto v = unpack_or<variant<bool, int>>(1);
  ASSERT_EQ(v.index(), 1u);
  auto w = unpack_or<variant<bool, int>>(true);
  ASSERT_EQ(w.index(), 0u);
}

TEST(UnpackOr, PlainLvalueIsCopied) {
  const string source =
      "a fairly long string, definitely not in the small buffer";
  auto v = unpack_or<variant<int, string>>(source);
  ASSERT_EQ(v.index(), 1u);
  EXPECT_EQ(std::get<1>(v), source);
}

TEST(UnpackOr, PlainNonConstLvalueIsCopied) {
  string source = "a fairly long string, definitely not in the small buffer";
  auto v = unpack_or<variant<int, string>>(source);
  EXPECT_EQ(std::get<1>(v), source);
  EXPECT_FALSE(source.empty());
}

TEST(UnpackOr, PlainRvalueIsMoved) {
  auto ptr = std::make_unique<int>(7);
  int *raw = ptr.get();
  auto v = unpack_or<variant<int, unique_ptr<int>>>(std::move(ptr));
  ASSERT_EQ(v.index(), 1u);
  EXPECT_EQ(std::get<1>(v).get(), raw);
  EXPECT_EQ(ptr, nullptr);
}

TEST(UnpackOr, TupleIsAnAlternativeNotUnpacked) {
  using R = variant<tuple<int, char>, string>;
  auto v = unpack_or<R>(tuple<int, char>(1, 'x'));
  ASSERT_EQ(v.index(), 0u);
  EXPECT_EQ(std::get<0>(v), (tuple<int, char>(1, 'x')));
}

TEST(UnpackOr, PlainIntoPlainTarget) {
  // R is not a variant (both sides of the or had the same type)
  EXPECT_EQ(unpack_or<int>(7), 7);
  EXPECT_EQ(unpack_or<string>(string("abc")), "abc");
  const string s = "lvalue";
  EXPECT_EQ(unpack_or<string>(s), "lvalue");
}

TEST(UnpackOr, PlainIntoPlainTargetMovesRvalues) {
  auto ptr = std::make_unique<int>(3);
  int *raw = ptr.get();
  unique_ptr<int> out = unpack_or<unique_ptr<int>>(std::move(ptr));
  EXPECT_EQ(out.get(), raw);
  EXPECT_EQ(ptr, nullptr);
}

// ------------------------------------------------------------------ unpack_or:
// variant value

TEST(UnpackOr, VariantIntoBiggerVariantKeepsTheAlternative) {
  using Small = variant<int, char>;
  using Big = variant<int, char, double>;

  auto a = unpack_or<Big>(Small(5));
  ASSERT_EQ(a.index(), 0u);
  EXPECT_EQ(std::get<0>(a), 5);

  auto b = unpack_or<Big>(Small('c'));
  ASSERT_EQ(b.index(), 1u);
  EXPECT_EQ(std::get<1>(b), 'c');
}

TEST(UnpackOr, VariantIntoBiggerVariantWithDifferentOrder) {
  // The alternative is looked up by type, not by index.
  using Small = variant<int, char>;
  using Big = variant<double, char, int>;

  auto a = unpack_or<Big>(Small(5));
  ASSERT_EQ(a.index(), 2u);
  EXPECT_EQ(std::get<2>(a), 5);

  auto b = unpack_or<Big>(Small('c'));
  ASSERT_EQ(b.index(), 1u);
  EXPECT_EQ(std::get<1>(b), 'c');
}

TEST(UnpackOr, VariantIntoSameVariant) {
  using V = variant<int, string>;
  auto a = unpack_or<V>(V(string("x")));
  ASSERT_EQ(a.index(), 1u);
  EXPECT_EQ(std::get<1>(a), "x");
}

TEST(UnpackOr, VariantLvalueIsCopied) {
  const variant<int, string> source{
      string("a fairly long string, definitely not in the buffer")};
  auto v = unpack_or<variant<int, string, double>>(source);
  ASSERT_EQ(v.index(), 1u);
  EXPECT_EQ(std::get<1>(v), std::get<1>(source));
  EXPECT_FALSE(std::get<1>(source).empty());
}

TEST(UnpackOr, VariantRvalueIsMoved) {
  auto ptr = std::make_unique<int>(11);
  int *raw = ptr.get();
  variant<unique_ptr<int>, int> source{std::move(ptr)};

  auto v = unpack_or<variant<unique_ptr<int>, int, char>>(std::move(source));
  ASSERT_EQ(v.index(), 0u);
  EXPECT_EQ(std::get<0>(v).get(), raw);
}

TEST(UnpackOr, SingletonVariantIntoPlainTarget) {
  // R collapsed to X, the value is variant<X>
  EXPECT_EQ(unpack_or<int>(variant<int>(4)), 4);
  EXPECT_EQ(unpack_or<string>(variant<string>(string("s"))), "s");
}

TEST(UnpackOr, SingletonVariantIntoPlainTargetMoves) {
  auto ptr = std::make_unique<int>(5);
  int *raw = ptr.get();
  unique_ptr<int> out =
      unpack_or<unique_ptr<int>>(variant<unique_ptr<int>>(std::move(ptr)));
  EXPECT_EQ(out.get(), raw);
}

TEST(UnpackOr, VariantWithDuplicatedTypeAcrossSidesIsStillBuildable) {
  // or-ing variant<int, char> with variant<char, double>: a `char` from either
  // side lands in the one `char` slot.
  using R = unpack_or_t<variant<int, char>, variant<char, double>>;
  auto from_left = unpack_or<R>(variant<int, char>('l'));
  auto from_right = unpack_or<R>(variant<char, double>('r'));
  ASSERT_EQ(from_left.index(), 1u);
  ASSERT_EQ(from_right.index(), 1u);
  EXPECT_EQ(std::get<1>(from_left), 'l');
  EXPECT_EQ(std::get<1>(from_right), 'r');
}

TEST(UnpackOr, ReturnTypeIsExactlyR) {
  static_assert(std::is_same_v<decltype(unpack_or<variant<int, char>>(1)),
                               variant<int, char>>);
  static_assert(std::is_same_v<decltype(unpack_or<int>(1)), int>);
  SUCCEED();
}

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"
#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"
#include "nodle/parser/rules/or.hpp"
#include "nodle/parser/rules/token.hpp"

using nodle::io::SourceFile;
using nodle::io::SourceLocation;
using nodle::lexer::Token;
using nodle::parser::ParserContext;
using nodle::parser::ParserContextPtr;
using nodle::parser::rules::BasicRule;
using nodle::parser::rules::BasicRuleTrait;
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

ParserContextPtr empty_context() {
  return std::make_shared<ParserContext>(std::vector<Token>{});
}

// ---- small rules with controllable behaviour. `parse` must be const:
// OrRule::parse is. -----

// Matches without consuming anything.
template <typename T> struct ZeroRule : BasicRuleTrait<ZeroRule<T>, T> {
  T value;
  explicit ZeroRule(T v) : value(std::move(v)) {}
  ParseResult<T> parse(ParserContextPtr) const {
    return ParseResult<T>::Match(value, false);
  }
  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    (void)graph;
    (void)rule;
    return {0, 0};
  }
};

// Never matches.
template <typename T> struct FailRule : BasicRuleTrait<FailRule<T>, T> {
  ParseResult<T> parse(ParserContextPtr) const {
    return ParseResult<T>::NoMatch();
  }
  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    (void)graph;
    (void)rule;
    return {0, 0};
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
  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    (void)graph;
    (void)rule;
    return {0, 0};
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
  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    (void)graph;
    (void)rule;
    return {0, 0};
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
  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    (void)graph;
    (void)rule;
    return {0, 0};
  }
};

using Log = std::shared_ptr<std::vector<std::string>>;
Log new_log() { return std::make_shared<std::vector<std::string>>(); }

template <BasicRule R>
LoggedRule<R> logged(R rule, std::string name, const Log &log) {
  return LoggedRule<R>(std::move(rule), std::move(name), log);
}

std::size_t drain(const ParserContextPtr &ctx) {
  std::size_t n = 0;
  while (ctx->remaining()) {
    ctx->pop();
    ++n;
  }
  return n;
}

// Number of tokens not yet consumed (`remaining()` only says whether there is
// any). Works on a copy of the context, so it does not disturb the test.
std::size_t left(const ParserContextPtr &ctx) {
  ParserContext copy = *ctx;
  std::size_t n = 0;
  while (copy.remaining()) {
    copy.pop();
    ++n;
  }
  return n;
}

} // namespace

// ------------------------------------------------------------------ the rule's
// type

using TokOrTok = OrRule<TokenRule, TokenRule>;
using IntOrCharRule = OrRule<EatRule<int>, EatRule<char>>;

static_assert(BasicRule<TokOrTok>);
static_assert(BasicRule<IntOrCharRule>);

// two rules of the same type collapse to that type (variant<Token> => Token)
static_assert(std::is_same_v<TokOrTok::return_type, Token>);
static_assert(
    std::is_same_v<OrRule<EatRule<int>, EatRule<int>>::return_type, int>);
static_assert(std::is_same_v<OrRule<UniqueRule, UniqueRule>::return_type,
                             std::unique_ptr<int>>);

// different types give a variant
static_assert(
    std::is_same_v<IntOrCharRule::return_type, std::variant<int, char>>);
static_assert(std::is_same_v<OrRule<TokenRule, EatRule<int>>::return_type,
                             std::variant<Token, int>>);

// nested ors are flattened (either side), and duplicates removed
static_assert(
    std::is_same_v<OrRule<IntOrCharRule, EatRule<double>>::return_type,
                   std::variant<int, char, double>>);
static_assert(
    std::is_same_v<OrRule<EatRule<double>, IntOrCharRule>::return_type,
                   std::variant<double, int, char>>);
static_assert(std::is_same_v<OrRule<EatRule<int>, IntOrCharRule>::return_type,
                             std::variant<int, char>>);
static_assert(std::is_same_v<OrRule<IntOrCharRule, EatRule<int>>::return_type,
                             std::variant<int, char>>);
static_assert(std::is_same_v<OrRule<IntOrCharRule, IntOrCharRule>::return_type,
                             std::variant<int, char>>);
static_assert(
    std::is_same_v<OrRule<IntOrCharRule,
                          OrRule<EatRule<char>, EatRule<double>>>::return_type,
                   std::variant<int, char, double>>);

// a rule is its own BasicRuleTrait base, so `return_type` is the same either
// way
static_assert(std::is_base_of_v<BasicRuleTrait<TokOrTok, Token>, TokOrTok>);

// ------------------------------------------------------------------ operator|

static_assert(
    std::is_same_v<decltype(TokenRule(K_VAL) | TokenRule(L_INT)), TokOrTok>);
static_assert(std::is_same_v<decltype(EatRule<int>(1) | EatRule<char>('a')),
                             IntOrCharRule>);

// left associative: a | b | c  ==  (a | b) | c
static_assert(std::is_same_v<decltype(EatRule<int>(1) | EatRule<char>('a') |
                                      EatRule<double>(1.0)),
                             OrRule<IntOrCharRule, EatRule<double>>>);
static_assert(
    std::is_same_v<
        decltype(EatRule<int>(1) | (EatRule<char>('a') | EatRule<double>(1.0))),
        OrRule<EatRule<int>, OrRule<EatRule<char>, EatRule<double>>>>);

// ... and both groupings give the same return type here
static_assert(std::is_same_v<decltype(EatRule<int>(1) | EatRule<char>('a') |
                                      EatRule<double>(1.0))::return_type,
                             decltype(EatRule<int>(1) |
                                      (EatRule<char>('a') |
                                       EatRule<double>(1.0)))::return_type>);

// the operator is constrained to rules: plain integer or-ing is untouched
static_assert(std::is_same_v<decltype(1 | 2), int>);

TEST(OrRuleOperator, BuildsAnOrRuleThatParses) {
  auto rule = TokenRule(K_VAL) | TokenRule(L_INT);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(result->token_type(), K_VAL);
}

TEST(OrRuleOperator, WorksOnRulesDefinedInAnotherNamespace) {
  // EatRule lives in an anonymous namespace: the operator is found through the
  // BasicRuleTrait base class (ADL).
  auto rule = EatRule<int>(1) | EatRule<char>('a');
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<int>(*result), 1);
}

TEST(OrRuleOperator, TakesLvalueRulesByValue) {
  TokenRule a(K_VAL);
  TokenRule b(L_INT);
  auto rule = a | b;
  auto ctx = sample_context();
  EXPECT_TRUE(rule.parse(ctx).is_match_consumed());
  // the originals are still usable
  EXPECT_TRUE(b.parse(ctx).is_no_match()); // next token is I_IDENTIFIER
}

TEST(OrRuleOperator, CtadWithTheConstructor) {
  OrRule rule{TokenRule(K_VAL), TokenRule(L_INT)};
  static_assert(std::is_same_v<decltype(rule), TokOrTok>);
  EXPECT_TRUE(rule.parse(sample_context()).is_match_consumed());
}

// ------------------------------------------------------------------ ordered
// choice

TEST(OrRule, FirstRuleMatches) {
  auto rule = TokenRule(K_VAL) | TokenRule(I_IDENTIFIER);
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(result->token_type(), K_VAL);
  EXPECT_EQ(left(ctx), 4u);
}

TEST(OrRule, SecondRuleMatchesWhenFirstDoesNot) {
  auto rule = TokenRule(I_IDENTIFIER) | TokenRule(K_VAL);
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(result->token_type(), K_VAL);
  EXPECT_EQ(left(ctx), 4u);
}

TEST(OrRule, NeitherMatches) {
  auto rule = TokenRule(L_INT) | TokenRule(D_SEMICOLON);
  auto ctx = sample_context();
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
  EXPECT_EQ(left(ctx), 5u); // nothing consumed
}

TEST(OrRule, NoMatchDoesNotMoveTheContext) {
  auto rule = FailRule<int>() | FailRule<char>();
  auto ctx = sample_context();
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
  EXPECT_EQ(left(ctx), 5u);
  EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

TEST(OrRule, FirstWinsWhenBothWouldMatch) {
  auto rule = EatRule<int>(1) | EatRule<char>('a');
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<int>(*result), 1);
  EXPECT_EQ(left(ctx), 4u); // ONE token consumed, not two
}

TEST(OrRule, FirstWinsWithTokenRules) {
  // both rules match the first token; the order is irrelevant for the result
  // but the context is advanced only once
  auto rule = TokenRule(K_VAL) | TokenRule(K_VAL);
  auto ctx = sample_context();
  ASSERT_TRUE(rule.parse(ctx).is_match_consumed());
  EXPECT_EQ(left(ctx), 4u);
}

TEST(OrRule, SecondRuleIsNotRunWhenFirstMatches) {
  auto log = new_log();
  auto rule =
      logged(EatRule<int>(1), "a", log) | logged(EatRule<char>('x'), "b", log);
  ASSERT_TRUE(rule.parse(sample_context()).has_value());
  EXPECT_EQ(*log, (std::vector<std::string>{"a"}));
}

TEST(OrRule, SecondRuleIsRunWhenFirstFails) {
  auto log = new_log();
  auto rule =
      logged(FailRule<int>(), "a", log) | logged(EatRule<char>('x'), "b", log);
  ASSERT_TRUE(rule.parse(sample_context()).has_value());
  EXPECT_EQ(*log, (std::vector<std::string>{"a", "b"}));
}

TEST(OrRule, BothRulesAreRunWhenBothFail) {
  auto log = new_log();
  auto rule =
      logged(FailRule<int>(), "a", log) | logged(FailRule<char>(), "b", log);
  EXPECT_TRUE(rule.parse(sample_context()).is_no_match());
  EXPECT_EQ(*log, (std::vector<std::string>{"a", "b"}));
}

TEST(OrRule, SecondRuleSeesTheContextFromTheSamePosition) {
  // the first rule fails without consuming, so the second one starts on the
  // very first token
  auto rule = TokenRule(L_INT) | TokenRule(K_VAL);
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->token_type(), K_VAL);
}

TEST(OrRule, TheMatchedTokenIsTheOneFromTheStream) {
  auto rule = TokenRule(L_INT) | TokenRule(I_IDENTIFIER);
  auto ctx = sample_context();
  ctx->pop(); // skip `val`
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->token_type(), I_IDENTIFIER);
  EXPECT_EQ(result->text(), "x");
}

// ------------------------------------------------------------------ which
// alternative holds

TEST(OrRule, VariantIndexIsTheWinner) {
  auto ctx = sample_context();
  auto rule = FailRule<int>() | EatRule<char>('c');
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 1u);
  EXPECT_EQ(std::get<char>(*result), 'c');
}

TEST(OrRule, VariantHoldsTokenOrOtherType) {
  auto rule = TokenRule(L_INT) | EatRule<int>(9);
  auto ctx = sample_context();
  auto first = rule.parse(ctx); // K_VAL is not L_INT, so EatRule wins
  ASSERT_TRUE(first.has_value());
  ASSERT_EQ(first->index(), 1u);
  EXPECT_EQ(std::get<int>(*first), 9);
}

TEST(OrRule, VariantHoldsTokenWhenTokenRuleWins) {
  auto rule = TokenRule(K_VAL) | EatRule<int>(9);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<Token>(*result).token_type(), K_VAL);
}

TEST(OrRule, SameTypeResultIsNotWrappedInAVariant) {
  auto rule = FailRule<int>() | EatRule<int>(5);
  auto result = rule.parse(sample_context());
  static_assert(std::is_same_v<std::decay_t<decltype(*result)>, int>);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 5);
}

TEST(OrRule, SameTypeFirstWinsStill) {
  auto rule = EatRule<int>(1) | EatRule<int>(2);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 1);
}

TEST(OrRule, SameTypeNoMatch) {
  auto rule = FailRule<int>() | FailRule<int>();
  EXPECT_TRUE(rule.parse(sample_context()).is_no_match());
}

// ------------------------------------------------------------------ consumed /
// zero

TEST(OrRule, ConsumedFlagComesFromTheFirstRule) {
  auto rule = EatRule<int>(1) | ZeroRule<char>('z');
  auto result = rule.parse(sample_context());
  EXPECT_TRUE(result.is_match_consumed());
}

TEST(OrRule, ConsumedFlagComesFromTheSecondRule) {
  auto rule = FailRule<int>() | EatRule<char>('c');
  auto result = rule.parse(sample_context());
  EXPECT_TRUE(result.is_match_consumed());
}

TEST(OrRule, ZeroMatchOfFirstRuleIsKept) {
  auto rule = ZeroRule<int>(1) | EatRule<char>('c');
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_zero());
  EXPECT_EQ(std::get<int>(*result), 1);
  EXPECT_EQ(left(ctx), 5u);
}

TEST(OrRule, ZeroMatchOfSecondRuleIsKept) {
  auto rule = FailRule<int>() | ZeroRule<char>('z');
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_zero());
  EXPECT_EQ(std::get<char>(*result), 'z');
  EXPECT_EQ(left(ctx), 5u);
}

TEST(OrRule, ZeroMatchShadowsTheSecondRule) {
  // an empty match is still a match: the (consuming) second rule is never tried
  auto log = new_log();
  auto rule =
      logged(ZeroRule<int>(0), "a", log) | logged(EatRule<char>('c'), "b", log);
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  EXPECT_TRUE(result.is_match_zero());
  EXPECT_EQ(*log, (std::vector<std::string>{"a"}));
  EXPECT_EQ(left(ctx), 5u);
}

TEST(OrRule, ZeroMatchOnSameTypes) {
  auto rule = ZeroRule<int>(4) | EatRule<int>(5);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.is_match_zero());
  EXPECT_EQ(*result, 4);
}

TEST(OrRule, ZeroMatchWorksOnAnEmptyContext) {
  auto rule = FailRule<int>() | ZeroRule<char>('z');
  auto result = rule.parse(empty_context());
  ASSERT_TRUE(result.is_match_zero());
  EXPECT_EQ(std::get<char>(*result), 'z');
}

// ------------------------------------------------------------------ the stream

TEST(OrRule, EmptyContextNeverMatchesTokens) {
  auto rule = TokenRule(K_VAL) | TokenRule(L_INT);
  EXPECT_TRUE(rule.parse(empty_context()).is_no_match());
}

TEST(OrRule, ExhaustedContextNeverMatches) {
  auto ctx = sample_context();
  ASSERT_EQ(drain(ctx), 5u);
  auto rule = TokenRule(K_VAL) | TokenRule(L_INT);
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
}

TEST(OrRule, LastTokenCanBeMatched) {
  auto ctx = sample_context();
  for (int i = 0; i < 4; ++i) {
    ctx->pop();
  }
  auto rule = TokenRule(K_VAL) | TokenRule(D_SEMICOLON);
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(result->token_type(), D_SEMICOLON);
  EXPECT_EQ(left(ctx), 0u);
}

TEST(OrRule, CanBeCalledRepeatedlyOnTheSameContext) {
  auto rule = TokenRule(K_VAL) | TokenRule(I_IDENTIFIER) | TokenRule(D_ASSIGN);
  auto ctx = sample_context();

  EXPECT_EQ(rule.parse(ctx)->token_type(), K_VAL);
  EXPECT_EQ(rule.parse(ctx)->token_type(), I_IDENTIFIER);
  EXPECT_EQ(rule.parse(ctx)->token_type(), D_ASSIGN);
  EXPECT_TRUE(rule.parse(ctx).is_no_match()); // L_INT is next
  EXPECT_EQ(left(ctx), 2u);
}

TEST(OrRule, CanBeReusedAcrossContexts) {
  auto rule = TokenRule(I_IDENTIFIER) | TokenRule(K_VAL);
  auto a = sample_context();
  auto b = sample_context();
  EXPECT_TRUE(rule.parse(a).is_match_consumed());
  EXPECT_TRUE(rule.parse(b).is_match_consumed());
  EXPECT_EQ(left(a), 4u);
  EXPECT_EQ(left(b), 4u);
}

TEST(OrRule, AConstRuleCanParse) {
  const auto rule = TokenRule(K_VAL) | TokenRule(L_INT);
  EXPECT_TRUE(rule.parse(sample_context()).is_match_consumed());
}

TEST(OrRule, IsCopyable) {
  auto rule = TokenRule(K_VAL) | TokenRule(L_INT);
  auto copy = rule;
  EXPECT_TRUE(copy.parse(sample_context()).is_match_consumed());
  EXPECT_TRUE(rule.parse(sample_context()).is_match_consumed());
}

// ------------------------------------------------------------------ move-only
// values

TEST(OrRule, MoveOnlyValueFromTheFirstRule) {
  auto rule = UniqueRule(3) | EatRule<int>(0);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 0u);
  ASSERT_NE(std::get<0>(*result), nullptr);
  EXPECT_EQ(*std::get<0>(*result), 3);
}

TEST(OrRule, MoveOnlyValueFromTheSecondRule) {
  auto rule = FailRule<int>() | UniqueRule(4);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 1u);
  ASSERT_NE(std::get<1>(*result), nullptr);
  EXPECT_EQ(*std::get<1>(*result), 4);
}

TEST(OrRule, MoveOnlySameTypeCollapses) {
  auto rule = FailRule<std::unique_ptr<int>>() | UniqueRule(8);
  auto result = rule.parse(sample_context());
  static_assert(
      std::is_same_v<std::decay_t<decltype(*result)>, std::unique_ptr<int>>);
  ASSERT_TRUE(result.has_value());
  ASSERT_NE(*result, nullptr);
  EXPECT_EQ(**result, 8);
}

TEST(OrRule, MoveOnlyIsMovedOutOfTheResult) {
  auto rule = UniqueRule(1) | UniqueRule(2);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<int> p = *std::move(result);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(*p, 1);
}

// ------------------------------------------------------------------ three (or
// more) alternatives

TEST(OrRuleThree, FirstWins) {
  auto rule = EatRule<int>(1) | EatRule<char>('c') | EatRule<double>(2.5);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<int>(*result), 1);
}

TEST(OrRuleThree, SecondWins) {
  auto rule = FailRule<int>() | EatRule<char>('c') | EatRule<double>(2.5);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 1u);
  EXPECT_EQ(std::get<char>(*result), 'c');
}

TEST(OrRuleThree, ThirdWins) {
  auto rule = FailRule<int>() | FailRule<char>() | EatRule<double>(2.5);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 2u);
  EXPECT_EQ(std::get<double>(*result), 2.5);
}

TEST(OrRuleThree, NoneWins) {
  auto rule = FailRule<int>() | FailRule<char>() | FailRule<double>();
  auto ctx = sample_context();
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
  EXPECT_EQ(left(ctx), 5u);
}

TEST(OrRuleThree, ShortCircuitsLeftToRight) {
  auto log = new_log();
  auto rule = logged(FailRule<int>(), "a", log) |
              logged(EatRule<char>('c'), "b", log) |
              logged(EatRule<double>(1.0), "c", log);
  ASSERT_TRUE(rule.parse(sample_context()).has_value());
  EXPECT_EQ(*log, (std::vector<std::string>{"a", "b"}));
}

TEST(OrRuleThree, AllRulesRunWhenAllFail) {
  auto log = new_log();
  auto rule = logged(FailRule<int>(), "a", log) |
              logged(FailRule<char>(), "b", log) |
              logged(FailRule<double>(), "c", log);
  EXPECT_TRUE(rule.parse(sample_context()).is_no_match());
  EXPECT_EQ(*log, (std::vector<std::string>{"a", "b", "c"}));
}

TEST(OrRuleThree, RightNestedGivesTheSameResults) {
  auto rule = FailRule<int>() | (FailRule<char>() | EatRule<double>(2.5));
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 2u);
  EXPECT_EQ(std::get<double>(*result), 2.5);
}

TEST(OrRuleThree, RightNestedSecondWins) {
  auto rule = FailRule<int>() | (EatRule<char>('c') | EatRule<double>(2.5));
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 1u);
  EXPECT_EQ(std::get<char>(*result), 'c');
}

TEST(OrRuleThree, DuplicateTypeLandsInTheExistingSlot) {
  // (int | char) | int  ->  variant<int, char>: the third rule's int uses slot
  // 0
  auto rule = FailRule<int>() | FailRule<char>() | EatRule<int>(9);
  auto result = rule.parse(sample_context());
  static_assert(
      std::is_same_v<std::decay_t<decltype(*result)>, std::variant<int, char>>);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<int>(*result), 9);
}

TEST(OrRuleThree, DuplicateTypeOnTheRight) {
  // int | (char | int)  ->  variant<int, char>
  auto rule = FailRule<int>() | (FailRule<char>() | EatRule<int>(9));
  auto result = rule.parse(sample_context());
  static_assert(
      std::is_same_v<std::decay_t<decltype(*result)>, std::variant<int, char>>);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<int>(*result), 9);
}

TEST(OrRuleThree, ThreeTokenRulesStillGiveAToken) {
  auto rule = TokenRule(L_INT) | TokenRule(D_ASSIGN) | TokenRule(K_VAL);
  static_assert(std::is_same_v<decltype(rule)::return_type, Token>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(result->token_type(), K_VAL);
}

TEST(OrRuleThree, FourAlternatives) {
  auto rule = FailRule<int>() | FailRule<char>() | FailRule<double>() |
              EatRule<std::string>("s");
  static_assert(std::is_same_v<decltype(rule)::return_type,
                               std::variant<int, char, double, std::string>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->index(), 3u);
  EXPECT_EQ(std::get<std::string>(*result), "s");
}

TEST(OrRuleThree, MixedZeroAndConsumed) {
  auto rule = FailRule<int>() | ZeroRule<char>('z') | EatRule<double>(1.0);
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_zero());
  EXPECT_EQ(result->index(), 1u);
  EXPECT_EQ(left(ctx), 5u);
}

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
using nodle::parser::ParserContext;
using nodle::parser::ParserContextPtr;
using nodle::parser::rules::BasicRule;
using nodle::parser::rules::BasicRuleTrait;
using nodle::parser::rules::ChainRule;
using nodle::parser::rules::OrRule;
using nodle::parser::rules::ParseResult;
using nodle::parser::rules::TokenRule;
using enum nodle::lexer::TokenType;

// ------------------------------------------------------------------ types

using TokTok = ChainRule<TokenRule, TokenRule>;

// or of a chain and a token: the chain's tuple is ONE alternative (not
// unpacked)
static_assert(std::is_same_v<OrRule<TokTok, TokenRule>::return_type,
                             std::variant<std::tuple<Token, Token>, Token>>);
static_assert(std::is_same_v<OrRule<TokenRule, TokTok>::return_type,
                             std::variant<Token, std::tuple<Token, Token>>>);

// two chains of the same shape collapse
static_assert(std::is_same_v<OrRule<TokTok, TokTok>::return_type,
                             std::tuple<Token, Token>>);

// chains of different length are different alternatives
static_assert(
    std::is_same_v<OrRule<TokTok, ChainRule<TokTok, TokenRule>>::return_type,
                   std::variant<std::tuple<Token, Token>,
                                std::tuple<Token, Token, Token>>>);

// a chain over an or: the or's variant is ONE element of the tuple (not
// unpacked)
static_assert(std::is_same_v<ChainRule<OrRule<EatRule<int>, EatRule<char>>,
                                       TokenRule>::return_type,
                             std::tuple<std::variant<int, char>, Token>>);
static_assert(
    std::is_same_v<
        ChainRule<TokenRule, OrRule<EatRule<int>, EatRule<char>>>::return_type,
        std::tuple<Token, std::variant<int, char>>>);

// a chain over a collapsed or gets the plain type
static_assert(std::is_same_v<
              ChainRule<OrRule<TokenRule, TokenRule>, TokenRule>::return_type,
              std::tuple<Token, Token>>);

static_assert(BasicRule<OrRule<TokTok, TokenRule>>);
static_assert(
    BasicRule<ChainRule<OrRule<EatRule<int>, EatRule<char>>, TokenRule>>);

// ------------------------------------------------------------------ or over
// chains

TEST(OrChain, ChainAlternativeMatches) {
  // val x | val
  OrRule rule{TokTok{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
              TokenRule(K_VAL)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_consumed());
  ASSERT_EQ(result->index(), 0u);
  auto &[a, b] = std::get<0>(*result);
  EXPECT_EQ(a.token_type(), K_VAL);
  EXPECT_EQ(b.token_type(), I_IDENTIFIER);
  EXPECT_EQ(left(ctx), 3u);
}

TEST(OrChain, ChainFailingWithoutConsumingFallsBackToTheNextAlternative) {
  // the chain's first token does not match: plain NoMatch, so the token rule
  // gets its turn
  OrRule rule{TokTok{TokenRule(L_INT), TokenRule(I_IDENTIFIER)},
              TokenRule(K_VAL)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_consumed());
  ASSERT_EQ(result->index(), 1u);
  EXPECT_EQ(std::get<1>(*result).token_type(), K_VAL);
  EXPECT_EQ(left(ctx), 4u);
}

TEST(OrChain, ChainThatConsumedThenFailsThrowsAndTheAlternativeIsNotTried) {
  // `val` then `=` is required, but `x` follows: the chain is committed and
  // throws, even though the second alternative (`val`) would have matched
  OrRule rule{TokTok{TokenRule(K_VAL), TokenRule(D_ASSIGN)}, TokenRule(K_VAL)};
  EXPECT_THROW(rule.parse(sample_context()), std::runtime_error);
}

TEST(OrChain, ShorterAlternativeFirstShadowsTheLongerChain) {
  // ordered choice: `val` alone wins, the longer `val x` chain is never
  // considered
  OrRule rule{TokenRule(K_VAL),
              TokTok{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(result->index(), 0u);
  EXPECT_EQ(left(ctx), 4u);
}

TEST(OrChain, LongestFirstOrdering) {
  OrRule rule{TokTok{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
              TokenRule(K_VAL)};
  auto ctx = sample_context();
  auto first = rule.parse(ctx);
  ASSERT_EQ(first->index(), 0u);
  EXPECT_EQ(left(ctx), 3u);
}

TEST(OrChain, NoAlternativeMatches) {
  OrRule rule{TokTok{TokenRule(L_INT), TokenRule(L_INT)},
              TokenRule(D_SEMICOLON)};
  auto ctx = sample_context();
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
  EXPECT_EQ(left(ctx), 5u);
}

TEST(OrChain, TwoChainsOfTheSameShapeCollapseToATuple) {
  OrRule rule{TokTok{TokenRule(L_INT), TokenRule(L_INT)},
              TokTok{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)}};
  auto result = rule.parse(sample_context());
  static_assert(std::is_same_v<std::decay_t<decltype(*result)>,
                               std::tuple<Token, Token>>);
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(std::get<0>(*result).token_type(), K_VAL);
  EXPECT_EQ(std::get<1>(*result).token_type(), I_IDENTIFIER);
}

TEST(OrChain, ChainsOfDifferentLengthAreDistinctAlternatives) {
  OrRule rule{TokTok{TokenRule(L_INT), TokenRule(L_INT)},
              ChainRule{TokTok{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
                        TokenRule(D_ASSIGN)}};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.is_match_consumed());
  ASSERT_EQ(result->index(), 1u);
  static_assert(std::is_same_v<
                std::variant_alternative_t<1, std::decay_t<decltype(*result)>>,
                std::tuple<Token, Token, Token>>);
  EXPECT_EQ(std::get<2>(std::get<1>(*result)).token_type(), D_ASSIGN);
}

// ------------------------------------------------------------------ chain over
// ors

TEST(ChainOr, OrInsideChainFirstAlternative) {
  // (int | char) then token
  ChainRule rule{OrRule{EatRule<int>(1), EatRule<char>('c')},
                 TokenRule(I_IDENTIFIER)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_consumed());
  auto &[choice, tok] = *result;
  ASSERT_EQ(choice.index(), 0u);
  EXPECT_EQ(std::get<int>(choice), 1);
  EXPECT_EQ(tok.token_type(), I_IDENTIFIER);
  EXPECT_EQ(left(ctx), 3u);
}

TEST(ChainOr, OrInsideChainSecondAlternative) {
  ChainRule rule{OrRule{FailRule<int>(), EatRule<char>('c')},
                 TokenRule(I_IDENTIFIER)};
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.is_match_consumed());
  auto &[choice, tok] = *result;
  ASSERT_EQ(choice.index(), 1u);
  EXPECT_EQ(std::get<char>(choice), 'c');
  EXPECT_EQ(tok.token_type(), I_IDENTIFIER);
}

TEST(ChainOr, OrAsSecondElementOfTheChain) {
  // token then (identifier | int-literal)
  ChainRule rule{TokenRule(K_VAL),
                 OrRule{TokenRule(L_INT), TokenRule(I_IDENTIFIER)}};
  static_assert(
      std::is_same_v<decltype(rule)::return_type, std::tuple<Token, Token>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(std::get<0>(*result).token_type(), K_VAL);
  EXPECT_EQ(std::get<1>(*result).token_type(), I_IDENTIFIER);
}

TEST(ChainOr, OrInsideChainFailingAfterConsumingThrows) {
  // `val` consumed, then (int-literal | semicolon) matches neither `x`
  ChainRule rule{TokenRule(K_VAL),
                 OrRule{TokenRule(L_INT), TokenRule(D_SEMICOLON)}};
  EXPECT_THROW(rule.parse(sample_context()), std::runtime_error);
}

TEST(ChainOr, FailingOrFirstGivesNoMatchWithoutThrowing) {
  ChainRule rule{OrRule{TokenRule(L_INT), TokenRule(D_SEMICOLON)},
                 TokenRule(K_VAL)};
  auto ctx = sample_context();
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
  EXPECT_EQ(left(ctx), 5u);
}

TEST(ChainOr, ZeroMatchingOrFollowedByFailingRuleIsNoMatch) {
  // The or matches without consuming; the second rule then fails: NoMatch, not
  // an exception
  ChainRule rule{OrRule{FailRule<int>(), ZeroRule<char>('z')},
                 TokenRule(L_INT)};
  auto ctx = sample_context();
  EXPECT_TRUE(rule.parse(ctx).is_no_match());
  EXPECT_EQ(left(ctx), 5u);
}

TEST(ChainOr, OrOfCollapsedTokensInAChainGivesPlainTokens) {
  // (val | x) then (x | =)  ->  tuple<Token, Token>
  ChainRule rule{OrRule{TokenRule(I_IDENTIFIER), TokenRule(K_VAL)},
                 OrRule{TokenRule(I_IDENTIFIER), TokenRule(D_ASSIGN)}};
  static_assert(
      std::is_same_v<decltype(rule)::return_type, std::tuple<Token, Token>>);
  auto result = rule.parse(sample_context());
  ASSERT_TRUE(result.is_match_consumed());
  EXPECT_EQ(std::get<0>(*result).token_type(), K_VAL);
  EXPECT_EQ(std::get<1>(*result).token_type(), I_IDENTIFIER);
}

TEST(ChainOr, ChainOfChainsInsideOr) {
  // ((val x =) | semicolon): the three-token chain is one alternative
  auto three = ChainRule{TokTok{TokenRule(K_VAL), TokenRule(I_IDENTIFIER)},
                         TokenRule(D_ASSIGN)};
  OrRule rule{three, TokenRule(D_SEMICOLON)};
  auto ctx = sample_context();
  auto result = rule.parse(ctx);
  ASSERT_TRUE(result.is_match_consumed());
  ASSERT_EQ(result->index(), 0u);
  EXPECT_EQ(std::get<2>(std::get<0>(*result)).token_type(), D_ASSIGN);
  EXPECT_EQ(left(ctx), 2u);
}