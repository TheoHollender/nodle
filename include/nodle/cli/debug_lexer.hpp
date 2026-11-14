
#include "nodle/cli/runner.hpp"

namespace nodle::cli {

struct DebugLexerCLI : public RunnerCLI {
private:
  bool debug_lexer_enabled = false;
  bool verbose_source_location = false;

public:
  void pre_argument_parser(argparse::ArgumentParser &parser) override {
    parser.add_argument("-Dl", "--debug-lexer")
        .help("Debug the lexer stage, printing the entire token stream token "
              "by token")
        .flag();
    parser.add_argument("-Vl", "--verbose-lexer-source")
        .help("Print the source locations of tokens")
        .flag();
  };
  void post_argument_parser(argparse::ArgumentParser &parser) override {
    if (parser.get<bool>("-Dl"))
      debug_lexer_enabled = true;
    if (parser.get<bool>("-Vl")) {
      verbose_source_location = true;
      if (!debug_lexer_enabled) {
        std::cerr << "warning: option --verbose-lexer-source/-Vl is useless "
                     "without option --debug-lexer/-Dl\n";
      }
    }
  };

  bool on_lexer(const nodle::io::SourceFile::Ptr &file,
                const nodle::lexer::Tokens &tokens) override {
    if (!debug_lexer_enabled) {
      return true;
    }

    if (verbose_source_location) {
      std::cout << "Token Stream of " << file->path().string() << ": " << "\n";
      for (auto token : tokens) {
        std::cout << token.source_location().as_debug_string(true)
                  << "token type = " << token.token_type() << "\n\n";
      }
    } else {
      std::cout << "Token Stream of " << file->path().string() << ": \n";
      for (auto token : tokens) {
        std::cout << token.token_type() << ":\"" << token.text() << "\" ";
      }

      std::cout << "\n";
    }

    return false;
  };
};

} // namespace nodle::cli
