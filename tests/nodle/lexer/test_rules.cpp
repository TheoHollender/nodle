#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/rules.hpp"
#include "nodle/lexer/token.hpp"

using namespace nodle::io;
using namespace nodle::lexer;
using namespace nodle::lexer::rules;

namespace {

using Result = std::optional<LexerResult>;

// Applies `rule` the way a lexer would: the location covers everything from the
// current position (`offset`) to the end of the file.
Result run_rule(const LexerRule &rule, const std::string &input,
                std::uint32_t offset = 0) {
  auto file = SourceFile::from_string("test.nodle", input);
  const auto size = static_cast<std::uint32_t>(input.size());
  return rule(SourceLocation(file, offset, size - offset));
}

void expect_match(const Result &result, std::uint32_t length, TokenType type) {
  ASSERT_TRUE(result.has_value()) << "expected a match";
  EXPECT_EQ(result->length, length);
  EXPECT_EQ(result->type, type);
}

#define EXPECT_MATCH(result, length, type)                                     \
  do {                                                                         \
    SCOPED_TRACE(#result);                                                     \
    expect_match((result), (length), (type));                                  \
  } while (0)

// Maximal munch: the longest match wins, and the earliest rule wins ties.
// This is the lexer behaviour that create_all_rules() is designed for.
Result lex_longest(const std::vector<LexerRule> &rules,
                   const std::string &input) {
  Result best;
  for (const auto &rule : rules) {
    Result r = run_rule(rule, input);
    if (r && (!best || r->length > best->length)) {
      best = r;
    }
  }
  return best;
}

// Overwrites stack memory that a dead stack frame used to occupy.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void clobber_stack() {
  volatile char junk[8192];
  for (auto &c : junk) {
    c = 0x55;
  }
}

} // namespace

// ---------------------------------------------------------------- number rule

TEST(NumberRule, Integer) {
  auto rule = create_number_rule();
  EXPECT_MATCH(run_rule(rule, "123"), 3u, L_INT);
  EXPECT_MATCH(run_rule(rule, "0"), 1u, L_INT);
  EXPECT_MATCH(run_rule(rule, "007"), 3u, L_INT);
}

TEST(NumberRule, Float) {
  auto rule = create_number_rule();
  EXPECT_MATCH(run_rule(rule, "3.14"), 4u, L_FLOAT);
  EXPECT_MATCH(run_rule(rule, "0.5"), 3u, L_FLOAT);
}

TEST(NumberRule, StopsAtFirstNonNumericCharacter) {
  auto rule = create_number_rule();
  EXPECT_MATCH(run_rule(rule, "42 + 1"), 2u, L_INT);
  EXPECT_MATCH(run_rule(rule, "3.14)"), 4u, L_FLOAT);
  EXPECT_MATCH(run_rule(rule, "7;"), 1u, L_INT);
}

TEST(NumberRule, RejectsANumberGluedToAnIdentifier) {
  auto rule = create_number_rule();
  EXPECT_FALSE(run_rule(rule, "2int").has_value());
  EXPECT_FALSE(run_rule(rule, "12abc").has_value());
  EXPECT_FALSE(run_rule(rule, "2x").has_value());
  EXPECT_FALSE(run_rule(rule, "2_x").has_value());
  EXPECT_FALSE(run_rule(rule, "2_").has_value());
  EXPECT_FALSE(run_rule(rule, "3.14f").has_value());
  EXPECT_FALSE(run_rule(rule, "0x10").has_value());
}

TEST(NumberRule,
     AcceptsANumberFollowedByAnythingThatIsNotAnIdentifierCharacter) {
  auto rule = create_number_rule();
  EXPECT_MATCH(run_rule(rule, "2 int"), 1u, L_INT);
  EXPECT_MATCH(run_rule(rule, "2\nint"), 1u, L_INT);
  EXPECT_MATCH(run_rule(rule, "2+int"), 1u, L_INT);
  EXPECT_MATCH(run_rule(rule, "2)"), 1u, L_INT);
  EXPECT_MATCH(run_rule(rule, "2,3"), 1u, L_INT);
  EXPECT_MATCH(run_rule(rule, "2;"), 1u, L_INT);
  EXPECT_MATCH(run_rule(rule, "2"), 1u, L_INT);
  EXPECT_MATCH(run_rule(rule, "3.14 x"), 4u, L_FLOAT);
}

