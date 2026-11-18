
#pragma once
#include <variant>

#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"

namespace nodle::parser::rules {

namespace or_util {

namespace detail {

template <typename T> struct is_std_variant : std::false_type {};

template <typename... Ts>
struct is_std_variant<std::variant<Ts...>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_variant_v = is_std_variant<std::decay_t<T>>::value;

template <typename T> struct as_variant {
  using type = std::variant<std::decay_t<T>>;
};
template <typename... Ts> struct as_variant<std::variant<Ts...>> {
  using type = std::variant<Ts...>;
};
template <typename T>
using as_variant_t = typename as_variant<std::decay_t<T>>::type;

// Appends T to the variant V unless V already contains T.
template <typename V, typename T> struct append_unique;
template <typename... Ts, typename T>
struct append_unique<std::variant<Ts...>, T> {
  using type = std::conditional_t<(std::is_same_v<Ts, T> || ...),
                                  std::variant<Ts...>, std::variant<Ts..., T>>;
};

// Appends every alternative of the variant W to V, skipping duplicates.
template <typename V, typename W> struct merge_unique;
template <typename V> struct merge_unique<V, std::variant<>> {
  using type = V;
};
template <typename V, typename T, typename... Ts>
struct merge_unique<V, std::variant<T, Ts...>> {
  using type = typename merge_unique<typename append_unique<V, T>::type,
                                     std::variant<Ts...>>::type;
};

template <typename T> struct singleton_to_type {
  using type = T;
};
template <typename T> struct singleton_to_type<std::variant<T>> {
  using type = T;
};

} // namespace detail

/**
 * Type of the flat variant obtained by or-ing X and Y.
 *
 *   unpack_or_t<X, Y>                      -> variant<X, Y>
 *   unpack_or_t<variant<X, Y>, Z>          -> variant<X, Y, Z>
 *   unpack_or_t<X, variant<Y, Z>>          -> variant<X, Y, Z>
 *   unpack_or_t<variant<X, Y>, variant<Z>> -> variant<X, Y, Z>
 */
template <typename X, typename Y>
using unpack_or_t =
    typename detail::singleton_to_type<typename detail::merge_unique<
        typename detail::merge_unique<std::variant<>,
                                      detail::as_variant_t<X>>::type,
        detail::as_variant_t<Y>>::type>::type;

/**
 * Puts a value (plain or variant) into the flat variant R, which is
 * unpack_or_t<X, Y>. Only one of the two sides exists at runtime, so the
 * target type R is given explicitly (unlike unpack_chain).
 *
 *   unpack_or<R>(x)                -> R holding x
 *   unpack_or<R>(variant<X, Y>{y}) -> R holding y
 */
template <typename R, typename T> constexpr R unpack_or(T &&value) {
  if constexpr (detail::is_std_variant_v<T>) {
    return std::visit(
        [](auto &&alt) -> R {
          if constexpr (detail::is_std_variant_v<R>) {
            return R(std::in_place_type<std::decay_t<decltype(alt)>>,
                     std::forward<decltype(alt)>(alt));
          } else {
            return alt;
          }
        },
        std::forward<T>(value));
  } else {
    if constexpr (detail::is_std_variant_v<R>) {
      return R(std::in_place_type<std::decay_t<T>>, std::forward<T>(value));
    } else {
      return value;
    }
  }
}

} // namespace or_util

template <BasicRule SR1, BasicRule SR2>
struct OrRule
    : public BasicRuleTrait<OrRule<SR1, SR2>,
                            or_util::unpack_or_t<typename SR1::return_type,
                                                 typename SR2::return_type>> {
  using return_type = or_util::unpack_or_t<typename SR1::return_type,
                                           typename SR2::return_type>;

private:
  SR1 sub_rule1_;
  SR2 sub_rule2_;

public:
  OrRule(SR1 sub_rule1, SR2 sub_rule2)
      : sub_rule1_(sub_rule1), sub_rule2_(sub_rule2) {}

  ParseResult<return_type> parse(ParserContextPtr ctx) const {
    ParseResult<typename SR1::return_type> res1 = sub_rule1_.parse(ctx);
    if (res1.has_value()) {
      return ParseResult<return_type>::Match(
          or_util::unpack_or<return_type>(std::move(*res1)),
          res1.is_match_consumed());
    }

    ParseResult<typename SR2::return_type> res2 = sub_rule2_.parse(ctx);
    if (res2.has_value()) {
      return ParseResult<return_type>::Match(
          or_util::unpack_or<return_type>(std::move(*res2)),
          res2.is_match_consumed());
    }

    return ParseResult<return_type>::NoMatch();
  }
  graph::Subgraph generate_graph (graph::ParserGraph &graph, std::string &rule) const {
    int start_or = graph.allocate_node(rule);

    graph::Subgraph internal1 = sub_rule1_.generate_graph(graph, rule);
    graph::Subgraph internal2 = sub_rule2_.generate_graph(graph, rule);

    int end_or = graph.allocate_node(rule);

    graph.add_edge(graph::Edge::epsilon(start_or, internal1.start_node));
    graph.add_edge(graph::Edge::epsilon(start_or, internal2.start_node));
    
    graph.add_edge(graph::Edge::epsilon(internal1.end_node, end_or));
    graph.add_edge(graph::Edge::epsilon(internal2.end_node, end_or));

    return graph::Subgraph(start_or, end_or);
  }
};

template <BasicRule SR1, BasicRule SR2>
OrRule<SR1, SR2> operator|(SR1 sub_rule1, SR2 sub_rule2) {
  return OrRule<SR1, SR2>(std::move(sub_rule1), std::move(sub_rule2));
}

} // namespace nodle::parser::rules