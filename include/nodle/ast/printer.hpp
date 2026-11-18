#pragma once

#include <ostream>
#include <string>

#include "nodle/ast/ast.hpp"
#include "nodle/ast/ast_visitor.hpp"
#include "nodle/ast/index.hpp"

namespace nodle::ast {

class BaseAstPrinter : public Visitor {
private:
  std::ostream &out_;

  AstIndex index_;

  bool new_lines_;
  int maximal_depth_;
  int current_depth_ = 0;
  int added_indent_ = 0;
  int base_indent_ = 0;

  void newline();

  template <typename F> void print_block(std::string name, F func);
  template <typename T, typename F>
  void print_list(std::string name, const std::vector<T> &values, F func);
  void print_field(std::string field, const Node *value);
  void print_field(std::string field, std::string value);

  void print_child(const Node *node);

public:
  void set_base_indent(int base_indent);

  std::ostream &get_stream();
  AstIndex &get_index();

  explicit BaseAstPrinter(std::ostream &out, bool new_lines = true,
                          int maximal_depth = -1);

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
};

struct SourceAstPrinter : public Visitor {
private:
  BaseAstPrinter &printer_;

public:
  explicit SourceAstPrinter(BaseAstPrinter &printer);

  void print(const Node *node);

  template <typename T> void print_vecptr(const vec_ptr<T> &values);

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
};

class PrettyPrinter : public Visitor {
public:
  explicit PrettyPrinter(std::ostream &out);

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

  void print(const Proof &proof);

private:
  std::ostream &out_;
  int indent_ = 0;

  void print_child(const Node *node);
  template <typename T> void print_list(const vec_ptr<T> &items);
  template <typename T> void print_block(const vec_ptr<T> &items);
  void block_item(bool &first);
  void newline();
};

std::string to_string(const Node &node);

std::ostream &operator<<(std::ostream &out, const Node &node);

} // namespace nodle::ast
