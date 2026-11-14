
#pragma once
#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"

namespace nodle::lexer {

enum TokenType {
  // ==== Literals ====
  L_INT,
  L_FLOAT,
  L_FALSE,
  L_TRUE,

  // ==== Identifiers ====
  I_IDENTIFIER,

  // ==== Keywords ====
  K_INT,   // int type
  K_FLOAT, // float type
  K_BOOL,  // boolean type

  K_VAL,

  // if statement
  K_IF,
  K_THEN,
  K_ELSE,

  // class utils
  K_SELF,
  K_CLASS,
  K_GRAPH,
  K_STATE,
  K_WEIGHT,
  K_EDGE,

  // proof context
  K_PROVE,
  K_WITH,
  K_PROOF,
  K_SUCH,
  K_THAT,
  K_FOLLOWS,
  K_EQUIVALENCE,
  K_PROPERTY,
  K_LEMMA,
  K_USE,

  // function definition
  K_DEF,
  K_OVERRIDE,

  // ==== OPERANDS ====
  O_NOT,
  O_PLUS,
  O_MINUS,
  O_TIMES,
  O_DIVIDE,
  O_MODULO,
  O_EQUAL,
  O_DIFFERENT,
  O_LESS_THAN,
  O_LESS_EQUAL,
  O_GREATER_THAN,
  O_GREATER_EQUAL,
  O_AND,
  O_OR,

  // ==== DELIMITERS ====
  D_LPAREN,
  D_RPAREN,
  D_DOT,
  D_COMMA,
  D_COLON,
  D_SEMICOLON,
  D_ASSIGN,
  D_EDGE,
  D_PIPE,

  // ==== MISC ====
  COMMENT,
  END_OF_FILE,
};

struct Token {
private:
  TokenType token_type_;

  io::SourceLocation source_location_;

public:
  /**
   * Create a token.
   *
   * @param token_type the type of token
   * @param source_location the location of the input
   */
  Token(TokenType token_type, io::SourceLocation source_location);

  /**
   * Returns the token's type.
   */
  TokenType token_type() const;
  /**
   * Returns where the token is located in the input file.
   */
  io::SourceLocation source_location() const;

  /**
   * Returns the text of the token.
   */
  std::string_view text() const;
};

}; // namespace nodle::lexer