TEST(NumberRule, SecondDotEndsTheNumber) {
  auto rule = create_number_rule();
  EXPECT_MATCH(run_rule(rule, "1.2.3"), 3u, L_FLOAT);
}

TEST(NumberRule,
     TrailingDotIsPartOfTheFloat) { // characterization of current behaviour
  auto rule = create_number_rule();
  EXPECT_MATCH(run_rule(rule, "3."), 2u, L_FLOAT);
}

TEST(NumberRule, LeadingDotIsAFloat) { // characterization of current behaviour
  auto rule = create_number_rule();
  EXPECT_MATCH(run_rule(rule, ".5"), 2u, L_FLOAT);
}

TEST(NumberRule, DoesNotMatchWithoutDigits) {
  auto rule = create_number_rule();
  EXPECT_FALSE(run_rule(rule, "").has_value());
  EXPECT_FALSE(run_rule(rule, "abc").has_value());
  EXPECT_FALSE(run_rule(rule, ".").has_value());
  EXPECT_FALSE(run_rule(rule, ". 5").has_value());
  EXPECT_FALSE(
      run_rule(rule, " 5").has_value()); // leading whitespace is not skipped
}

TEST(NumberRule, SignIsNotPartOfTheNumber) {
  auto rule = create_number_rule();
  EXPECT_FALSE(run_rule(rule, "-5").has_value());
  EXPECT_FALSE(run_rule(rule, "+5").has_value());
}

TEST(NumberRule, ReadsFromTheLocationNotFromTheStartOfTheFile) {
  auto rule = create_number_rule();
  EXPECT_MATCH(run_rule(rule, "x = 12;", 4), 2u, L_INT);
  EXPECT_FALSE(run_rule(rule, "12 x", 3).has_value());
}

TEST(NumberRule, EmptyLocationAtEndOfFile) {
  auto rule = create_number_rule();
  EXPECT_FALSE(run_rule(rule, "12", 2).has_value());
}

// ---------------------------------------------------------------- string rule

TEST(StringRule, MatchesPrefix) {
  auto rule = create_string_rule("if", K_IF);
  EXPECT_MATCH(run_rule(rule, "if x"), 2u, K_IF);
  EXPECT_MATCH(run_rule(rule, "if"), 2u, K_IF);
}

TEST(StringRule, DoesNotMatchWhenKeywordIsNotAtTheStart) {
  auto rule = create_string_rule("if", K_IF);
  EXPECT_FALSE(run_rule(rule, "x if").has_value());
  EXPECT_FALSE(run_rule(rule, " if").has_value());
}

TEST(StringRule, DoesNotMatchInputShorterThanKeyword) {
  auto rule = create_string_rule("if", K_IF);
  EXPECT_FALSE(run_rule(rule, "i").has_value());
  EXPECT_FALSE(run_rule(rule, "").has_value());
}

TEST(StringRule, IsCaseSensitive) {
  auto rule = create_string_rule("if", K_IF);
  EXPECT_FALSE(run_rule(rule, "IF").has_value());
  EXPECT_FALSE(run_rule(rule, "If").has_value());
}

TEST(StringRule,
     DoesNotCheckForAWordBoundary) { // characterization: the lexer must use
                                     // maximal munch
  auto rule = create_string_rule("if", K_IF);
  EXPECT_MATCH(run_rule(rule, "iffy"), 2u, K_IF);
}

TEST(StringRule, MultiCharacterOperator) {
  auto eq = create_string_rule("==", O_EQUAL);
  EXPECT_MATCH(run_rule(eq, "== 1"), 2u, O_EQUAL);
  EXPECT_FALSE(run_rule(eq, "= 1").has_value());
  EXPECT_FALSE(run_rule(eq, "=").has_value());
}

TEST(StringRule, ShorterOperatorMatchesPrefixOfLongerOne) {
  auto assign = create_string_rule("=", D_ASSIGN);
  EXPECT_MATCH(run_rule(assign, "=="), 1u, D_ASSIGN);
}

TEST(StringRule, ReadsFromTheLocationNotFromTheStartOfTheFile) {
  auto rule = create_string_rule("then", K_THEN);
  EXPECT_MATCH(run_rule(rule, "if x then y", 5), 4u, K_THEN);
  EXPECT_FALSE(run_rule(rule, "if x then y", 0).has_value());
}

TEST(StringRule, ReturnsTheTypeItWasCreatedWith) {
  EXPECT_MATCH(run_rule(create_string_rule("a", K_VAL), "a"), 1u, K_VAL);
  EXPECT_MATCH(run_rule(create_string_rule("a", K_DEF), "a"), 1u, K_DEF);
}

