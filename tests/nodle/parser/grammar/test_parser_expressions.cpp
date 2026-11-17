#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "./test_helper.hpp"

// Expressions are parsed as the body of `def f() : Integer = <expr>`; see
// ast_print.hpp.
using parser_test::expr_of;

namespace {

void expect_syntax_error(const std::string &expr) {
  EXPECT_THROW(expr_of(expr), std::runtime_error) << "expression: " << expr;
}

} // namespace

// ------------------------------------------------------------------ literals
// and variables

TEST(ParserExpr, IntegerLiterals) {
  EXPECT_EQ(expr_of("0"), "0");
  EXPECT_EQ(expr_of("1"), "1");
  EXPECT_EQ(expr_of("42"), "42");
  EXPECT_EQ(expr_of("2147483647"), "2147483647");
}

TEST(ParserExpr, FloatLiterals) {
  EXPECT_EQ(expr_of("2.5"), "2.5");
  EXPECT_EQ(expr_of("0.25"), "0.25");
  EXPECT_EQ(expr_of("10.0"), "10");
}

TEST(ParserExpr, BooleanLiterals) {
  EXPECT_EQ(expr_of("true"), "true");
  EXPECT_EQ(expr_of("false"), "false");
}

TEST(ParserExpr, Variables) {
  EXPECT_EQ(expr_of("x"), "x");
  EXPECT_EQ(expr_of("some_name2"), "some_name2");
}

TEST(ParserExpr, Self) { EXPECT_EQ(expr_of("self"), "self"); }

TEST(ParserExpr, LiteralKindsAreDistinguished) {
  // 1 is an integer, 1.0 a float: the printer shows floats without a trailing
  // ".0", so check the dynamic type through the parser directly.
  auto program = parser_test::parse_source("def f() : Integer = 1");
  auto &body = *parser_test::only_function(*program).body;
  EXPECT_NE(dynamic_cast<const nodle::ast::EInteger *>(&body), nullptr);

  program = parser_test::parse_source("def f() : Float = 1.0");
  auto &fbody = *parser_test::only_function(*program).body;
  EXPECT_NE(dynamic_cast<const nodle::ast::EFloat *>(&fbody), nullptr);

  program = parser_test::parse_source("def f() : Boolean = true");
  auto &bbody = *parser_test::only_function(*program).body;
  EXPECT_NE(dynamic_cast<const nodle::ast::EBool *>(&bbody), nullptr);
}

// ------------------------------------------------------------------ negation

TEST(ParserExpr, Not) { EXPECT_EQ(expr_of("!x"), "(! x)"); }
TEST(ParserExpr, NotOfLiteral) { EXPECT_EQ(expr_of("!true"), "(! true)"); }
TEST(ParserExpr, DoubleNot) { EXPECT_EQ(expr_of("!!x"), "(! (! x))"); }
TEST(ParserExpr, NotBindsTighterThanBinaryOperators) {
  EXPECT_EQ(expr_of("!a && b"), "(&& (! a) b)");
  EXPECT_EQ(expr_of("a && !b"), "(&& a (! b))");
  EXPECT_EQ(expr_of("!a == b"), "(== (! a) b)");
}
TEST(ParserExpr, NotOfParenthesisedExpression) {
  EXPECT_EQ(expr_of("!(a || b)"), "(! (|| a b))");
}
TEST(ParserExpr, NotOfCall) { EXPECT_EQ(expr_of("!f(x)"), "(! (call f x))"); }
TEST(ParserExpr, NotOfFieldAccess) {
  EXPECT_EQ(expr_of("!a.b"), "(! (. a b))");
}

// ------------------------------------------------------------------ every
// binary operator

TEST(ParserExpr, EveryBinaryOperator) {
  const std::vector<std::string> ops = {"+", "-",  "*", "/",  "%",  "==", "!=",
                                        "<", "<=", ">", ">=", "&&", "||", "^"};
  for (const auto &op : ops) {
    SCOPED_TRACE("operator " + op);
    EXPECT_EQ(expr_of("a " + op + " b"), "(" + op + " a b)");
    EXPECT_EQ(expr_of("1" + op + "2"),
              "(" + op + " 1 2)"); // no whitespace needed
  }
}

TEST(ParserExpr, BinaryOperatorOperandsAreFullTerms) {
  EXPECT_EQ(expr_of("f(1) + a.b"), "(+ (call f 1) (. a b))");
  EXPECT_EQ(expr_of("true && !false"), "(&& true (! false))");
  EXPECT_EQ(expr_of("1.5 * 2"), "(* 1.5 2)");
}

// ------------------------------------------------------------------ precedence
// and associativity

