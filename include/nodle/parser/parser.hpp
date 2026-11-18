
#pragma once

#include "nodle/ast/ast.hpp"
#include "nodle/parser/context.hpp"
#include "nodle/parser/graph/graph.hpp"
#include "nodle/parser/rules/basic.hpp"
#include "nodle/parser/rules/call.hpp"
#include "nodle/parser/rules/chain.hpp"
#include "nodle/parser/rules/option.hpp"
#include "nodle/parser/rules/or.hpp"
#include "nodle/parser/rules/token.hpp"

#include <memory>

namespace nodle::parser {

struct Parser {
  std::unique_ptr<nodle::ast::Program>
  parse(nodle::parser::ParserContextPtr context);

  graph::ParserGraph generate_graph();
};

}; // namespace nodle::parser
