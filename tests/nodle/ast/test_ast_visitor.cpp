#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "nodle/ast/ast.hpp"
#include "nodle/ast/ast_visitor.hpp"
#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"

using namespace nodle::ast;
using nodle::io::SourceFile;
using nodle::io::SourceLocation;

namespace {

// Records the type name of every node it visits, without recursing.
struct RecordingVisitor : public Visitor {
  std::vector<std::string> visited;

#define NODLE_RECORD(T)                                                        \
  void visit(const T &) override{visited.push_back(#T);                        \
  }

  NODLE_RECORD(TInteger)
  NODLE_RECORD(TFloat)
  NODLE_RECORD(TBoolean)
  NODLE_RECORD(TTuple)
  NODLE_RECORD(TNamed)
  NODLE_RECORD(EInteger)
  NODLE_RECORD(EFloat)
  NODLE_RECORD(EBool)
  NODLE_RECORD(Var)
  NODLE_RECORD(ETuple)
  NODLE_RECORD(Self)
  NODLE_RECORD(EBinOp)
  NODLE_RECORD(Not)
  NODLE_RECORD(If)
  NODLE_RECORD(FieldAccess)
  NODLE_RECORD(Call)
  NODLE_RECORD(PExp)
  NODLE_RECORD(PFollows)
  NODLE_RECORD(Use)
  NODLE_RECORD(Param)
  NODLE_RECORD(Function)
  NODLE_RECORD(SuchThat)
  NODLE_RECORD(Prove)
  NODLE_RECORD(ValDecl)
  NODLE_RECORD(ValDef)
  NODLE_RECORD(Method)
  NODLE_RECORD(StateComponent)
  NODLE_RECORD(Edge)
  NODLE_RECORD(PropertyDecl)
  NODLE_RECORD(LemmaDecl)
  NODLE_RECORD(ClassDecl)
  NODLE_RECORD(GraphDecl)
  NODLE_RECORD(FunctionDecl)
  NODLE_RECORD(Program)

#undef NODLE_RECORD
};

// Overrides only a few nodes and recurses into their children, the way a pass
// built on a default base class does.
struct RecursingVisitor : public RecordingVisitor {
  using RecordingVisitor::visit;

  void visit(const EBinOp &node) override {
    RecordingVisitor::visit(node);
    node.lhs->accept(*this);
    node.rhs->accept(*this);
  }
  void visit(const Not &node) override {
    RecordingVisitor::visit(node);
    node.exp->accept(*this);
  }
};

class AstVisitor : public ::testing::Test {
protected:
  SourceFile::Ptr file_ = SourceFile::from_string("test.nodle", "x");

