#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "./test_helper.hpp"

using parser_test::make_context;
using parser_test::parse_source;
using parser_test::program_of;
using parser_test::show;

namespace {

// The message of the std::runtime_error thrown by the parser for `source` (""
// if none).
std::string error_of(const std::string &source) {
  try {
    parse_source(source);
  } catch (const std::runtime_error &e) {
    return e.what();
  }
  return "";
}

void expect_syntax_error(const std::string &source) {
  EXPECT_THROW(parse_source(source), std::runtime_error)
      << "program: " << source;
}

} // namespace

// ------------------------------------------------------------------ functions

TEST(ParserFunction, Minimal) {
  EXPECT_EQ(program_of("def f() : Integer = 1"), "(def f () Integer [] 1)");
}

TEST(ParserFunction, Parameters) {
  EXPECT_EQ(program_of("def add(x : Integer, y : Integer) : Integer = x + y"),
            "(def add (x:Integer y:Integer) Integer [] (+ x y))");
}

TEST(ParserFunction, ReturnTypes) {
  EXPECT_EQ(program_of("def f() : Boolean = true"),
            "(def f () Boolean [] true)");
  EXPECT_EQ(program_of("def f() : Float = 1.5"), "(def f () Float [] 1.5)");
  EXPECT_EQ(program_of("def f() : (Integer, Float) = (1, 2.5)"),
            "(def f () (tuple Integer Float) [] (tuple 1 2.5))");
  EXPECT_EQ(program_of("def f(p : Point) : Point = p"),
            "(def f (p:Point) Point [] p)");
}

TEST(ParserFunction, ComplexBody) {
  EXPECT_EQ(
      program_of("def abs(x : Integer) : Integer = if x < 0 then 0 - x else x"),
      "(def abs (x:Integer) Integer [] (if (< x 0) (- 0 x) x))");
}

TEST(ParserFunction, Conditions) {
  EXPECT_EQ(program_of("def f(x : Integer) : Integer = such that x > 0 x"),
            "(def f (x:Integer) Integer [(> x 0)] x)");
  EXPECT_EQ(
      program_of(
          "def f(x : Integer) : Integer = such that x > 0 such that x < 9 x"),
      "(def f (x:Integer) Integer [(> x 0) (< x 9)] x)");
  EXPECT_EQ(
      program_of("def f(x : Integer) : Integer = such that x follows p(1) x"),
      "(def f (x:Integer) Integer [(follows x p 1)] x)");
}

TEST(ParserFunction, FlagsOfPlainFunction) {
  auto program = parse_source("def f() : Integer = 1");
  const auto &f = parser_test::only_function(*program);
  EXPECT_FALSE(f.is_override);
  EXPECT_FALSE(f.equivalence.has_value());
  EXPECT_EQ(f.name, "f");
}

TEST(ParserFunction, NameIsKept) {
  auto program = parse_source("def some_long_name2() : Integer = 1");
  EXPECT_EQ(parser_test::only_function(*program).name, "some_long_name2");
}

TEST(ParserFunctionErrors, Malformed) {
  expect_syntax_error("def");
  expect_syntax_error("def f");
  expect_syntax_error("def f(");
  expect_syntax_error("def f()");
  expect_syntax_error("def f() :");
  expect_syntax_error("def f() : Integer");
  expect_syntax_error("def f() : Integer =");
  expect_syntax_error("def f() Integer = 1");
  expect_syntax_error("def f : Integer = 1");
  expect_syntax_error("def () : Integer = 1");
  expect_syntax_error("def f(x) : Integer = 1");
  expect_syntax_error("def f(x : Integer : Integer = 1");
  expect_syntax_error("def f() : Integer 1");
  expect_syntax_error("def f() = 1");
  expect_syntax_error("def 1() : Integer = 1");
  expect_syntax_error("def val() : Integer = 1");
}

