#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"
#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"
#include "nodle/parser/rules/token.hpp"

using nodle::io::SourceFile;
using nodle::io::SourceLocation;
using nodle::lexer::Token;
using nodle::lexer::TokenType;
using nodle::parser::ParserContext;
using nodle::parser::ParserContextPtr;
using nodle::parser::rules::BasicRule;
using nodle::parser::rules::ParseResult;
using nodle::parser::rules::TokenRule;
using enum nodle::lexer::TokenType;

namespace {

using Status = ParseResult<Token>::Status;

// The tokens of "val x = 42;"
const std::vector<TokenType> kTypes = {K_VAL, I_IDENTIFIER, D_ASSIGN, L_INT, D_SEMICOLON};

std::vector<Token> sample_tokens(const SourceFile::Ptr &file) {
    return {
        Token(K_VAL, SourceLocation(file, 0, 3)),
        Token(I_IDENTIFIER, SourceLocation(file, 4, 1)),
        Token(D_ASSIGN, SourceLocation(file, 6, 1)),
        Token(L_INT, SourceLocation(file, 8, 2)),
        Token(D_SEMICOLON, SourceLocation(file, 10, 1)),
    };
}

SourceFile::Ptr sample_file() { return SourceFile::from_string("t.nodle", "val x = 42;"); }

ParserContextPtr sample_context(const SourceFile::Ptr &file) {
    return std::make_shared<ParserContext>(sample_tokens(file));
}

ParserContextPtr context_of(const std::vector<TokenType> &types) {
    auto file = SourceFile::from_string("t.nodle", std::string(types.size() + 1, 'x'));
    std::vector<Token> tokens;
    for (std::uint32_t i = 0; i < types.size(); ++i) {
        tokens.emplace_back(types[i], SourceLocation(file, i, 1));
    }
    return std::make_shared<ParserContext>(std::move(tokens));
}

} // namespace

// ------------------------------------------------------------- the rule's contract

static_assert(BasicRule<TokenRule>, "TokenRule must satisfy the BasicRule concept");
static_assert(std::is_same_v<TokenRule::return_type, Token>);

// ------------------------------------------------------------------- matching

TEST(TokenRule, MatchesATokenOfTheExpectedType) {
    auto ctx = sample_context(sample_file());
    TokenRule rule(K_VAL);
    auto result = rule.parse(ctx);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_FALSE(result.is_no_match());
}

TEST(TokenRule, AMatchConsumesTokens) {
    auto ctx = sample_context(sample_file());
    auto result = TokenRule(K_VAL).parse(ctx);
    EXPECT_TRUE(result.is_match_consumed());
    EXPECT_FALSE(result.is_match_zero());
    EXPECT_EQ(result.status(), Status::MatchConsumed);
}

TEST(TokenRule, ReturnsTheMatchedToken) {
    auto file = sample_file();
    auto ctx = sample_context(file);
    auto result = TokenRule(K_VAL).parse(ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->token_type(), K_VAL);
    EXPECT_EQ(result->text(), "val");
    EXPECT_EQ(result->source_location().file(), file);
    EXPECT_EQ(result->source_location().offset(), 0u);
    EXPECT_EQ(result->source_location().length(), 3u);
}

TEST(TokenRule, MatchAdvancesTheContextByExactlyOneToken) {
    auto ctx = sample_context(sample_file());
    TokenRule(K_VAL).parse(ctx);
    ASSERT_TRUE(ctx->remaining());
    EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
    EXPECT_EQ(ctx->peek().text(), "x");
}

TEST(TokenRule, MatchesAtTheCurrentPositionNotTheStart) {
    auto ctx = sample_context(sample_file());
    ctx->pop();
    ctx->pop();
    auto result = TokenRule(D_ASSIGN).parse(ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->text(), "=");
    EXPECT_EQ(ctx->peek().token_type(), L_INT);
}

TEST(TokenRule, MatchesTheLastToken) {
    auto ctx = context_of({K_VAL, D_SEMICOLON});
    ctx->pop();
    auto result = TokenRule(D_SEMICOLON).parse(ctx);
    EXPECT_TRUE(result.is_match_consumed());
    EXPECT_FALSE(ctx->remaining());
}

// ------------------------------------------------------------------ not matching

TEST(TokenRule, DoesNotMatchADifferentTokenType) {
    auto ctx = sample_context(sample_file());
    auto result = TokenRule(D_SEMICOLON).parse(ctx);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_TRUE(result.is_no_match());
    EXPECT_EQ(result.status(), Status::NoMatch);
}

TEST(TokenRule, NoMatchLeavesTheContextUntouched) {
    auto ctx = sample_context(sample_file());
    TokenRule rule(D_SEMICOLON);
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(rule.parse(ctx).is_no_match());
        ASSERT_TRUE(ctx->remaining());
        EXPECT_EQ(ctx->peek().token_type(), K_VAL);
    }
}

TEST(TokenRule, DoesNotSkipOverNonMatchingTokens) {
    // ';' exists later in the stream, but the rule only looks at the current token.
    auto ctx = sample_context(sample_file());
    EXPECT_TRUE(TokenRule(D_SEMICOLON).parse(ctx).is_no_match());
    EXPECT_EQ(ctx->peek().text(), "val");
}