TEST(StringRule, RulesAreIndependentOfEachOther) {
  auto if_rule = create_string_rule("if", K_IF);
  auto else_rule = create_string_rule("else", K_ELSE);
  EXPECT_MATCH(run_rule(else_rule, "else"), 4u, K_ELSE);
  EXPECT_MATCH(run_rule(if_rule, "if"), 2u, K_IF);
  EXPECT_FALSE(run_rule(if_rule, "else").has_value());
  EXPECT_FALSE(run_rule(else_rule, "if").has_value());
}

// The rule must own its keyword and type: it is used long after
// create_string_rule() returned, and after the caller's own variables are gone.
LexerRule make_rule_from_locals() {
  std::string keyword = "hello";
  TokenType type = K_VAL;
  return create_string_rule(keyword, type);
}

TEST(StringRule, OutlivesTheArgumentsItWasCreatedFrom) {
  auto rule = make_rule_from_locals();
  clobber_stack();
  EXPECT_MATCH(run_rule(rule, "hello world"), 5u, K_VAL);
  EXPECT_FALSE(run_rule(rule, "world").has_value());
}

TEST(StringRule, OutlivesAKeywordThatIsLaterModified) {
  std::string keyword = "abc";
  auto rule = create_string_rule(keyword, K_VAL);
  keyword = "zzz";
  EXPECT_MATCH(run_rule(rule, "abc"), 3u, K_VAL);
  EXPECT_FALSE(run_rule(rule, "zzz").has_value());
}

TEST(StringRule, CopiedRuleBehavesTheSame) {
  auto rule = create_string_rule("if", K_IF);
  LexerRule copy = rule;
  clobber_stack();
  EXPECT_MATCH(run_rule(copy, "if"), 2u, K_IF);
  EXPECT_MATCH(run_rule(rule, "if"), 2u, K_IF);
}

// ------------------------------------------------------------ identifier rule

TEST(IdentifierRule, SimpleIdentifiers) {
  auto rule = create_identifier_rule(I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "foo"), 3u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "Hello"), 5u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "x"), 1u, I_IDENTIFIER);
}

TEST(IdentifierRule, UnderscoreAndDigits) {
  auto rule = create_identifier_rule(I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "_private"), 8u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "_"), 1u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "x1"), 2u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "foo_bar9"), 8u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "a_1_B"), 5u, I_IDENTIFIER);
}

TEST(IdentifierRule, CannotStartWithADigit) {
  auto rule = create_identifier_rule(I_IDENTIFIER);
  EXPECT_FALSE(run_rule(rule, "1x").has_value());
  EXPECT_FALSE(run_rule(rule, "9").has_value());
}

TEST(IdentifierRule, StopsAtFirstInvalidCharacter) {
  auto rule = create_identifier_rule(I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "foo bar"), 3u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "foo-bar"), 3u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "foo.bar"), 3u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "foo(x)"), 3u, I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "foo\nbar"), 3u, I_IDENTIFIER);
}

TEST(IdentifierRule, DoesNotMatchEmptyOrNonIdentifierStart) {
  auto rule = create_identifier_rule(I_IDENTIFIER);
  EXPECT_FALSE(run_rule(rule, "").has_value());
  EXPECT_FALSE(run_rule(rule, " foo").has_value());
  EXPECT_FALSE(run_rule(rule, "+").has_value());
  EXPECT_FALSE(run_rule(rule, "(x)").has_value());
  EXPECT_FALSE(run_rule(rule, "@x").has_value());
}

TEST(IdentifierRule, NonAsciiBytesAreNotIdentifierCharacters) {
  auto rule = create_identifier_rule(I_IDENTIFIER);
  EXPECT_FALSE(run_rule(rule, "\xC3\xA9").has_value()); // "é" in UTF-8
  EXPECT_MATCH(run_rule(rule, "caf\xC3\xA9"), 3u,
               I_IDENTIFIER); // stops before the é
}

TEST(IdentifierRule, ReadsFromTheLocationNotFromTheStartOfTheFile) {
  auto rule = create_identifier_rule(I_IDENTIFIER);
  EXPECT_MATCH(run_rule(rule, "val foo = 1", 4), 3u, I_IDENTIFIER);
  EXPECT_FALSE(run_rule(rule, "val foo = 1", 7).has_value()); // at the space
}

