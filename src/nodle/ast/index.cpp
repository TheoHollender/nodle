#include "nodle/ast/index.hpp"

using namespace nodle::ast;

AstIndex::AstIndex() {}

void AstIndex::prepareCache(const Node *node) {
  if (id_of.find(node) != id_of.end()) {
    return;
  }

  id_of[node] = node_of.size();
  node_of.push_back(node);

  node->accept(*this);
}

uint32_t AstIndex::indexOf(const Node *node) { return id_of.at(node); }
const Node *AstIndex::nodeOf(uint32_t index) { return node_of.at(index); }

// Types
void AstIndex::visit(const TInteger &node) { (void)node; }
void AstIndex::visit(const TFloat &node) { (void)node; }
void AstIndex::visit(const TBoolean &node) { (void)node; }
void AstIndex::visit(const TNamed &node) { (void)node; }
void AstIndex::visit(const TTuple &node) {
  for (auto &subtype : node.values) {
    prepareCache(subtype.get());
  }
}

// Expressions
void AstIndex::visit(const EInteger &node) { (void)node; }
void AstIndex::visit(const EFloat &node) { (void)node; }
void AstIndex::visit(const EBool &node) { (void)node; }
void AstIndex::visit(const Var &node) { (void)node; }
void AstIndex::visit(const ETuple &node) {
  for (auto &subexpr : node.values) {
    prepareCache(subexpr.get());
  }
}
void AstIndex::visit(const Self &node) { (void)node; }
void AstIndex::visit(const EBinOp &node) {
  prepareCache(node.lhs.get());
  prepareCache(node.rhs.get());
}
void AstIndex::visit(const Not &node) { prepareCache(node.exp.get()); }
void AstIndex::visit(const If &node) {
  prepareCache(node.cond.get());
  prepareCache(node.then_branch.get());
  prepareCache(node.else_branch.get());
}
void AstIndex::visit(const FieldAccess &node) {
  prepareCache(node.object.get());
}
void AstIndex::visit(const Call &node) {
  prepareCache(node.object.get());
  for (const auto &arg : node.args) {
    prepareCache(arg.get());
  }
}

// Propositions and proofs
void AstIndex::visit(const PExp &node) { prepareCache(node.exp.get()); }
void AstIndex::visit(const PFollows &node) {
  prepareCache(node.e0.get());
  for (const auto &arg : node.args) {
    prepareCache(arg.get());
  }
}
void AstIndex::visit(const Use &node) {
  for (const auto &arg : node.args) {
    prepareCache(arg.get());
  }
}

// Functions
void AstIndex::visit(const Param &node) { prepareCache(node.type.get()); }
void AstIndex::visit(const Function &node) {
  for (const auto &param : node.params) {
    prepareCache(param.get());
  }
  prepareCache(node.return_type.get());

  for (auto &condition : node.conditions) {
    prepareCache(condition.get());
  }

  prepareCache(node.body.get());

  if (node.equivalence.has_value()) {
    for (const auto &use : node.equivalence.value()) {
      prepareCache(use.get());
    }
  }
}

// Members
void AstIndex::visit(const SuchThat &node) { prepareCache(node.prop.get()); }
void AstIndex::visit(const Prove &node) {
  prepareCache(node.goal.get());
  for (const auto &use : node.proof) {
    prepareCache(use.get());
  }
}
void AstIndex::visit(const ValDecl &node) { prepareCache(node.type.get()); }
void AstIndex::visit(const ValDef &node) {
  prepareCache(node.type.get());
  prepareCache(node.value.get());
}
void AstIndex::visit(const Method &node) { prepareCache(&node.fn); }

// Graph components
void AstIndex::visit(
    const StateComponent &node) { /* TODO when graph is better defined */ }
void AstIndex::visit(const Edge &node) { /* TODO when graph is better defined */
}

// Declarations
void AstIndex::visit(const PropertyDecl &node) {
  for (auto &param : node.params) {
    prepareCache(param.get());
  }
  for (auto &mb : node.requirements) {
    prepareCache(mb.get());
  }
}
void AstIndex::visit(const LemmaDecl &node) {
  for (auto &param : node.params) {
    prepareCache(param.get());
  }
  for (auto &mb : node.statements) {
    prepareCache(mb.get());
  }
}
void AstIndex::visit(const ClassDecl &node) {
  for (auto &param : node.params) {
    prepareCache(param.get());
  }
  for (auto &mb : node.members) {
    prepareCache(mb.get());
  }
}
void AstIndex::visit(
    const GraphDecl &node) { /* TODO when graph is better defined */ }
void AstIndex::visit(const FunctionDecl &node) { prepareCache(&node.fn); }

// Program
void AstIndex::visit(const Program &node) {
  for (auto &decl : node.decls) {
    prepareCache(decl.get());
  }
}
