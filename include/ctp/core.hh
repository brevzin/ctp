#ifndef CTP_CORE_HH
#define CTP_CORE_HH

#include <meta>
#include <ranges>

namespace ctp {

// The customization point to opt-in to having your type be usable as a
// ctp::Param. Reflect<T> can take one of two forms.
//
// Either:
//
//  struct {
//      using target_type = ????;
//      static consteval auto serialize(Serializer&, T const&) -> void;
//      static consteval auto deserialize(std::meta::info...) -> target_type;
//  };
//
// 1. The target_type need not be C++20-structural itself, but cannot allocate.
// 2. serialize() can push any number of std::meta::info's into the serializer.
// 3. deserializer() will be invoked with the std::meta::info's that were
//    pushed.
//
// Or:
//
//  struct {
//      using target_type = ????;
//      static consteval auto serialize(T const&) -> std::meta::info;
//  };
//
// Where serialize() has to return a reflection of an object of type target_type.

template <class T>
struct Reflect;

namespace impl {
    template <class T> using custom_target = Reflect<T>::target_type;
}

// The target type for a given T. For structural types, the target is always
// T in order to be consistent with regular C++20 template parameters.
template <class T>
using target = [: is_structural_type(^^T) ? ^^T : substitute(^^impl::custom_target, {^^T}) :];

// For those cases where references need to be preserved,
// target_or_ref<T&> is T& but target_or_ref<T> is target<T>
template <class T>
using target_or_ref = [: is_lvalue_reference_type(^^T) ? ^^T : substitute(^^target, {^^T}) :];


// extract<T>(r) works for values and objects (but copies) while
// extract<T const&>(r) works only for objects.
// This ensures that we do the efficient thing.
template <class T>
inline constexpr auto extract_maybe_ref = [](std::meta::info r) -> decltype(auto) {
    if constexpr (is_class_type(^^T)) {
        return extract<T const&>(r);
    } else {
        return extract<T>(r);
    }
};

namespace impl {
    // This is the singular (private) object that will be
    // constructed from the serialization-deserialization round trip.
    template <class T, std::meta::info... Is>
    inline constexpr target<T> the_object = Reflect<T>::deserialize(Is...);

    // This is the singular (private) object that will be used for reflect_constant_array
    template <class T, std::meta::info... Is>
    inline constexpr target<T> the_array[] = {[:Is:]...};

    // The default/simple approach to serialization, using Serializer
    template <class T>
    consteval auto default_serialize(T const& v) -> std::meta::info;
}

// Extension of std::meta::reflect_constant that is customizable via Reflect<T>.
// For scalar types, returns a reflection representing a value.
// For class types, returns a reflection representing an object.
inline constexpr auto reflect_constant =
    []<class T>(T const& v){
        if constexpr (is_structural_type(^^T)) {
            return std::meta::reflect_constant(v);
        } else {
            // For non-structural types, customize via Reflect<T>::serialize
            if constexpr (requires { Reflect<T>::serialize(v); }) {
                return Reflect<T>::serialize(v);
            } else {
                return impl::default_serialize(v);
            }
        }
    };

// Extension of std::meta::reflect_constant_array. Returns a reflection representing
// an array of target<T>, where T is the value type of the range.
inline constexpr auto reflect_constant_array =
    []<std::ranges::input_range R>(R&& r){
        std::vector<std::meta::info> elems = {^^std::ranges::range_value_t<R>};
        for (auto&& e : r) {
            elems.push_back(reflect_constant(reflect_constant(e)));
        }
        return substitute(^^impl::the_array, elems);
    };

// Extension of std::define_static_object, except based on ctp::reflect_constant
inline constexpr auto define_static_object =
    []<class T>(T const& v) -> target<T> const& {
        // ctp::reflect_constant gives us a reflection representing an object
        // UNLESS T is a scalar type, in which case we have to do something else
        if constexpr (is_class_type(^^T)) {
            return extract<target<T> const&>(reflect_constant(v));
        } else {
            // should be std::define_static_object but not implemented in clang
            return std::define_static_array(std::span(std::addressof(v), 1))[0];
        }
    };


inline constexpr auto normalize =
    []<class T>([[maybe_unused]] T& v) -> void {
        #ifdef CTP_HAS_STRING_LITERAL
        if constexpr (requires { std::is_string_literal(v); }) {
            if (char const* root = std::string_literal_from(v)) {
                char const* global = std::define_static_string(std::string_view(root));
                v = global + (v - root);
            }
        }
        #endif
    };

}

#endif