TEST(IdentifierRule, ReturnsTheTypeItWasCreatedWith) {
  EXPECT_MATCH(run_rule(create_identifier_rule(K_VAL), "abc"), 3u, K_VAL);
  EXPECT_MATCH(run_rule(create_identifier_rule(I_IDENTIFIER), "abc"), 3u,
               I_IDENTIFIER);
}

LexerRule make_identifier_rule_from_local() {
  TokenType type = K_DEF;
  return create_identifier_rule(type);
}

TEST(IdentifierRule, OutlivesTheArgumentItWasCreatedFrom) {
  auto rule = make_identifier_rule_from_local();
  clobber_stack();
  EXPECT_MATCH(run_rule(rule, "abc"), 3u, K_DEF);
}

TEST(NonCommentRules, AreNotSkipped) {
  EXPECT_FALSE(run_rule(create_number_rule(), "1")->should_skip);
  EXPECT_FALSE(run_rule(create_string_rule("val", K_VAL), "val")->should_skip);
  EXPECT_FALSE(
      run_rule(create_identifier_rule(I_IDENTIFIER), "x")->should_skip);
}

// --------------------------------------------------------------- comment rule

TEST(CommentRule, RunsUntilTheEndOfTheLine) {
  auto rule = create_comment_rule("//", COMMENT);
  EXPECT_MATCH(run_rule(rule, "// hello\nval"), 8u, COMMENT);
  EXPECT_MATCH(run_rule(rule, "//x\n"), 3u, COMMENT);
}

TEST(CommentRule, RunsUntilTheEndOfTheFileWithoutANewline) {
  auto rule = create_comment_rule("//", COMMENT);
  EXPECT_MATCH(run_rule(rule, "// hello"), 8u, COMMENT);
  EXPECT_MATCH(run_rule(rule, "//"), 2u, COMMENT);
}

TEST(CommentRule, KeepsACarriageReturnBeforeTheNewline) {
  // Characterization: with CRLF line endings the '\r' is part of the comment.
  EXPECT_MATCH(run_rule(create_comment_rule("//", COMMENT), "//a\r\nb"), 4u,
               COMMENT);
}

TEST(CommentRule, IsSkipped) {
  auto result = run_rule(create_comment_rule("//", COMMENT), "// hi");
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_skip);
}

TEST(CommentRule, DoesNotMatchWithoutTheFullPrefix) {
  auto rule = create_comment_rule("//", COMMENT);
  EXPECT_FALSE(run_rule(rule, "/ x").has_value());
  EXPECT_FALSE(run_rule(rule, "a // x").has_value());
  EXPECT_FALSE(run_rule(rule, "").has_value());
}

TEST(CommentRule, ReadsFromTheLocationNotFromTheStartOfTheFile) {
  auto rule = create_comment_rule("//", COMMENT);
  EXPECT_MATCH(run_rule(rule, "x = 1; // one\ny", 7), 6u, COMMENT);
}

// --------------------------------------------------------------- all rules

namespace {

struct Expected {
  const char *text;
  TokenType type;
};

const std::vector<Expected> kKeywords = {
    {"false", L_FALSE},
    {"true", L_TRUE},
    {"int", K_INT},
    {"float", K_FLOAT},
    {"bool", K_BOOL},
    {"val", K_VAL},
    {"if", K_IF},
    {"then", K_THEN},
    {"else", K_ELSE},
    {"self", K_SELF},
    {"class", K_CLASS},
    {"graph", K_GRAPH},
    {"state", K_STATE},
    {"weight", K_WEIGHT},
    {"edge", K_EDGE},
    {"prove", K_PROVE},
    {"with", K_WITH},
    {"proof", K_PROOF},
    {"such", K_SUCH},
    {"that", K_THAT},
    {"follows", K_FOLLOWS},
    {"equivalence", K_EQUIVALENCE},
    {"property", K_PROPERTY},
    {"lemma", K_LEMMA},
    {"use", K_USE},
    {"def", K_DEF},
    {"override", K_OVERRIDE},
};

const std::vector<Expected> kOperators = {
    {"!", O_NOT},         {"+", O_PLUS},         {"-", O_MINUS},
    {"*", O_TIMES},       {"/", O_DIVIDE},       {"%", O_MODULO},
    {"==", O_EQUAL},      {"!=", O_DIFFERENT},   {"<", O_LESS_THAN},
    {"<=", O_LESS_EQUAL}, {">", O_GREATER_THAN}, {">=", O_GREATER_EQUAL},
    {"&&", O_AND},        {"||", O_OR},
};

const std::vector<Expected> kDelimiters = {
    {"(", D_LPAREN}, {")", D_RPAREN}, {".", D_DOT},
    {",", D_COMMA},  {":", D_COLON},  {";", D_SEMICOLON},
    {"=", D_ASSIGN}, {"--", D_EDGE},  {"|", D_PIPE},
};

void expect_all_lex_to(const std::vector<LexerRule> &rules,
                       const std::vector<Expected> &table) {
  for (const auto &e : table) {
    SCOPED_TRACE(e.text);
    const std::string text = e.text;
    EXPECT_MATCH(lex_longest(rules, text), text.size(), e.type);
    // Still the same token when followed by something that cannot extend it.
    EXPECT_MATCH(lex_longest(rules, text + " "), text.size(), e.type);
  }
}

} // namespace