TEST(ParserExprPrecedence, TimesBindsTighterThanPlus) {
  EXPECT_EQ(expr_of("1 + 2 * 3"), "(+ 1 (* 2 3))");
  EXPECT_EQ(expr_of("1 * 2 + 3"), "(+ (* 1 2) 3)");
  EXPECT_EQ(expr_of("1 - 2 / 3"), "(- 1 (/ 2 3))");
  EXPECT_EQ(expr_of("1 % 2 - 3"), "(- (% 1 2) 3)");
}

TEST(ParserExprPrecedence, MultiplicativeOperatorsShareALevel) {
  EXPECT_EQ(expr_of("a * b / c % d"), "(% (/ (* a b) c) d)");
  EXPECT_EQ(expr_of("a % b * c"), "(* (% a b) c)");
}

TEST(ParserExprPrecedence, AdditiveOperatorsShareALevel) {
  EXPECT_EQ(expr_of("a + b - c + d"), "(+ (- (+ a b) c) d)");
}

TEST(ParserExprPrecedence, SubtractionAndDivisionAreLeftAssociative) {
  EXPECT_EQ(expr_of("1 - 2 - 3"), "(- (- 1 2) 3)");
  EXPECT_EQ(expr_of("8 / 4 / 2"), "(/ (/ 8 4) 2)");
}

TEST(ParserExprPrecedence, AdditionBindsTighterThanComparison) {
  EXPECT_EQ(expr_of("a + 1 < b - 2"), "(< (+ a 1) (- b 2))");
  EXPECT_EQ(expr_of("a * 2 >= b"), "(>= (* a 2) b)");
}

TEST(ParserExprPrecedence, ComparisonsShareALevelLeftAssociative) {
  EXPECT_EQ(expr_of("a < b == c"), "(== (< a b) c)");
  EXPECT_EQ(expr_of("a != b <= c"), "(<= (!= a b) c)");
}

TEST(ParserExprPrecedence, ComparisonBindsTighterThanBooleanOperators) {
  EXPECT_EQ(expr_of("a == b && c != d"), "(&& (== a b) (!= c d))");
  EXPECT_EQ(expr_of("a < b || c > d"), "(|| (< a b) (> c d))");
  EXPECT_EQ(expr_of("a ^ b == c"), "(^ a (== b c))");
}

// The grammar does not say how &&, || and ^ relate to each other. The parser
// gives them one level, left associative: this documents that behaviour.
TEST(ParserExprPrecedence, BooleanOperatorsShareALevelLeftAssociative) {
  EXPECT_EQ(expr_of("a && b || c"), "(|| (&& a b) c)");
  EXPECT_EQ(expr_of("a || b && c"), "(&& (|| a b) c)");
  EXPECT_EQ(expr_of("a ^ b && c"), "(&& (^ a b) c)");
  EXPECT_EQ(expr_of("a && b && c"), "(&& (&& a b) c)");
}

TEST(ParserExprPrecedence, AllLevelsTogether) {
  EXPECT_EQ(expr_of("a + b * c < d && !e"), "(&& (< (+ a (* b c)) d) (! e))");
}

TEST(ParserExprPrecedence, ParenthesesOverridePrecedence) {
  EXPECT_EQ(expr_of("(1 + 2) * 3"), "(* (+ 1 2) 3)");
  EXPECT_EQ(expr_of("1 * (2 + 3)"), "(* 1 (+ 2 3))");
  EXPECT_EQ(expr_of("1 - (2 - 3)"), "(- 1 (- 2 3))");
  EXPECT_EQ(expr_of("a && (b || c)"), "(&& a (|| b c))");
}

// ------------------------------------------------------------------
// parentheses and tuples

TEST(ParserExpr, ParenthesisedExpressionIsNotATuple) {
  EXPECT_EQ(expr_of("(x)"), "x");
  EXPECT_EQ(expr_of("((x))"), "x");
  EXPECT_EQ(expr_of("(((1)))"), "1");
  EXPECT_EQ(expr_of("(1 + 2)"), "(+ 1 2)");
}

TEST(ParserExpr, Tuples) {
  EXPECT_EQ(expr_of("(1, 2)"), "(tuple 1 2)");
  EXPECT_EQ(expr_of("(1, 2, 3)"), "(tuple 1 2 3)");
  EXPECT_EQ(expr_of("(a, b, c, d, e)"), "(tuple a b c d e)");
}

TEST(ParserExpr, TupleElementsAreFullExpressions) {
  EXPECT_EQ(expr_of("(a + 1, f(b))"), "(tuple (+ a 1) (call f b))");
  EXPECT_EQ(expr_of("(if a then 1 else 2, b)"), "(tuple (if a 1 2) b)");
  EXPECT_EQ(expr_of("(a && b, c)"), "(tuple (&& a b) c)");
}

