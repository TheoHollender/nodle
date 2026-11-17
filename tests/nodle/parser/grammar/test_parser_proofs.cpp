#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "./test_helper.hpp"

using parser_test::conditions_of;
using parser_test::params_of;
using parser_test::parse_source;
using parser_test::proof_of;
using parser_test::type_of;

// Types are parsed as the return type of `def f() : <type> = 1`, parameters as
// `def f(<ps>) : ...`, propositions as `such that` conditions of a function and
// proofs inside `lemma l() : prove 1 with proof <proof>`; see ast_print.hpp.

// ------------------------------------------------------------------ types

TEST(ParserType, BasicTypes) {
  EXPECT_EQ(type_of("Integer"), "Integer");
  EXPECT_EQ(type_of("Float"), "Float");
  EXPECT_EQ(type_of("Boolean"), "Boolean");
}

TEST(ParserType, NamedTypes) {
  EXPECT_EQ(type_of("Point"), "Point");
  EXPECT_EQ(type_of("my_class2"), "my_class2");
}

TEST(ParserType, TupleTypes) {
  EXPECT_EQ(type_of("(Integer, Float)"), "(tuple Integer Float)");
  EXPECT_EQ(type_of("(Integer, Float, Boolean)"),
            "(tuple Integer Float Boolean)");
  EXPECT_EQ(type_of("(Point, Point)"), "(tuple Point Point)");
}

TEST(ParserType, NestedTupleTypes) {
  EXPECT_EQ(type_of("((Integer, Float), Boolean)"),
            "(tuple (tuple Integer Float) Boolean)");
  EXPECT_EQ(type_of("(Integer, (Float, Boolean))"),
            "(tuple Integer (tuple Float Boolean))");
}

TEST(ParserType, ParenthesisedTypeIsNotATuple) {
  EXPECT_EQ(type_of("(Integer)"), "Integer");
  EXPECT_EQ(type_of("((Integer))"), "Integer");
  EXPECT_EQ(type_of("((Integer, Float))"), "(tuple Integer Float)");
}

TEST(ParserTypeErrors, MalformedTuples) {
  EXPECT_THROW(type_of("()"), std::runtime_error);
  EXPECT_THROW(type_of("(Integer,"), std::runtime_error);
  EXPECT_THROW(type_of("(Integer, Float"), std::runtime_error);
  EXPECT_THROW(type_of("(Integer Float)"), std::runtime_error);
  EXPECT_THROW(type_of("(Integer,)"), std::runtime_error);
  EXPECT_THROW(type_of("(, Integer)"), std::runtime_error);
  EXPECT_THROW(type_of("Integer)"), std::runtime_error);
}

TEST(ParserTypeErrors, NotATypeAtAll) {
  EXPECT_THROW(type_of("1"), std::runtime_error);
  EXPECT_THROW(type_of("true"), std::runtime_error);
  EXPECT_THROW(type_of("val"), std::runtime_error);
  EXPECT_THROW(type_of("+"), std::runtime_error);
  EXPECT_THROW(type_of(""), std::runtime_error);
}

// ------------------------------------------------------------------ parameters

TEST(ParserParams, NoParameters) { EXPECT_EQ(params_of(""), "()"); }

TEST(ParserParams, OneParameter) {
  EXPECT_EQ(params_of("x : Integer"), "(x:Integer)");
}

TEST(ParserParams, SeveralParameters) {
  EXPECT_EQ(params_of("x : Integer, y : Float"), "(x:Integer y:Float)");
  EXPECT_EQ(params_of("a : Integer, b : Float, c : Boolean"),
            "(a:Integer b:Float c:Boolean)");
}

TEST(ParserParams, CompoundTypes) {
  EXPECT_EQ(params_of("p : (Integer, Float), q : Point"),
            "(p:(tuple Integer Float) q:Point)");
}

TEST(ParserParams, NoSpacesNeeded) {
  EXPECT_EQ(params_of("x:Integer,y:Float"), "(x:Integer y:Float)");
}

TEST(ParserParamsErrors, Malformed) {
  EXPECT_THROW(params_of("x"), std::runtime_error);
  EXPECT_THROW(params_of("x :"), std::runtime_error);
  EXPECT_THROW(params_of("x Integer"), std::runtime_error);
  EXPECT_THROW(params_of(": Integer"), std::runtime_error);
  EXPECT_THROW(params_of("x : Integer,"), std::runtime_error);
  EXPECT_THROW(params_of(", x : Integer"), std::runtime_error);
  EXPECT_THROW(params_of("x : Integer y : Integer"), std::runtime_error);
  EXPECT_THROW(params_of("x : Integer,, y : Integer"), std::runtime_error);
  EXPECT_THROW(params_of("1 : Integer"), std::runtime_error);
  EXPECT_THROW(params_of("x : 1"), std::runtime_error);
}

// ------------------------------------------------------------------
// propositions (conditions)

TEST(ParserCondition, NoCondition) { EXPECT_EQ(conditions_of(""), "[]"); }

TEST(ParserCondition, PlainExpression) {
  EXPECT_EQ(conditions_of("such that x > 0"), "[(> x 0)]");
  EXPECT_EQ(conditions_of("such that true"), "[true]");
  EXPECT_EQ(conditions_of("such that f(x)"), "[(call f x)]");
}