TEST(AllRules, IsNotEmpty) { EXPECT_FALSE(create_all_rules().empty()); }

TEST(AllRules, DoubleSlashIsACommentNotTwoDivides) {
  auto rules = create_all_rules();
  EXPECT_MATCH(lex_longest(rules, "// a / b"), 8u, COMMENT);
  EXPECT_MATCH(lex_longest(rules, "/ b"), 1u, O_DIVIDE);
}

TEST(AllRules, Numbers) {
  auto rules = create_all_rules();
  EXPECT_MATCH(lex_longest(rules, "123"), 3u, L_INT);
  EXPECT_MATCH(lex_longest(rules, "1.5"), 3u, L_FLOAT);
  EXPECT_MATCH(lex_longest(rules, ".5"), 2u, L_FLOAT);
}

TEST(AllRules, Keywords) { expect_all_lex_to(create_all_rules(), kKeywords); }

TEST(AllRules, Operators) { expect_all_lex_to(create_all_rules(), kOperators); }

TEST(AllRules, Delimiters) {
  expect_all_lex_to(create_all_rules(), kDelimiters);
}

TEST(AllRules, LongerOperatorBeatsItsPrefix) {
  auto rules = create_all_rules();
  EXPECT_MATCH(lex_longest(rules, "==="), 2u, O_EQUAL);
  EXPECT_MATCH(lex_longest(rules, "=="), 2u, O_EQUAL);
  EXPECT_MATCH(lex_longest(rules, "= ="), 1u, D_ASSIGN);
  EXPECT_MATCH(lex_longest(rules, "!="), 2u, O_DIFFERENT);
  EXPECT_MATCH(lex_longest(rules, "!x"), 1u, O_NOT);
  EXPECT_MATCH(lex_longest(rules, "<="), 2u, O_LESS_EQUAL);
  EXPECT_MATCH(lex_longest(rules, ">="), 2u, O_GREATER_EQUAL);
  EXPECT_MATCH(lex_longest(rules, "||"), 2u, O_OR);
  EXPECT_MATCH(lex_longest(rules, "|x"), 1u, D_PIPE);
  EXPECT_MATCH(lex_longest(rules, "--"), 2u, D_EDGE);
  EXPECT_MATCH(lex_longest(rules, "- -"), 1u, O_MINUS);
}

TEST(AllRules, DotIsADelimiterUnlessFollowedByDigits) {
  auto rules = create_all_rules();
  EXPECT_MATCH(lex_longest(rules, "."), 1u, D_DOT);
  EXPECT_MATCH(lex_longest(rules, ".x"), 1u, D_DOT);
  EXPECT_MATCH(lex_longest(rules, ".5"), 2u, L_FLOAT);
}

TEST(AllRules, IdentifiersThatStartWithAKeyword) {
  auto rules = create_all_rules();
  EXPECT_MATCH(lex_longest(rules, "integer"), 7u, I_IDENTIFIER);
  EXPECT_MATCH(lex_longest(rules, "iffy"), 4u, I_IDENTIFIER);
  EXPECT_MATCH(lex_longest(rules, "int2"), 4u, I_IDENTIFIER);
  EXPECT_MATCH(lex_longest(rules, "true_value"), 10u, I_IDENTIFIER);
  EXPECT_MATCH(lex_longest(rules, "useful"), 6u, I_IDENTIFIER);
  EXPECT_MATCH(lex_longest(rules, "self_"), 5u, I_IDENTIFIER);
}

