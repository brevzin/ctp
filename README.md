# ctp

Extending support for class types as `c`onstant `t`emplate `p`arameters with reflection.

One way to think about the library is that it is a generalization of existing language and library features by way of extension:

|C++26|With `ctp`|
|-|-|
|`std::meta::reflect_constant(v)`|`ctp::reflect_constant(v)`|
|`std::meta::reflect_constant_array(r)`|`ctp::reflect_constant_array(v)`|
|`std::define_static_object(v)`|`ctp::define_static_object(v)`|
|`template <auto V>`|`template <ctp::Param V>`|
|`template <T V>`|`template <ctp::Param<T> V>`|

It's really as simple as that. If you wanted a `std::string` template parameter, you cannot write this:

```cpp
template <std::string S> // ill-formed
struct C {
    // would use S here
};
```

But you can now write this:

```cpp
template <ctp::Param<std::string> S>
struct C {
    // use S.value or S.get() here
};
```

The library supports: `std::string_view` and `std::string`, `std::optional<T>` and `std::variant<Ts...>`, `std::tuple<Ts...>`, `std::reference_wrapper<T>`, and `std::vector<T>`.

If you want to add support for your own (non-C++20 structural) type, you can do so by specializing `ctp::Reflect<T>`, which has to have three public members:

1. A type named `target_type`. This is you are going to deserialize as, which can be just the very same `T`. But if `T` requires allocation, then it cannot be, and you'll have to come up with an approximation (e.g. for `std::string`, the `target_type` is `std::string_view`).
2. The function `static consteval auto serialize(Serializer&, T const&) -> void;`, which pushes an arbitrary amount of reflections onto the `Serializer`. These reflections define template-argument-equivalence for `T`: two values that serialize the same reflections will produce the same object.
3. A deserialization function, which is going to take as its input each reflection that was serialized by `serialize`, in one of three forms. Choose the one most appropriate for your type:

    ```cpp
    // 1. Take each serialization as a constant template parameter
    template <meta::info R1, meta::info R2, ...>
    static consteval auto deserialize() -> target_type;

    // 2. Take each serialization as a function parameter
    static consteval auto deserialize(meta:info R1, meta::info R2, ...) -> target_type;

    // 3. Take the splice of serialization as a function parameter
    static consteval auto deserialize_constants(T1 t1, T2 t2, ...) -> target_type;
    ```

    In the library, `variant` uses the first form, `vector` and `string` use the second, and `optional`, `tuple`, `reference_wrapper`, `span`, and `string_view` use the third.
