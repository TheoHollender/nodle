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

void BaseAstPrinter::set_base_indent(int base_indent) {
  base_indent_ = base_indent;
}

std::ostream &BaseAstPrinter::get_stream() { return out_; }
AstIndex &BaseAstPrinter::get_index() { return index_; }

BaseAstPrinter::BaseAstPrinter(std::ostream &out, bool new_lines,
                               int maximal_depth)
    : out_(out), new_lines_(new_lines), maximal_depth_(maximal_depth),
      current_depth_(maximal_depth) {}

void BaseAstPrinter::newline() {
  if (new_lines_) {
    out_ << "\n";
    out_ << std::string(2 * added_indent_ + base_indent_, ' ');
  } else {
    out_ << " ";
  }
}

void BaseAstPrinter::print_child(const Node *node) {
  if (node == nullptr) {
    out_ << "<null>";
    return;
  }
  current_depth_--;
  if (current_depth_ == 0) {
    current_depth_++;
    index_.prepareCache(node);
    out_ << "<node #" << index_.indexOf(node) << ">";

    return;
  }
  node->accept(*this);
  current_depth_++;
}

template <typename F>
void BaseAstPrinter::print_block(std::string name, F func) {
  out_ << name << "{";
  added_indent_++;

  func();

  added_indent_--;
  newline();
  out_ << "}";
}
template <typename T, typename F>
void BaseAstPrinter::print_list(std::string name, const std::vector<T> &values,
                                F func) {
  int offset = 0;
  for (const auto &val : values) {
    newline();
    out_ << name << "#" << offset << " = ";
    func(val);
    offset++;
  }
}
void BaseAstPrinter::print_field(std::string field, std::string value) {
  newline();
  out_ << field << " = \"" << value << "\"";
}
void BaseAstPrinter::print_field(std::string field, const Node *value) {
  newline();
  out_ << field << " = ";
  print_child(value);
}

// ==== Types ====

void BaseAstPrinter::visit(const TInteger &) { out_ << "int"; }
void BaseAstPrinter::visit(const TFloat &) { out_ << "float"; }
void BaseAstPrinter::visit(const TBoolean &) { out_ << "bool"; }
void BaseAstPrinter::visit(const TTuple &node) {
  print_block("Tuple", [&node, this]() {
    print_list("subtype", node.values,
               [this](auto &x) { print_child(x.get()); });
  });
}
void BaseAstPrinter::visit(const TNamed &node) {
  out_ << "Class{name = " << node.name << "}";
}

// ==== Expressions ====

void BaseAstPrinter::visit(const EInteger &node) {
  out_ << "int{" << node.value << "}";
}
void BaseAstPrinter::visit(const EFloat &node) {
  out_ << "float{" << node.value << "}";
}
void BaseAstPrinter::visit(const EBool &node) {
  out_ << (node.value ? "bool{true}" : "bool{false}");
}
void BaseAstPrinter::visit(const Var &node) {
  out_ << "var{" << node.name << "}";
}
void BaseAstPrinter::visit(const ETuple &node) {
  print_block("Tuple", [&node, this]() {
    print_list("el", node.values, [this](auto &x) { print_child(x.get()); });
  });
}
void BaseAstPrinter::visit(const Self &) { out_ << "Self"; }

void BaseAstPrinter::visit(const EBinOp &node) {
  print_block("BinOp", [&node, this]() {
    print_field("op", std::string(symbol(node.op)));
    print_field("lhs", node.lhs.get());
    print_field("rhs", node.rhs.get());
  });
}

void BaseAstPrinter::visit(const Not &node) {
  print_block("Not", [&node, this]() { print_field("expr", node.exp.get()); });
}

void BaseAstPrinter::visit(const If &node) {
  print_block("If", [&node, this]() {
    print_field("cond", node.cond.get());
    print_field("then", node.then_branch.get());
    print_field("else", node.else_branch.get());
  });
}

void BaseAstPrinter::visit(const FieldAccess &node) {
  print_block("Access", [&node, this]() {
    print_field("object", node.object.get());
    print_field("field", node.field);
  });
}

void BaseAstPrinter::visit(const Call &node) {
  print_block("Call", [&node, this]() {
    print_field("object", node.object.get());
    print_list("arg", node.args, [this](auto &x) { print_child(x.get()); });
  });
}

// ==== Propositions and proofs ====

void BaseAstPrinter::visit(const PExp &node) { node.exp->accept(*this); }

void BaseAstPrinter::visit(const PFollows &node) {
  print_block("Follows", [&node, this]() {
    print_field("lhs", node.e0.get());
    print_field("property", node.property);
    print_list("arg", node.args, [this](auto &x) { print_child(x.get()); });
  });
}