TEST(AllRules, NumberGluedToAnIdentifierMatchesNothing) {
  auto rules = create_all_rules();
  EXPECT_FALSE(lex_longest(rules, "2int").has_value());
  EXPECT_FALSE(lex_longest(rules, "2abc").has_value());
  EXPECT_FALSE(lex_longest(rules, "2_x").has_value());
  EXPECT_FALSE(lex_longest(rules, "3.14f").has_value());
  EXPECT_MATCH(lex_longest(rules, "2 int"), 1u, L_INT);
}

TEST(AllRules, KeywordBeatsIdentifierOnATie) {
  // "true" is matched by both the keyword rule and the identifier rule at
  // length 4. The keyword rules must therefore come before the identifier rule.
  auto rules = create_all_rules();
  EXPECT_MATCH(lex_longest(rules, "true"), 4u, L_TRUE);
  EXPECT_MATCH(lex_longest(rules, "val x"), 3u, K_VAL);
}

TEST(AllRules, PlainIdentifiers) {
  auto rules = create_all_rules();
  EXPECT_MATCH(lex_longest(rules, "foo"), 3u, I_IDENTIFIER);
  EXPECT_MATCH(lex_longest(rules, "_x1"), 3u, I_IDENTIFIER);
  EXPECT_MATCH(lex_longest(rules, "Class"), 5u,
               I_IDENTIFIER); // keywords are case-sensitive
}

TEST(AllRules, ExpressionPrefixes) {
  auto rules = create_all_rules();
  EXPECT_MATCH(lex_longest(rules, "x+1"), 1u, I_IDENTIFIER);
  EXPECT_MATCH(lex_longest(rules, "+1"), 1u, O_PLUS);
  EXPECT_MATCH(lex_longest(rules, "(a)"), 1u, D_LPAREN);
  EXPECT_MATCH(lex_longest(rules, "1+2"), 1u, L_INT);
}

TEST(AllRules, UnknownCharactersMatchNothing) {
  auto rules = create_all_rules();
  EXPECT_FALSE(lex_longest(rules, "@").has_value());
  EXPECT_FALSE(lex_longest(rules, "#").has_value());
  EXPECT_FALSE(lex_longest(rules, "&").has_value()); // only "&&" exists
  EXPECT_FALSE(lex_longest(rules, " ")
                   .has_value()); // whitespace must be skipped by the lexer
  EXPECT_FALSE(lex_longest(rules, "").has_value());
}

TEST(AllRules, RulesStayValidAfterTheirCreationFrameIsGone) {
  auto rules = create_all_rules();
  clobber_stack();
  EXPECT_MATCH(lex_longest(rules, "class"), 5u, K_CLASS);
  EXPECT_MATCH(lex_longest(rules, "override"), 8u, K_OVERRIDE);
  EXPECT_MATCH(lex_longest(rules, "foo"), 3u, I_IDENTIFIER);
}

TEST(AllRules, EachCallReturnsIndependentWorkingRules) {
  auto first = create_all_rules();
  auto second = create_all_rules();
  first.clear();
  clobber_stack();
  EXPECT_MATCH(lex_longest(second, "graph"), 5u, K_GRAPH);
}

TEST(AllRules, RulesAreStateless) {
  auto rules = create_all_rules();
  for (int i = 0; i < 3; ++i) {
    EXPECT_MATCH(lex_longest(rules, "weight"), 6u, K_WEIGHT);
    EXPECT_MATCH(lex_longest(rules, "12.5"), 4u, L_FLOAT);
  }
}

TEST(AllRules, WorkOnAnInputPositionInTheMiddleOfAFile) {
  auto rules = create_all_rules();
  auto file = SourceFile::from_string("t.nodle", "val x = 42;");
  const auto size = static_cast<std::uint32_t>(file->text(0, 100).size());
  auto lex_at = [&](std::uint32_t offset) {
    Result best;
    for (const auto &rule : rules) {
      Result r = rule(SourceLocation(file, offset, size - offset));
      if (r && (!best || r->length > best->length))
        best = r;
    }
    return best;
  };
  EXPECT_MATCH(lex_at(0), 3u, K_VAL);
  EXPECT_MATCH(lex_at(4), 1u, I_IDENTIFIER);
  EXPECT_MATCH(lex_at(6), 1u, D_ASSIGN);
  EXPECT_MATCH(lex_at(8), 2u, L_INT);
  EXPECT_MATCH(lex_at(10), 1u, D_SEMICOLON);
}