  SourceLocation loc() const { return SourceLocation(file_, 0, 1); }
  std::unique_ptr<Expression> expr() const {
    return std::make_unique<Var>(loc(), "x");
  }
  std::unique_ptr<Type> type() const {
    return std::make_unique<TInteger>(loc());
  }
};

} // namespace

TEST(AstVisitorTypes, VisitorAndBaseNodesAreAbstract) {
  static_assert(std::is_abstract_v<Visitor>);
  static_assert(std::is_abstract_v<Node>);
  static_assert(std::is_abstract_v<Type>);
  static_assert(std::is_abstract_v<Expression>);
  static_assert(std::is_abstract_v<Proposition>);
  static_assert(std::is_abstract_v<Member>);
  static_assert(std::is_abstract_v<Declaration>);
}

TEST_F(AstVisitor, EveryNodeDispatchesToItsOwnVisitThroughNode) {
  std::vector<std::pair<std::unique_ptr<Node>, std::string>> cases;
  auto add = [&](std::unique_ptr<Node> node, std::string name) {
    cases.emplace_back(std::move(node), std::move(name));
  };

  add(std::make_unique<TInteger>(loc()), "TInteger");
  add(std::make_unique<TFloat>(loc()), "TFloat");
  add(std::make_unique<TBoolean>(loc()), "TBoolean");
  add(std::make_unique<TTuple>(loc(), vec_ptr<Type>{}, loc()), "TTuple");
  add(std::make_unique<TNamed>(loc(), "C"), "TNamed");

  add(std::make_unique<EInteger>(loc(), 1), "EInteger");
  add(std::make_unique<EFloat>(loc(), 1.5), "EFloat");
  add(std::make_unique<EBool>(loc(), true), "EBool");
  add(std::make_unique<Var>(loc(), "x"), "Var");
  add(std::make_unique<ETuple>(loc(), vec_ptr<Expression>{}, loc()), "ETuple");
  add(std::make_unique<Self>(loc()), "Self");
  add(std::make_unique<EBinOp>(BinOp::Add, expr(), expr()), "EBinOp");
  add(std::make_unique<Not>(loc(), expr()), "Not");
  add(std::make_unique<If>(loc(), expr(), expr(), expr()), "If");
  add(std::make_unique<FieldAccess>(expr(), loc(), "f"), "FieldAccess");
  add(std::make_unique<Call>(expr(), vec_ptr<Expression>{}, loc()), "Call");

  add(std::make_unique<PExp>(expr()), "PExp");
  add(std::make_unique<PFollows>(expr(), "p", vec_ptr<Expression>{}, loc()),
      "PFollows");
  add(std::make_unique<Use>(loc(), "l", vec_ptr<Expression>{}, loc()), "Use");

  add(std::make_unique<Param>(loc(), "x", type()), "Param");
  add(std::make_unique<Function>(loc()), "Function");

  add(std::make_unique<SuchThat>(loc(), std::make_unique<PExp>(expr())),
      "SuchThat");
  add(std::make_unique<Prove>(loc(), std::make_unique<PExp>(expr()), Proof{}),
      "Prove");
  add(std::make_unique<ValDecl>(loc(), "x", type()), "ValDecl");
  add(std::make_unique<ValDef>(loc(), "x", type(), expr()), "ValDef");
  add(std::make_unique<Method>(Function(loc())), "Method");

  add(std::make_unique<StateComponent>(expr(), "x", expr()), "StateComponent");
  add(std::make_unique<Edge>(expr(), expr(), expr()), "Edge");

  add(std::make_unique<PropertyDecl>(loc()), "PropertyDecl");
  add(std::make_unique<LemmaDecl>(loc()), "LemmaDecl");
  add(std::make_unique<ClassDecl>(loc()), "ClassDecl");
  add(std::make_unique<GraphDecl>(loc()), "GraphDecl");
  add(std::make_unique<FunctionDecl>(Function(loc())), "FunctionDecl");

  add(std::make_unique<Program>(loc()), "Program");

  ASSERT_EQ(cases.size(), 34u);
  for (const auto &[node, name] : cases) {
    RecordingVisitor visitor;
    node->accept(visitor);
    EXPECT_EQ(visitor.visited, std::vector<std::string>{name});
  }
}

TEST_F(AstVisitor, NodesDispatchThroughTheirHierarchyBase) {
  std::unique_ptr<Expression> e = std::make_unique<EInteger>(loc(), 1);
  std::unique_ptr<Type> t = std::make_unique<TNamed>(loc(), "C");
  std::unique_ptr<Proposition> p = std::make_unique<PExp>(expr());
  std::unique_ptr<Member> m = std::make_unique<ValDecl>(loc(), "x", type());
  std::unique_ptr<Declaration> d = std::make_unique<ClassDecl>(loc());

  RecordingVisitor visitor;
  e->accept(visitor);
  t->accept(visitor);
  p->accept(visitor);
  m->accept(visitor);
  d->accept(visitor);

  EXPECT_EQ(visitor.visited,
            (std::vector<std::string>{"EInteger", "TNamed", "PExp", "ValDecl",
                                      "ClassDecl"}));
}

TEST_F(AstVisitor, ChildrenDispatchOnTheirRuntimeType) {
  // !(1 + x)
  auto sum = std::make_unique<EBinOp>(
      BinOp::Add, std::make_unique<EInteger>(loc(), 1), expr());
  Not root(loc(), std::move(sum));

  RecursingVisitor visitor;
  root.accept(visitor);

  EXPECT_EQ(visitor.visited,
            (std::vector<std::string>{"Not", "EBinOp", "EInteger", "Var"}));
}

TEST_F(AstVisitor, ValueMembersCanBeVisitedDirectly) {
  // Function is stored by value in Method, so a pass visits it directly.
  Method method{Function(loc())};

  RecordingVisitor visitor;
  visitor.visit(method.fn);

  EXPECT_EQ(visitor.visited, std::vector<std::string>{"Function"});
}