TEST(ParserFunctionErrors, TrailingTokens) {
  EXPECT_EQ(error_of("def f() : Integer = 1 2"), "Some tokens remain.");
  EXPECT_EQ(error_of("def f() : Integer = 1 )"), "Some tokens remain.");
  EXPECT_EQ(error_of("def f() : Integer = 1 ;"), "Some tokens remain.");
}

// ------------------------------------------------------------------ override

TEST(ParserOverride, WithEmptyProof) {
  EXPECT_EQ(program_of("override def f() : Integer = 1 prove equivalence ()"),
            "(override-def f () Integer [] 1 equiv [])");
}

TEST(ParserOverride, WithProof) {
  EXPECT_EQ(
      program_of(
          "override def f(x : Integer) : Integer = x + 1 prove equivalence "
          "use a(x); use b()"),
      "(override-def f (x:Integer) Integer [] (+ x 1) equiv [(use a x) (use "
      "b)])");
}

TEST(ParserOverride, WithConditions) {
  EXPECT_EQ(
      program_of("override def f(x : Integer) : Integer = such that x > 0 x "
                 "prove equivalence use a()"),
      "(override-def f (x:Integer) Integer [(> x 0)] x equiv [(use a)])");
}

TEST(ParserOverride, ProofIsNotConfusedWithTheBody) {
  EXPECT_EQ(program_of(
                "override def f() : Integer = g(1) prove equivalence use a(2)"),
            "(override-def f () Integer [] (call g 1) equiv [(use a 2)])");
}

TEST(ParserOverride, FlagsAndEquivalence) {
  auto program =
      parse_source("override def f() : Integer = 1 prove equivalence use a()");
  const auto &f = parser_test::only_function(*program);
  EXPECT_TRUE(f.is_override);
  ASSERT_TRUE(f.equivalence.has_value());
  EXPECT_EQ(f.equivalence->size(), 1u);
}

TEST(ParserOverride, EmptyProofStillSetsTheEquivalence) {
  auto program =
      parse_source("override def f() : Integer = 1 prove equivalence ()");
  const auto &f = parser_test::only_function(*program);
  ASSERT_TRUE(f.equivalence.has_value());
  EXPECT_TRUE(f.equivalence->empty());
}

TEST(ParserOverrideErrors, Malformed) {
  expect_syntax_error("override");
  expect_syntax_error("override def f() : Integer = 1");
  expect_syntax_error("override def f() : Integer = 1 prove");
  expect_syntax_error("override def f() : Integer = 1 prove equivalence");
  expect_syntax_error("override def f() : Integer = 1 equivalence ()");
  expect_syntax_error("override f() : Integer = 1 prove equivalence ()");
  expect_syntax_error(
      "override override def f() : Integer = 1 prove equivalence ()");
  expect_syntax_error("override def f() : Integer = 1 prove equivalence use a");
}

// ------------------------------------------------------------------ lemma and
// statements

TEST(ParserLemma, SuchThatStatement) {
  EXPECT_EQ(program_of("lemma l() : such that x > 0"),
            "(lemma l () (such (> x 0)))");
}

TEST(ParserLemma, ProveWithProofStatement) {
  EXPECT_EQ(program_of("lemma l() : prove x > 0 with proof use a(x)"),
            "(lemma l () (prove (> x 0) [(use a x)]))");
  EXPECT_EQ(program_of("lemma l() : prove x > 0 with proof ()"),
            "(lemma l () (prove (> x 0) []))");
}

TEST(ParserLemma, PropositionWithFollows) {
  EXPECT_EQ(program_of("lemma l() : prove x follows p(1) with proof ()"),
            "(lemma l () (prove (follows x p 1) []))");
  EXPECT_EQ(program_of("lemma l() : such that x follows p()"),
            "(lemma l () (such (follows x p)))");
}

TEST(ParserLemma, SeveralStatements) {
  EXPECT_EQ(program_of("lemma l() : such that a such that b"),
            "(lemma l () (such a) (such b))");
  EXPECT_EQ(
      program_of(
          "lemma l() : such that a prove b with proof use p() such that c"),
      "(lemma l () (such a) (prove b [(use p)]) (such c))");
}

