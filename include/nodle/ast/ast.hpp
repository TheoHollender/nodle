#include <memory>
#include <string>
#include <vector>

namespace nodle::ast {

template <typename T> using vec_ptr = std::vector<std::unique_ptr<T>>;

// Type
struct Type {
  virtual ~Type() = default;
};

struct TInteger : Type {};
struct TFloat : Type {};
struct TBoolean : Type {};
struct TTuple : Type {
  vec_ptr<Type> values;
};

struct TNammed : Type {
  std::string name;
};

// Expression
struct Expression {
  virtual ~Expression() = default;
};

struct EInteger : Expression {
  long long value;
};

struct EFloat : Expression {
  double value;
};

struct EBool : Expression {
  bool value;
};

struct Var : Expression {
  std::string name;
};

struct ETuple : Expression {
  vec_ptr<Expression> values;
};

struct Self : Expression {};

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

struct EBinOp : Expression {
  BinOp op;
  std::unique_ptr<Expression> lhs, rhs;
};

struct Not : Expression {
  std::unique_ptr<Expression> exp;
};

struct If : Expression {
  std::unique_ptr<Expression> cond, then_branch, else_branch;
};

struct FieldAccess : Expression {
  std::unique_ptr<Expression> object;
  std::string field;
};

struct Call : Expression {
  std::string func;
  vec_ptr<Expression> args;
};

struct MethodCall : Expression {
  std::unique_ptr<Expression> object;
  std::string func;
  vec_ptr<Expression> args;
};

struct Instantiate : Expression {
  std::string type_name;
  vec_ptr<Expression> args;
};

// Proposition
struct Proposition {
  virtual ~Proposition() = default;
};

struct PExp : Proposition {
  std::unique_ptr<Expression> exp;
};

struct PFollows : Proposition {
  std::unique_ptr<Expression> e0;
  std::string property;
  vec_ptr<Expression> args;
};
// Proof
struct Use {
  std::string name;
  vec_ptr<Expression> args;
};

using Proof = vec_ptr<Use>;

// Function
struct Param {
  std::string name;
  std::unique_ptr<Type> type;
};

using Params = vec_ptr<Param>;

struct Function {
  std::string name;
  Params params;
  std::unique_ptr<Type> return_type;
  std::unique_ptr<Expression> body;

  bool is_override = false;
  vec_ptr<Proposition> conditions;
  Proof equivalence;
};

// Member and Statement

struct Member {
  virtual ~Member() = default;
};

struct SuchThat : Member {
  std::unique_ptr<Proposition> prop;
};
struct Prove : Member {
  std::unique_ptr<Proposition> goal;
  Proof proof;
};
struct ValDecl : Member {
  std::string name;
  std::unique_ptr<Type> type;
};
struct ValDef : Member {
  std::string name;
  std::unique_ptr<Type> type;
  std::unique_ptr<Expression> value;
};
struct Method : Member {
  Function fn;
};

// Graph components
struct StateComponent {
  std::unique_ptr<Expression> lower;
  std::string var;
  std::unique_ptr<Expression> upper;
};

struct Edge {
  std::unique_ptr<Expression> source, target, weight;
};

// Declaration
struct Declaration {
  virtual ~Declaration() = default;
};

struct PropertyDecl : Declaration {
  std::string name;
  Params params;
  vec_ptr<Member> requirements;
};

struct LemmaDecl : Declaration {
  std::string name;
  Params params;
  vec_ptr<Member> statements;
};

struct ClassDecl : Declaration {
  std::string name;
  Params params;
  vec_ptr<Member> members;
};

struct GraphDecl : Declaration {
  std::string name;
  Params params;
  vec_ptr<StateComponent> state;
  vec_ptr<Type> weight;
  vec_ptr<Edge> edges;
  vec_ptr<Member> members;
};

struct FunctionDecl : Declaration {
  Function fn;
};

// Program
struct Program {
  vec_ptr<Declaration> decls;
};

} // namespace nodle::ast