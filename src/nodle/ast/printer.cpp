#include "nodle/ast/printer.hpp"

#include <sstream>
#include <string_view>

using namespace nodle::ast;

namespace {

std::string_view symbol(BinOp op) {
  switch (op) {
  case BinOp::Add:
    return "+";
  case BinOp::Sub:
    return "-";
  case BinOp::Mul:
    return "*";
  case BinOp::Div:
    return "/";
  case BinOp::Mod:
    return "%";
  case BinOp::Eq:
    return "==";
  case BinOp::Neq:
    return "!=";
  case BinOp::Lt:
    return "<";
  case BinOp::Le:
    return "<=";
  case BinOp::Gt:
    return ">";
  case BinOp::Ge:
    return ">=";
  case BinOp::And:
    return "&&";
  case BinOp::Or:
    return "||";
  case BinOp::Xor:
    return "^";
  }
  return "<unknown operator>";
}

} // namespace

Printer::Printer(std::ostream &out) : out_(out) {}

void Printer::newline() { out_ << '\n' << std::string(2 * indent_, ' '); }

void Printer::print_child(const Node *node) {
  if (node == nullptr) {
    out_ << "<null>";
    return;
  }
  node->accept(*this);
}

template <typename T> void Printer::print_list(const vec_ptr<T> &items) {
  out_ << '(';
  for (std::size_t i = 0; i < items.size(); i++) {
    if (i > 0) {
      out_ << ", ";
    }
    print_child(items[i].get());
  }
  out_ << ')';
}

void Printer::block_item(bool &first) {
  if (!first) {
    out_ << ';';
  }
  first = false;
  newline();
}

template <typename T> void Printer::print_block(const vec_ptr<T> &items) {
  indent_++;
  bool first = true;
  for (const auto &item : items) {
    block_item(first);
    print_child(item.get());
  }
  indent_--;
}

void Printer::print(const Proof &proof) {
  if (proof.empty()) {
    out_ << "()";
    return;
  }
  for (std::size_t i = 0; i < proof.size(); i++) {
    if (i > 0) {
      out_ << "; ";
    }
    print_child(proof[i].get());
  }
}

// ==== Types ====

void Printer::visit(const TInteger &) { out_ << "int"; }
void Printer::visit(const TFloat &) { out_ << "float"; }
void Printer::visit(const TBoolean &) { out_ << "bool"; }
void Printer::visit(const TTuple &node) { print_list(node.values); }
void Printer::visit(const TNamed &node) { out_ << node.name; }

// ==== Expressions ====

void Printer::visit(const EInteger &node) { out_ << node.value; }
void Printer::visit(const EFloat &node) { out_ << node.value; }
void Printer::visit(const EBool &node) {
  out_ << (node.value ? "true" : "false");
}
void Printer::visit(const Var &node) { out_ << node.name; }
void Printer::visit(const ETuple &node) { print_list(node.values); }
void Printer::visit(const Self &) { out_ << "self"; }

void Printer::visit(const EBinOp &node) {
  out_ << '(';
  print_child(node.lhs.get());
  out_ << ' ' << symbol(node.op) << ' ';
  print_child(node.rhs.get());
  out_ << ')';
}

void Printer::visit(const Not &node) {
  out_ << "(!";
  print_child(node.exp.get());
  out_ << ')';
}

void Printer::visit(const If &node) {
  out_ << "(if ";
  print_child(node.cond.get());
  out_ << " then ";
  print_child(node.then_branch.get());
  out_ << " else ";
  print_child(node.else_branch.get());
  out_ << ')';
}

void Printer::visit(const FieldAccess &node) {
  print_child(node.object.get());
  out_ << '.' << node.field;
}

void Printer::visit(const Call &node) {
  print_child(node.object.get());
  print_list(node.args);
}

// ==== Propositions and proofs ====

void Printer::visit(const PExp &node) { print_child(node.exp.get()); }

