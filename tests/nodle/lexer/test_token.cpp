#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"

using nodle::io::SourceFile;
using nodle::io::SourceLocation;
using nodle::lexer::Token;
using nodle::lexer::TokenType;

namespace {

SourceFile::Ptr make_file(const char *content) {
  return SourceFile::from_string("t.nodle", content);
}

} // namespace

TEST(Token, StoresTokenType) {
  auto file = make_file("foo 42");
  EXPECT_EQ(
      Token(TokenType::I_IDENTIFIER, SourceLocation(file, 0, 3)).token_type(),
      TokenType::I_IDENTIFIER);
  EXPECT_EQ(Token(TokenType::L_INT, SourceLocation(file, 4, 2)).token_type(),
            TokenType::L_INT);
}

TEST(Token, StoresSourceLocation) {
  auto file = make_file("foo 42");
  Token tok(TokenType::L_INT, SourceLocation(file, 4, 2));
  const SourceLocation loc = tok.source_location();
  EXPECT_EQ(loc.file(), file);
  EXPECT_EQ(loc.offset(), 4u);
  EXPECT_EQ(loc.length(), 2u);
}

TEST(Token, TextIsTheCoveredSlice) {
  auto file = make_file("foo 42");
  EXPECT_EQ(Token(TokenType::I_IDENTIFIER, SourceLocation(file, 0, 3)).text(),
            "foo");
  EXPECT_EQ(Token(TokenType::L_INT, SourceLocation(file, 4, 2)).text(), "42");
}

TEST(Token, ZeroLengthTokenHasEmptyText) {
  auto file = make_file("foo");
  Token eof(TokenType::END_OF_FILE, SourceLocation(file, 3, 0));
  EXPECT_TRUE(eof.text().empty());
  EXPECT_EQ(eof.source_location().offset(), 3u);
}

TEST(Token, TextMatchesSourceLocationText) {
  Token tok(TokenType::I_IDENTIFIER, SourceLocation(make_file("let x"), 4, 1));
  EXPECT_EQ(tok.text(), tok.source_location().text());
}

TEST(Token, TextStaysValidAfterOriginalFilePointerIsReleased) {
  // The token's location owns the file, so views into it must stay valid.
  auto file = make_file("hello world");
  Token tok(TokenType::I_IDENTIFIER, SourceLocation(file, 6, 5));
  file.reset();
  EXPECT_EQ(tok.text(), "world");
}

TEST(Token, CopyKeepsTypeAndLocation) {
  auto file = make_file("foo 42");
  Token a(TokenType::L_INT, SourceLocation(file, 4, 2));
  Token b = a;
  EXPECT_EQ(b.token_type(), TokenType::L_INT);
  EXPECT_EQ(b.text(), "42");
  EXPECT_EQ(b.source_location().file(), a.source_location().file());
}

TEST(Token, MoveKeepsTypeAndLocation) {
  auto file = make_file("foo 42");
  Token a(TokenType::L_INT, SourceLocation(file, 4, 2));
  Token b = std::move(a);
  EXPECT_EQ(b.token_type(), TokenType::L_INT);
  EXPECT_EQ(b.text(), "42");
}

TEST(Token, SourceLocationAccessorReturnsIndependentCopy) {
  // Mutating-by-union on the returned value must not affect the token.
  auto file = make_file("foo bar");
  Token tok(TokenType::I_IDENTIFIER, SourceLocation(file, 0, 3));
  SourceLocation merged = tok.source_location() | SourceLocation(file, 4, 3);
  EXPECT_EQ(merged.text(), "foo bar");
  EXPECT_EQ(tok.text(), "foo");
}

TEST(Token, TextThrowsWhenLocationIsOutOfRange) {
  Token tok(TokenType::I_IDENTIFIER, SourceLocation(make_file("abc"), 10, 1));
  EXPECT_THROW(tok.text(), std::out_of_range);
}

TEST(Token, UnionOfTokenLocationsSpansASequence) {
  auto file = make_file("let x = 1");
  Token first(TokenType::I_IDENTIFIER, SourceLocation(file, 0, 3));
  Token last(TokenType::L_INT, SourceLocation(file, 8, 1));
  EXPECT_EQ((first.source_location() | last.source_location()).text(),
            "let x = 1");
}