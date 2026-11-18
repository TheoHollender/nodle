
#pragma once

#include <unordered_map>
#include <vector>

#include "nodle/ast/ast.hpp"
#include "nodle/ast/ast_visitor.hpp"

namespace nodle::ast {

class AstIndex : public Visitor {
public:
  explicit AstIndex();

  void prepareCache(const Node *node);

  uint32_t indexOf(const Node *node);
  const Node *nodeOf(uint32_t index);

  // Types
  void visit(const TInteger &node) override;
  void visit(const TFloat &node) override;
  void visit(const TBoolean &node) override;
  void visit(const TTuple &node) override;
  void visit(const TNamed &node) override;

  // Expressions
  void visit(const EInteger &node) override;
  void visit(const EFloat &node) override;
  void visit(const EBool &node) override;
  void visit(const Var &node) override;
  void visit(const ETuple &node) override;
  void visit(const Self &node) override;
  void visit(const EBinOp &node) override;
  void visit(const Not &node) override;
  void visit(const If &node) override;
  void visit(const FieldAccess &node) override;
  void visit(const Call &node) override;

  // Propositions and proofs
  void visit(const PExp &node) override;
  void visit(const PFollows &node) override;
  void visit(const Use &node) override;

  // Functions
  void visit(const Param &node) override;
  void visit(const Function &node) override;

  // Members
  void visit(const SuchThat &node) override;
  void visit(const Prove &node) override;
  void visit(const ValDecl &node) override;
  void visit(const ValDef &node) override;
  void visit(const Method &node) override;

  // Graph components
  void visit(const StateComponent &node) override;
  void visit(const Edge &node) override;

  // Declarations
  void visit(const PropertyDecl &node) override;
  void visit(const LemmaDecl &node) override;
  void visit(const ClassDecl &node) override;
  void visit(const GraphDecl &node) override;
  void visit(const FunctionDecl &node) override;

  // Program
  void visit(const Program &node) override;

private:
  std::unordered_map<const Node *, std::size_t> id_of;
  std::vector<const Node *> node_of;
};

}; // namespace nodle::ast
