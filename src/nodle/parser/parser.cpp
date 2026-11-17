
#include "nodle/parser/parser.hpp"
#include "nodle/ast/ast.hpp"
#include <string>

using namespace nodle::io;
using namespace nodle::ast;
using namespace nodle::lexer;
using namespace nodle::parser;

template <typename T> using ptr = std::unique_ptr<T>;
template <typename T> using result = rules::ParseResult<ptr<T>>;

namespace {

using ArgumentsList = std::tuple<Token, vec_ptr<Expression>, Token>;

result<Expression> object(ParserContextPtr ptr);
rules::ParseResult<ArgumentsList> arguments(ParserContextPtr ptr);
rules::ParseResult<ArgumentsList> nonempty_arguments(ParserContextPtr ptr);
result<Expression> term(ParserContextPtr ptr);
result<Expression> expr_times(ParserContextPtr ptr);
result<Expression> expr_addition(ParserContextPtr ptr);
result<Expression> expr_comparison(ParserContextPtr ptr);
result<Expression> expr_bool(ParserContextPtr ptr);
result<Expression> expr(ParserContextPtr ptr);

result<Type> type(ParserContextPtr ptr);
result<Proposition> proposition(ParserContextPtr ptr);
rules::ParseResult<Proof> proof(ParserContextPtr ptr);

rules::ParseResult<Params> params(ParserContextPtr ptr);
rules::ParseResult<Function> function(ParserContextPtr ptr);

auto make_interior_arguments_list_rule() {
  auto argument = rules::CallRule<ptr<Expression>, expr>();
  auto other_arg =
      (rules::TokenRule{D_COMMA} >> argument)
          .map<ptr<Expression>>([](std::tuple<Token, ptr<Expression>> x) {
            return std::move(std::get<1>(x));
          });

  auto all_args =
      (argument >> other_arg.many())
          .map<vec_ptr<Expression>>(
              [](std::tuple<ptr<Expression>, vec_ptr<Expression>> x) {
                ptr<Expression> &lhs = std::get<0>(x);
                vec_ptr<Expression> &rhs = std::get<1>(x);

                vec_ptr<Expression> res;
                res.push_back(std::move(lhs));
                for (auto &of_rhs : rhs) {
                  res.push_back(std::move(of_rhs));
                }

                return res;
              });

  return all_args;
}
auto make_arguments_list_rule() {
  auto all_args = make_interior_arguments_list_rule();

  return (rules::TokenRule{D_LPAREN} >> (~all_args) >>
          rules::TokenRule{D_RPAREN})
      .map<ArgumentsList>(
          [](std::tuple<Token, std::optional<vec_ptr<Expression>>, Token> x) {
            Token lparen = std::get<0>(x);
            auto &internal = std::get<1>(x);
            Token rparen = std::get<2>(x);

            if (internal.has_value()) {
              return std::make_tuple(lparen, std::move(internal.value()),
                                     rparen);
            }

            return std::make_tuple(lparen, vec_ptr<Expression>(), rparen);
          });
}
auto make_nonempty_arguments_list_rule() {
  auto all_args = make_interior_arguments_list_rule();

  return (rules::TokenRule{D_LPAREN} >> all_args >> rules::TokenRule{D_RPAREN});
}

auto make_object_rule() {
  auto first =
      (rules::TokenRule{I_IDENTIFIER}.map<ptr<Expression>>([](Token tok) {
        return std::make_unique<Var>(tok.source_location(),
                                     std::string(tok.text()));
      })) |
      (rules::TokenRule{K_SELF}.map<ptr<Expression>>([](Token tok) {
        return std::make_unique<Self>(tok.source_location());
      }));

  using FieldAccessType = Token;
  using CallType = ArgumentsList;

  auto field_access =
      (rules::TokenRule{D_DOT} >> rules::TokenRule{I_IDENTIFIER})
          .map<FieldAccessType>(
              [](std::tuple<Token, Token> x) { return std::get<1>(x); });
  auto call = rules::CallRule<ArgumentsList, arguments>{};

  using AccessAndCallsType =
      std::vector<std::variant<FieldAccessType, CallType>>;
  auto accesses_and_calls = (field_access | call).many();

  return (first >> accesses_and_calls)
      .map<ptr<Expression>>(
          [](std::tuple<ptr<Expression>, AccessAndCallsType> x) {
            ptr<Expression> expr = std::move(std::get<0>(x));

            for (auto &modifier : std::get<1>(x)) {
              if (modifier.index() == 0) {
                Token field = std::get<0>(modifier);
                expr = std::make_unique<FieldAccess>(std::move(expr),
                                                     field.source_location(),
                                                     std::string(field.text()));
              } else {
                auto &call_modifier = std::get<1>(modifier);

                expr = std::make_unique<Call>(
                    std::move(expr), std::move(std::get<1>(call_modifier)),
                    std::get<2>(call_modifier).source_location());
              }
            }

            return expr;
          });
}

auto make_term_rule() {
  auto int_literal =
      rules::TokenRule{L_INT}.map<std::unique_ptr<Expression>>([](Token tok) {
        return std::make_unique<EInteger>(
            tok.source_location(), std::atoi(std::string(tok.text()).c_str()));
      });
  auto float_literal =
      rules::TokenRule{L_FLOAT}.map<std::unique_ptr<Expression>>([](Token tok) {
        return std::make_unique<EFloat>(
            tok.source_location(), std::atof(std::string(tok.text()).c_str()));
      });
  auto true_literal =
      rules::TokenRule{L_TRUE}.map<std::unique_ptr<Expression>>([](Token tok) {
        return std::make_unique<EBool>(tok.source_location(), true);
      });
  auto false_literal =
      rules::TokenRule{L_FALSE}.map<std::unique_ptr<Expression>>([](Token tok) {
        return std::make_unique<EBool>(tok.source_location(), false);
      });
  auto not_literal =
      (rules::TokenRule{O_NOT} >> rules::CallRule<ptr<Expression>, term>())
          .map<ptr<Expression>>(
              [](std::tuple<Token, ptr<Expression>> not_and_subexpr) {
                Token tok = std::get<0>(not_and_subexpr);
                ptr<Expression> expr = std::move(std::get<1>(not_and_subexpr));
                return std::make_unique<Not>(tok.source_location(),
                                             std::move(expr));
              });
  auto tuple_or_subexpr =
      rules::CallRule<ArgumentsList, nonempty_arguments>{}.map<ptr<Expression>>(
          [](ArgumentsList x) -> ptr<Expression> {
            Token lparen = std::get<0>(x);
            Token rparen = std::get<2>(x);

            auto &list = std::get<1>(x);

            if (list.size() == 1) {
              // subexpression
              ptr<Expression> expr = std::move(list[0]);

              expr->extend(lparen.source_location());
              expr->extend(rparen.source_location());

              return expr;
            }

            return std::make_unique<ETuple>(lparen.source_location(),
                                            std::move(list),
                                            rparen.source_location());
          });

  return int_literal | float_literal | true_literal | false_literal |
         rules::CallRule<ptr<Expression>, object>() | not_literal |
         tuple_or_subexpr;
}

BinOp binop_of_token(Token &tok) {
  switch (tok.token_type()) {
  case O_PLUS:
    return BinOp::Add;
  case O_MINUS:
    return BinOp::Sub;
  case O_TIMES:
    return BinOp::Mul;
  case O_DIVIDE:
    return BinOp::Div;
  case O_MODULO:
    return BinOp::Mod;
  case O_EQUAL:
    return BinOp::Eq;
  case O_DIFFERENT:
    return BinOp::Neq;
  case O_LESS_THAN:
    return BinOp::Lt;
  case O_LESS_EQUAL:
    return BinOp::Le;
  case O_GREATER_THAN:
    return BinOp::Gt;
  case O_GREATER_EQUAL:
    return BinOp::Ge;
  case O_AND:
    return BinOp::And;
  case O_OR:
    return BinOp::Or;
  case O_XOR:
    return BinOp::Xor;
  default:
    break;
  }

  throw std::runtime_error("Unknown binary operation.");
}

template <auto NextFunc> auto make_expr_of_binop(auto TokensOfStage) {
  auto next_rule = rules::CallRule<ptr<Expression>, NextFunc>{};
  auto steps_rule = (TokensOfStage >> next_rule).many();

  using Steps = std::vector<std::tuple<Token, ptr<Expression>>>;

  return (next_rule >> steps_rule)
      .template map<ptr<Expression>>([](std::tuple<ptr<Expression>, Steps> x) {
        ptr<Expression> expr = std::move(std::get<0>(x));
        auto &ops = std::get<1>(x);

        for (auto &[tok_op, rhs] : ops) {
          expr = std::make_unique<EBinOp>(binop_of_token(tok_op),
                                          std::move(expr), std::move(rhs));
        }

        return expr;
      });
}

auto make_expr_times_rule() {
  return make_expr_of_binop<term>(rules::TokenRule{O_TIMES} |
                                  rules::TokenRule{O_DIVIDE} |
                                  rules::TokenRule{O_MODULO});
}
auto make_expr_addition_rule() {
  return make_expr_of_binop<expr_times>(rules::TokenRule{O_PLUS} |
                                        rules::TokenRule{O_MINUS});
}
auto make_expr_comparison_rule() {
  return make_expr_of_binop<expr_addition>(
      rules::TokenRule{O_EQUAL} | rules::TokenRule{O_DIFFERENT} |
      rules::TokenRule{O_LESS_THAN} | rules::TokenRule{O_LESS_EQUAL} |
      rules::TokenRule{O_GREATER_THAN} | rules::TokenRule{O_GREATER_EQUAL});
}
auto make_expr_bool_rule() {
  return make_expr_of_binop<expr_comparison>(rules::TokenRule{O_OR} |
                                             rules::TokenRule{O_AND} |
                                             rules::TokenRule{O_XOR});
}

auto make_expr_rule() {
  auto iff_expr =
      (rules::TokenRule{K_IF} >> rules::CallRule<ptr<Expression>, expr>() >>
       rules::TokenRule{K_THEN} >> rules::CallRule<ptr<Expression>, expr>() >>
       rules::TokenRule{K_ELSE} >> rules::CallRule<ptr<Expression>, expr>())
          .map<ptr<Expression>>([](auto iff_data) {
            return std::make_unique<If>(std::get<0>(iff_data).source_location(),
                                        std::move(std::get<1>(iff_data)),
                                        std::move(std::get<3>(iff_data)),
                                        std::move(std::get<5>(iff_data)));
          });

  return iff_expr | rules::CallRule<ptr<Expression>, expr_bool>{};
}

auto make_type_rule() {
  auto int_type = rules::TokenRule{K_INT}.map<ptr<Type>>(
      [](auto x) { return std::make_unique<TInteger>(x.source_location()); });
  auto float_type = rules::TokenRule{K_FLOAT}.map<ptr<Type>>(
      [](auto x) { return std::make_unique<TFloat>(x.source_location()); });
  auto bool_type = rules::TokenRule{K_BOOL}.map<ptr<Type>>(
      [](auto x) { return std::make_unique<TBoolean>(x.source_location()); });

  auto tuple_type = ([]() {
    auto base = rules::CallRule<ptr<Type>, type>{};
    auto secd =
        (rules::TokenRule{D_COMMA} >> base).map<ptr<Type>>([](auto comma_type) {
          return std::move(std::get<1>(comma_type));
        });

    auto full = rules::TokenRule{D_LPAREN} >> base >> ~(secd.many()) >>
                rules::TokenRule{D_RPAREN};

    return full.map<ptr<Type>>([](auto x) -> ptr<Type> {
      Token lparen = std::get<0>(x);
      Token rparen = std::get<3>(x);

      auto &first = std::get<1>(x);
      auto &second = std::get<2>(x);

      if (!second.has_value()) {
        first->extend(lparen.source_location());
        first->extend(rparen.source_location());

        return std::move(first);
      }

      vec_ptr<Type> types;
      types.push_back(std::move(first));
      for (auto &rest : second.value()) {
        types.push_back(std::move(rest));
      }

      return std::make_unique<TTuple>(
          lparen.source_location(), std::move(types), rparen.source_location());
    });
  })();

  auto named_type =
      rules::TokenRule{I_IDENTIFIER}.map<ptr<Type>>([](Token tok) {
        return std::make_unique<TNamed>(tok.source_location(),
                                        std::string(tok.text()));
      });

  return int_type | float_type | bool_type | tuple_type | named_type;
}

auto make_proposition_rule() {
  auto basic =
      rules::CallRule<ptr<Expression>, expr>{} >>
      ~(rules::TokenRule{K_FOLLOWS} >> rules::TokenRule{I_IDENTIFIER} >>
        rules::CallRule<ArgumentsList, arguments>{});

  return basic.map<ptr<Proposition>>([](auto x) -> ptr<Proposition> {
    auto expr = std::move(std::get<0>(x));
    auto opt_rhs = std::move(std::get<1>(x));

    if (!opt_rhs.has_value()) {
      return std::make_unique<PExp>(std::move(expr));
    }

    auto rhs = std::move(opt_rhs.value());

    return std::make_unique<PFollows>(
        std::move(expr), std::string(std::get<1>(rhs).text()),
        std::move(std::get<3>(rhs)), std::get<4>(rhs).source_location());
  });
}
auto make_proof_rule() {
  auto basic = (rules::TokenRule{D_LPAREN} >> rules::TokenRule{D_RPAREN})
                   .map<nodle::io::SourceLocation>([](auto x) {
                     return std::get<1>(x).source_location();
                   }) |
               (rules::TokenRule{K_USE} >> rules::TokenRule{I_IDENTIFIER} >>
                rules::CallRule<ArgumentsList, arguments>{})
                   .map<ptr<Use>>([](auto content) {
                     auto use_tok = std::get<0>(content).source_location();
                     auto name = std::get<1>(content).text();
                     auto &expr = std::get<3>(content);
                     auto rparen = std::get<4>(content).source_location();

                     return std::make_unique<Use>(use_tok, std::string(name),
                                                  std::move(expr), rparen);
                   });
  auto rest = (rules::TokenRule{D_SEMICOLON} >> basic)
                  .map<std::variant<nodle::io::SourceLocation, ptr<Use>>>(
                      [](auto x) { return std::move(std::get<1>(x)); });

  return (basic >> rest.many()).map<Proof>([](auto x) {
    auto &rhs = std::get<1>(x);
    rhs.insert(rhs.begin(), std::move(std::get<0>(x)));

    Proof proof;
    for (auto &v : rhs) {
      if (v.index() == 1) {
        proof.emplace_back(std::move(std::get<1>(v)));
      }
    }

    return proof;
  });
}

auto make_params_rule() {
  auto basic = (rules::TokenRule{I_IDENTIFIER} >> rules::TokenRule{D_COLON} >>
                rules::CallRule<ptr<Type>, type>{})
                   .map<ptr<Param>>([](auto x) {
                     auto ident = std::get<0>(x);
                     auto type = std::move(std::get<2>(x));

                     return std::make_unique<Param>(ident.source_location(),
                                                    std::string(ident.text()),
                                                    std::move(type));
                   });

  auto rest =
      (rules::TokenRule{D_COMMA} >> basic)
          .map<ptr<Param>>([](auto x) { return std::move(std::get<1>(x)); })
          .many();

  return (~(basic >> rest)).map<Params>([](auto x) {
    Params res;

    if (x.has_value()) {
      auto &xv = x.value();

      res.push_back(std::move(std::get<0>(xv)));

      for (auto &xve : std::get<1>(xv)) {
        res.push_back(std::move(xve));
      }
    }

    return res;
  });
}

auto make_function_rule() {
  auto props = (rules::TokenRule{K_SUCH} >> rules::TokenRule{K_THAT} >>
                rules::CallRule<ptr<Proposition>, proposition>{})
                   .map<ptr<Proposition>>(
                       [](auto x) { return std::move(std::get<2>(x)); })
                   .many();
  auto definition =
      (rules::TokenRule{K_DEF} >> rules::TokenRule{I_IDENTIFIER} >>
       rules::TokenRule{D_LPAREN} >> rules::CallRule<Params, params>{} >>
       rules::TokenRule{D_RPAREN} >> rules::CallRule<ptr<Type>, type>{} >>
       rules::TokenRule{D_ASSIGN} >> props >>
       rules::CallRule<ptr<Expression>, expr>{})
          .map<Function>([](auto x) {
            auto &[def, ident, lparen, params, rparen, rett, assgn, such, e] =
                x;

            Function f(def.source_location() | e->source_location);
            f.body = std::move(e);
            f.conditions = std::move(such);
            f.equivalence = {};
            f.is_override = false;
            f.name = std::string(ident.text());
            f.params = std::move(params);
            f.return_type = std::move(rett);
            return f;
          });

  auto overriden =
      (rules::TokenRule{K_OVERRIDE} >> definition >>
       rules::TokenRule{K_PROVE} >> rules::TokenRule{K_EQUIVALENCE} >>
       rules::CallRule<Proof, proof>{})
          .map<Function>([](auto x) {
            auto &[override, func, prove, equi, prf] = x;
            func.extend(override.source_location());
            func.extend(equi.source_location());
            for (auto &use : prf) {
              func.extend(use->source_location);
            }
            func.equivalence = {std::move(prf)};
            return std::move(func);
          });

  return definition | overriden;
}

auto make_such_that_rule() {
  return (rules::TokenRule{K_SUCH} >> rules::TokenRule{K_THAT} >>
          rules::CallRule<ptr<Proposition>, proposition>{})
      .map<ptr<Member>>([](auto x) {
        auto &[such, that, prop] = x;

        return std::make_unique<SuchThat>(such.source_location(),
                                          std::move(prop));
      });
}
auto make_prove_with_rule() {
  return (rules::TokenRule{K_PROVE} >>
          rules::CallRule<ptr<Proposition>, proposition>{} >>
          rules::TokenRule{K_WITH} >> rules::TokenRule{K_PROOF} >>
          rules::CallRule<Proof, proof>{})
      .map<ptr<Member>>([](auto x) {
        auto &[prove, prop, with, prf, prf_w] = x;

        return std::make_unique<Prove>(prove.source_location(), std::move(prop),
                                       std::move(prf_w));
      });
}
auto make_val_decl_rule() {
  return (rules::TokenRule{K_VAL} >> rules::TokenRule{I_IDENTIFIER} >>
          rules::TokenRule{D_COMMA} >> rules::CallRule<ptr<Type>, type>{})
      .map<ptr<Member>>([](auto x) {
        auto &[val, ident, comma, type] = x;

        return std::make_unique<ValDecl>(
            val.source_location(), std::string(ident.text()), std::move(type));
      });
}
auto make_val_def_rule() {
  return (rules::TokenRule{K_VAL} >> rules::TokenRule{I_IDENTIFIER} >>
          rules::TokenRule{D_COMMA} >> rules::CallRule<ptr<Type>, type>{} >>
          rules::TokenRule{D_ASSIGN} >>
          rules::CallRule<ptr<Expression>, expr>{})
      .map<ptr<Member>>([](auto x) {
        auto &[val, ident, comma, type, assgn, exp] = x;

        return std::make_unique<ValDef>(val.source_location(),
                                        std::string(ident.text()),
                                        std::move(type), std::move(exp));
      });
}
auto make_class_member_rule() {
  auto func_member = make_function_rule().map<ptr<Member>>(
      [](auto f) { return std::make_unique<Method>(std::move(f)); });

  return func_member | make_val_def_rule() | make_such_that_rule() |
         make_prove_with_rule();
}

template <typename T> auto make_many1_rule(auto base) {
  return (base >> (base.many())).template map<vec_ptr<T>>([](auto x) {
    auto &[b0, bs] = x;
    bs.insert(bs.begin(), std::move(b0));

    return std::move(bs);
  });
}

auto make_property_rule() {
  auto requirements =
      make_many1_rule<Member>(make_such_that_rule() | make_val_decl_rule());

  return (rules::TokenRule{K_PROPERTY} >> rules::TokenRule{I_IDENTIFIER} >>
          rules::TokenRule{D_LPAREN} >> rules::CallRule<Params, params>{} >>
          rules::TokenRule{D_RPAREN} >> rules::TokenRule{D_COMMA} >>
          requirements)
      .map<ptr<Declaration>>([](auto x) {
        auto &[kprop, ident, lparen, params, rparen, comma, reqs] = x;

        ptr<PropertyDecl> prop = std::make_unique<PropertyDecl>(
            kprop.source_location() | reqs.back()->source_location);
        prop->name = std::string(ident.text());
        prop->params = std::move(params);
        prop->requirements = std::move(reqs);
        return prop;
      });
}
auto make_lemma_rule() {
  auto statements =
      make_many1_rule<Member>(make_such_that_rule() | make_prove_with_rule());

  return (rules::TokenRule{K_LEMMA} >> rules::TokenRule{I_IDENTIFIER} >>
          rules::TokenRule{D_LPAREN} >> rules::CallRule<Params, params>{} >>
          rules::TokenRule{D_RPAREN} >> rules::TokenRule{D_COMMA} >> statements)
      .map<ptr<Declaration>>([](auto x) {
        auto &[klemma, ident, lparen, params, rparen, comma, sttms] = x;

        ptr<LemmaDecl> prop = std::make_unique<LemmaDecl>(
            klemma.source_location() | sttms.back()->source_location);
        prop->name = std::string(ident.text());
        prop->params = std::move(params);
        prop->statements = std::move(sttms);
        return prop;
      });
}
auto make_class_rule() {
  auto members = make_many1_rule<Member>(make_class_member_rule());

  return (rules::TokenRule{K_CLASS} >> rules::TokenRule{I_IDENTIFIER} >>
          rules::TokenRule{D_LPAREN} >> rules::CallRule<Params, params>{} >>
          rules::TokenRule{D_RPAREN} >> rules::TokenRule{D_COMMA} >> members)
      .map<ptr<Declaration>>([](auto x) {
        auto &[kclass, ident, lparen, params, rparen, comma, membs] = x;

        ptr<ClassDecl> prop = std::make_unique<ClassDecl>(
            kclass.source_location() | membs.back()->source_location);
        prop->name = std::string(ident.text());
        prop->params = std::move(params);
        prop->members = std::move(membs);
        return prop;
      });
}

auto make_declaration_rule() {
  auto func_declaration = make_function_rule().map<ptr<Declaration>>(
      [](auto x) { return std::make_unique<FunctionDecl>(std::move(x)); });

  return make_property_rule() | make_lemma_rule() | make_class_rule() |
         func_declaration;
}
auto make_program_rule() {
  return make_many1_rule<Declaration>(make_declaration_rule())
      .map<ptr<Program>>([](auto x) {
        ptr<Program> prog =
            std::make_unique<Program>(x.back()->source_location);
        prog->decls = std::move(x);

        return prog;
      });
}

static const auto object_rule = make_object_rule();
static const auto arguments_rule = make_arguments_list_rule();
static const auto nonempty_arguments_rule = make_nonempty_arguments_list_rule();
static const auto term_rule = make_term_rule();
static const auto expr_times_rule = make_expr_times_rule();
static const auto expr_addition_rule = make_expr_addition_rule();
static const auto expr_comparison_rule = make_expr_comparison_rule();
static const auto expr_bool_rule = make_expr_bool_rule();
static const auto expr_rule = make_expr_rule();

static const auto type_rule = make_type_rule();
static const auto proposition_rule = make_proposition_rule();
static const auto proof_rule = make_proof_rule();

static const auto params_rule = make_params_rule();
static const auto function_rule = make_function_rule();

static const auto program_rule = make_program_rule();

result<Expression> object(ParserContextPtr ctx) {
  return object_rule.parse(ctx);
}
rules::ParseResult<ArgumentsList> arguments(ParserContextPtr ptr) {
  return arguments_rule.parse(ptr);
}
rules::ParseResult<ArgumentsList> nonempty_arguments(ParserContextPtr ptr) {
  return nonempty_arguments_rule.parse(ptr);
}
result<Expression> term(ParserContextPtr ctx) { return term_rule.parse(ctx); }
result<Expression> expr_times(ParserContextPtr ctx) {
  return expr_times_rule.parse(ctx);
}
result<Expression> expr_addition(ParserContextPtr ctx) {
  return expr_addition_rule.parse(ctx);
}
result<Expression> expr_comparison(ParserContextPtr ctx) {
  return expr_comparison_rule.parse(ctx);
}
result<Expression> expr_bool(ParserContextPtr ctx) {
  return expr_bool_rule.parse(ctx);
}
result<Expression> expr(ParserContextPtr ctx) { return expr_rule.parse(ctx); }

result<Type> type(ParserContextPtr ctx) { return type_rule.parse(ctx); }
result<Proposition> proposition(ParserContextPtr ptr) {
  return proposition_rule.parse(ptr);
}
rules::ParseResult<Proof> proof(ParserContextPtr ptr) {
  return proof_rule.parse(ptr);
}

rules::ParseResult<Params> params(ParserContextPtr ptr) {
  return params_rule.parse(ptr);
}
rules::ParseResult<Function> function(ParserContextPtr ptr) {
  return function_rule.parse(ptr);
}

}; // namespace

std::unique_ptr<nodle::ast::Program>
nodle::parser::Parser::parse(nodle::parser::ParserContextPtr context) {
  if (context->remaining() == 0) {
    throw std::runtime_error("Empty program, unhandled.");
  }

  auto result = program_rule.parse(context);
  if (!result.has_value()) {
    throw std::runtime_error("Error on first token.");
  }

  if (context->remaining() != 0) {
    throw std::runtime_error("Some tokens remain.");
  }

  return std::move(*result);
}
