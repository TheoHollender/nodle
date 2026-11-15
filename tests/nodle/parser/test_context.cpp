#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"
#include "nodle/parser/context.hpp"

using nodle::io::SourceFile;
using nodle::io::SourceLocation;
using nodle::lexer::Token;
using nodle::lexer::TokenType;
using nodle::parser::ParserContext;
using nodle::parser::ParserContextPtr;
using enum nodle::lexer::TokenType;

namespace {

// The tokens of "val x = 42;"
//   val = 0..3   x = 4   '=' = 6   42 = 8..10   ';' = 10
const std::vector<TokenType> kTypes = {K_VAL, I_IDENTIFIER, D_ASSIGN, L_INT,
                                       D_SEMICOLON};
const std::vector<std::string> kTexts = {"val", "x", "=", "42", ";"};

SourceFile::Ptr sample_file() {
  return SourceFile::from_string("t.nodle", "val x = 42;");
}

std::vector<Token> sample_tokens(const SourceFile::Ptr &file) {
  return {
      Token(K_VAL, SourceLocation(file, 0, 3)),
      Token(I_IDENTIFIER, SourceLocation(file, 4, 1)),
      Token(D_ASSIGN, SourceLocation(file, 6, 1)),
      Token(L_INT, SourceLocation(file, 8, 2)),
      Token(D_SEMICOLON, SourceLocation(file, 10, 1)),
  };
}

} // namespace

TEST(ParserContext, EmptyContextHasNothingRemaining) {
  ParserContext ctx({});
  EXPECT_FALSE(ctx.remaining());
}

TEST(ParserContext, NonEmptyContextHasTokensRemaining) {
  ParserContext ctx(sample_tokens(sample_file()));
  EXPECT_TRUE(ctx.remaining());
}

TEST(ParserContext, PeekReturnsTheFirstTokenWithoutConsumingIt) {
  ParserContext ctx(sample_tokens(sample_file()));
  EXPECT_EQ(ctx.peek().token_type(), K_VAL);
  EXPECT_EQ(ctx.peek().text(), "val");
  // Peeking again gives the same token: nothing was consumed.
  EXPECT_EQ(ctx.peek().token_type(), K_VAL);
  EXPECT_TRUE(ctx.remaining());
}

TEST(ParserContext, PopMovesToTheNextToken) {
  ParserContext ctx(sample_tokens(sample_file()));
  ctx.pop();
  EXPECT_EQ(ctx.peek().token_type(), I_IDENTIFIER);
  EXPECT_EQ(ctx.peek().text(), "x");
  ctx.pop();
  EXPECT_EQ(ctx.peek().token_type(), D_ASSIGN);
}

TEST(ParserContext, WalkingThroughAllTokensVisitsThemInOrder) {
  ParserContext ctx(sample_tokens(sample_file()));
  std::vector<TokenType> types;
  std::vector<std::string> texts;
  while (ctx.remaining()) {
    types.push_back(ctx.peek().token_type());
    texts.emplace_back(ctx.peek().text());
    ctx.pop();
  }
  EXPECT_EQ(types, kTypes);
  EXPECT_EQ(texts, kTexts);
}

TEST(ParserContext, NothingRemainsAfterTheLastTokenIsPopped) {
  ParserContext ctx(sample_tokens(sample_file()));
  for (std::size_t i = 0; i < kTypes.size(); ++i) {
    EXPECT_TRUE(ctx.remaining()) << "before pop " << i;
    ctx.pop();
  }
  EXPECT_FALSE(ctx.remaining());
}

TEST(ParserContext, LastTokenIsStillRemainingBeforeItIsPopped) {
  ParserContext ctx(sample_tokens(sample_file()));
  for (std::size_t i = 0; i + 1 < kTypes.size(); ++i) {
    ctx.pop();
  }
  EXPECT_TRUE(ctx.remaining());
  EXPECT_EQ(ctx.peek().token_type(), D_SEMICOLON);
}

