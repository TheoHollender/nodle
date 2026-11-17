#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "nodle/ast/ast_visitor.hpp"
#include "nodle/io/location.hpp"

namespace nodle::ast {

template <typename T> using vec_ptr = std::vector<std::unique_ptr<T>>;

// Node
struct Node {
  explicit Node(io::SourceLocation loc) : source_location(std::move(loc)) {}
  virtual ~Node() = default;

  /**
   * Grows the location of the node so that it also covers `loc`.
   */
  void extend(const io::SourceLocation &loc) {
    source_location = source_location | loc;
  }

  /**
   * Calls the `visit` overload of `visitor` that matches this node's type.
   */
  virtual void accept(Visitor &visitor) const = 0;

  io::SourceLocation source_location;
};

// Type
struct Type : public Node {
  using Node::Node;
};

struct TInteger : public Type {
  using Type::Type;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};
struct TFloat : public Type {
  using Type::Type;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};
struct TBoolean : public Type {
  using Type::Type;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};
struct TTuple : public Type {
  TTuple(io::SourceLocation lparen, vec_ptr<Type> values,
         io::SourceLocation rparen)
      : Type(lparen | rparen), values(std::move(values)) {}
  vec_ptr<Type> values;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct TNamed : public Type {
  TNamed(io::SourceLocation tok, std::string name)
      : Type(std::move(tok)), name(std::move(name)) {}
  std::string name;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

// Expression
struct Expression : public Node {
  using Node::Node;
};

struct EInteger : public Expression {
  EInteger(io::SourceLocation tok, long long value)
      : Expression(std::move(tok)), value(value) {}
  long long value;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct EFloat : public Expression {
  EFloat(io::SourceLocation tok, double value)
      : Expression(std::move(tok)), value(value) {}
  double value;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct EBool : public Expression {
  EBool(io::SourceLocation tok, bool value)
      : Expression(std::move(tok)), value(value) {}
  bool value;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct Var : public Expression {
  Var(io::SourceLocation tok, std::string name)
      : Expression(std::move(tok)), name(std::move(name)) {}
  std::string name;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct ETuple : public Expression {
  ETuple(io::SourceLocation lparen, vec_ptr<Expression> values,
         io::SourceLocation rparen)
      : Expression(lparen | rparen), values(std::move(values)) {}
  vec_ptr<Expression> values;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct Self : public Expression {
  using Expression::Expression;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

enum class BinOp {
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Eq,
  Neq,
  Lt,
  Le,
  Gt,
  Ge,
  And,
  Or,
  Xor
};

struct EBinOp : public Expression {
  EBinOp(BinOp op, std::unique_ptr<Expression> lhs,
         std::unique_ptr<Expression> rhs)
      : Expression(lhs->source_location | rhs->source_location), op(op),
        lhs(std::move(lhs)), rhs(std::move(rhs)) {}
  BinOp op;
  std::unique_ptr<Expression> lhs, rhs;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct Not : public Expression {
  Not(io::SourceLocation not_tok, std::unique_ptr<Expression> exp)
      : Expression(not_tok | exp->source_location), exp(std::move(exp)) {}
  std::unique_ptr<Expression> exp;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct If : public Expression {
  If(io::SourceLocation if_tok, std::unique_ptr<Expression> cond,
     std::unique_ptr<Expression> then_branch,
     std::unique_ptr<Expression> else_branch)
      : Expression(if_tok | else_branch->source_location),
        cond(std::move(cond)), then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}
  std::unique_ptr<Expression> cond, then_branch, else_branch;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct FieldAccess : public Expression {
  FieldAccess(std::unique_ptr<Expression> object, io::SourceLocation field_tok,
              std::string field)
      : Expression(object->source_location | field_tok),
        object(std::move(object)), field(std::move(field)) {}
  std::unique_ptr<Expression> object;
  std::string field;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct Call : public Expression {
  Call(std::unique_ptr<Expression> object, vec_ptr<Expression> args,
       io::SourceLocation rparen)
      : Expression(object->source_location | rparen), object(std::move(object)),
        args(std::move(args)) {}
  std::unique_ptr<Expression> object;
  vec_ptr<Expression> args;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

// Proposition
struct Proposition : public Node {
  using Node::Node;
};

struct PExp : public Proposition {
  explicit PExp(std::unique_ptr<Expression> exp)
      : Proposition(exp->source_location), exp(std::move(exp)) {}
  std::unique_ptr<Expression> exp;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct PFollows : public Proposition {
  PFollows(std::unique_ptr<Expression> e0, std::string property,
           vec_ptr<Expression> args, io::SourceLocation end)
      : Proposition(e0->source_location | end), e0(std::move(e0)),
        property(std::move(property)), args(std::move(args)) {}
  std::unique_ptr<Expression> e0;
  std::string property;
  vec_ptr<Expression> args;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

// Proof
struct Use : public Node {
  Use(io::SourceLocation use_tok, std::string name, vec_ptr<Expression> args,
      io::SourceLocation end)
      : Node(use_tok | end), name(std::move(name)), args(std::move(args)) {}
  std::string name;
  vec_ptr<Expression> args;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

using Proof = vec_ptr<Use>;

// Function
struct Param : public Node {
  Param(io::SourceLocation name_tok, std::string name,
        std::unique_ptr<Type> type)
      : Node(name_tok | type->source_location), name(std::move(name)),
        type(std::move(type)) {}
  std::string name;
  std::unique_ptr<Type> type;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

using Params = vec_ptr<Param>;

struct Function : public Node {
  using Node::Node;
  std::string name;
  Params params;
  std::unique_ptr<Type> return_type;
  std::unique_ptr<Expression> body;

  bool is_override = false;
  vec_ptr<Proposition> conditions;
  std::optional<Proof> equivalence;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

// Member and Statement
struct Member : public Node {
  using Node::Node;
};

struct SuchThat : public Member {
  SuchThat(io::SourceLocation such_tok, std::unique_ptr<Proposition> prop)
      : Member(such_tok | prop->source_location), prop(std::move(prop)) {}
  std::unique_ptr<Proposition> prop;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};
struct Prove : public Member {
  Prove(io::SourceLocation prove_tok, std::unique_ptr<Proposition> goal,
        Proof proof)
      : Member(prove_tok | (proof.empty() ? goal->source_location
                                          : proof.back()->source_location)),
        goal(std::move(goal)), proof(std::move(proof)) {}
  std::unique_ptr<Proposition> goal;
  Proof proof;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};
struct ValDecl : public Member {
  ValDecl(io::SourceLocation val_tok, std::string name,
          std::unique_ptr<Type> type)
      : Member(val_tok | type->source_location), name(std::move(name)),
        type(std::move(type)) {}
  std::string name;
  std::unique_ptr<Type> type;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};
struct ValDef : public Member {
  ValDef(io::SourceLocation val_tok, std::string name,
         std::unique_ptr<Type> type, std::unique_ptr<Expression> value)
      : Member(val_tok | value->source_location), name(std::move(name)),
        type(std::move(type)), value(std::move(value)) {}
  std::string name;
  std::unique_ptr<Type> type;
  std::unique_ptr<Expression> value;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};
struct Method : public Member {
  explicit Method(Function fn)
      : Member(fn.source_location), fn(std::move(fn)) {}
  Function fn;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

// Graph components
struct StateComponent : public Node {
  StateComponent(std::unique_ptr<Expression> lower, std::string var,
                 std::unique_ptr<Expression> upper)
      : Node(lower->source_location | upper->source_location),
        lower(std::move(lower)), var(std::move(var)), upper(std::move(upper)) {}
  std::unique_ptr<Expression> lower;
  std::string var;
  std::unique_ptr<Expression> upper;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct Edge : public Node {
  Edge(std::unique_ptr<Expression> source, std::unique_ptr<Expression> target,
       std::unique_ptr<Expression> weight)
      : Node(source->source_location |
             (weight ? weight : target)->source_location),
        source(std::move(source)), target(std::move(target)),
        weight(std::move(weight)) {}
  std::unique_ptr<Expression> source, target, weight;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

// Declaration
struct Declaration : public Node {
  using Node::Node;
};

struct PropertyDecl : public Declaration {
  using Declaration::Declaration;
  std::string name;
  Params params;
  vec_ptr<Member> requirements;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct LemmaDecl : public Declaration {
  using Declaration::Declaration;
  std::string name;
  Params params;
  vec_ptr<Member> statements;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct ClassDecl : public Declaration {
  using Declaration::Declaration;
  std::string name;
  Params params;
  vec_ptr<Member> members;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct GraphDecl : public Declaration {
  using Declaration::Declaration;
  std::string name;
  Params params;
  vec_ptr<StateComponent> state;
  vec_ptr<Type> weight;
  vec_ptr<Edge> edges;
  vec_ptr<Member> members;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

struct FunctionDecl : public Declaration {
  explicit FunctionDecl(Function fn)
      : Declaration(fn.source_location), fn(std::move(fn)) {}
  Function fn;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

// Program
struct Program : public Node {
  using Node::Node;
  vec_ptr<Declaration> decls;

  void accept(Visitor &visitor) const override { visitor.visit(*this); }
};

} // namespace nodle::ast
