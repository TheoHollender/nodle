
#pragma once

#include "nodle/parser/context.hpp"
#include "nodle/parser/graph/graph.hpp"
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
concept BasicRule = requires(const T t, ParserContextPtr ctx,
                             graph::ParserGraph &graph, std::string &rule) {
  typename T::return_type;

  { t.parse(ctx) } -> std::same_as<ParseResult<typename T::return_type>>;
  /* graph::Subgraph generate_graph (graph::ParserGraph &graph) const */
  { t.generate_graph(graph, rule) } -> std::same_as<graph::Subgraph>;
};

template <typename Derived, typename Ret> struct BasicRuleTrait;

template <BasicRule SR>
struct ManyRule : public BasicRuleTrait<ManyRule<SR>,
                                        std::vector<typename SR::return_type>> {
  using return_type = std::vector<typename SR::return_type>;

private:
  SR sub_rule_;

public:
  ManyRule(SR sub_rule) : sub_rule_(std::move(sub_rule)) {}

  ParseResult<return_type> parse(ParserContextPtr ctx) const {
    return_type result;

    while (true) {
      ParseResult<typename SR::return_type> sub_result = sub_rule_.parse(ctx);

      if (!sub_result.is_match_consumed()) {
        break;
      }

      result.push_back(std::move(*sub_result));
    }

    bool consumed = !result.empty();
    return ParseResult<return_type>::Match(std::move(result), consumed);
  }
  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    int start_loop = graph.allocate_node(rule);

    graph::Subgraph internal = sub_rule_.generate_graph(graph, rule);

    int end_loop = graph.allocate_node(rule);

    // enter loop
    graph.add_edge(graph::Edge::epsilon(start_loop, internal.start_node));
    graph.add_edge(graph::Edge::epsilon(internal.end_node, end_loop));
    // redo loop
    graph.add_edge(graph::Edge::back_edge(end_loop, internal.start_node));
    // skip loop
    graph.add_edge(graph::Edge::epsilon(start_loop, end_loop));

    return graph::Subgraph(start_loop, end_loop);
  }
};

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
  graph::Subgraph generate_graph(graph::ParserGraph &graph,
                                 std::string &rule) const {
    return sub_rule_.generate_graph(graph, rule);
  }
};

template <typename Derived, typename Ret> struct BasicRuleTrait {
  using return_type = Ret;

  template <typename U> MapRule<Derived, U> map(std::function<U(Ret)> func) {
    return MapRule<Derived, U>(static_cast<const Derived &>(*this),
                               std::move(func));
  }

  auto many() const {
    return ManyRule<Derived>(static_cast<const Derived &>(*this));
  }
};

} // namespace nodle::parser::rules
