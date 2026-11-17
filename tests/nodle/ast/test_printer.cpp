#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "nodle/ast/ast.hpp"
#include "nodle/ast/printer.hpp"
#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"

using namespace nodle::ast;
using nodle::io::SourceFile;
using nodle::io::SourceLocation;

namespace {

class AstPrinter : public ::testing::Test {
protected:
  SourceFile::Ptr file_ = SourceFile::from_string("test.nodle", "x");

  SourceLocation loc() const { return SourceLocation(file_, 0, 1); }

  std::unique_ptr<Expression> num(long long value) const {
    return std::make_unique<EInteger>(loc(), value);
  }
  std::unique_ptr<Expression> var(std::string name) const {
    return std::make_unique<Var>(loc(), std::move(name));
  }
  std::unique_ptr<Expression> binop(BinOp op, std::unique_ptr<Expression> lhs,
                                    std::unique_ptr<Expression> rhs) const {
    return std::make_unique<EBinOp>(op, std::move(lhs), std::move(rhs));
  }
  std::unique_ptr<Type> int_type() const {
    return std::make_unique<TInteger>(loc());
  }
  std::unique_ptr<Param> param(std::string name) const {
    return std::make_unique<Param>(loc(), std::move(name), int_type());
  }

  // def <name>(): int = <body>
  Function function(std::string name, std::unique_ptr<Expression> body) const {
    Function fn(loc());
    fn.name = std::move(name);
    fn.return_type = int_type();
    fn.body = std::move(body);
    return fn;
  }

  // Builds a vec_ptr<T> from move-only items.
  template <typename T, typename... Items>
  static vec_ptr<T> list(Items... items) {
    vec_ptr<T> result;
    (result.push_back(std::move(items)), ...);
    return result;
  }
};

} // namespace

// ==== Expressions ====

TEST_F(AstPrinter, BinaryOperationIsParenthesized) {
  EXPECT_EQ(to_string(EBinOp(BinOp::Add, num(3), num(4))), "(3 + 4)");
}

TEST_F(AstPrinter, NestedBinaryOperationsShowTheTreeShape) {
  EXPECT_EQ(
      to_string(*binop(BinOp::Mul, binop(BinOp::Add, num(1), num(2)), num(3))),
      "((1 + 2) * 3)");
  EXPECT_EQ(
      to_string(*binop(BinOp::Add, num(1), binop(BinOp::Mul, num(2), num(3)))),
      "(1 + (2 * 3))");
}

TEST_F(AstPrinter, EveryBinaryOperatorUsesItsSourceSymbol) {
  std::vector<std::pair<BinOp, std::string>> cases = {
      {BinOp::Add, "+"},  {BinOp::Sub, "-"}, {BinOp::Mul, "*"},
      {BinOp::Div, "/"},  {BinOp::Mod, "%"}, {BinOp::Eq, "=="},
      {BinOp::Neq, "!="}, {BinOp::Lt, "<"},  {BinOp::Le, "<="},
      {BinOp::Gt, ">"},   {BinOp::Ge, ">="}, {BinOp::And, "&&"},
      {BinOp::Or, "||"},  {BinOp::Xor, "^"},
  };
  for (const auto &[op, symbol] : cases) {
    EXPECT_EQ(to_string(*binop(op, var("a"), var("b"))),
              "(a " + symbol + " b)");
  }
}

TEST_F(AstPrinter, Literals) {
  EXPECT_EQ(to_string(EInteger(loc(), 42)), "42");
  EXPECT_EQ(to_string(EInteger(loc(), -7)), "-7");
  EXPECT_EQ(to_string(EFloat(loc(), 1.5)), "1.5");
  EXPECT_EQ(to_string(EBool(loc(), true)), "true");
  EXPECT_EQ(to_string(EBool(loc(), false)), "false");
  EXPECT_EQ(to_string(Var(loc(), "x")), "x");
  EXPECT_EQ(to_string(Self(loc())), "self");
}

