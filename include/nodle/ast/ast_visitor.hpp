#pragma once

namespace nodle::ast {

// Types
struct TInteger;
struct TFloat;
struct TBoolean;
struct TTuple;
struct TNamed;

// Expressions
struct EInteger;
struct EFloat;
struct EBool;
struct Var;
struct ETuple;
struct Self;
struct EBinOp;
struct Not;
struct If;
struct FieldAccess;
struct Call;

// Propositions and proofs
struct PExp;
struct PFollows;
struct Use;

// Functions
struct Param;
struct Function;

// Members
struct SuchThat;
struct Prove;
struct ValDecl;
struct ValDef;
struct Method;

// Graph components
struct StateComponent;
struct Edge;

// Declarations
struct PropertyDecl;
struct LemmaDecl;
struct ClassDecl;
struct GraphDecl;
struct FunctionDecl;

// Program
struct Program;

struct Visitor {
  virtual ~Visitor() = default;

  // Types
  virtual void visit(const TInteger &node) = 0;
  virtual void visit(const TFloat &node) = 0;
  virtual void visit(const TBoolean &node) = 0;
  virtual void visit(const TTuple &node) = 0;
  virtual void visit(const TNamed &node) = 0;

  // Expressions
  virtual void visit(const EInteger &node) = 0;
  virtual void visit(const EFloat &node) = 0;
  virtual void visit(const EBool &node) = 0;
  virtual void visit(const Var &node) = 0;
  virtual void visit(const ETuple &node) = 0;
  virtual void visit(const Self &node) = 0;
  virtual void visit(const EBinOp &node) = 0;
  virtual void visit(const Not &node) = 0;
  virtual void visit(const If &node) = 0;
  virtual void visit(const FieldAccess &node) = 0;
  virtual void visit(const Call &node) = 0;

  // Propositions and proofs
  virtual void visit(const PExp &node) = 0;
  virtual void visit(const PFollows &node) = 0;
  virtual void visit(const Use &node) = 0;

  // Functions
  virtual void visit(const Param &node) = 0;
  virtual void visit(const Function &node) = 0;

  // Members
  virtual void visit(const SuchThat &node) = 0;
  virtual void visit(const Prove &node) = 0;
  virtual void visit(const ValDecl &node) = 0;
  virtual void visit(const ValDef &node) = 0;
  virtual void visit(const Method &node) = 0;

  // Graph components
  virtual void visit(const StateComponent &node) = 0;
  virtual void visit(const Edge &node) = 0;

  // Declarations
  virtual void visit(const PropertyDecl &node) = 0;
  virtual void visit(const LemmaDecl &node) = 0;
  virtual void visit(const ClassDecl &node) = 0;
  virtual void visit(const GraphDecl &node) = 0;
  virtual void visit(const FunctionDecl &node) = 0;

  // Program
  virtual void visit(const Program &node) = 0;
};

} // namespace nodle::ast
