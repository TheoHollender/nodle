#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "nodle/ast/ast.hpp"
#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include "nodle/lexer/lexer.hpp"
#include "nodle/lexer/token.hpp"

using namespace nodle::ast;
using nodle::io::SourceFile;
using nodle::io::SourceLocation;
using nodle::lexer::Lexer;
using nodle::lexer::Token;

namespace {

// Lexes a source string and builds AST nodes from its tokens the way the
// parser does, so that node locations can be checked against the source text.
class AstLocation : public ::testing::Test {
protected:
  void lex(const std::string &source) {
    file_ = SourceFile::from_string("test.nodle", source);
    tokens_ = Lexer().lex(file_);
  }

  // Location of the `nth` (0-based) token whose text is `text`.
  SourceLocation tok(std::string_view text, std::size_t nth = 0) const {
    for (const Token &t : tokens_) {
      if (t.text() == text && nth-- == 0) {
        return t.source_location();
      }
    }
    throw std::logic_error("no such token: '" + std::string(text) + "'");
  }

  std::unique_ptr<Expression> var(std::string_view name,
                                  std::size_t nth = 0) const {
    return std::make_unique<Var>(tok(name, nth), std::string(name));
  }

  SourceLocation whole_file() const {
    return SourceLocation(file_, 0, file_->content().size());
  }

  static std::string text(const Node &node) {
    return std::string(node.source_location.text());
  }

  SourceFile::Ptr file_;
  std::vector<Token> tokens_;
};

} // namespace

// ==== Node ====

TEST(AstNode, NodesCannotBeBuiltWithoutALocation) {
  static_assert(!std::is_default_constructible_v<Node>);
  static_assert(!std::is_default_constructible_v<EInteger>);
  static_assert(!std::is_default_constructible_v<EBinOp>);
  static_assert(!std::is_default_constructible_v<TInteger>);
  static_assert(!std::is_default_constructible_v<Function>);
  static_assert(!std::is_default_constructible_v<Program>);
}

TEST_F(AstLocation, ExtendGrowsTheLocation) {
  lex("(a)");
  auto a = var("a");
  a->extend(tok("("));
  EXPECT_EQ(text(*a), "(a");
  a->extend(tok(")"));
  EXPECT_EQ(text(*a), "(a)");
}

TEST_F(AstLocation, ExtendNeverShrinksTheLocation) {
  lex("abc + d");
  auto sum = std::make_unique<EBinOp>(BinOp::Add, var("abc"), var("d"));
  sum->extend(tok("+"));
  EXPECT_EQ(text(*sum), "abc + d");
}

// ==== Leaves ====

TEST_F(AstLocation, LiteralsTakeTheLocationOfTheirToken) {
  lex("42 3.5 true");
  EInteger i(tok("42"), 42);
  EFloat f(tok("3.5"), 3.5);
  EBool b(tok("true"), true);

  EXPECT_EQ(text(i), "42");
  EXPECT_EQ(i.value, 42);
  EXPECT_EQ(text(f), "3.5");
  EXPECT_DOUBLE_EQ(f.value, 3.5);
  EXPECT_EQ(text(b), "true");
  EXPECT_TRUE(b.value);
}

TEST_F(AstLocation, VarAndSelfTakeTheLocationOfTheirToken) {
  lex("x self");
  Var x(tok("x"), "x");
  Self self(tok("self"));

  EXPECT_EQ(text(x), "x");
  EXPECT_EQ(x.name, "x");
  EXPECT_EQ(text(self), "self");
}

TEST_F(AstLocation, ScalarAndNamedTypesTakeTheLocationOfTheirToken) {
  lex("int float bool Graph");
  TInteger i(tok("int"));
  TFloat f(tok("float"));
  TBoolean b(tok("bool"));
  TNamed n(tok("Graph"), "Graph");

  EXPECT_EQ(text(i), "int");
  EXPECT_EQ(text(f), "float");
  EXPECT_EQ(text(b), "bool");
  EXPECT_EQ(text(n), "Graph");
  EXPECT_EQ(n.name, "Graph");
}

// ==== Expressions ====

TEST_F(AstLocation, BinOpSpansFromLhsToRhs) {
  lex("a + b");
  EBinOp sum(BinOp::Add, var("a"), var("b"));

  EXPECT_EQ(text(sum), "a + b");
  EXPECT_EQ(sum.op, BinOp::Add);
}

TEST_F(AstLocation, ChildrenKeepTheirOwnLocation) {
  lex("a + b");
  EBinOp sum(BinOp::Add, var("a"), var("b"));

  EXPECT_EQ(text(*sum.lhs), "a");
  EXPECT_EQ(text(*sum.rhs), "b");
}

