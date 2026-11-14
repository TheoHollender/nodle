
#include "nodle/lexer/rules.hpp"
#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"

using namespace nodle::io;
using namespace nodle::lexer;
using namespace nodle::lexer::rules;

LexerRule nodle::lexer::rules::createNumberRule() {
  return [](SourceLocation location) -> std::optional<LexerResult> {
    std::string_view view = location.text();

    uint32_t offset = 0;
    bool had_digit = false;
    bool had_dot = false;
    while (offset < view.length() && std::isdigit(view[offset])) {
      offset++;
      had_digit = true;
    }

    if (offset < view.length() && view[offset] == '.') {
      offset++;
      had_dot = true;

      while (offset < view.length() && std::isdigit(view[offset])) {
        offset++;
        had_digit = true;
      }
    }

    if (!had_digit) {
      return {};
    }

    if (offset < view.length() &&
        (view[offset] == '_' || std::islower(view[offset]) ||
         std::isupper(view[offset]))) {
      return {};
    }

    return {{offset, had_dot ? L_FLOAT : L_INT}};
  };
}
LexerRule nodle::lexer::rules::createStringRule(std::string keyword,
                                                TokenType type) {
  return [keyword = std::move(keyword),
          type](SourceLocation location) -> std::optional<LexerResult> {
    if (location.text().substr(0, keyword.size()) != keyword) {
      return {};
    }

    return {{static_cast<uint32_t>(keyword.size()), type}};
  };
}
LexerRule nodle::lexer::rules::createIdentifierRule(TokenType type) {
  return [type](SourceLocation location) -> std::optional<LexerResult> {
    std::string_view view = location.text();

    std::function<bool(uint32_t)> is_valid = [&](uint32_t offset) {
      char chr = view[offset];

      return (std::isdigit(chr) && offset != 0) || (std::islower(chr)) ||
             (std::isupper(chr)) || (chr == '_');
    };

    uint32_t offset = 0;
    while (offset < view.size() && is_valid(offset)) {
      offset++;
    }

    if (offset == 0) {
      return {};
    }

    return {{offset, type}};
  };
}

std::vector<LexerRule> nodle::lexer::rules::createAllRules() {
  return {
      createNumberRule(),

      createStringRule("false", L_FALSE),
      createStringRule("true", L_TRUE),

      createStringRule("int", K_INT),
      createStringRule("float", K_FLOAT),
      createStringRule("bool", K_BOOL),

      createStringRule("val", K_VAL),

      createStringRule("if", K_IF),
      createStringRule("then", K_THEN),
      createStringRule("else", K_ELSE),

      createStringRule("self", K_SELF),
      createStringRule("class", K_CLASS),
      createStringRule("graph", K_GRAPH),
      createStringRule("state", K_STATE),
      createStringRule("weight", K_WEIGHT),
      createStringRule("edge", K_EDGE),

      createStringRule("prove", K_PROVE),
      createStringRule("with", K_WITH),
      createStringRule("proof", K_PROOF),
      createStringRule("such", K_SUCH),
      createStringRule("that", K_THAT),
      createStringRule("follows", K_FOLLOWS),
      createStringRule("equivalence", K_EQUIVALENCE),
      createStringRule("property", K_PROPERTY),
      createStringRule("lemma", K_LEMMA),
      createStringRule("use", K_USE),

      createStringRule("def", K_DEF),
      createStringRule("override", K_OVERRIDE),

      createStringRule("!", O_NOT),
      createStringRule("+", O_PLUS),
      createStringRule("-", O_MINUS),
      createStringRule("*", O_TIMES),
      createStringRule("/", O_DIVIDE),
      createStringRule("%", O_MODULO),
      createStringRule("==", O_EQUAL),
      createStringRule("!=", O_DIFFERENT),
      createStringRule("<", O_LESS_THAN),
      createStringRule("<=", O_LESS_EQUAL),
      createStringRule(">", O_GREATER_THAN),
      createStringRule(">=", O_GREATER_EQUAL),
      createStringRule("&&", O_AND),
      createStringRule("||", O_OR),

      createStringRule("(", D_LPAREN),
      createStringRule(")", D_RPAREN),
      createStringRule(".", D_DOT),
      createStringRule(",", D_COMMA),
      createStringRule(":", D_COLON),
      createStringRule(";", D_SEMICOLON),
      createStringRule("=", D_ASSIGN),
      createStringRule("--", D_EDGE),
      createStringRule("|", D_PIPE),

      createIdentifierRule(I_IDENTIFIER),
  };
}
