#ifndef CTP_CTP_HH
#define CTP_CTP_HH

#include <meta>
#include <ranges>

// Extending support for class types as constant template parameters with the
// use of C++26 Reflection

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
    // Where serialize() has to return a reflection of an object of type target-Type.

    template <class T>
    struct Reflect;

    namespace impl {
        template <class T>
        using custom_target = Reflect<T>::target_type;
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
        inline constexpr target<T> the_object = Reflect<T>::deserialize(Is...);

        // This is the simpler (private) object that just exists for reflect_constant on scalars
        template <auto V>
        inline constexpr auto simple_object = V;

        // This is the singular (private) object that will be used for reflect_constant_array
        template <class T, std::meta::info... Is>
        inline constexpr target<T> the_array[] = {[:Is:]...};
    }

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
        consteval auto push_constant(T const& v) -> void;

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

    // Extension of std::meta::reflect_constant. Returns a reflection representing
    // the object that is equivalent to the provided value. For non-structural
    // class types, this is customized via Reflect<T>
    inline constexpr auto reflect_constant = []<class T>(T const& v){
        if constexpr (is_structural_type(^^T)) {
            if constexpr (is_class_type(^^T)) {
                // For class types, std::meta::reflect_constant does what we want
                return std::meta::reflect_constant(v);
            } else {
                // For scalar types, it gives a value though. So we need to do more
                // work to actually ensure an object
                return object_of(substitute(^^impl::simple_object, {std::meta::reflect_constant(v)}));
            }
        } else {
            // For non-structural types, customize via Reflect<T>::serialize
            if constexpr (requires { Reflect<T>::serialize(v); }) {
                return Reflect<T>::serialize(v);
            } else {
                auto s = Serializer(^^T);
                Reflect<T>::serialize(s, v);
                return s.finalize();
            }
        }
    };

    // Extension of std::meta::reflect_constant_array. Returns a reflection representing
    // an array of target<T>, where T is the value type of the range.
    inline constexpr auto reflect_constant_array = []<std::ranges::input_range R>(R&& r){
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
            // of type target<T>, for all types T. so here we just extract
            return extract<target<T> const&>(reflect_constant(v));
        };

    template <class T>
    consteval auto Serializer::push_constant(T const& v) -> void {
        push(reflect_constant(v));
    }

    // The main user-facing interface: ctp::Param<T> is the way to have a constant
    // template parameter of type T(~ish).
    template <class T>
    struct Param {
        using U = target<T>;
        U const& value;

        consteval Param(T const& v) : value(define_static_object(v)) { }
        consteval operator U const&() const { return value; }
        consteval auto get() const -> U const& { return value; }
        consteval auto operator*() const -> U const& { return value; }
        consteval auto operator->() const -> U const* { return std::addressof(value); }
    };
}

// customizations follow
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

        static consteval auto deserialize() -> target_type { return {}; }
        static consteval auto deserialize(std::meta::info r) -> target_type { return extract<target<T> const&>(r); }
    };

    template <class... Ts>
    struct Reflect<std::tuple<Ts...>> {
        using target_type = std::tuple<target_or_ref<Ts>...>;

        static consteval auto serialize(Serializer& s, std::tuple<Ts...> const& t) -> void {
            auto& [...elems] = t;
            (s.push_constant_or_object(^^decltype(elems), elems), ...);
        }

        static consteval auto deserialize(auto... rs) -> target_type {
            return target_type(extract<target_or_ref<Ts> const&>(rs)...);
        }
    };

    template <class... Ts>
    struct Reflect<std::variant<Ts...>> {
        using target_type = std::variant<target<Ts>...>;

        template <size_t I, std::meta::info R>
        static constexpr auto the_object = target_type(std::in_place_index<I>, [:R:]);

        static consteval auto serialize(std::variant<Ts...> const& v) -> std::meta::info {
            // visit should work, but can't because of LWG4197
            template for (constexpr size_t I : std::views::iota(0zu, sizeof...(Ts))) {
                if (I == v.index()) {
                    return substitute(
                        ^^the_object,
                        {
                            reflect_constant(I),
                            reflect_constant(reflect_constant(std::get<I>(v)))
                        }
                    );
                }
            }

            std::unreachable();
        }
    };
}

#endif