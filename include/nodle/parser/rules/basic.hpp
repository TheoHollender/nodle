
#pragma once

#include "nodle/parser/context.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace nodle::parser::rules {

template <typename T> class ParseResult {
public:
  enum class Status { NoMatch, MatchZero, MatchConsumed };

private:
  Status status_;
  std::optional<T> value_;

  ParseResult(Status status, std::optional<T> val)
      : status_(status), value_(std::move(val)) {}

public:
  static ParseResult NoMatch() {
    return ParseResult(Status::NoMatch, std::nullopt);
  }
  static ParseResult Match(T value, bool consumed_tokens) {
    Status s = consumed_tokens ? Status::MatchConsumed : Status::MatchZero;
    return ParseResult(s, std::move(value));
  }

  Status status() const { return status_; }

  bool has_value() const { return status_ != Status::NoMatch; }

  bool is_no_match() const { return status_ == Status::NoMatch; }
  bool is_match_zero() const { return status_ == Status::MatchZero; }
  bool is_match_consumed() const { return status_ == Status::MatchConsumed; }

  T &operator*() & { return *value_; }
  const T &operator*() const & { return *value_; }
  T &&operator*() && { return std::move(*value_); }

  T *operator->() { return &(*value_); }
  const T *operator->() const { return &(*value_); }

  explicit operator bool() const { return has_value(); }
};

template <typename T>
concept BasicRule = requires(const T t, ParserContextPtr ctx) {
  typename T::return_type;

  { t.parse(ctx) } -> std::same_as<ParseResult<typename T::return_type>>;
};

template <typename Derived, typename Ret> struct BasicRuleTrait;

template <BasicRule SR, typename U>
struct MapRule : public BasicRuleTrait<MapRule<SR, U>, U> {
  using return_type = U;
  using source_type = typename SR::return_type;

  using func_type = std::function<return_type(source_type)>;

private:
  SR sub_rule_;
  func_type func_;

public:
  MapRule(SR sub_rule, func_type func)
      : sub_rule_(std::move(sub_rule)), func_(std::move(func)) {}

  ParseResult<return_type> parse(ParserContextPtr ctx) const {
    ParseResult<source_type> res = sub_rule_.parse(ctx);

    if (res.has_value()) {
      return ParseResult<return_type>::Match(func_(std::move(*res)),
                                             res.is_match_consumed());
    }

    return ParseResult<return_type>::NoMatch();
  }
};

template <typename Derived, typename Ret> struct BasicRuleTrait {
  using return_type = Ret;

  template <typename U> MapRule<Derived, U> map(std::function<U(Ret)> func) {
    return MapRule<Derived, U>(static_cast<const Derived &>(*this), std::move(func));
  }
};

} // namespace nodle::parser::rules