void BaseAstPrinter::visit(const Use &node) {
  print_block("Use", [&node, this]() {
    print_field("lemma", node.name);
    print_list("arg", node.args, [this](auto &x) { print_child(x.get()); });
  });
}

// ==== Functions ====

void BaseAstPrinter::visit(const Param &node) {
  print_block("Param", [&node, this]() {
    print_field("lemma", node.name);
    print_field("type", node.type.get());
  });
}

void BaseAstPrinter::visit(const Function &node) {
  print_block((node.is_override ? "Function[override]" : "Function"), [&node,
                                                                       this]() {
    print_field("name", node.name);
    print_list("param", node.params, [this](auto &x) { print_child(x.get()); });
    print_field("return", node.return_type.get());
    if (node.is_override && !node.conditions.empty()) {
      print_list("cond", node.conditions,
                 [this](auto &x) { print_child(x.get()); });
    }
    print_field("body", node.body.get());
    if (node.equivalence) {
      print_block("Equivalence", [&node, this]() {
        print_list("use", node.equivalence.value(),
                   [this](auto &x) { print_child(x.get()); });
      });
    }
  });
}

// ==== Members ====

void BaseAstPrinter::visit(const SuchThat &node) {
  print_block("SuchThat",
              [&node, this]() { print_field("prop", node.prop.get()); });
}

void BaseAstPrinter::visit(const Prove &node) {
  print_block("Prove", [&node, this]() {
    print_field("goal", node.goal.get());
    newline();
    print_block("Proof", [&node, this]() {
      print_list("use", node.proof, [this](auto &x) { print_child(x.get()); });
    });
  });
}

void BaseAstPrinter::visit(const ValDecl &node) {
  print_block("ValDecl", [&node, this]() {
    print_field("name", node.name);
    print_field("type", node.type.get());
  });
}

void BaseAstPrinter::visit(const ValDef &node) {
  print_block("ValDef", [&node, this]() {
    print_field("name", node.name);
    print_field("type", node.type.get());
    print_field("value", node.value.get());
  });
}

void BaseAstPrinter::visit(const Method &node) {
  out_ << "Method ";
  visit(node.fn);
}

// ==== Graph components ====

void BaseAstPrinter::visit(const StateComponent &node) {
  print_block("State", [&node, this]() {
    print_field("lower", node.lower.get());
    print_field("var", node.var);
    print_field("upper", node.upper.get());
  });
}

void BaseAstPrinter::visit(const Edge &node) {
  print_block("Edge", [&node, this]() {
    print_field("source", node.source.get());
    print_field("target", node.target.get());
    print_field("weight", node.weight.get());
  });
}

// ==== Declarations ====

void BaseAstPrinter::visit(const PropertyDecl &node) {
  print_block("Property", [&node, this]() {
    print_field("name", node.name);
    print_list("param", node.params, [this](auto &x) { print_child(x.get()); });
    print_list("req", node.requirements,
               [this](auto &x) { print_child(x.get()); });
  });
}

void BaseAstPrinter::visit(const LemmaDecl &node) {
  print_block("Lemma", [&node, this]() {
    print_field("name", node.name);
    print_list("param", node.params, [this](auto &x) { print_child(x.get()); });
    print_list("sttm", node.statements,
               [this](auto &x) { print_child(x.get()); });
  });
}

void BaseAstPrinter::visit(const ClassDecl &node) {
  print_block("Class", [&node, this]() {
    print_field("name", node.name);
    print_list("param", node.params, [this](auto &x) { print_child(x.get()); });
    print_list("members", node.members,
               [this](auto &x) { print_child(x.get()); });
  });
}

void BaseAstPrinter::visit(const GraphDecl &node) {
  print_block("Graph", [&node, this]() {
    print_field("name", node.name);
    print_list("param", node.params, [this](auto &x) { print_child(x.get()); });
    print_list("state", node.state, [this](auto &x) { print_child(x.get()); });
    print_list("weight", node.weight,
               [this](auto &x) { print_child(x.get()); });
    print_list("edges", node.edges, [this](auto &x) { print_child(x.get()); });
    print_list("members", node.members,
               [this](auto &x) { print_child(x.get()); });
  });
}

void BaseAstPrinter::visit(const FunctionDecl &node) { visit(node.fn); }

void BaseAstPrinter::visit(const Program &node) {
  print_block("Program", [&node, this]() {
    bool first = true;

    for (auto &decl : node.decls) {
      newline();
      print_child(decl.get());

      first = false;
    }
  });
}