TEST(ParserContext, PoppingPastTheEndKeepsNothingRemaining) {
  ParserContext ctx(sample_tokens(sample_file()));
  for (int i = 0; i < 20; ++i) {
    ctx.pop();
  }
  EXPECT_FALSE(ctx.remaining());
}

TEST(ParserContext, PoppingAnEmptyContextKeepsNothingRemaining) {
  ParserContext ctx({});
  ctx.pop();
  EXPECT_FALSE(ctx.remaining());
}

TEST(ParserContext, SingleTokenContext) {
  auto file = sample_file();
  ParserContext ctx({Token(K_VAL, SourceLocation(file, 0, 3))});
  EXPECT_TRUE(ctx.remaining());
  EXPECT_EQ(ctx.peek().text(), "val");
  ctx.pop();
  EXPECT_FALSE(ctx.remaining());
}

TEST(ParserContext, PeekedTokensKeepTheirSourceLocation) {
  auto file = sample_file();
  ParserContext ctx(sample_tokens(file));
  ctx.pop();
  ctx.pop();
  ctx.pop(); // "42"
  const SourceLocation loc = ctx.peek().source_location();
  EXPECT_EQ(loc.file(), file);
  EXPECT_EQ(loc.offset(), 8u);
  EXPECT_EQ(loc.length(), 2u);
}

TEST(ParserContext, PeekedTokenOutlivesThePop) {
  ParserContext ctx(sample_tokens(sample_file()));
  const Token first = ctx.peek();
  ctx.pop();
  ctx.pop();
  EXPECT_EQ(first.token_type(), K_VAL);
  EXPECT_EQ(first.text(), "val");
}

TEST(ParserContext, KeepsItsOwnCopyOfTheTokens) {
  auto file = sample_file();
  std::vector<Token> tokens = sample_tokens(file);
  ParserContext ctx(tokens);
  tokens.clear();
  EXPECT_EQ(ctx.peek().text(), "val");
  ctx.pop();
  EXPECT_EQ(ctx.peek().text(), "x");
}

TEST(ParserContext, KeepsTheSourceFileAliveThroughItsTokens) {
  ParserContext ctx(
      sample_tokens(sample_file())); // the only other owner is the tokens
  ctx.pop();
  EXPECT_EQ(ctx.peek().text(), "x");
}

TEST(ParserContext, IndependentContextsDoNotShareProgress) {
  auto file = sample_file();
  ParserContext a(sample_tokens(file));
  ParserContext b(sample_tokens(file));
  a.pop();
  a.pop();
  EXPECT_EQ(a.peek().token_type(), D_ASSIGN);
  EXPECT_EQ(b.peek().token_type(), K_VAL);
}

TEST(ParserContext, SharedPointerHandlesShareProgress) {
  ParserContextPtr ctx =
      std::make_shared<ParserContext>(sample_tokens(sample_file()));
  ParserContextPtr alias = ctx;
  alias->pop();
  EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
  ctx->pop();
  EXPECT_EQ(alias->peek().token_type(), D_ASSIGN);
}

TEST(ParserContext, ContextOfALexedFile) {
  // End to end with the real lexer output would need nodle::lexer::Lexer; here
  // we only check that a context built from many tokens stays consistent.
  auto file = SourceFile::from_string("t.nodle", std::string(1000, 'x'));
  std::vector<Token> tokens;
  for (std::uint32_t i = 0; i < 1000; ++i) {
    tokens.emplace_back(I_IDENTIFIER, SourceLocation(file, i, 1));
  }
  ParserContext ctx(std::move(tokens));
  std::uint32_t count = 0;
  while (ctx.remaining()) {
    EXPECT_EQ(ctx.peek().source_location().offset(), count);
    ctx.pop();
    ++count;
  }
  EXPECT_EQ(count, 1000u);
}