#include "nodle/ast/printer.hpp"

#include <sstream>
#include <string_view>

using namespace nodle::ast;

SourceAstPrinter::SourceAstPrinter(BaseAstPrinter &printer)
    : printer_(printer) {}

void SourceAstPrinter::print (const Node* node) {
  node->accept(*this);

  printer_.get_index().prepareCache(node);

  printer_.get_stream()
    << node->source_location.as_debug_string(true)
    << "<node #" << printer_.get_index().indexOf(node) << "> AST"
    << "\n";

  int padding = node->source_location.debug_string_padding() + 2;
  printer_.get_stream() << std::string(padding, ' ');

  printer_.set_base_indent(padding);
  node->accept(printer_);
  
  printer_.get_stream() << "\n\n";
}

template<typename T>
void SourceAstPrinter::print_vecptr (const vec_ptr<T> &values) {
  for (auto &val : values) {
    print(val.get());
  }
}

// ==== Types ====

void SourceAstPrinter::visit(const TInteger &) {}
void SourceAstPrinter::visit(const TFloat &) {}
void SourceAstPrinter::visit(const TBoolean &) {}
void SourceAstPrinter::visit(const TTuple &node) {
  print_vecptr(node.values);
}
void SourceAstPrinter::visit(const TNamed &) {}

// ==== Expressions ====

void SourceAstPrinter::visit(const EInteger &) {}
void SourceAstPrinter::visit(const EFloat &) {}
void SourceAstPrinter::visit(const EBool &) {}
void SourceAstPrinter::visit(const Var &) {}
void SourceAstPrinter::visit(const ETuple &node) {
  print_vecptr(node.values);
}
void SourceAstPrinter::visit(const Self &) {}

void SourceAstPrinter::visit(const EBinOp &node) {
  print(node.lhs.get());
  print(node.rhs.get());
}

void SourceAstPrinter::visit(const Not &node) {
  print(node.exp.get());
}

void SourceAstPrinter::visit(const If &node) {
  print(node.cond.get());
  print(node.then_branch.get());
  print(node.else_branch.get());
}

void SourceAstPrinter::visit(const FieldAccess &node) {
  print(node.object.get());
}

void SourceAstPrinter::visit(const Call &node) {
  print(node.object.get());
  print_vecptr(node.args);
}

// ==== Propositions and proofs ====

void SourceAstPrinter::visit(const PExp &node) { node.exp->accept(*this); }

void SourceAstPrinter::visit(const PFollows &node) {
  print(node.e0.get());
  print_vecptr(node.args);
}

void SourceAstPrinter::visit(const Use &node) {
  print_vecptr(node.args);
}

// ==== Functions ====

void SourceAstPrinter::visit(const Param &node) {
  print(node.type.get());
}

void SourceAstPrinter::visit(const Function &node) {
  print_vecptr(node.params);
  print(node.return_type.get());
  if (node.is_override && !node.conditions.empty()) {
    print_vecptr(node.conditions);
  }
  print(node.body.get());
  if (node.equivalence) {
    print_vecptr(node.equivalence.value());
  }
}

// ==== Members ====

void SourceAstPrinter::visit(const SuchThat &node) {
  print(node.prop.get());
}

void SourceAstPrinter::visit(const Prove &node) {
  print(node.goal.get());
  print_vecptr(node.proof);
}

void SourceAstPrinter::visit(const ValDecl &node) {
  print(node.type.get());
}

void SourceAstPrinter::visit(const ValDef &node) {
  print(node.type.get());
  print(node.value.get());
}

void SourceAstPrinter::visit(const Method &node) {
  visit(node.fn);
}

// ==== Graph components ====

void SourceAstPrinter::visit(const StateComponent &node) {
  print(node.lower.get());
  print(node.upper.get());
}

void SourceAstPrinter::visit(const Edge &node) {
  print(node.source.get());
  print(node.target.get());
  print(node.weight.get());
}

// ==== Declarations ====

void SourceAstPrinter::visit(const PropertyDecl &node) {
  print_vecptr(node.params);
  print_vecptr(node.requirements);
}

void SourceAstPrinter::visit(const LemmaDecl &node) {
  print_vecptr(node.params);
  print_vecptr(node.statements);
}

void SourceAstPrinter::visit(const ClassDecl &node) {
  print_vecptr(node.params);
  print_vecptr(node.members);
}

void SourceAstPrinter::visit(const GraphDecl &node) {
  print_vecptr(node.params);
  print_vecptr(node.state);
  print_vecptr(node.weight);
  print_vecptr(node.edges);
  print_vecptr(node.members);
}

void SourceAstPrinter::visit(const FunctionDecl &node) { visit(node.fn); }

void SourceAstPrinter::visit(const Program &node) {
  print_vecptr(node.decls);
}
