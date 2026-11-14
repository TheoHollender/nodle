#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/lexer.hpp"
#include "nodle/lexer/rules.hpp"
#include "nodle/lexer/token.hpp"

using namespace nodle::io;
using namespace nodle::lexer;
using namespace nodle::lexer::rules;

namespace {

SourceFile::Ptr make_file(const std::string &content) {
  return SourceFile::from_string("test.nodle", content);
}

std::vector<Token> lex(const std::string &input) {
  Lexer lexer;
  return lexer.lex(make_file(input));
}

std::vector<TokenType> types_of(const std::vector<Token> &tokens) {
  std::vector<TokenType> types;
  for (const auto &t : tokens) {
    types.push_back(t.token_type());
  }
  return types;
}

std::vector<std::string> texts_of(const std::vector<Token> &tokens) {
  std::vector<std::string> texts;
  for (const auto &t : tokens) {
    texts.emplace_back(t.text());
  }
  return texts;
}

using Types = std::vector<TokenType>;
using Texts = std::vector<std::string>;

// A rule that matches the literal `text` and yields `type`.
LexerRule literal(std::string text, TokenType type) {
  return [text = std::move(text),
          type](SourceLocation loc) -> std::optional<LexerResult> {
    if (loc.text().substr(0, text.size()) != text) {
      return std::nullopt;
    }
    return LexerResult{static_cast<std::uint32_t>(text.size()), type};
  };
}

void expect_throws_mentioning(const std::string &input,
                              const std::string &needle) {
  try {
    lex(input);
    FAIL() << "expected an exception for: " << input;
  } catch (const std::runtime_error &e) {
    EXPECT_NE(std::string(e.what()).find(needle), std::string::npos)
        << "message was: " << e.what();
  }
}

} // namespace

// ------------------------------------------------------------------ lex():
// basics

TEST(LexerLex, EmptyInputGivesNoTokens) { EXPECT_TRUE(lex("").empty()); }

TEST(LexerLex, WhitespaceOnlyGivesNoTokens) {
  EXPECT_TRUE(lex(" ").empty());
  EXPECT_TRUE(lex("   \t\r\n \n\n\t").empty());
}

TEST(LexerLex, SingleToken) {
  auto tokens = lex("val");
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].token_type(), K_VAL);
  EXPECT_EQ(tokens[0].text(), "val");
}

TEST(LexerLex, SimpleStatement) {
  auto tokens = lex("val x = 42;");
  EXPECT_EQ(types_of(tokens),
            (Types{K_VAL, I_IDENTIFIER, D_ASSIGN, L_INT, D_SEMICOLON}));
  EXPECT_EQ(texts_of(tokens), (Texts{"val", "x", "=", "42", ";"}));
}

TEST(LexerLex, StatementWithTypeAndExpression) {
  auto tokens = lex("val x: int = 1 + 2;");
  EXPECT_EQ(types_of(tokens),
            (Types{K_VAL, I_IDENTIFIER, D_COLON, K_INT, D_ASSIGN, L_INT, O_PLUS,
                   L_INT, D_SEMICOLON}));
}

TEST(LexerLex, WhitespaceBetweenTokensIsOptionalWhenTheyCannotMerge) {
  EXPECT_EQ(types_of(lex("x+1")), (Types{I_IDENTIFIER, O_PLUS, L_INT}));
  EXPECT_EQ(types_of(lex("f(1,2)")),
            (Types{I_IDENTIFIER, D_LPAREN, L_INT, D_COMMA, L_INT, D_RPAREN}));
  EXPECT_EQ(types_of(lex("x.y")), (Types{I_IDENTIFIER, D_DOT, I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("-1")), (Types{O_MINUS, L_INT}));
}

TEST(LexerLex, AllWhitespaceKindsSeparateTokens) {
  auto tokens = lex("a\tb\r\nc  d\ne");
  EXPECT_EQ(texts_of(tokens), (Texts{"a", "b", "c", "d", "e"}));
}

TEST(LexerLex, LeadingAndTrailingWhitespaceIsIgnored) {
  auto tokens = lex("  \n x \t\n");
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].text(), "x");
  EXPECT_EQ(tokens[0].source_location().offset(), 4u);
}