TEST(TokenRule, AFailedRuleDoesNotPreventALaterRuleFromMatching) {
    auto ctx = sample_context(sample_file());
    EXPECT_TRUE(TokenRule(L_INT).parse(ctx).is_no_match());
    EXPECT_TRUE(TokenRule(K_VAL).parse(ctx).is_match_consumed());
}

TEST(TokenRule, OnlyTheExactTypeMatchesAtEveryPosition) {
    const std::vector<TokenType> all = {K_VAL, I_IDENTIFIER, D_ASSIGN, L_INT, D_SEMICOLON, K_IF};
    for (std::size_t position = 0; position < kTypes.size(); ++position) {
        for (TokenType candidate : all) {
            SCOPED_TRACE("position " + std::to_string(position) + ", candidate " +
                         std::to_string(static_cast<int>(candidate)));
            auto ctx = sample_context(sample_file());
            for (std::size_t i = 0; i < position; ++i) {
                ctx->pop();
            }
            auto result = TokenRule(candidate).parse(ctx);
            EXPECT_EQ(result.has_value(), candidate == kTypes[position]);
        }
    }
}

// ------------------------------------------------------------- running out of tokens

TEST(TokenRule, EmptyContextGivesNoMatch) {
    auto ctx = std::make_shared<ParserContext>(std::vector<Token>{});
    auto result = TokenRule(K_VAL).parse(ctx);
    EXPECT_TRUE(result.is_no_match());
    EXPECT_FALSE(ctx->remaining());
}

TEST(TokenRule, ExhaustedContextGivesNoMatch) {
    auto ctx = context_of({K_VAL});
    EXPECT_TRUE(TokenRule(K_VAL).parse(ctx).is_match_consumed());
    ASSERT_FALSE(ctx->remaining());

    EXPECT_TRUE(TokenRule(K_VAL).parse(ctx).is_no_match());
    EXPECT_TRUE(TokenRule(D_SEMICOLON).parse(ctx).is_no_match());
    EXPECT_FALSE(ctx->remaining());
}

TEST(TokenRule, RepeatedFailuresAtTheEndStayNoMatch) {
    auto ctx = context_of({});
    TokenRule rule(K_VAL);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(rule.parse(ctx).is_no_match());
    }
}

// ------------------------------------------------------------------- sequences

TEST(TokenRule, ParsesAWholeStatementWithOneRulePerToken) {
    auto ctx = sample_context(sample_file());
    std::vector<std::string> texts;
    for (TokenType type : kTypes) {
        auto result = TokenRule(type).parse(ctx);
        ASSERT_TRUE(result.has_value()) << "type " << static_cast<int>(type);
        texts.emplace_back(result->text());
    }
    EXPECT_EQ(texts, (std::vector<std::string>{"val", "x", "=", "42", ";"}));
    EXPECT_FALSE(ctx->remaining());
}

TEST(TokenRule, OneRuleObjectCanBeReusedForConsecutiveTokens) {
    auto ctx = context_of({I_IDENTIFIER, I_IDENTIFIER, I_IDENTIFIER, K_VAL});
    TokenRule rule(I_IDENTIFIER);
    EXPECT_TRUE(rule.parse(ctx).is_match_consumed());
    EXPECT_TRUE(rule.parse(ctx).is_match_consumed());
    EXPECT_TRUE(rule.parse(ctx).is_match_consumed());
    EXPECT_TRUE(rule.parse(ctx).is_no_match()); // next token is K_VAL
    EXPECT_EQ(ctx->peek().token_type(), K_VAL);
}

TEST(TokenRule, OneRuleObjectCanBeUsedOnDifferentContexts) {
    TokenRule rule(K_VAL);
    auto first = context_of({K_VAL});
    auto second = context_of({K_VAL});
    EXPECT_TRUE(rule.parse(first).is_match_consumed());
    EXPECT_TRUE(second->remaining()); // untouched by the first parse
    EXPECT_TRUE(rule.parse(second).is_match_consumed());
}

TEST(TokenRule, ConsumesThroughTheSharedContextPointer) {
    auto ctx = sample_context(sample_file());
    auto alias = ctx;
    TokenRule(K_VAL).parse(alias);
    EXPECT_EQ(ctx->peek().token_type(), I_IDENTIFIER);
}

TEST(TokenRule, ReturnedTokenIsIndependentOfLaterParsing) {
    auto ctx = sample_context(sample_file());
    auto first = TokenRule(K_VAL).parse(ctx);
    TokenRule(I_IDENTIFIER).parse(ctx);
    TokenRule(D_ASSIGN).parse(ctx);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->text(), "val");
}

TEST(TokenRule, ZeroLengthTokensMatchLikeAnyOther) {
    auto file = sample_file();
    std::vector<Token> tokens = {Token(K_VAL, SourceLocation(file, 11, 0))}; // e.g. an EOF token
    auto ctx = std::make_shared<ParserContext>(std::move(tokens));
    auto result = TokenRule(K_VAL).parse(ctx);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->text().empty());
}