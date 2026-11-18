
#pragma once
#include "nodle/lexer/token.hpp"
#include <variant>
#include <string>
#include <map>

namespace nodle::parser::graph {

enum EdgeKind {
    EPSILON, BACK_EDGE, TOKEN
};

struct Edge {
    /* index of the source */
    std::variant<int, std::string> source;
    /* kind of edge */
    EdgeKind edge;
    /* either index or label */
    std::variant<int, std::string> target;
    /* token type */
    nodle::lexer::TokenType token_type;
private:
    Edge (std::variant<int, std::string> source, EdgeKind edge, std::variant<int, std::string> target, nodle::lexer::TokenType token_type)
        : source(source), edge(edge), target(target), token_type(token_type) {}
public:
    static Edge epsilon (std::variant<int, std::string> source, std::variant<int, std::string> target) {
        return Edge(source, EPSILON, target, lexer::END_OF_FILE);
    }
    static Edge back_edge (std::variant<int, std::string> source, std::variant<int, std::string> target) {
        return Edge(source, BACK_EDGE, target, lexer::END_OF_FILE);
    }
    static Edge token (std::variant<int, std::string> source, std::variant<int, std::string> target, lexer::TokenType token_type) {
        return Edge(source, TOKEN, target, token_type);
    }
};

struct Subgraph {
    int start_node;
    int end_node;

    Subgraph (int start_node, int end_node) : start_node(start_node), end_node(end_node) {}
};

struct ParserGraph {
private:
    std::map<std::string, int> label_to_index_;
    std::vector<Edge> edges_;
    std::vector<std::string> rules_;
    std::vector<std::vector<std::string>> attributes_;
    int nb_nodes_ = 0;
public:
    int  allocate_node (std::string rule);
    void add_label (int node, std::string label);
    void add_attribute (int node, std::string attribute);
    void add_edge (Edge edge);

    int node_count () const;
    int node_of_label (std::string label) const;
    std::string rule_of_node (int node) const;
    const std::vector<std::string>& attributes_of_node (int node) const;
    const std::vector<Edge>& all_edges () const;
};

};
