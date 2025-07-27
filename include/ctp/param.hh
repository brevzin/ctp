#ifndef CTP_PARAM_HH
#define CTP_PARAM_HH

#include <ctp/core.hh>

namespace ctp {

// The main user-facing interface: ctp::Param<T> is the way to have a constant
// template parameter of type T(~ish).
template <class T>
struct Param {
    using type = target<T>;
    type const& value;

    consteval Param(T const& v) : value(define_static_object(v)) { }
    consteval operator type const&() const { return value; }
    consteval auto get() const -> type const& { return value; }
    consteval auto operator*() const -> type const& { return value; }
    consteval auto operator->() const -> type const* { return std::addressof(value); }
};

template <class T> requires (is_structural_type(^^T))
struct Param<T> {
    using type = T;
    T value;

    consteval Param(T const& v) : value(v) { ctp::normalize(value); }
    consteval operator T const&() const { return value; }
    consteval auto get() const -> T const& { return value; }
    consteval auto operator*() const -> T const& { return value; }
    consteval auto operator->() const -> T const* { return std::addressof(value); }
};

template <class T>
Param(T) -> Param<T>;

}

#endif