TEST_F(AstPrinter, NotAndIfAreParenthesized) {
  EXPECT_EQ(to_string(Not(loc(), var("x"))), "(!x)");
  EXPECT_EQ(to_string(If(loc(), var("c"), num(1), num(2))),
            "(if c then 1 else 2)");
}

TEST_F(AstPrinter, Tuples) {
  EXPECT_EQ(to_string(ETuple(loc(), list<Expression>(num(1), var("x")), loc())),
            "(1, x)");
  EXPECT_EQ(to_string(ETuple(loc(), {}, loc())), "()");
}

TEST_F(AstPrinter, FieldAccessesAndCalls) {
  EXPECT_EQ(to_string(FieldAccess(var("a"), loc(), "b")), "a.b");
  EXPECT_EQ(to_string(Call(var("f"), list<Expression>(num(1), num(2)), loc())),
            "f(1, 2)");

  auto method = std::make_unique<FieldAccess>(var("a"), loc(), "f");
  EXPECT_EQ(to_string(Call(std::move(method), {}, loc())), "a.f()");

  // The parentheses around `!x` keep it apart from `!(x.f)`.
  auto negated = std::make_unique<Not>(loc(), var("x"));
  EXPECT_EQ(to_string(FieldAccess(std::move(negated), loc(), "f")), "(!x).f");
}

TEST_F(AstPrinter, CallsAndFieldAccessesChain) {
  auto call = std::make_unique<Call>(var("f"), list<Expression>(num(1)), loc());
  EXPECT_EQ(to_string(Call(std::move(call), list<Expression>(num(2)), loc())),
            "f(1)(2)");

  // self.x.g().y
  std::unique_ptr<Expression> e = std::make_unique<Self>(loc());
  e = std::make_unique<FieldAccess>(std::move(e), loc(), "x");
  e = std::make_unique<FieldAccess>(std::move(e), loc(), "g");
  e = std::make_unique<Call>(std::move(e), vec_ptr<Expression>{}, loc());
  e = std::make_unique<FieldAccess>(std::move(e), loc(), "y");
  EXPECT_EQ(to_string(*e), "self.x.g().y");
}

TEST_F(AstPrinter, ExpressionsNestInsideEachOther) {
  auto args =
      list<Expression>(std::make_unique<If>(loc(), var("c"), num(1), num(2)),
                       std::make_unique<Not>(loc(), var("x")),
                       binop(BinOp::Add, var("a"), var("b")));
  EXPECT_EQ(to_string(Call(var("f"), std::move(args), loc())),
            "f((if c then 1 else 2), (!x), (a + b))");

  // !(a && (b || !c))
  auto inner =
      binop(BinOp::Or, var("b"), std::make_unique<Not>(loc(), var("c")));
  EXPECT_EQ(
      to_string(Not(loc(), binop(BinOp::And, var("a"), std::move(inner)))),
      "(!(a && (b || (!c))))");
}

TEST_F(AstPrinter, NegativeNumbersKeepTheirSign) {
  EXPECT_EQ(to_string(*binop(BinOp::Sub, num(-1), num(-2))), "(-1 - -2)");
}

// ==== Types ====

TEST_F(AstPrinter, Types) {
  EXPECT_EQ(to_string(TInteger(loc())), "int");
  EXPECT_EQ(to_string(TFloat(loc())), "float");
  EXPECT_EQ(to_string(TBoolean(loc())), "bool");
  EXPECT_EQ(to_string(TNamed(loc(), "Graph")), "Graph");

  auto values = list<Type>(int_type(), std::make_unique<TBoolean>(loc()));
  EXPECT_EQ(to_string(TTuple(loc(), std::move(values), loc())), "(int, bool)");
}

