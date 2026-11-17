
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

}; // namespace
