#ifndef CTP_SERIALIZE_HH
#define CTP_SERIALIZE_HH

#include <ctp/core.hh>

namespace ctp {

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