TEST(ParserLemma, Parameters) {
  EXPECT_EQ(program_of("lemma l(x : Integer, y : Float) : such that x > 0"),
            "(lemma l (x:Integer y:Float) (such (> x 0)))");
}

TEST(ParserLemma, ProofStepsDoNotSwallowTheNextStatement) {
  EXPECT_EQ(program_of("lemma l() : prove a with proof use p(1) prove b with "
                       "proof use q(2)"),
            "(lemma l () (prove a [(use p 1)]) (prove b [(use q 2)]))");
}

TEST(ParserLemmaErrors, Malformed) {
  expect_syntax_error("lemma");
  expect_syntax_error("lemma l");
  expect_syntax_error("lemma l()");
  expect_syntax_error("lemma l() :");
  expect_syntax_error("lemma l() : such");
  expect_syntax_error("lemma l() : such that");
  expect_syntax_error("lemma l() : prove");
  expect_syntax_error("lemma l() : prove x");
  expect_syntax_error("lemma l() : prove x with");
  expect_syntax_error("lemma l() : prove x with proof");
  expect_syntax_error("lemma l() : prove x proof ()");
  expect_syntax_error("lemma : such that x");
  expect_syntax_error("lemma l() : val x : Integer"); // not a statement
  expect_syntax_error("lemma l() : def f() : Integer = 1");
}

// ------------------------------------------------------------------ property

TEST(ParserProperty, SuchThatRequirement) {
  EXPECT_EQ(program_of("property p() : such that x > 0"),
            "(property p () (such (> x 0)))");
}

TEST(ParserProperty, ValRequirement) {
  EXPECT_EQ(program_of("property p() : val x : Integer"),
            "(property p () (val x Integer))");
  EXPECT_EQ(program_of("property p() : val x : (Integer, Float)"),
            "(property p () (val x (tuple Integer Float)))");
}

TEST(ParserProperty, SeveralRequirements) {
  EXPECT_EQ(
      program_of("property p(a : Integer) : val x : Integer such that x > a "
                 "val y : Float"),
      "(property p (a:Integer) (val x Integer) (such (> x a)) (val y Float))");
}

TEST(ParserPropertyErrors, Malformed) {
  expect_syntax_error("property");
  expect_syntax_error("property p");
  expect_syntax_error("property p()");
  expect_syntax_error("property p() :");
  expect_syntax_error("property p() : val");
  expect_syntax_error("property p() : val x");
  expect_syntax_error("property p() : val x :");
  expect_syntax_error("property : val x : Integer");
}

TEST(ParserPropertyErrors, OnlyRequirementsAreAllowed) {
  expect_syntax_error("property p() : prove x with proof ()");
  expect_syntax_error("property p() : def f() : Integer = 1");
  expect_syntax_error(
      "property p() : val x : Integer = 1"); // a requirement has no value
}

// ------------------------------------------------------------------ class

TEST(ParserClass, ValDefinition) {
  EXPECT_EQ(program_of("class C() : val x : Integer = 1"),
            "(class C () (val x Integer 1))");
}

TEST(ParserClass, Method) {
  EXPECT_EQ(program_of("class C() : method def f() : Integer = 1"),
            "(class C () (method (def f () Integer [] 1)))");
}

TEST(ParserClass, OverrideMethod) {
  EXPECT_EQ(
      program_of(
          "class C() : method override def f() : Integer = 1 prove equivalence ()"),
      "(class C () (method (override-def f () Integer [] 1 equiv [])))");
}

TEST(ParserClass, Statements) {
  EXPECT_EQ(program_of("class C() : such that true"),
            "(class C () (such true))");
  EXPECT_EQ(program_of("class C() : prove x with proof ()"),
            "(class C () (prove x []))");
}

