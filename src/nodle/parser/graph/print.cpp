
#include "nodle/parser/graph/graph.hpp"

using namespace nodle::parser;
using namespace nodle::parser::graph;

namespace {
struct GraphvizNode {
  std::string cluster;
  std::string label;
  std::string name;
};

struct GraphvizEdge {
  int source;
  int target;

  bool dotted;
  bool constrained;
  std::string label;
};
}; // namespace

std::ostream &operator<<(std::ostream &out,
                         const nodle::parser::graph::ParserGraph &graph) {
  std::vector<GraphvizEdge> edges;
  std::vector<GraphvizNode> nodes;
  std::map<std::string, std::vector<GraphvizNode>> nodes_per_cluster;

  for (int node = 0; node < graph.node_count(); node++) {
    GraphvizNode gNode;

    gNode.name = std::string("node") + std::to_string(node);
    gNode.cluster = graph.rule_of_node(node);

    auto &attrs = graph.attributes_of_node(node);
    gNode.label = attrs.size() == 0 ? "" : attrs[0];

    nodes_per_cluster[gNode.cluster].push_back(gNode);
    nodes.push_back(gNode);
  }

  auto to_node = [&](std::variant<int, std::string> node_or_label) {
    if (node_or_label.index() == 0) {
      return std::get<0>(node_or_label);
    } else {
      return graph.node_of_label(std::get<1>(node_or_label));
    }
  };

  for (const auto &edge : graph.all_edges()) {
    std::stringstream tok2string;
    tok2string << edge.token_type;

    edges.push_back({to_node(edge.source), to_node(edge.target),
                     edge.edge == EdgeKind::BACK_EDGE ? true : false,
                     edge.edge == EdgeKind::TOKEN,
                     edge.edge != EdgeKind::TOKEN ? std::string("epsilon")
                                                  : tok2string.str()});
  }

  out << "digraph Parser {\n";
  out << "  rankdir = \"TB\";\n";
  for (auto &[cluster_name, nodes] : nodes_per_cluster) {
    out << "  subgraph cluster_" << cluster_name << " {\n";
    out << "    color = black;\n";
    out << "    label = \"" << cluster_name << "\";\n";
    for (auto &node : nodes) {
      out << "    " << node.name << " [label=\"" << node.label << "\"];\n";
    }
    out << "  }\n";
  }
  for (auto &edge : edges) {
    out << "  " << nodes[edge.source].name << " -> " << nodes[edge.target].name
        << " [label=\"" << edge.label << "\"";
    if (edge.dotted) {
      out << ", style=dotted";
    }
    if (!edge.constrained) {
      out << ", weight=0";
    }
    out << "]\n";
  }
  out << "}\n";

  return out;
}
