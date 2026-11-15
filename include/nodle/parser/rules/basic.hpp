
#pragma once

#include "nodle/parser/context.hpp"
#include <optional>
#include <memory>

namespace nodle::parser::rules {
    
template <typename T>
class ParseResult {
public:
    enum class Status {
        NoMatch,
        MatchZero,
        MatchConsumed
    };

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

    bool has_value() const { 
        return status_ != Status::NoMatch; 
    }

    bool is_no_match() const { return status_ == Status::NoMatch; }
    bool is_match_zero() const { return status_ == Status::MatchZero; }
    bool is_match_consumed() const { return status_ == Status::MatchConsumed; }

    T& operator*() & {
        return *value_;
    }
    const T& operator*() const & {
        return *value_;
    }
    T&& operator*() && {
        return std::move(*value_);
    }

    T* operator->() {
        return &(*value_);
    }
    const T* operator->() const {
        return &(*value_);
    }

    explicit operator bool() const {
        return has_value();
    }
};

template<typename T>
concept BasicRule = requires (T t, std::shared_ptr<ParserContext> ctx) {
    typename T::return_type;

    { t.parse(ctx) } -> std::same_as<ParseResult<typename T::return_type>>;
};

template<typename Derived, typename Ret>
struct BasicRuleTrait {
    using return_type = Ret;
};

}