TEST(ParserClass, ParametersAndSeveralMembers) {
  EXPECT_EQ(
      program_of("class Point(a : Integer, b : Integer) : "
                 "val x : Integer = a "
                 "val y : Integer = b "
                 "method def sum() : Integer = x + y "
                 "such that x >= 0 "
                 "prove x >= 0 with proof use nonneg(a)"),
      "(class Point (a:Integer b:Integer) (val x Integer a) (val y Integer b) "
      "(method (def sum () Integer [] (+ x y))) (such (>= x 0)) "
      "(prove (>= x 0) [(use nonneg a)]))");
}

TEST(ParserClass, MethodBodyEndsBeforeTheNextMember) {
  EXPECT_EQ(program_of("class C() : method def f() : Integer = x val y : Integer = 2"),
            "(class C () (method (def f () Integer [] x)) (val y Integer 2))");
  EXPECT_EQ(
      program_of("class C() : method def f() : Integer = x method def g() : Integer = y"),
      "(class C () (method (def f () Integer [] x)) (method (def g () Integer "
      "[] y)))");
}

TEST(ParserClass, ValueEndsBeforeTheNextMember) {
  EXPECT_EQ(program_of("class C() : val a : Integer = f(1) such that a > 0"),
            "(class C () (val a Integer (call f 1)) (such (> a 0)))");
}

TEST(ParserClassErrors, Malformed) {
  expect_syntax_error("class");
  expect_syntax_error("class C");
  expect_syntax_error("class C()");
  expect_syntax_error("class C() :");
  expect_syntax_error("class : val x : Integer = 1");
  expect_syntax_error(
      "class C() : val x : Integer"); // a class value needs a definition
  expect_syntax_error("class C() : val x : Integer =");
  expect_syntax_error("class C() : val x = 1");
  expect_syntax_error("class C() : x");
  expect_syntax_error("class C() : class D() : val y : Integer = 1");
}

// ------------------------------------------------------------------ whole
// programs

TEST(ParserProgram, SeveralDeclarationsKeepTheirOrder) {
  EXPECT_EQ(program_of("def f() : Integer = 1 "
                       "lemma l() : such that true "
                       "property p() : val x : Integer "
                       "def g() : Integer = 3 "
                       "class C() : val y : Integer = 2"),
            "(def f () Integer [] 1) (lemma l () (such true)) (property p () "
            "(val x Integer)) "
            "(def g () Integer [] 3) (class C () (val y Integer 2))");
}

// A function after a class is parsed as one more method of the class: both `d
// ::= F` and `m ::= F` accept it, and the parser has no separator to tell them
// apart. The grammar separates members with `;`, which the parser does not
// implement yet (see the next test).
TEST(ParserProgram, FunctionAfterAClassIsATopLevelDeclaration) {
  EXPECT_EQ(program_of("class C() : val y : Integer = 2 def g() : Integer = 3"),
            "(class C () (val y Integer 2)) (def g () Integer [] 3)");
}

// The grammar writes `class C(ps) : m1 ; ... ; mn` (same for property and
// lemma).
TEST(ParserClass, MembersAreSeparatedBySemicolons) {
  EXPECT_EQ(program_of("class C() : val x : Integer = 1 val y : Integer = 2"),
            "(class C () (val x Integer 1) (val y Integer 2))");
  EXPECT_EQ(program_of("property p() : val x : Integer such that x > 0"),
            "(property p () (val x Integer) (such (> x 0)))");
  EXPECT_EQ(program_of("lemma l() : such that a such that b"),
            "(lemma l () (such a) (such b))");
}

TEST(ParserProgram, DeclarationCount) {
  auto program = parse_source(
      "def a() : Integer = 1 def b() : Integer = 2 def c() : Integer = 3");
  EXPECT_EQ(program->decls.size(), 3u);
}