TEST_F(AstPrinter, TupleTypesNest) {
  auto pair = std::make_unique<TTuple>(
      loc(), list<Type>(int_type(), std::make_unique<TBoolean>(loc())), loc());
  auto values = list<Type>(std::move(pair), std::make_unique<TFloat>(loc()));
  EXPECT_EQ(to_string(TTuple(loc(), std::move(values), loc())),
            "((int, bool), float)");

  EXPECT_EQ(to_string(TTuple(loc(), {}, loc())), "()");
}

// ==== Propositions and proofs ====

TEST_F(AstPrinter, Propositions) {
  EXPECT_EQ(to_string(PExp(binop(BinOp::Gt, var("x"), num(0)))), "(x > 0)");
  EXPECT_EQ(to_string(PFollows(var("g"), "connected", list<Expression>(num(1)),
                               loc())),
            "g follows connected(1)");
}

TEST_F(AstPrinter, Proofs) {
  EXPECT_EQ(to_string(Use(loc(), "l", list<Expression>(var("x")), loc())),
            "use l(x)");

  Proof proof;
  proof.emplace_back(
      std::make_unique<Use>(loc(), "a", list<Expression>(var("x")), loc()));
  proof.emplace_back(
      std::make_unique<Use>(loc(), "b", vec_ptr<Expression>{}, loc()));

  std::ostringstream steps;
  Printer(steps).print(proof);
  EXPECT_EQ(steps.str(), "use a(x); use b()");

  std::ostringstream empty;
  Printer(empty).print(Proof{});
  EXPECT_EQ(empty.str(), "()");
}

// ==== Functions ====

TEST_F(AstPrinter, ParameterShowsItsType) {
  EXPECT_EQ(to_string(*param("x")), "x: int");
}

TEST_F(AstPrinter, PlainFunctionFitsOnOneLine) {
  Function fn = function("inc", binop(BinOp::Add, var("x"), num(1)));
  fn.params.push_back(param("x"));
  fn.params.push_back(param("y"));

  EXPECT_EQ(to_string(fn), "def inc(x: int, y: int): int = (x + 1)");
}

TEST_F(AstPrinter, OverrideFunctionPutsEachPartOnItsOwnLine) {
  Function fn = function("f", var("x"));
  fn.is_override = true;
  fn.equivalence.emplace();
  fn.params.push_back(param("x"));
  fn.conditions.push_back(
      std::make_unique<PExp>(binop(BinOp::Gt, var("x"), num(0))));
  fn.conditions.push_back(std::make_unique<PExp>(var("ok")));
  fn.equivalence->emplace_back(
      std::make_unique<Use>(loc(), "l", list<Expression>(var("x")), loc()));

  EXPECT_EQ(to_string(fn), "override def f(x: int): int =\n"
                           "  such that (x > 0);\n"
                           "  such that ok;\n"
                           "  x\n"
                           "  prove equivalence use l(x)");
}

TEST_F(AstPrinter, ConditionsWithoutOverrideHaveNoEquivalenceProof) {
  Function fn = function("f", var("x"));
  fn.conditions.push_back(std::make_unique<PExp>(var("ok")));

  EXPECT_EQ(to_string(fn), "def f(): int =\n"
                           "  such that ok;\n"
                           "  x");
}

TEST_F(AstPrinter, OverrideWithoutConditionsStillShowsItsProof) {
  Function fn = function("f", var("x"));
  fn.is_override = true;
  fn.equivalence.emplace();

  EXPECT_EQ(to_string(fn), "override def f(): int =\n"
                           "  x\n"
                           "  prove equivalence ()");
}

// ==== Members and graph components ====

