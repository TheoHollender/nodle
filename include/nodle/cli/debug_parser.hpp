
#include "nodle/ast/printer.hpp"
#include "nodle/cli/runner.hpp"
#include "nodle/parser/graph/print.hpp"

namespace nodle::cli {

struct DebugParserCLI : public RunnerCLI {
private:
  bool debug_parser_enabled = false;
  bool verbose_source_location = false;
  bool show_parser_graph = false;

public:
  void pre_argument_parser(argparse::ArgumentParser &parser) override {
    parser.add_argument("-Dp", "--debug-parser")
        .help("Debug the parser stage, printing the AST")
        .flag();
    parser.add_argument("-Vp", "--verbose-parser-source")
        .help("Print the source locations of AST nodes.")
        .flag();
    parser.add_argument("-Spg", "--show-parser-graph")
        .help("Generate parser graph and output it on standard output.")
        .flag();
  };
  void post_argument_parser(argparse::ArgumentParser &parser) override {
    if (parser.get<bool>("-Dp"))
      debug_parser_enabled = true;
    if (parser.get<bool>("-Vp")) {
      verbose_source_location = true;
      if (!debug_parser_enabled) {
        std::cerr << "warning: option --verbose-parser-source/-Vp is useless "
                     "without option --debug-parser/-Dp\n";
      }
    }
    if (parser.get<bool>("-Spg")) {
      show_parser_graph = true;

      std::cerr << "Generating parser graph...\n";
      nodle::parser::Parser parser;
      nodle::parser::graph::ParserGraph graph = parser.generate_graph();
      std::cout << graph;
    
      exit(0);
    }
  };

  bool on_parser(const nodle::io::SourceFile::Ptr &file,
                 const nodle::ast::Program &program) {
    (void)file;

    if (!debug_parser_enabled) {
      return true;
    }

    if (verbose_source_location) {
      std::cout << "AST of " << file->path().string() << ": \n";
      nodle::ast::BaseAstPrinter baseAstPrinter(std::cout, true, 1);
      nodle::ast::SourceAstPrinter sourceAstPrinter(baseAstPrinter);
      sourceAstPrinter.print(&program);
    } else {
      std::cout << "AST of " << file->path().string() << ": \n";

      nodle::ast::BaseAstPrinter baseAstPrinter(std::cout);
      program.accept(baseAstPrinter);
    }

    return false;
  }
};

} // namespace nodle::cli