TEST(ParserProgram, DeclarationKinds) {
  auto program = parse_source("def a() : Integer = 1 "
                              "lemma l() : such that true "
                              "property p() : val x : Integer "
                              "class C() : val y : Integer = 2");
  ASSERT_EQ(program->decls.size(), 4u);
  using namespace nodle::ast;
  EXPECT_NE(dynamic_cast<FunctionDecl *>(program->decls[0].get()), nullptr);
  EXPECT_NE(dynamic_cast<LemmaDecl *>(program->decls[1].get()), nullptr);
  EXPECT_NE(dynamic_cast<PropertyDecl *>(program->decls[2].get()), nullptr);
  EXPECT_NE(dynamic_cast<ClassDecl *>(program->decls[3].get()), nullptr);
}

TEST(ParserProgram, WhitespaceAndNewlinesDoNotMatter) {
  EXPECT_EQ(program_of("def\n  f(\n x : Integer\n)\n :\n Integer\n =\n "
                       "x\n\n\ndef g() : Integer = 1"),
            "(def f (x:Integer) Integer [] x) (def g () Integer [] 1)");
}

TEST(ParserProgram, ManyDeclarations) {
  std::string source, expected;
  for (int i = 0; i < 200; ++i) {
    std::string n = std::to_string(i);
    source += "def f" + n + "() : Integer = " + n + " ";
    expected += (expected.empty() ? "" : " ") + std::string("(def f") + n +
                " () Integer [] " + n + ")";
  }
  EXPECT_EQ(program_of(source), expected);
}

TEST(ParserProgram, ParserCanBeUsedRepeatedly) {
  // the rules are shared (static): parsing must not leave any state behind
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(program_of("def f() : Integer = 1"), "(def f () Integer [] 1)");
    EXPECT_EQ(program_of("lemma l() : such that x"), "(lemma l () (such x))");
    EXPECT_THROW(parse_source("def f() : Integer ="), std::runtime_error);
  }
}

TEST(ParserProgram, TheWholeStreamIsConsumed) {
  auto ctx = make_context("def f() : Integer = 1 def g() : Integer = 2");
  nodle::parser::Parser parser;
  auto program = parser.parse(ctx);
  ASSERT_NE(program, nullptr);
  EXPECT_FALSE(ctx->remaining());
}

// ------------------------------------------------------------------
// program-level errors

TEST(ParserProgramErrors, EmptyProgram) {
  EXPECT_EQ(error_of(""), "Empty program, unhandled.");
  EXPECT_EQ(error_of("   \n  "), "Empty program, unhandled.");
}

TEST(ParserProgramErrors, NoDeclarationAtTheStart) {
  EXPECT_EQ(error_of("1"), "Error on first token.");
  EXPECT_EQ(error_of("x"), "Error on first token.");
  EXPECT_EQ(error_of("val x : Integer = 1"),
            "Error on first token."); // a member, not a declaration
  EXPECT_EQ(error_of("such that x"), "Error on first token.");
  EXPECT_EQ(error_of("prove x with proof ()"), "Error on first token.");
  EXPECT_EQ(error_of(")"), "Error on first token.");
  EXPECT_EQ(error_of("="), "Error on first token.");
}

TEST(ParserProgramErrors, GarbageAfterADeclaration) {
  EXPECT_EQ(error_of("def f() : Integer = 1 )"), "Some tokens remain.");
  EXPECT_EQ(error_of("def f() : Integer = 1 x"), "Some tokens remain.");
  EXPECT_EQ(error_of("def f() : Integer = 1 val x : Integer = 2"),
            "Some tokens remain.");
}

TEST(ParserProgramErrors, BrokenSecondDeclaration) {
  expect_syntax_error("def f() : Integer = 1 def g() : Integer =");
  expect_syntax_error("def f() : Integer = 1 lemma");
  expect_syntax_error("def f() : Integer = 1 class C()");
}

TEST(ParserProgramErrors, ErrorsAreStdRuntimeErrors) {
  EXPECT_THROW(parse_source("def"), std::runtime_error);
  EXPECT_THROW(parse_source("1"), std::runtime_error);
  EXPECT_THROW(parse_source(""), std::runtime_error);
}