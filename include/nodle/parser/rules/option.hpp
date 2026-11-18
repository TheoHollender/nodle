#pragma once

#include <optional>
#include <utility>

#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"

namespace nodle::parser::rules {

namespace option_utils {

namespace details {

// T                      -> std::optional<T>
// std::optional<T>       -> collapsed<T>      (so optionals never nest)
template <typename T> struct optional_collapsed {
  using type = std::optional<T>;
};

template <typename T> struct optional_collapsed<std::optional<T>> {
  using type = typename optional_collapsed<T>::type;
};

} // namespace details

template <typename T>
using optional_collapsed_t = typename details::optional_collapsed<T>::type;

/**
 * Wraps a value into a single optional.
 *
 *   collapse_into_option(x)                      -> optional<X>{x}
 *   collapse_into_option(optional<X>{x})         -> optional<X>{x}
 *   collapse_into_option(optional<X>{})          -> optional<X>{}
 *   collapse_into_option(optional<optional<X>>)  -> optional<X>
 *
 * NOTE: always call it with the template argument deduced. With an explicit
 * `collapse_into_option<optional<X>>(...)` the plain overload would be picked
 * and the optional would not be collapsed.
 */
template <typename T> optional_collapsed_t<T> collapse_into_option(T x) {
  return std::optional<T>(std::move(x));
}

template <typename T>
optional_collapsed_t<std::optional<T>>
collapse_into_option(std::optional<T> x) {
  if (x.has_value()) {
    return collapse_into_option(std::move(x.value()));
  }
  return optional_collapsed_t<std::optional<T>>();
}

} // namespace option_utils

/**
 * Optionally matches a sub rule. It never fails:
 *   - if the sub rule matches, the result holds its value (and keeps the
 *     consumed / zero-length status of the sub rule),
 *   - otherwise the result is an empty optional and a zero-length match.
 *
 * Optionals are collapsed: if the sub rule already returns a
 * std::optional<T>, the rule returns a std::optional<T> (not an
 * std::optional<std::optional<T>>). An empty optional returned by the sub rule
 * gives an empty optional (the match status of the sub rule is still kept).
 */
template <BasicRule SR>
struct OptionRule
    : public BasicRuleTrait<OptionRule<SR>, option_utils::optional_collapsed_t<
                                                typename SR::return_type>> {
  using return_type =
      option_utils::optional_collapsed_t<typename SR::return_type>;

private:
  SR sub_rule_;

public:
  OptionRule(SR sub_rule) : sub_rule_(std::move(sub_rule)) {}

  ParseResult<return_type> parse(ParserContextPtr ctx) const {
    ParseResult<typename SR::return_type> res = sub_rule_.parse(ctx);
    if (res.has_value()) {
      return ParseResult<return_type>::Match(
          option_utils::collapse_into_option(std::move(*res)),
          res.is_match_consumed());
    }

    return ParseResult<return_type>::Match(return_type(), false);
  }

  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    graph::Subgraph internal = sub_rule_.generate_graph(graph, rule);

    // add skip
    graph.add_edge(
        graph::Edge::epsilon(internal.start_node, internal.end_node));

    return internal;
  }
};

template <BasicRule SR> OptionRule<SR> operator~(SR sub_rule) {
  return OptionRule<SR>(std::move(sub_rule));
}

} // namespace nodle::parser::rules