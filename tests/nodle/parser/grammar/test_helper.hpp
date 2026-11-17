#pragma once

// Renders the AST as S-expressions so tests can compare whole trees as strings,
// plus helpers that wrap a fragment into a program and extract the interesting
// part.
//
// This is the ONLY test file that touches AST member names. If your AST names
// differ from the ones used below (e.g. `EBinOp::lhs`, `Call::callee`,
// `Function::conditions`), adapt them here.

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nodle/ast/ast.hpp"
#include "nodle/lexer/lexer.hpp"
#include "nodle/parser/parser.hpp"

namespace parser_test {

using namespace nodle::ast;

inline std::string show(const Expression &e);
inline std::string show(const Type &t);
inline std::string show(const Proposition &p);
inline std::string show(const Function &f);

template <typename Range, typename F>
std::string join(const Range &range, F f) {
  std::string out;
  for (const auto &item : range) {
    out += (out.empty() ? "" : " ") + f(*item);
  }
  return out;
}

inline std::string show(BinOp op) {
  switch (op) {
  case BinOp::Add:
    return "+";
  case BinOp::Sub:
    return "-";
  case BinOp::Mul:
    return "*";
  case BinOp::Div:
    return "/";
  case BinOp::Mod:
    return "%";
  case BinOp::Eq:
    return "==";
  case BinOp::Neq:
    return "!=";
  case BinOp::Lt:
    return "<";
  case BinOp::Le:
    return "<=";
  case BinOp::Gt:
    return ">";
  case BinOp::Ge:
    return ">=";
  case BinOp::And:
    return "&&";
  case BinOp::Or:
    return "||";
  case BinOp::Xor:
    return "^";
  }
  return "?op";
}

// "(head a b c)" or "(head)" when there are no arguments
inline std::string list(const std::string &head, const std::string &args) {
  return "(" + head + (args.empty() ? "" : " " + args) + ")";
}

inline std::string show(const Expression &e) {
  if (auto *x = dynamic_cast<const EInteger *>(&e))
    return std::to_string(x->value);
  if (auto *x = dynamic_cast<const EFloat *>(&e)) {
    std::ostringstream os;
    os << x->value;
    return os.str();
  }
  if (auto *x = dynamic_cast<const EBool *>(&e))
    return x->value ? "true" : "false";
  if (auto *x = dynamic_cast<const Var *>(&e))
    return x->name;
  if (dynamic_cast<const Self *>(&e))
    return "self";
  if (auto *x = dynamic_cast<const Not *>(&e))
    return list("!", show(*x->exp));
  if (auto *x = dynamic_cast<const ETuple *>(&e))
    return list("tuple",
                join(x->values, [](const Expression &y) { return show(y); }));
  if (auto *x = dynamic_cast<const FieldAccess *>(&e))
    return list(".", show(*x->object) + " " + x->field);
  if (auto *x = dynamic_cast<const Call *>(&e)) {
    std::string args =
        join(x->args, [](const Expression &y) { return show(y); });
    return list("call", show(*x->object) + (args.empty() ? "" : " " + args));
  }
  if (auto *x = dynamic_cast<const EBinOp *>(&e))
    return list(show(x->op), show(*x->lhs) + " " + show(*x->rhs));
  if (auto *x = dynamic_cast<const If *>(&e))
    return list("if", show(*x->cond) + " " + show(*x->then_branch) + " " +
                          show(*x->else_branch));
  return "<unknown expression>";
}

inline std::string show(const Type &t) {
  if (dynamic_cast<const TInteger *>(&t))
    return "Integer";
  if (dynamic_cast<const TFloat *>(&t))
    return "Float";
  if (dynamic_cast<const TBoolean *>(&t))
    return "Boolean";
  if (auto *x = dynamic_cast<const TNamed *>(&t))
    return x->name;
  if (auto *x = dynamic_cast<const TTuple *>(&t))
    return list("tuple",
                join(x->values, [](const Type &y) { return show(y); }));
  return "<unknown type>";
}

inline std::string show(const Proposition &p) {
  if (auto *x = dynamic_cast<const PExp *>(&p))
    return show(*x->exp);
  if (auto *x = dynamic_cast<const PFollows *>(&p)) {
    std::string args =
        join(x->args, [](const Expression &y) { return show(y); });
    return list("follows", show(*x->e0) + " " + x->property +
                               (args.empty() ? "" : " " + args));
  }
  return "<unknown proposition>";
}

inline std::string show(const Use &u) {
  std::string args = join(u.args, [](const Expression &y) { return show(y); });
  return list("use", u.name + (args.empty() ? "" : " " + args));
}
inline std::string show(const Proof &proof) {
  return "[" + join(proof, [](const Use &u) { return show(u); }) + "]";
}
inline std::string show(const Params &params) {
  return "(" +
         join(params,
              [](const Param &p) { return p.name + ":" + show(*p.type); }) +
         ")";
}

inline std::string show(const Function &f) {
  std::string out = f.is_override ? "(override-def " : "(def ";
  out += f.name + " " + show(f.params) + " " + show(*f.return_type);
  out += " [" +
         join(f.conditions, [](const Proposition &p) { return show(p); }) +
         "] ";
  out += show(*f.body);
  if (f.equivalence) {
    out += " equiv " + show(*f.equivalence);
  }
  return out + ")";
}

inline std::string show(const Member &m) {
  if (auto *x = dynamic_cast<const SuchThat *>(&m))
    return list("such", show(*x->prop));
  if (auto *x = dynamic_cast<const Prove *>(&m))
    return list("prove", show(*x->goal) + " " + show(x->proof));
  if (auto *x = dynamic_cast<const ValDecl *>(&m))
    return list("val", x->name + " " + show(*x->type));
  if (auto *x = dynamic_cast<const ValDef *>(&m))
    return list("val", x->name + " " + show(*x->type) + " " + show(*x->value));
  if (auto *x = dynamic_cast<const Method *>(&m))
    return list("method", show(x->fn));
  return "<unknown member>";
}

inline std::string show(const Declaration &d) {
  auto members = [](const vec_ptr<Member> &ms) {
    std::string args = join(ms, [](const Member &m) { return show(m); });
    return args.empty() ? "" : " " + args;
  };
  if (auto *x = dynamic_cast<const FunctionDecl *>(&d))
    return show(x->fn);
  if (auto *x = dynamic_cast<const PropertyDecl *>(&d))
    return "(property " + x->name + " " + show(x->params) +
           members(x->requirements) + ")";
  if (auto *x = dynamic_cast<const LemmaDecl *>(&d))
    return "(lemma " + x->name + " " + show(x->params) +
           members(x->statements) + ")";
  if (auto *x = dynamic_cast<const ClassDecl *>(&d))
    return "(class " + x->name + " " + show(x->params) + members(x->members) +
           ")";
  return "<unknown declaration>";
}

inline std::string show(const Program &p) {
  return join(p.decls, [](const Declaration &d) { return show(d); });
}

// ----------------------------------------------------------------------------
// entry points

inline nodle::parser::ParserContextPtr make_context(const std::string &source) {
  nodle::lexer::Lexer lexer;
  auto tokens =
      lexer.lex(nodle::io::SourceFile::from_string("t.nodle", source));

  return std::make_shared<nodle::parser::ParserContext>(std::move(tokens));
}

// Parses a whole program. Throws std::runtime_error on a syntax error.
inline std::unique_ptr<Program> parse_source(const std::string &source) {
  nodle::lexer::Lexer lexer;
  auto tokens =
      lexer.lex(nodle::io::SourceFile::from_string("t.nodle", source));

  nodle::parser::ParserContextPtr ctx =
      std::make_shared<nodle::parser::ParserContext>(std::move(tokens));
  nodle::parser::Parser parser;
  std::unique_ptr<Program> program = parser.parse(ctx);
  if (ctx->remaining()) {
    throw std::logic_error("Parser::parse returned but tokens remain");
  }
  return program;
}

// The whole program as one string; declarations are separated by a space.
inline std::string program_of(const std::string &source) {
  return show(*parse_source(source));
}

inline const Function &only_function(const Program &program) {
  if (program.decls.size() != 1) {
    throw std::logic_error("expected exactly one declaration");
  }
  auto *decl = dynamic_cast<const FunctionDecl *>(program.decls[0].get());
  if (!decl) {
    throw std::logic_error("expected a function declaration");
  }
  return decl->fn;
}

// `def f() : Integer = <expr>`  ->  the body
inline std::string expr_of(const std::string &expr) {
  auto program = parse_source("def f() : Integer = " + expr);
  return show(*only_function(*program).body);
}

// `def f() : <type> = 1`  ->  the return type
inline std::string type_of(const std::string &type) {
  auto program = parse_source("def f() : " + type + " = 1");
  return show(*only_function(*program).return_type);
}

// `def f(<params>) : Integer = 1`  ->  the parameters
inline std::string params_of(const std::string &params) {
  auto program = parse_source("def f(" + params + ") : Integer = 1");
  return show(only_function(*program).params);
}

// `def f() : Integer = <conditions> 1`  ->  the conditions, one per `such that`
inline std::string conditions_of(const std::string &conditions) {
  auto program = parse_source("def f() : Integer = " + conditions + " 1");
  const Function &f = only_function(*program);
  return "[" +
         join(f.conditions, [](const Proposition &p) { return show(p); }) + "]";
}

// `lemma l() : prove 1 with proof <proof>`  ->  the proof
inline std::string proof_of(const std::string &proof) {
  auto program = parse_source("lemma l() : prove 1 with proof " + proof);
  auto *lemma = dynamic_cast<const LemmaDecl *>(program->decls.at(0).get());
  auto *prove = lemma
                    ? dynamic_cast<const Prove *>(lemma->statements.at(0).get())
                    : nullptr;
  if (!prove) {
    throw std::logic_error("expected a lemma with a `prove` statement");
  }
  return show(prove->proof);
}

} // namespace parser_test