TEST_F(AstLocation, NestedBinOpsSpanAllOperands) {
  lex("a + b * c");
  auto mul = std::make_unique<EBinOp>(BinOp::Mul, var("b"), var("c"));
  EBinOp sum(BinOp::Add, var("a"), std::move(mul));

  EXPECT_EQ(text(*sum.rhs), "b * c");
  EXPECT_EQ(text(sum), "a + b * c");
}

TEST_F(AstLocation, ParenthesizedOperandIsIncludedOnceExtended) {
  lex("(a + b) * c");
  auto sum = std::make_unique<EBinOp>(BinOp::Add, var("a"), var("b"));
  sum->extend(tok("(") | tok(")"));
  EBinOp mul(BinOp::Mul, std::move(sum), var("c"));

  EXPECT_EQ(text(*mul.lhs), "(a + b)");
  EXPECT_EQ(text(mul), "(a + b) * c");
}

TEST_F(AstLocation, NestedParenthesesAreAllIncluded) {
  lex("((a))");
  auto a = var("a");
  a->extend(tok("(", 1) | tok(")", 0));
  a->extend(tok("(", 0) | tok(")", 1));

  EXPECT_EQ(text(*a), "((a))");
}

TEST_F(AstLocation, NotIncludesTheOperator) {
  lex("!(a + b)");
  auto sum = std::make_unique<EBinOp>(BinOp::Add, var("a"), var("b"));
  sum->extend(tok("(") | tok(")"));
  Not neg(tok("!"), std::move(sum));

  EXPECT_EQ(text(neg), "!(a + b)");
}

TEST_F(AstLocation, IfSpansFromKeywordToElseBranch) {
  lex("if c then a else b");
  If e(tok("if"), var("c"), var("a"), var("b"));

  EXPECT_EQ(text(e), "if c then a else b");
}

TEST_F(AstLocation, TupleIncludesItsParentheses) {
  lex("(2, 3.5)");
  vec_ptr<Expression> values;
  values.push_back(std::make_unique<EInteger>(tok("2"), 2));
  values.push_back(std::make_unique<EFloat>(tok("3.5"), 3.5));
  ETuple t(tok("("), std::move(values), tok(")"));

  EXPECT_EQ(text(t), "(2, 3.5)");
  ASSERT_EQ(t.values.size(), 2u);
  EXPECT_EQ(text(*t.values[1]), "3.5");
}

TEST_F(AstLocation, FieldAccessSpansFromObjectToField) {
  lex("obj.field");
  FieldAccess access(var("obj"), tok("field"), "field");

  EXPECT_EQ(text(access), "obj.field");
  EXPECT_EQ(access.field, "field");
}

TEST_F(AstLocation, NodesCanBeOwnedThroughTheirBase) {
  lex("a + b");
  std::unique_ptr<Node> node =
      std::make_unique<EBinOp>(BinOp::Add, var("a"), var("b"));

  EXPECT_EQ(text(*node), "a + b");
  EXPECT_NE(dynamic_cast<EBinOp *>(node.get()), nullptr);
}

// ==== Types ====

TEST_F(AstLocation, TupleTypeIncludesItsParentheses) {
  lex("(int, bool)");
  vec_ptr<Type> values;
  values.push_back(std::make_unique<TInteger>(tok("int")));
  values.push_back(std::make_unique<TBoolean>(tok("bool")));
  TTuple t(tok("("), std::move(values), tok(")"));

  EXPECT_EQ(text(t), "(int, bool)");
}

// ==== Propositions and proofs ====

TEST_F(AstLocation, PropositionExpressionHasTheLocationOfItsExpression) {
  lex("a < b");
  PExp p(std::make_unique<EBinOp>(BinOp::Lt, var("a"), var("b")));

  EXPECT_EQ(text(p), "a < b");
}

TEST_F(AstLocation, FollowsSpansFromExpressionToEnd) {
  lex("p follows Q(a)");
  vec_ptr<Expression> args;
  args.push_back(var("a"));
  PFollows p(var("p"), "Q", std::move(args), tok(")"));

  EXPECT_EQ(text(p), "p follows Q(a)");
  EXPECT_EQ(p.property, "Q");
}

TEST_F(AstLocation, UseSpansFromKeywordToEnd) {
  lex("use lem(q)");
  vec_ptr<Expression> args;
  args.push_back(var("q"));
  Use use(tok("use"), "lem", std::move(args), tok(")"));

  EXPECT_EQ(text(use), "use lem(q)");
  EXPECT_EQ(use.name, "lem");
}

// ==== Members ====

TEST_F(AstLocation, ProveWithoutProofEndsAtGoal) {
  lex("prove a < b");
  auto goal = std::make_unique<PExp>(
      std::make_unique<EBinOp>(BinOp::Lt, var("a"), var("b")));
  Prove prove(tok("prove"), std::move(goal), {});

  EXPECT_EQ(text(prove), "prove a < b");
}

TEST_F(AstLocation, SuchThatSpansFromKeywordToProposition) {
  lex("such that x");
  SuchThat st(tok("such"), std::make_unique<PExp>(var("x")));

  EXPECT_EQ(text(st), "such that x");
}

