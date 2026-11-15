
#pragma once

#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"

namespace nodle::parser::rules {

template <BasicRule SR>
struct OptionRule
    : public BasicRuleTrait<OptionRule<SR>,
                            std::optional<typename SR::return_type>> {
  using return_type = std::optional<typename SR::return_type>;

private:
  SR sub_rule_;

public:
  OptionRule(SR sub_rule) : sub_rule_(std::move(sub_rule)) {}

  ParseResult<return_type> parse(ParserContextPtr ctx) {
    ParseResult<typename SR::return_type> res = sub_rule_.parse(ctx);
    if (res.has_value()) {
      return ParseResult<return_type>::Match(
          std::optional<typename SR::return_type>(std::move(*res)),
          res.is_match_consumed());
    }

    return ParseResult<return_type>::Match(
        std::optional<typename SR::return_type>(), false);
  }
};

} // namespace nodle::parser::rules
