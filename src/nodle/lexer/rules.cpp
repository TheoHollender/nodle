
#include "nodle/lexer/rules.hpp"
#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/token.hpp"

using namespace nodle::io;
using namespace nodle::lexer;
using namespace nodle::lexer::rules;

LexerRule nodle::lexer::rules::create_number_rule() {
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
LexerRule nodle::lexer::rules::create_string_rule(std::string keyword,
                                                  TokenType type) {
  return [keyword = std::move(keyword),
          type](SourceLocation location) -> std::optional<LexerResult> {
    if (location.text().substr(0, keyword.size()) != keyword) {
      return {};
    }

    return {{static_cast<uint32_t>(keyword.size()), type}};
  };
}
LexerRule nodle::lexer::rules::create_identifier_rule(TokenType type) {
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

LexerRule nodle::lexer::rules::create_comment_rule(std::string comment_start,
                                                   TokenType type) {
  return [comment_start = std::move(comment_start),
          type](SourceLocation location) -> std::optional<LexerResult> {
    std::string_view view = location.text();

    if (view.substr(0, comment_start.size()) != comment_start) {
      return {};
    }
    auto offset = static_cast<uint32_t>(comment_start.size());
    while (offset < view.size() && view[offset] != '\n') {
      offset++;
    }

    return {{offset, type, true}};
  };
}

std::vector<LexerRule> nodle::lexer::rules::create_all_rules() {
  return {
      create_number_rule(),

      create_string_rule("false", L_FALSE),
      create_string_rule("true", L_TRUE),

      create_string_rule("int", K_INT),
      create_string_rule("float", K_FLOAT),
      create_string_rule("bool", K_BOOL),

      create_string_rule("val", K_VAL),

      create_string_rule("if", K_IF),
      create_string_rule("then", K_THEN),
      create_string_rule("else", K_ELSE),

      create_string_rule("self", K_SELF),
      create_string_rule("class", K_CLASS),
      create_string_rule("graph", K_GRAPH),
      create_string_rule("state", K_STATE),
      create_string_rule("weight", K_WEIGHT),
      create_string_rule("edge", K_EDGE),

      create_string_rule("prove", K_PROVE),
      create_string_rule("with", K_WITH),
      create_string_rule("proof", K_PROOF),
      create_string_rule("such", K_SUCH),
      create_string_rule("that", K_THAT),
      create_string_rule("follows", K_FOLLOWS),
      create_string_rule("equivalence", K_EQUIVALENCE),
      create_string_rule("property", K_PROPERTY),
      create_string_rule("lemma", K_LEMMA),
      create_string_rule("use", K_USE),

      create_string_rule("def", K_DEF),
      create_string_rule("override", K_OVERRIDE),

      create_string_rule("!", O_NOT),
      create_string_rule("+", O_PLUS),
      create_string_rule("-", O_MINUS),
      create_string_rule("*", O_TIMES),
      create_string_rule("/", O_DIVIDE),
      create_string_rule("%", O_MODULO),
      create_string_rule("==", O_EQUAL),
      create_string_rule("!=", O_DIFFERENT),
      create_string_rule("<", O_LESS_THAN),
      create_string_rule("<=", O_LESS_EQUAL),
      create_string_rule(">", O_GREATER_THAN),
      create_string_rule(">=", O_GREATER_EQUAL),
      create_string_rule("&&", O_AND),
      create_string_rule("||", O_OR),

      create_string_rule("(", D_LPAREN),
      create_string_rule(")", D_RPAREN),
      create_string_rule(".", D_DOT),
      create_string_rule(",", D_COMMA),
      create_string_rule(":", D_COLON),
      create_string_rule(";", D_SEMICOLON),
      create_string_rule("=", D_ASSIGN),
      create_string_rule("--", D_EDGE),
      create_string_rule("|", D_PIPE),

      create_comment_rule("//", COMMENT),
      create_identifier_rule(I_IDENTIFIER),
  };
}