TEST_F(AstLocation, ValDeclSpansFromKeywordToType) {
  lex("val v: int");
  ValDecl decl(tok("val"), "v", std::make_unique<TInteger>(tok("int")));

  EXPECT_EQ(text(decl), "val v: int");
  EXPECT_EQ(decl.name, "v");
}

TEST_F(AstLocation, ValDefSpansFromKeywordToValue) {
  lex("val v: int = 42");
  ValDef def(tok("val"), "v", std::make_unique<TInteger>(tok("int")),
             std::make_unique<EInteger>(tok("42"), 42));

  EXPECT_EQ(text(def), "val v: int = 42");
}

TEST_F(AstLocation, ValDefWithoutTypeSpansFromKeywordToValue) {
  lex("val v = 42");
  ValDef def(tok("val"), "v", nullptr,
             std::make_unique<EInteger>(tok("42"), 42));

  EXPECT_EQ(text(def), "val v = 42");
  EXPECT_EQ(def.type, nullptr);
}

TEST_F(AstLocation, ParamSpansFromNameToType) {
  lex("x: int");
  Param param(tok("x"), "x", std::make_unique<TInteger>(tok("int")));

  EXPECT_EQ(text(param), "x: int");
  EXPECT_EQ(param.name, "x");
}

// ==== Functions ====

TEST_F(AstLocation, FunctionIsExtendedFromFirstToLastToken) {
  lex("override def f(x: int): int = x");
  Function fn(tok("override"));
  fn.is_override = true;
  fn.name = "f";
  fn.params.push_back(std::make_unique<Param>(
      tok("x", 0), "x", std::make_unique<TInteger>(tok("int", 0))));
  fn.return_type = std::make_unique<TInteger>(tok("int", 1));
  fn.body = var("x", 1);
  fn.extend(fn.body->source_location);

  EXPECT_EQ(text(fn), "override def f(x: int): int = x");
  EXPECT_EQ(text(*fn.params[0]), "x: int");
}

TEST_F(AstLocation, MethodAndFunctionDeclTakeTheLocationOfTheirFunction) {
  lex("def f(): int = 1");
  Function fn(tok("def"));
  fn.body = std::make_unique<EInteger>(tok("1"), 1);
  fn.extend(fn.body->source_location);

  Method method{Function(fn.source_location)};
  FunctionDecl decl(std::move(fn));

  EXPECT_EQ(text(method), "def f(): int = 1");
  EXPECT_EQ(text(decl), "def f(): int = 1");
  EXPECT_EQ(text(decl.fn), "def f(): int = 1");
}

// ==== Graph components ====

TEST_F(AstLocation, StateComponentSpansFromLowerToUpperBound) {
  lex("0 <= i < n");
  StateComponent state(std::make_unique<EInteger>(tok("0"), 0), "i", var("n"));

  EXPECT_EQ(text(state), "0 <= i < n");
  EXPECT_EQ(state.var, "i");
}

TEST_F(AstLocation, EdgeSpansFromSourceToWeight) {
  lex("a -- b : 3");
  Edge edge(var("a"), var("b"), std::make_unique<EInteger>(tok("3"), 3));

  EXPECT_EQ(text(edge), "a -- b : 3");
}

TEST_F(AstLocation, EdgeWithoutWeightEndsAtTarget) {
  lex("a -- b");
  Edge edge(var("a"), var("b"), nullptr);

  EXPECT_EQ(text(edge), "a -- b");
  EXPECT_EQ(edge.weight, nullptr);
}

// ==== Declarations and program ====

TEST_F(AstLocation, DeclarationIsExtendedFromKeywordToLastMember) {
  lex("class C(x: int) val v: int");
  ClassDecl decl(tok("class"));
  decl.name = "C";
  decl.params.push_back(std::make_unique<Param>(
      tok("x"), "x", std::make_unique<TInteger>(tok("int", 0))));
  decl.members.push_back(std::make_unique<ValDecl>(
      tok("val"), "v", std::make_unique<TInteger>(tok("int", 1))));
  decl.extend(decl.members.back()->source_location);

  EXPECT_EQ(text(decl), "class C(x: int) val v: int");
  EXPECT_EQ(text(*decl.members[0]), "val v: int");
}

TEST_F(AstLocation, ProgramSpansTheWholeFile) {
  lex("def f(): int = 1\ndef g(): int = 2\n");
  Program program(whole_file());
  program.decls.push_back(
      std::make_unique<FunctionDecl>(Function(tok("def", 0) | tok("1"))));
  program.decls.push_back(
      std::make_unique<FunctionDecl>(Function(tok("def", 1) | tok("2"))));

  EXPECT_EQ(text(program), "def f(): int = 1\ndef g(): int = 2\n");
  EXPECT_EQ(text(*program.decls[1]), "def g(): int = 2");
}
