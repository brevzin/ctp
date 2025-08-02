#ifndef CTP_CTP_HH
#define CTP_CTP_HH

#ifndef CTP_CORE_HH
#define CTP_CORE_HH

#include <meta>
#include <ranges>

namespace ctp {

// The customization point to opt-in to having your type be usable as a
// ctp::Param.
//
// Reflect<T> must provide a target_type, a serialize() with the
// following signature, and one of three forms of deserialization function:
//
//
//  struct {
//      using target_type = ????;
//      static consteval auto serialize(Serializer&, T const&) -> void;
//
//      // Option 1. Take all of the serialized infos as constant template parameters
//      template <std::meta::info... Is>
//      static consteval auto deserialize() -> target_type;
//
//      // Option 2: Take all of the serialized infos as function parameters
//      static consteval auto deserialize(std::meta::info...) -> target_type;
//
//      // Option 3: Take the splices of all of the serialized infos as function
//      // parameters
//      static consteval auto deserialize_constants(auto&&...) -> target_type;
//
//  };

#ifndef CTP_META_IS_STRUCTURAL
#include <algorithm>
namespace std::meta {
    // this really should be in std:: but it's not yet, so...
    consteval auto is_structural_type(info type) -> bool {
        auto ctx = access_context::unchecked();

        return is_scalar_type(type)
            or is_lvalue_reference_type(type)
            or is_class_type(type)
                and ranges::all_of(bases_of(type, ctx),
                        [](info o){
                            return is_public(o)
                            and is_structural_type(type_of(o));
                        })
                and ranges::all_of(nonstatic_data_members_of(type, ctx),
                        [](info o){
                            return is_public(o)
                            and not is_mutable_member(o)
                            and is_structural_type(
                                    remove_all_extents(type_of(o)));
                        });

    }
}
#endif

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


namespace impl {
    // This is the singular (private) object that will be
    // constructed from the serialization-deserialization round trip.
    template <class T, std::meta::info... Is>
    inline constexpr target<T> the_object = []{
        if constexpr (requires { Reflect<T>::template deserialize<Is...>(); }) {
            return Reflect<T>::template deserialize<Is...>();
        } else if constexpr (requires { Reflect<T>::deserialize(Is...); }) {
            return Reflect<T>::deserialize(Is...);
        } else {
            return Reflect<T>::deserialize_constants([:Is:]...);
        }
    }();

    // This is the singular (private) object that will be used for reflect_constant_array
    template <class T, std::meta::info... Is>
    inline constexpr target<T> the_array[] = {[:Is:]...};

