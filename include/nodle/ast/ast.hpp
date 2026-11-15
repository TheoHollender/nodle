#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

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

  io::SourceLocation source_location;
};

// Type
struct Type : public Node {
  using Node::Node;
};

struct TInteger : public Type {
  using Type::Type;
};
struct TFloat : public Type {
  using Type::Type;
};
struct TBoolean : public Type {
  using Type::Type;
};
struct TTuple : public Type {
  TTuple(io::SourceLocation lparen, vec_ptr<Type> values,
         io::SourceLocation rparen)
      : Type(lparen | rparen), values(std::move(values)) {}
  vec_ptr<Type> values;
};

struct TNamed : public Type {
  TNamed(io::SourceLocation tok, std::string name)
      : Type(std::move(tok)), name(std::move(name)) {}
  std::string name;
};

// Expression
struct Expression : public Node {
  using Node::Node;
};

struct EInteger : public Expression {
  EInteger(io::SourceLocation tok, long long value)
      : Expression(std::move(tok)), value(value) {}
  long long value;
};

struct EFloat : public Expression {
  EFloat(io::SourceLocation tok, double value)
      : Expression(std::move(tok)), value(value) {}
  double value;
};

struct EBool : public Expression {
  EBool(io::SourceLocation tok, bool value)
      : Expression(std::move(tok)), value(value) {}
  bool value;
};

struct Var : public Expression {
  Var(io::SourceLocation tok, std::string name)
      : Expression(std::move(tok)), name(std::move(name)) {}
  std::string name;
};

struct ETuple : public Expression {
  ETuple(io::SourceLocation lparen, vec_ptr<Expression> values,
         io::SourceLocation rparen)
      : Expression(lparen | rparen), values(std::move(values)) {}
  vec_ptr<Expression> values;
};

struct Self : public Expression {
  using Expression::Expression;
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
};

struct Not : public Expression {
  Not(io::SourceLocation not_tok, std::unique_ptr<Expression> exp)
      : Expression(not_tok | exp->source_location), exp(std::move(exp)) {}
  std::unique_ptr<Expression> exp;
};

struct If : public Expression {
  If(io::SourceLocation if_tok, std::unique_ptr<Expression> cond,
     std::unique_ptr<Expression> then_branch,
     std::unique_ptr<Expression> else_branch)
      : Expression(if_tok | else_branch->source_location),
        cond(std::move(cond)), then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}
  std::unique_ptr<Expression> cond, then_branch, else_branch;
};

struct FieldAccess : public Expression {
  FieldAccess(std::unique_ptr<Expression> object, io::SourceLocation field_tok,
              std::string field)
      : Expression(object->source_location | field_tok),
        object(std::move(object)), field(std::move(field)) {}
  std::unique_ptr<Expression> object;
  std::string field;
};

struct Call : public Expression {
  Call(io::SourceLocation func_tok, std::string func, vec_ptr<Expression> args,
       io::SourceLocation rparen)
      : Expression(func_tok | rparen), func(std::move(func)),
        args(std::move(args)) {}
  std::string func;
  vec_ptr<Expression> args;
};

struct MethodCall : public Expression {
  MethodCall(std::unique_ptr<Expression> object, std::string func,
             vec_ptr<Expression> args, io::SourceLocation rparen)
      : Expression(object->source_location | rparen), object(std::move(object)),
        func(std::move(func)), args(std::move(args)) {}
  std::unique_ptr<Expression> object;
  std::string func;
  vec_ptr<Expression> args;
};

// Proposition
struct Proposition : public Node {
  using Node::Node;
};

struct PExp : public Proposition {
  explicit PExp(std::unique_ptr<Expression> exp)
      : Proposition(exp->source_location), exp(std::move(exp)) {}
  std::unique_ptr<Expression> exp;
};

struct PFollows : public Proposition {
  PFollows(std::unique_ptr<Expression> e0, std::string property,
           vec_ptr<Expression> args, io::SourceLocation end)
      : Proposition(e0->source_location | end), e0(std::move(e0)),
        property(std::move(property)), args(std::move(args)) {}
  std::unique_ptr<Expression> e0;
  std::string property;
  vec_ptr<Expression> args;
};

// Proof
struct Use : public Node {
  Use(io::SourceLocation use_tok, std::string name, vec_ptr<Expression> args,
      io::SourceLocation end)
      : Node(use_tok | end), name(std::move(name)), args(std::move(args)) {}
  std::string name;
  vec_ptr<Expression> args;
};

using Proof = std::vector<Use>;

// Function
struct Param : public Node {
  Param(io::SourceLocation name_tok, std::string name,
        std::unique_ptr<Type> type)
      : Node(name_tok | type->source_location), name(std::move(name)),
        type(std::move(type)) {}
  std::string name;
  std::unique_ptr<Type> type;
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
  Proof equivalence;
};

// Member and Statement
struct Member : public Node {
  using Node::Node;
};

struct SuchThat : public Member {
  SuchThat(io::SourceLocation such_tok, std::unique_ptr<Proposition> prop)
      : Member(such_tok | prop->source_location), prop(std::move(prop)) {}
  std::unique_ptr<Proposition> prop;
};
struct Prove : public Member {
  Prove(io::SourceLocation prove_tok, std::unique_ptr<Proposition> goal,
        Proof proof)
      : Member(prove_tok | (proof.empty() ? goal->source_location
                                          : proof.back().source_location)),
        goal(std::move(goal)), proof(std::move(proof)) {}
  std::unique_ptr<Proposition> goal;
  Proof proof;
};
struct ValDecl : public Member {
  ValDecl(io::SourceLocation val_tok, std::string name,
          std::unique_ptr<Type> type)
      : Member(val_tok | type->source_location), name(std::move(name)),
        type(std::move(type)) {}
  std::string name;
  std::unique_ptr<Type> type;
};
struct ValDef : public Member {
  ValDef(io::SourceLocation val_tok, std::string name,
         std::unique_ptr<Type> type, std::unique_ptr<Expression> value)
      : Member(val_tok | value->source_location), name(std::move(name)),
        type(std::move(type)), value(std::move(value)) {}
  std::string name;
  std::unique_ptr<Type> type;
  std::unique_ptr<Expression> value;
};
struct Method : public Member {
  explicit Method(Function fn)
      : Member(fn.source_location), fn(std::move(fn)) {}
  Function fn;
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
};

struct Edge : public Node {
  Edge(std::unique_ptr<Expression> source, std::unique_ptr<Expression> target,
       std::unique_ptr<Expression> weight)
      : Node(source->source_location |
             (weight ? weight : target)->source_location),
        source(std::move(source)), target(std::move(target)),
        weight(std::move(weight)) {}
  std::unique_ptr<Expression> source, target, weight;
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
};

struct LemmaDecl : public Declaration {
  using Declaration::Declaration;
  std::string name;
  Params params;
  vec_ptr<Member> statements;
};

struct ClassDecl : public Declaration {
  using Declaration::Declaration;
  std::string name;
  Params params;
  vec_ptr<Member> members;
};

struct GraphDecl : public Declaration {
  using Declaration::Declaration;
  std::string name;
  Params params;
  vec_ptr<StateComponent> state;
  vec_ptr<Type> weight;
  vec_ptr<Edge> edges;
  vec_ptr<Member> members;
};

struct FunctionDecl : public Declaration {
  explicit FunctionDecl(Function fn)
      : Declaration(fn.source_location), fn(std::move(fn)) {}
  Function fn;
};

// Program
struct Program : public Node {
  using Node::Node;
  vec_ptr<Declaration> decls;
};

} // namespace nodle::ast