TEST(ParserExpr, NestedTuples) {
  EXPECT_EQ(expr_of("((1, 2), 3)"), "(tuple (tuple 1 2) 3)");
  EXPECT_EQ(expr_of("(1, (2, 3))"), "(tuple 1 (tuple 2 3))");
  EXPECT_EQ(expr_of("((1, 2), (3, 4))"), "(tuple (tuple 1 2) (tuple 3 4))");
}

TEST(ParserExpr, ParenthesisedTupleInsideOperators) {
  EXPECT_EQ(expr_of("(1, 2) == (3, 4)"), "(== (tuple 1 2) (tuple 3 4))");
}

// ------------------------------------------------------------------ if

TEST(ParserExprIf, Simple) {
  EXPECT_EQ(expr_of("if a then b else c"), "(if a b c)");
}

TEST(ParserExprIf, WithComplexParts) {
  EXPECT_EQ(expr_of("if x < 1 then x + 1 else x * 2"),
            "(if (< x 1) (+ x 1) (* x 2))");
}

TEST(ParserExprIf, ElseBranchExtendsAsFarAsPossible) {
  EXPECT_EQ(expr_of("if a then b else c + 1"), "(if a b (+ c 1))");
}

TEST(ParserExprIf, NestedInThen) {
  EXPECT_EQ(expr_of("if a then if b then 1 else 2 else 3"),
            "(if a (if b 1 2) 3)");
}

TEST(ParserExprIf, NestedInElse) {
  EXPECT_EQ(expr_of("if a then b else if c then d else e"),
            "(if a b (if c d e))");
}

TEST(ParserExprIf, NestedInCondition) {
  EXPECT_EQ(expr_of("if if a then b else c then 1 else 2"),
            "(if (if a b c) 1 2)");
}

TEST(ParserExprIf, ParenthesisedIfAsOperand) {
  EXPECT_EQ(expr_of("(if a then 1 else 2) + 3"), "(+ (if a 1 2) 3)");
}

TEST(ParserExprIf, AsFunctionArgument) {
  EXPECT_EQ(expr_of("f(if a then 1 else 2, 3)"), "(call f (if a 1 2) 3)");
}

// ------------------------------------------------------------------ field
// access

TEST(ParserExprAccess, Field) { EXPECT_EQ(expr_of("a.b"), "(. a b)"); }
TEST(ParserExprAccess, FieldOfSelf) {
  EXPECT_EQ(expr_of("self.x"), "(. self x)");
}
TEST(ParserExprAccess, ChainedFields) {
  EXPECT_EQ(expr_of("a.b.c"), "(. (. a b) c)");
  EXPECT_EQ(expr_of("a.b.c.d"), "(. (. (. a b) c) d)");
}
TEST(ParserExprAccess, FieldInsideOperators) {
  EXPECT_EQ(expr_of("a.x + b.y * 2"), "(+ (. a x) (* (. b y) 2))");
}

// ------------------------------------------------------------------ calls and
// instantiation

TEST(ParserExprCall, NoArguments) { EXPECT_EQ(expr_of("f()"), "(call f)"); }
TEST(ParserExprCall, OneArgument) { EXPECT_EQ(expr_of("f(1)"), "(call f 1)"); }
TEST(ParserExprCall, SeveralArguments) {
  EXPECT_EQ(expr_of("f(1, 2, 3)"), "(call f 1 2 3)");
}
TEST(ParserExprCall, ArgumentsAreFullExpressions) {
  EXPECT_EQ(expr_of("f(a + b, c * d, !e)"), "(call f (+ a b) (* c d) (! e))");
}
TEST(ParserExprCall, NestedCalls) {
  EXPECT_EQ(expr_of("f(g(1), h(2, 3))"), "(call f (call g 1) (call h 2 3))");
}
TEST(ParserExprCall, CallOfACall) {
  EXPECT_EQ(expr_of("f(1)(2)"), "(call (call f 1) 2)");
}
TEST(ParserExprCall, ArgumentWithTuple) {
  EXPECT_EQ(expr_of("f((1, 2), 3)"), "(call f (tuple 1 2) 3)");
}
TEST(ParserExprCall, Instantiation) {
  EXPECT_EQ(expr_of("C(1, 2)"), "(call C 1 2)");
}
TEST(ParserExprCall, InsideOperators) {
  EXPECT_EQ(expr_of("f(x) + g(y) * 2"), "(+ (call f x) (* (call g y) 2))");
}

TEST(ParserExprMethodCall, NoArguments) {
  EXPECT_EQ(expr_of("a.f()"), "(call (. a f))");
}
TEST(ParserExprMethodCall, WithArguments) {
  EXPECT_EQ(expr_of("a.f(1, 2)"), "(call (. a f) 1 2)");
}
TEST(ParserExprMethodCall, OnSelf) {
  EXPECT_EQ(expr_of("self.f(1)"), "(call (. self f) 1)");
}
TEST(ParserExprMethodCall, ChainedCallsAndFields) {
  EXPECT_EQ(expr_of("a.b.f(x).g"), "(. (call (. (. a b) f) x) g)");
  EXPECT_EQ(expr_of("a.f().g()"), "(call (. (call (. a f)) g))");
}
TEST(ParserExprMethodCall, FieldOfCallResult) {
  EXPECT_EQ(expr_of("f(x).y"), "(. (call f x) y)");
}
TEST(ParserExprMethodCall, ArgumentIsAMethodCall) {
  EXPECT_EQ(expr_of("f(a.g(1))"), "(call f (call (. a g) 1))");
}

