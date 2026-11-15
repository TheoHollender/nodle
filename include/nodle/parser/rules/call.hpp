
#pragma once

#include "nodle/parser/context.hpp"
#include "nodle/parser/rules/basic.hpp"

namespace nodle::parser::rules {

template <typename RV, ParseResult<RV> (*Func)(ParserContextPtr)>
struct CallRule : public BasicRuleTrait<CallRule<RV, Func>, RV> {
public:
  using return_value = RV;

  ParseResult<RV> parse(ParserContextPtr ctx) { return Func(ctx); }
};

} // namespace nodle::parser::rules