TEST(ParserCondition, SeveralConditions) {
  EXPECT_EQ(conditions_of("such that a such that b such that c"), "[a b c]");
  EXPECT_EQ(conditions_of("such that x > 0 such that x < 10"),
            "[(> x 0) (< x 10)]");
}

TEST(ParserCondition, Follows) {
  EXPECT_EQ(conditions_of("such that x follows p()"), "[(follows x p)]");
  EXPECT_EQ(conditions_of("such that x follows p(1)"), "[(follows x p 1)]");
  EXPECT_EQ(conditions_of("such that x follows p(1, 2)"),
            "[(follows x p 1 2)]");
}

TEST(ParserCondition, FollowsWithComplexExpressions) {
  EXPECT_EQ(conditions_of("such that a + b follows sym(c, d.e)"),
            "[(follows (+ a b) sym c (. d e))]");
  EXPECT_EQ(conditions_of("such that f(x) follows p(g(y))"),
            "[(follows (call f x) p (call g y))]");
}

TEST(ParserCondition, MixedConditions) {
  EXPECT_EQ(conditions_of("such that a such that b follows p(c) such that d"),
            "[a (follows b p c) d]");
}

TEST(ParserConditionErrors, Malformed) {
  EXPECT_THROW(conditions_of("such that"), std::runtime_error);
  EXPECT_THROW(conditions_of("such"), std::runtime_error);
  EXPECT_THROW(conditions_of("that x"), std::runtime_error);
  EXPECT_THROW(conditions_of("such that x follows"), std::runtime_error);
  EXPECT_THROW(conditions_of("such that x follows p"), std::runtime_error);
  EXPECT_THROW(conditions_of("such that x follows (1)"), std::runtime_error);
  EXPECT_THROW(conditions_of("such that x follows p(1"), std::runtime_error);
  EXPECT_THROW(conditions_of("such that x follows 1(2)"), std::runtime_error);
}

TEST(ParserConditionErrors, BodyIsStillRequiredAfterConditions) {
  // `such that 1` is a complete condition, so no body is left
  EXPECT_THROW(parse_source("def f() : Integer = such that 1"),
               std::runtime_error);
}

// ------------------------------------------------------------------ proofs

TEST(ParserProof, EmptyProof) { EXPECT_EQ(proof_of("()"), "[]"); }

TEST(ParserProof, UseWithoutArguments) {
  EXPECT_EQ(proof_of("use a()"), "[(use a)]");
}

TEST(ParserProof, UseWithArguments) {
  EXPECT_EQ(proof_of("use a(1)"), "[(use a 1)]");
  EXPECT_EQ(proof_of("use a(1, x + 2, f(y))"),
            "[(use a 1 (+ x 2) (call f y))]");
}

TEST(ParserProof, SeveralSteps) {
  EXPECT_EQ(proof_of("use a(1); use b(2)"), "[(use a 1) (use b 2)]");
  EXPECT_EQ(proof_of("use a(); use b(); use c()"), "[(use a) (use b) (use c)]");
}

// `()` is a proof step that uses nothing: it does not show up in the resulting
// proof.
TEST(ParserProof, EmptyStepsAreDropped) {
  EXPECT_EQ(proof_of("(); use a(1)"), "[(use a 1)]");
  EXPECT_EQ(proof_of("use a(1); ()"), "[(use a 1)]");
  EXPECT_EQ(proof_of("use a(1); (); use b(2)"), "[(use a 1) (use b 2)]");
  EXPECT_EQ(proof_of("(); ()"), "[]");
  EXPECT_EQ(proof_of("(); (); ()"), "[]");
}

TEST(ParserProof, StepOrderIsKept) {
  EXPECT_EQ(proof_of("use c(); use a(); use b()"), "[(use c) (use a) (use b)]");
}

TEST(ParserProofErrors, Malformed) {
  EXPECT_THROW(proof_of(""), std::runtime_error);
  EXPECT_THROW(proof_of("use"), std::runtime_error);
  EXPECT_THROW(proof_of("use a"), std::runtime_error);
  EXPECT_THROW(proof_of("use (1)"), std::runtime_error);
  EXPECT_THROW(proof_of("use a(1"), std::runtime_error);
  EXPECT_THROW(proof_of("use 1()"), std::runtime_error);
  EXPECT_THROW(proof_of("a()"), std::runtime_error);
  EXPECT_THROW(proof_of("("), std::runtime_error);
  EXPECT_THROW(proof_of(")"), std::runtime_error);
}

TEST(ParserProofErrors, BadSeparators) {
  EXPECT_THROW(proof_of("use a();"), std::runtime_error);
  EXPECT_THROW(proof_of(";"), std::runtime_error);
  EXPECT_THROW(proof_of("; use a()"), std::runtime_error);
  EXPECT_THROW(proof_of("use a(); ; use b()"), std::runtime_error);
  EXPECT_THROW(proof_of("use a() use b()"), std::runtime_error);
  EXPECT_THROW(proof_of("use a(), use b()"), std::runtime_error);
}