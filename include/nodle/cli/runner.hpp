
#pragma once
#include "nodle/ast/ast.hpp"
#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/lexer.hpp"
#include "nodle/lexer/token.hpp"

#include <argparse/argparse.hpp>

namespace nodle::cli {

struct RunnerCLI {
  /**
   * Called before the CLI arguments are parsed.
   *
   * @param parser the argument parser
   */
  virtual void pre_argument_parser(argparse::ArgumentParser &parser) {
    (void)parser;
  };
  /**
   * Called after the CLI arguments are parsed.
   *
   * @param parser the argument parser
   */
  virtual void post_argument_parser(argparse::ArgumentParser &parser) {
    (void)parser;
  };

  /**
   * Called when the lexer phase is done for a given file.
   *
   * @param file the source file of the tokens
   * @param tokens the list of tokens from the lexer
   * @return whether the pipeline may continue.
   */
  virtual bool on_lexer(const nodle::io::SourceFile::Ptr &file,
                        const nodle::lexer::Tokens &tokens) {
    (void)file;
    (void)tokens;
    return true;
  };

  /**
   * Called when the parser phase is done for a given file.
   *
   * @param file the source file of the program AST
   * @param program the program AST
   * @return whether the pipeline may continue.
   */
  virtual bool on_parser(const nodle::io::SourceFile::Ptr &file,
                         const nodle::ast::Program &program) {
    (void)file;
    (void)program;
    return true;
  }
};

}; // namespace nodle::cli