void Printer::visit(const PFollows &node) {
  print_child(node.e0.get());
  out_ << " follows " << node.property;
  print_list(node.args);
}

void Printer::visit(const Use &node) {
  out_ << "use " << node.name;
  print_list(node.args);
}

// ==== Functions ====

void Printer::visit(const Param &node) {
  out_ << node.name << ": ";
  print_child(node.type.get());
}

void Printer::visit(const Function &node) {
  if (node.is_override) {
    out_ << "override ";
  }
  out_ << "def " << node.name;
  print_list(node.params);
  out_ << ": ";
  print_child(node.return_type.get());
  out_ << " =";

  if (!node.is_override && node.conditions.empty()) {
    out_ << ' ';
    print_child(node.body.get());
    return;
  }

  indent_++;
  for (const auto &condition : node.conditions) {
    newline();
    out_ << "such that ";
    print_child(condition.get());
    out_ << ';';
  }
  newline();
  print_child(node.body.get());
  if (node.equivalence) {
    newline();
    out_ << "prove equivalence ";
    print(*node.equivalence);
  }
  indent_--;
}

// ==== Members ====

void Printer::visit(const SuchThat &node) {
  out_ << "such that ";
  print_child(node.prop.get());
}

void Printer::visit(const Prove &node) {
  out_ << "prove ";
  print_child(node.goal.get());
  out_ << " with proof ";
  print(node.proof);
}

void Printer::visit(const ValDecl &node) {
  out_ << "val " << node.name << ": ";
  print_child(node.type.get());
}

void Printer::visit(const ValDef &node) {
  out_ << "val " << node.name << ": ";
  print_child(node.type.get());
  out_ << " = ";
  print_child(node.value.get());
}

void Printer::visit(const Method &node) {
  out_ << "method ";
  visit(node.fn);
}

// ==== Graph components ====

void Printer::visit(const StateComponent &node) {
  print_child(node.lower.get());
  out_ << " <= " << node.var << " < ";
  print_child(node.upper.get());
}

void Printer::visit(const Edge &node) {
  out_ << "edge ";
  print_child(node.source.get());
  out_ << " -> ";
  print_child(node.target.get());
  if (node.weight != nullptr) {
    out_ << " | ";
    print_child(node.weight.get());
  }
}

// ==== Declarations ====

void Printer::visit(const PropertyDecl &node) {
  out_ << "property " << node.name;
  print_list(node.params);
  out_ << ':';
  print_block(node.requirements);
}

void Printer::visit(const LemmaDecl &node) {
  out_ << "lemma " << node.name;
  print_list(node.params);
  out_ << ':';
  print_block(node.statements);
}

void Printer::visit(const ClassDecl &node) {
  out_ << "class " << node.name;
  print_list(node.params);
  out_ << ':';
  print_block(node.members);
}

void Printer::visit(const GraphDecl &node) {
  out_ << "graph " << node.name;
  print_list(node.params);
  out_ << ':';

  indent_++;
  bool first = true;
  block_item(first);
  out_ << "state ";
  print_list(node.state);
  block_item(first);
  out_ << "weight ";
  print_list(node.weight);
  for (const auto &edge : node.edges) {
    block_item(first);
    print_child(edge.get());
  }
  for (const auto &member : node.members) {
    block_item(first);
    print_child(member.get());
  }
  indent_--;
}

void Printer::visit(const FunctionDecl &node) { visit(node.fn); }

// ==== Program ====

// Declarations are separated by a blank line.
void Printer::visit(const Program &node) {
  for (std::size_t i = 0; i < node.decls.size(); i++) {
    if (i > 0) {
      out_ << '\n';
      newline();
    }
    print_child(node.decls[i].get());
  }
}

// ==== Free functions ====

std::string nodle::ast::to_string(const Node &node) {
  std::ostringstream out;
  out << node;
  return out.str();
}

std::ostream &nodle::ast::operator<<(std::ostream &out, const Node &node) {
  Printer printer(out);
  node.accept(printer);
  return out;
}