// ------------------------------------------------------------------ syntax
// errors

TEST(ParserExprErrors, MissingRightOperand) {
  expect_syntax_error("1 +");
  expect_syntax_error("a *");
  expect_syntax_error("a && ");
  expect_syntax_error("a == ");
}

TEST(ParserExprErrors, MissingLeftOperand) {
  expect_syntax_error("+ 1");
  expect_syntax_error("* 2");
  expect_syntax_error("== a");
}

TEST(ParserExprErrors, TwoOperatorsInARow) {
  expect_syntax_error("1 + * 2");
  expect_syntax_error("a == == b");
}

TEST(ParserExprErrors, UnbalancedParentheses) {
  expect_syntax_error("(1 + 2");
  expect_syntax_error("(1, 2");
  expect_syntax_error("((1)");
  expect_syntax_error("1 + 2)");
}

TEST(ParserExprErrors, EmptyParentheses) {
  // `()` is only the empty proof; it is not an expression
  expect_syntax_error("()");
}

TEST(ParserExprErrors, TrailingOrDoubledCommas) {
  expect_syntax_error("(1, 2,)");
  expect_syntax_error("(1,, 2)");
  expect_syntax_error("(, 1)");
  expect_syntax_error("f(1,)");
  expect_syntax_error("f(,1)");
  expect_syntax_error("f(1,, 2)");
}

TEST(ParserExprErrors, ArgumentsWithoutCommas) {
  expect_syntax_error("f(1 2)");
  expect_syntax_error("f(a b)");
}

TEST(ParserExprErrors, UnclosedCall) {
  expect_syntax_error("f(");
  expect_syntax_error("f(1");
  expect_syntax_error("f(1, 2");
}

TEST(ParserExprErrors, FieldAccessWithoutName) {
  expect_syntax_error("a.");
  expect_syntax_error("a.1");
  expect_syntax_error("a.(b)");
  expect_syntax_error("a..b");
}

TEST(ParserExprErrors, IncompleteIf) {
  expect_syntax_error("if a then b");
  expect_syntax_error("if a b else c");
  expect_syntax_error("if a then else c");
  expect_syntax_error("if then b else c");
  expect_syntax_error("if a then b else");
  expect_syntax_error("if");
}

TEST(ParserExprErrors, NotWithoutOperand) {
  expect_syntax_error("!");
  expect_syntax_error("a && !");
}

TEST(ParserExprErrors, KeywordsAreNotVariables) {
  expect_syntax_error("then");
  expect_syntax_error("else");
  expect_syntax_error("val");
}

TEST(ParserExprErrors, TwoExpressionsInARow) {
  expect_syntax_error("1 2");
  expect_syntax_error("a b");
}

TEST(ParserExprErrors, MissingExpression) {
  // the body of a function is required
  EXPECT_THROW(parser_test::parse_source("def f() : Integer ="),
               std::runtime_error);
}

// ------------------------------------------------------------------ robustness

TEST(ParserExprRobustness, DeeplyNestedParentheses) {
  const int depth = 200;
  std::string expr = std::string(depth, '(') + "x" + std::string(depth, ')');
  EXPECT_EQ(expr_of(expr), "x");
}

TEST(ParserExprRobustness, DeeplyNestedTuples) {
  const int depth = 100;
  std::string src, expected;
  for (int i = 0; i < depth; ++i) {
    src += "(";
    expected += "(tuple ";
  }
  src += "1";
  expected += "1";
  for (int i = 0; i < depth; ++i) {
    src += ", 2)";
    expected += " 2)";
  }
  EXPECT_EQ(expr_of(src), expected);
}

TEST(ParserExprRobustness, LongOperatorChainIsLeftAssociative) {
  std::string src = "1";
  std::string expected = "1";
  for (int i = 2; i <= 200; ++i) {
    src += " + " + std::to_string(i);
    expected = "(+ " + expected + " " + std::to_string(i) + ")";
  }
  EXPECT_EQ(expr_of(src), expected);
}

TEST(ParserExprRobustness, ManyArguments) {
  std::string src = "f(0", expected = "(call f 0";
  for (int i = 1; i < 300; ++i) {
    src += ", " + std::to_string(i);
    expected += " " + std::to_string(i);
  }
  EXPECT_EQ(expr_of(src + ")"), expected + ")");
}