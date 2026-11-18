
#pragma once
#include "nodle/parser/graph/graph.hpp"
#include <iostream>

std::ostream &operator<<(std::ostream &out,
                         const nodle::parser::graph::ParserGraph &graph);
