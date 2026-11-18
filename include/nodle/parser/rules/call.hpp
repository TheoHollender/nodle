
#pragma once

#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"

namespace nodle::parser::rules {

template <typename RV, ParseResult<RV> (*Func)(ParserContextPtr)>
struct CallRule : public BasicRuleTrait<CallRule<RV, Func>, RV> {
private:
  std::string rule_name_;

public:
  using return_value = RV;

  CallRule(std::string rule_name) : rule_name_(rule_name) {}

  ParseResult<RV> parse(ParserContextPtr ctx) const { return Func(ctx); }

  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    int start = graph.allocate_node(rule);
    int end = graph.allocate_node(rule);

    graph.add_edge(graph::Edge::epsilon(start, rule_name_ + "#start"));
    graph.add_edge(graph::Edge::epsilon(rule_name_ + "#end", end));

    return graph::Subgraph(start, end);
  }
};

} // namespace nodle::parser::rules
