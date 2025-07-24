#ifndef CTP_CUSTOM_HH
#define CTP_CUSTOM_HH

#include <ctp/core.hh>
#include <ctp/serialize.hh>

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
        static consteval auto deserialize(std::meta::info r) -> target_type {
            return extract_maybe_ref<target<T>>(r);
        }
    };

    template <class... Ts>
    struct Reflect<std::tuple<Ts...>> {
        using target_type = std::tuple<target_or_ref<Ts>...>;

        static consteval auto serialize(Serializer& s, std::tuple<Ts...> const& t) -> void {
            auto& [...elems] = t;
            (s.push_constant_or_object(^^decltype(elems), elems), ...);
        }

        static consteval auto deserialize(auto... rs) -> target_type {
            return target_type(extract_maybe_ref<target_or_ref<Ts>>(rs)...);
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