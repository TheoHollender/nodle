
#include "nodle/parser/graph/graph.hpp"

using namespace nodle::parser::graph;

int ParserGraph::allocate_node (std::string rule) {
    int uuid = rules_.size();

    rules_.push_back(rule);
    attributes_.emplace_back();

    nb_nodes_ ++;

    return uuid;
}
void ParserGraph::add_label (int node, std::string label) {
    if (label_to_index_.find(label) != label_to_index_.end()) {
        throw std::runtime_error("label already exists.");
    }

    label_to_index_[label] = node;
}
void ParserGraph::add_attribute (int node, std::string attribute) {
    attributes_.at(node).push_back(attribute);
}
void ParserGraph::add_edge (Edge edge) {
    edges_.push_back(edge);
}

int ParserGraph::node_count () const {
    return nb_nodes_;
}
int ParserGraph::node_of_label (std::string label) const {
    try {
        return label_to_index_.at(label);
    } catch (std::out_of_range &err) {
        throw std::out_of_range("node with label '" + label + "' does not exist");
    }
}
std::string ParserGraph::rule_of_node (int node) const {
    return rules_.at(node);
}
const std::vector<std::string>& ParserGraph::attributes_of_node (int node) const {
    return attributes_.at(node);
}
const std::vector<Edge>& ParserGraph::all_edges () const {
    return edges_;
}