TEST_F(AstPrinter, Members) {
  auto positive = [&] {
    return std::make_unique<PExp>(binop(BinOp::Gt, var("x"), num(0)));
  };
  Proof proof;
  proof.emplace_back(
      std::make_unique<Use>(loc(), "l", list<Expression>(var("x")), loc()));

  EXPECT_EQ(to_string(SuchThat(loc(), positive())), "such that (x > 0)");
  EXPECT_EQ(to_string(Prove(loc(), positive(), std::move(proof))),
            "prove (x > 0) with proof use l(x)");
  EXPECT_EQ(to_string(ValDecl(loc(), "x", int_type())), "val x: int");
  EXPECT_EQ(to_string(ValDef(loc(), "x", int_type(), num(3))),
            "val x: int = 3");
  EXPECT_EQ(to_string(Method(function("get", var("y")))), "def get(): int = y");
}

TEST_F(AstPrinter, GraphComponents) {
  EXPECT_EQ(to_string(StateComponent(num(0), "x", num(10))), "0 <= x < 10");
  EXPECT_EQ(to_string(Edge(var("a"), var("b"), num(1))), "edge a -> b | 1");
  EXPECT_EQ(to_string(Edge(var("a"), var("b"), nullptr)), "edge a -> b");
}

// ==== Declarations ====

TEST_F(AstPrinter, ClassMembersAreIndentedAndSeparatedBySemicolons) {
  ClassDecl decl(loc());
  decl.name = "Point";
  decl.params.push_back(param("x"));
  decl.members.push_back(
      std::make_unique<ValDef>(loc(), "y", int_type(), var("x")));
  decl.members.push_back(std::make_unique<Method>(function("get", var("y"))));

  EXPECT_EQ(to_string(decl), "class Point(x: int):\n"
                             "  val y: int = x;\n"
                             "  def get(): int = y");
}

TEST_F(AstPrinter, NestedBlocksIndentFurther) {
  Function fn = function("f", num(1));
  fn.is_override = true;
  fn.equivalence.emplace();
  fn.conditions.push_back(
      std::make_unique<PExp>(std::make_unique<EBool>(loc(), true)));

  ClassDecl decl(loc());
  decl.name = "C";
  decl.members.push_back(std::make_unique<Method>(std::move(fn)));

  EXPECT_EQ(to_string(decl), "class C():\n"
                             "  override def f(): int =\n"
                             "    such that true;\n"
                             "    1\n"
                             "    prove equivalence ()");
}

TEST_F(AstPrinter, PropertyAndLemma) {
  PropertyDecl property(loc());
  property.name = "p";
  property.params.push_back(param("x"));
  property.requirements.push_back(
      std::make_unique<ValDecl>(loc(), "y", int_type()));
  property.requirements.push_back(std::make_unique<SuchThat>(
      loc(), std::make_unique<PExp>(binop(BinOp::Gt, var("y"), var("x")))));

  EXPECT_EQ(to_string(property), "property p(x: int):\n"
                                 "  val y: int;\n"
                                 "  such that (y > x)");

  LemmaDecl lemma(loc());
  lemma.name = "l";
  lemma.params.push_back(param("x"));
  lemma.statements.push_back(std::make_unique<Prove>(
      loc(), std::make_unique<PExp>(binop(BinOp::Eq, var("x"), var("x"))),
      Proof{}));

  EXPECT_EQ(to_string(lemma), "lemma l(x: int):\n"
                              "  prove (x == x) with proof ()");
}

TEST_F(AstPrinter, GraphListsStateWeightEdgesThenMembers) {
  GraphDecl graph(loc());
  graph.name = "G";
  graph.params.push_back(param("n"));
  graph.state.push_back(
      std::make_unique<StateComponent>(num(0), "x", var("n")));
  graph.weight.push_back(int_type());
  graph.edges.push_back(std::make_unique<Edge>(
      var("x"), binop(BinOp::Add, var("x"), num(1)), num(1)));
  graph.members.push_back(std::make_unique<SuchThat>(
      loc(), std::make_unique<PExp>(binop(BinOp::Ge, var("x"), num(0)))));

  EXPECT_EQ(to_string(graph), "graph G(n: int):\n"
                              "  state (0 <= x < n);\n"
                              "  weight (int);\n"
                              "  edge x -> (x + 1) | 1;\n"
                              "  such that (x >= 0)");
}

