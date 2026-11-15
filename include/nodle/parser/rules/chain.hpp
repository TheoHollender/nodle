
#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"

namespace nodle::parser::rules {

namespace chain_util {

namespace detail {

template <typename T> struct is_std_tuple : std::false_type {};

template <typename... Ts>
struct is_std_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_tuple_v = is_std_tuple<std::decay_t<T>>::value;

template <typename T> constexpr auto as_tuple(T &&value) {
  if constexpr (is_std_tuple_v<T>) {
    return std::decay_t<T>(std::forward<T>(value));
  } else {
    return std::tuple<std::decay_t<T>>(std::forward<T>(value));
  }
}

} // namespace detail

/**
 * Chains two values into one flat tuple.
 *
 *   unpack_chain(x, y)                 -> {x, y}
 *   unpack_chain({a1..aK}, y)          -> {a1, ..., aK, y}
 *   unpack_chain(x, {b1..bL})          -> {x, b1, ..., bL}
 *   unpack_chain({a1..aK}, {b1..bL})   -> {a1, ..., aK, b1, ..., bL}
 */
template <typename X, typename Y> constexpr auto unpack_chain(X &&x, Y &&y) {
  return std::tuple_cat(detail::as_tuple(std::forward<X>(x)),
                        detail::as_tuple(std::forward<Y>(y)));
}

template <typename X, typename Y>
using unpack_chain_t =
    decltype(unpack_chain(std::declval<X>(), std::declval<Y>()));

} // namespace chain_util

template <BasicRule SR1, BasicRule SR2>
struct ChainRule : public BasicRuleTrait<
                       ChainRule<SR1, SR2>,
                       chain_util::unpack_chain_t<typename SR1::return_type,
                                                  typename SR2::return_type>> {
  using return_type = chain_util::unpack_chain_t<typename SR1::return_type,
                                                 typename SR2::return_type>;

private:
  SR1 sub_rule1_;
  SR2 sub_rule2_;

public:
  ChainRule(SR1 sub_rule1, SR2 sub_rule2)
      : sub_rule1_(sub_rule1), sub_rule2_(sub_rule2) {}

  ParseResult<return_type> parse(ParserContextPtr ctx) const {
    ParseResult<typename SR1::return_type> res1 = sub_rule1_.parse(ctx);
    if (!res1.has_value()) {
      return ParseResult<return_type>::NoMatch();
    }

    ParseResult<typename SR2::return_type> res2 = sub_rule2_.parse(ctx);
    if (!res2.has_value()) {
      if (res1.is_match_zero()) {
        return ParseResult<return_type>::NoMatch();
      }

      // TODO raise an exception more cleanly with source location.
      throw std::runtime_error("");
    }

    return ParseResult<return_type>::Match(
        chain_util::unpack_chain<typename SR1::return_type,
                                 typename SR2::return_type>(
            std::forward<typename SR1::return_type>(*res1),
            std::forward<typename SR2::return_type>(*res2)),
        res1.is_match_consumed() || res2.is_match_consumed());
  }
};

template <BasicRule SR1, BasicRule SR2>
ChainRule<SR1, SR2> operator>>(SR1 sub_rule1, SR2 sub_rule2) {
  return ChainRule<SR1, SR2>(std::move(sub_rule1), std::move(sub_rule2));
}

} // namespace nodle::parser::rules
