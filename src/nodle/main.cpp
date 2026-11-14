#include <argparse/argparse.hpp>
#include <filesystem>
#include <iostream>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/lexer/lexer.hpp"

#include "nodle/cli/debug_lexer.hpp"
#include "nodle/cli/runner.hpp"

std::vector<std::shared_ptr<nodle::cli::RunnerCLI>> runners = {
    std::make_shared<nodle::cli::DebugLexerCLI>()};

int main(int argc, char *argv[]) {
  argparse::ArgumentParser program("Nodle", "1.0.0");

  program.add_argument("files").remaining().help(
      "List of input source files (.nodle)");

  for (auto &runner : runners) {
    runner->pre_argument_parser(program);
  }

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    exit(1);
  }

  std::vector<nodle::io::SourceFile::Ptr> files;
  if (program.present("files")) {
    auto files_strs = program.get<std::vector<std::string>>("files");

    for (std::string &file : files_strs) {
      files.push_back(std::move(nodle::io::SourceFile::load(file)));
    }
  }

  if (files.size() == 0) {
    std::cerr << "Error: You must provide at least 1 input file.\n\n";
    std::cerr << program;
    exit(1);
  }

  for (auto &runner : runners) {
    runner->post_argument_parser(program);
  }

  nodle::lexer::Lexer lexer;

  for (const nodle::io::SourceFile::Ptr &file : files) {
    auto tokens = lexer.lex(file);

    bool continue_pipeline = true;

    for (auto &runner : runners) {
      continue_pipeline &= runner->on_lexer(file, tokens);
    }

    if (!continue_pipeline) {
      continue;
    }
  }

  return 0;
}