TEST_F(AstPrinter, EmptyDeclarationsPrintOnlyTheirHeader) {
  ClassDecl decl(loc());
  decl.name = "C";
  EXPECT_EQ(to_string(decl), "class C():");

  PropertyDecl property(loc());
  property.name = "p";
  EXPECT_EQ(to_string(property), "property p():");

  LemmaDecl lemma(loc());
  lemma.name = "l";
  EXPECT_EQ(to_string(lemma), "lemma l():");
}

TEST_F(AstPrinter, EmptyGraphStillListsStateAndWeight) {
  GraphDecl graph(loc());
  graph.name = "G";

  EXPECT_EQ(to_string(graph), "graph G():\n"
                              "  state ();\n"
                              "  weight ()");
}

TEST_F(AstPrinter, GraphMembersIndentLikeClassMembers) {
  Function fn = function("f", num(1));
  fn.is_override = true;
  fn.equivalence.emplace();

  GraphDecl graph(loc());
  graph.name = "G";
  graph.edges.push_back(std::make_unique<Edge>(var("a"), var("b"), nullptr));
  graph.members.push_back(std::make_unique<Method>(std::move(fn)));

  EXPECT_EQ(to_string(graph), "graph G():\n"
                              "  state ();\n"
                              "  weight ();\n"
                              "  edge a -> b;\n"
                              "  override def f(): int =\n"
                              "    1\n"
                              "    prove equivalence ()");
}

TEST_F(AstPrinter, ProgramSeparatesDeclarationsWithABlankLine) {
  Program program(loc());
  program.decls.push_back(
      std::make_unique<FunctionDecl>(function("a", num(1))));
  program.decls.push_back(
      std::make_unique<FunctionDecl>(function("b", num(2))));

  EXPECT_EQ(to_string(program), "def a(): int = 1\n"
                                "\n"
                                "def b(): int = 2");
}

TEST_F(AstPrinter, EmptyProgramPrintsNothing) {
  EXPECT_EQ(to_string(Program(loc())), "");
}

TEST_F(AstPrinter, IndentationResetsAfterEachDeclaration) {
  auto decl = std::make_unique<ClassDecl>(loc());
  decl->name = "C";
  decl->members.push_back(std::make_unique<ValDecl>(loc(), "x", int_type()));

  Function fn = function("f", num(1));
  fn.is_override = true;
  fn.equivalence.emplace();

  Program program(loc());
  program.decls.push_back(std::move(decl));
  program.decls.push_back(std::make_unique<FunctionDecl>(std::move(fn)));
  program.decls.push_back(
      std::make_unique<FunctionDecl>(function("g", num(2))));

  EXPECT_EQ(to_string(program), "class C():\n"
                                "  val x: int\n"
                                "\n"
                                "override def f(): int =\n"
                                "  1\n"
                                "  prove equivalence ()\n"
                                "\n"
                                "def g(): int = 2");
}

// ==== Robustness and entry points ====

TEST_F(AstPrinter, MissingChildrenPrintAsNull) {
  EXPECT_EQ(to_string(Function(loc())), "def (): <null> = <null>");
}

TEST_F(AstPrinter, MissingChildrenInListsAndBlocksPrintAsNull) {
  vec_ptr<Type> values;
  values.push_back(nullptr);
  EXPECT_EQ(to_string(TTuple(loc(), std::move(values), loc())), "(<null>)");

  ClassDecl decl(loc());
  decl.name = "C";
  decl.members.push_back(nullptr);
  EXPECT_EQ(to_string(decl), "class C():\n"
                             "  <null>");
}

TEST_F(AstPrinter, NodesCanBeStreamed) {
  std::unique_ptr<Expression> e = binop(BinOp::Add, num(3), num(4));
  std::ostringstream out;
  out << *e;
  EXPECT_EQ(out.str(), "(3 + 4)");
}