// ------------------------------------------------------------ lex(): maximal
// munch

TEST(LexerLex, KeywordsAndIdentifiers) {
  EXPECT_EQ(types_of(lex("int integer val value")),
            (Types{K_INT, I_IDENTIFIER, K_VAL, I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("iffy if")), (Types{I_IDENTIFIER, K_IF}));
  EXPECT_EQ(types_of(lex("true true_ false0")),
            (Types{L_TRUE, I_IDENTIFIER, I_IDENTIFIER}));
}

TEST(LexerLex, LongerOperatorBeatsItsPrefix) {
  EXPECT_EQ(types_of(lex("a==b")),
            (Types{I_IDENTIFIER, O_EQUAL, I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("a=b")),
            (Types{I_IDENTIFIER, D_ASSIGN, I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("a<=b<c")),
            (Types{I_IDENTIFIER, O_LESS_EQUAL, I_IDENTIFIER, O_LESS_THAN,
                   I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("a!=!b")),
            (Types{I_IDENTIFIER, O_DIFFERENT, O_NOT, I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("a||b|c")),
            (Types{I_IDENTIFIER, O_OR, I_IDENTIFIER, D_PIPE, I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("a&&b")), (Types{I_IDENTIFIER, O_AND, I_IDENTIFIER}));
}

TEST(LexerLex, DoubleDashIsAnEdgeNotTwoMinus) {
  EXPECT_EQ(types_of(lex("a--b")), (Types{I_IDENTIFIER, D_EDGE, I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("a - -b")),
            (Types{I_IDENTIFIER, O_MINUS, O_MINUS, I_IDENTIFIER}));
  EXPECT_EQ(types_of(lex("a---b")),
            (Types{I_IDENTIFIER, D_EDGE, O_MINUS, I_IDENTIFIER}));
}

TEST(LexerLex, Numbers) {
  EXPECT_EQ(types_of(lex("1 2.5 .5")), (Types{L_INT, L_FLOAT, L_FLOAT}));
  EXPECT_EQ(texts_of(lex("1 2.5 .5")), (Texts{"1", "2.5", ".5"}));
  EXPECT_EQ(types_of(lex("1+2")), (Types{L_INT, O_PLUS, L_INT}));
}

// ------------------------------------------------------------ lex(): locations

TEST(LexerLex, TokenLocationsPointIntoTheSourceFile) {
  auto file = make_file("val x = 42;");
  Lexer lexer;
  auto tokens = lexer.lex(file);
  ASSERT_EQ(tokens.size(), 5u);

  const std::uint32_t offsets[] = {0, 4, 6, 8, 10};
  const std::uint32_t lengths[] = {3, 1, 1, 2, 1};
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    SCOPED_TRACE(i);
    const SourceLocation loc = tokens[i].source_location();
    EXPECT_EQ(loc.offset(), offsets[i]);
    EXPECT_EQ(loc.length(), lengths[i]);
    EXPECT_EQ(loc.file(), file);
  }
}

TEST(LexerLex, OffsetsAcrossLines) {
  auto tokens = lex("a\nbb\n\n  ccc");
  ASSERT_EQ(tokens.size(), 3u);
  EXPECT_EQ(tokens[0].source_location().offset(), 0u);
  EXPECT_EQ(tokens[1].source_location().offset(), 2u);
  EXPECT_EQ(tokens[2].source_location().offset(), 8u);
  EXPECT_EQ(tokens[2].text(), "ccc");
}

TEST(LexerLex, CrlfLineEndings) {
  auto tokens = lex("val x\r\nval y\r\n");
  EXPECT_EQ(texts_of(tokens), (Texts{"val", "x", "val", "y"}));
  EXPECT_EQ(tokens[2].source_location().offset(), 7u);
}

TEST(LexerLex, TokensKeepTheFileAlive) {
  std::vector<Token> tokens;
  {
    auto file = make_file("val x");
    Lexer lexer;
    tokens = lexer.lex(file);
  }
  EXPECT_EQ(texts_of(tokens), (Texts{"val", "x"}));
}

TEST(LexerLex, TokensOfDifferentFilesStayIndependent) {
  auto a = make_file("x");
  auto b = make_file("y");
  Lexer lexer;
  auto ta = lexer.lex(a);
  auto tb = lexer.lex(b);
  EXPECT_EQ(ta[0].source_location().file(), a);
  EXPECT_EQ(tb[0].source_location().file(), b);
  EXPECT_EQ(ta[0].text(), "x");
  EXPECT_EQ(tb[0].text(), "y");
}

TEST(LexerLex, LexingTheSameFileTwiceGivesTheSameTokens) {
  auto file = make_file("val x = 1 + 2;");
  Lexer lexer;
  auto first = lexer.lex(file);
  auto second = lexer.lex(file);
  EXPECT_EQ(types_of(first), types_of(second));
  EXPECT_EQ(texts_of(first), texts_of(second));
}

TEST(LexerLex, ManyTokens) {
  std::string input;
  for (int i = 0; i < 5000; ++i) {
    input += "x ";
  }
  auto tokens = lex(input);
  ASSERT_EQ(tokens.size(), 5000u);
  EXPECT_EQ(tokens.back().source_location().offset(), 9998u);
}

// ------------------------------------------------------------------ lex():
// errors

TEST(LexerErrors, UnknownCharacterThrows) {
  EXPECT_THROW(lex("@"), std::runtime_error);
  EXPECT_THROW(lex("#"), std::runtime_error);
  EXPECT_THROW(lex("&"), std::runtime_error); // only "&&" exists
  EXPECT_THROW(lex("$"), std::runtime_error);
}

TEST(LexerErrors, MessageContainsThePositionOfTheBadCharacter) {
  //            0123456789
  expect_throws_mentioning("val x = @", "8");
  expect_throws_mentioning("x$", "1");
}

TEST(LexerErrors, ErrorAfterValidTokensStillThrows) {
  EXPECT_THROW(lex("val x = 1; #"), std::runtime_error);
  EXPECT_THROW(lex("a b c @ d"), std::runtime_error);
}

TEST(LexerErrors, NumberGluedToIdentifierThrows) {
  EXPECT_THROW(lex("2int"), std::runtime_error);
  EXPECT_THROW(lex("val x = 12abc;"), std::runtime_error);
  EXPECT_NO_THROW(lex("2 int"));
}

TEST(LexerErrors, NonAsciiBytesThrow) {
  EXPECT_THROW(lex("\xC3\xA9"), std::runtime_error);
  EXPECT_THROW(lex("caf\xC3\xA9"), std::runtime_error);
  EXPECT_THROW(lex("x \xA0 y"), std::runtime_error); // 0xA0 is not whitespace
}

TEST(LexerErrors, FormFeedAndVerticalTabCurrentlyCountAsWhitespace) {
  // Characterization: your lexer currently uses std::isspace, which accepts
  // these. Flip the expectation if you switch to an explicit " \t\r\n" check.
  EXPECT_NO_THROW(lex("a\fb"));
  EXPECT_NO_THROW(lex("a\vb"));
}

// ---------------------------------------------------------- injected rules

TEST(LexerRules, CustomRuleIsUsed) {
  Lexer lexer(std::vector<LexerRule>{literal("ab", K_VAL)});
  auto tokens = lexer.lex(make_file("ab ab"));
  EXPECT_EQ(types_of(tokens), (Types{K_VAL, K_VAL}));
  EXPECT_EQ(texts_of(tokens), (Texts{"ab", "ab"}));
}

TEST(LexerRules, OnlyCustomRulesAreActive) {
  Lexer lexer(std::vector<LexerRule>{literal("ab", K_VAL)});
  EXPECT_THROW(lexer.lex(make_file("val")), std::runtime_error);
}

TEST(LexerRules, NoRulesMeansAnyNonWhitespaceIsAnError) {
  Lexer lexer(std::vector<LexerRule>{});
  EXPECT_TRUE(lexer.lex(make_file("")).empty());
  EXPECT_TRUE(lexer.lex(make_file("  \n ")).empty());
  EXPECT_THROW(lexer.lex(make_file("x")), std::runtime_error);
}

TEST(LexerRules, LongestMatchWinsRegardlessOfRuleOrder) {
  const auto a = literal("a", K_IF);
  const auto ab = literal("ab", K_ELSE);
  const auto abc = literal("abc", K_THEN);
  for (const auto &order :
       {std::vector<LexerRule>{a, ab, abc}, std::vector<LexerRule>{abc, ab, a},
        std::vector<LexerRule>{ab, abc, a}}) {
    Lexer lexer(order);
    EXPECT_EQ(types_of(lexer.lex(make_file("a"))), (Types{K_IF}));
    EXPECT_EQ(types_of(lexer.lex(make_file("ab"))), (Types{K_ELSE}));
    EXPECT_EQ(types_of(lexer.lex(make_file("abc"))), (Types{K_THEN}));
  }
}

TEST(LexerRules, EarliestRuleWinsATie) {
  Lexer first_wins(
      std::vector<LexerRule>{literal("ab", K_IF), literal("ab", K_ELSE)});
  EXPECT_EQ(types_of(first_wins.lex(make_file("ab"))), (Types{K_IF}));

  Lexer other_first(
      std::vector<LexerRule>{literal("ab", K_ELSE), literal("ab", K_IF)});
  EXPECT_EQ(types_of(other_first.lex(make_file("ab"))), (Types{K_ELSE}));
}

TEST(LexerRules, ZeroLengthMatchIsNotAMatchAndCannotLoopForever) {
  Lexer lexer(
      std::vector<LexerRule>{[](SourceLocation) -> std::optional<LexerResult> {
        return LexerResult{0, K_VAL};
      }});
  EXPECT_THROW(lexer.lex(make_file("x")), std::runtime_error);
}

TEST(LexerRules, EveryRuleIsAskedAtEveryTokenPosition) {
  struct Call {
    std::uint32_t offset, length;
  };
  auto calls = std::make_shared<std::vector<Call>>();
  LexerRule recorder =
      [calls](SourceLocation loc) -> std::optional<LexerResult> {
    calls->push_back({loc.offset(), loc.length()});
    return LexerResult{1, K_VAL};
  };

  Lexer lexer(std::vector<LexerRule>{recorder});
  auto tokens = lexer.lex(make_file("ab c"));
  ASSERT_EQ(tokens.size(), 3u);

  // Each rule gets everything from the current position to the end of the file,
  // and whitespace is skipped before rules are consulted.
  ASSERT_EQ(calls->size(), 3u);
  EXPECT_EQ((*calls)[0].offset, 0u);
  EXPECT_EQ((*calls)[0].length, 4u);
  EXPECT_EQ((*calls)[1].offset, 1u);
  EXPECT_EQ((*calls)[1].length, 3u);
  EXPECT_EQ((*calls)[2].offset, 3u);
  EXPECT_EQ((*calls)[2].length, 1u);
}

TEST(LexerRules, DefaultConstructorUsesAllRules) {
  const std::string input =
      "val x: int = 1.5 + y -- z; if a == b then 2 else 3";
  Lexer defaults;
  Lexer explicit_rules(create_all_rules());
  auto a = defaults.lex(make_file(input));
  auto b = explicit_rules.lex(make_file(input));
  EXPECT_EQ(types_of(a), types_of(b));
  EXPECT_EQ(texts_of(a), texts_of(b));
}