    // The default/simple approach to serialization, using Serializer
    template <class T>
    consteval auto default_serialize(T const& v) -> std::meta::info;
}

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

// Extension of std::meta::reflect_constant that is customizable via Reflect<T>.
// For scalar types, returns a reflection representing a value.
// For class types, returns a reflection representing an object.
inline constexpr auto reflect_constant =
    []<class T>(T v){
        if constexpr (is_structural_type(^^T)) {
            normalize(v);
            return std::meta::reflect_constant(v);
        } else {
            // For non-structural types, customize via Reflect<T>::serialize
            return impl::default_serialize(v);
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

}

#endif
#ifndef CTP_SERIALIZE_HH
#define CTP_SERIALIZE_HH


namespace ctp {

// For a lot of types, the easiest way to do serialization is just to push a
// bunch of reflections and then get them all back out as function parameters.
// This API is provided as a convenience, and is used by providing both:
//
//      auto Reflect<T>::serialize(Serializer&, T const&) -> void;
//      auto Reflect<T>::deserialize(std::meta::info...) -> target_type;
class Serializer {
    std::vector<std::meta::info> parts;
public:
    explicit consteval Serializer(std::meta::info type) {
        parts.push_back(type);
    }

    // Push another reflection
    consteval auto push(std::meta::info r) -> void {
        parts.push_back(std::meta::reflect_constant(r));
    }

    // Push a ctp-reflectable value
    template <class T>
    consteval auto push_constant(T const& v) -> void {
        push(reflect_constant(v));
    }

    // Push an object (for when the identity of the object, as opposed to its value, matters)
    // e.g. this is for reference members
    template <class T>
    consteval auto push_object(T&& o) -> void {
        push(std::meta::reflect_object(o));
    }

    // Equivalent to: push_object(o) if type is an lvalue reference, otherwise push_constant(o)
    template <class T>
    consteval auto push_constant_or_object(std::meta::info type, T&& o) -> void {
        if (is_lvalue_reference_type(type)) {
            push_object(o);
        } else {
            push_constant(o);
        }
    }

    // Returns: A reflection representing an object of type target<T>,
    // where T is the type the Serializer was constructed from, that is
    // initialized with Reflect<T>::dserialize(r...) where {r...} is the
    // sequence of reflections that were push()-ed onto this Serializer
    consteval auto finalize() const -> std::meta::info {
        return object_of(substitute(^^impl::the_object, parts));
    }
};

namespace impl {
    template <class T>
    consteval auto default_serialize(T const& v) -> std::meta::info {
        auto s = Serializer(^^T);
        Reflect<T>::serialize(s, v);
        return s.finalize();
    }
}

}

#endif
#ifndef CTP_PARAM_HH
#define CTP_PARAM_HH


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
#ifndef CTP_CUSTOM_HH
#define CTP_CUSTOM_HH


namespace ctp {
    template <>
    struct Reflect<std::string> {
        using target_type = std::string_view;

        static consteval auto serialize(Serializer& ser, std::string const& str) -> void {
            ser.push(std::meta::reflect_constant_string(str));
        }

        static consteval auto deserialize(std::meta::info r) -> std::string_view {
            return std::string_view(extract<char const*>(r), extent(type_of(r)) - 1);
        }
    };

    template <class T>
    struct Reflect<std::vector<T>> {
        using target_type = std::span<target<T> const>;

        static consteval auto serialize(Serializer& s, std::vector<T> const& v) -> void {
            s.push(reflect_constant_array(v));
        }

        static consteval auto deserialize(std::meta::info r) -> std::span<target<T> const> {
            return std::span(extract<target<T> const*>(r), extent(type_of(r)));
        }
    };

    template <class T>
    struct Reflect<std::optional<T>> {
        using target_type = std::optional<target<T>>;

        static consteval auto serialize(Serializer& s, std::optional<T> const& o) -> void {
            if (o) {
                s.push_constant(*o);
            }
        }

        static consteval auto deserialize_constants() -> target_type { return {}; }
        static consteval auto deserialize_constants(target_or_ref<T> const& v) -> target_type {
            return v;
        }
    };

    template <class... Ts>
    struct Reflect<std::tuple<Ts...>> {
        using target_type = std::tuple<target_or_ref<Ts>...>;

        static consteval auto serialize(Serializer& s, std::tuple<Ts...> const& t) -> void {
            auto& [...elems] = t;
            (s.push_constant_or_object(^^decltype(elems), elems), ...);
        }

        static consteval auto deserialize_constants(target_or_ref<Ts> const&... vs) -> target_type {
            return target_type(vs...);
        }
    };

    template <class... Ts>
    struct Reflect<std::variant<Ts...>> {
        using target_type = std::variant<target<Ts>...>;

        static consteval auto serialize(Serializer& s, std::variant<Ts...> const& v) -> void {
            s.push_constant(v.index());
            // visit should work, but can't because of LWG4197
            template for (constexpr size_t I : std::views::iota(0zu, sizeof...(Ts))) {
                if (I == v.index()) {
                    s.push_constant(std::get<I>(v));
                    return;
                }
            }
        }

        template <std::meta::info I, std::meta::info R>
        static consteval auto deserialize() -> target_type {
            return target_type(std::in_place_index<([:I:])>, [:R:]);
        }
    };

    template <class T>
    struct Reflect<std::reference_wrapper<T>> {
        using target_type = std::reference_wrapper<T>;

        static consteval auto serialize(Serializer& s, std::reference_wrapper<T> r) -> void {
            s.push_object(r.get());
        }

        static consteval auto deserialize_constants(T& r) -> target_type {
            return r;
        }
    };

    template <>
    struct Reflect<std::string_view> {
        using target_type = std::string_view;

        static consteval auto serialize(Serializer& s, std::string_view sv) -> void {
            s.push_constant(sv.data());
            s.push_constant(sv.size());
        }

        static consteval auto deserialize_constants(char const* data, size_t size) -> std::string_view {
            return std::string_view(data, size);
        }
    };

    template <class T, size_t N>
    struct Reflect<std::span<T, N>> {
        using target_type = std::span<T, N>;

        static consteval auto serialize(Serializer& s, std::span<T, N> sp) -> void {
            s.push_constant(sp.data());
            s.push_constant(sp.size());
        }

        static consteval auto deserialize_constants(T const* data, size_t size) -> target_type {
            return target_type(data, size);
        }
    };
}

#endif

#endif
