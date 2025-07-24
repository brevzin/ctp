#define CTP_HAS_STRING_LITERAL
#include <ctp/ctp.hh>
// #include "ctp_single_header.hh"

constexpr int const& r1 = ctp::define_static_object(1);
constexpr int const& r2 = ctp::define_static_object(1);
static_assert(&r1 == &r2);

template <ctp::Param V>
struct X {
    static constexpr auto& value = V.value;
};

int main() {
    using namespace std::literals;

    {
        X<42> a;
        X<42> b;
        X<17> c;
        static_assert(std::same_as<decltype(a), decltype(b)>);
        static_assert(!std::same_as<decltype(a), decltype(c)>);
        static_assert(!std::same_as<decltype(b), decltype(c)>);
        static_assert(a.value == 42);
        static_assert(c.value == 17);
    }

    {
        X<"hello"s> a;
        X<"hello"s> b;
        X<"other"s> c;
        static_assert(std::same_as<decltype(a), decltype(b)>);
        static_assert(!std::same_as<decltype(a), decltype(c)>);
        static_assert(!std::same_as<decltype(b), decltype(c)>);
        static_assert(a.value.size() == 5);
        static_assert(a.value.data()[5] == '\0');
        static_assert(a.value == "hello"sv);
        static_assert(c.value == "other"sv);
    }

    {
        X<std::vector{1, 2, 3}> v1;
        X<std::vector{1, 2, 3}> v2;
        X<std::vector{1, 2, 3, 4}> v3;
        static_assert(std::same_as<decltype(v1), decltype(v2)>);
        static_assert(!std::same_as<decltype(v1), decltype(v3)>);
        static_assert(!std::same_as<decltype(v2), decltype(v3)>);
    }

    {
        X<std::optional<int>{}> o1;
        X<std::optional<int>(1)> o2;
        X<std::optional<int>(1)> o3;
        static_assert(std::same_as<decltype(o2), decltype(o3)>);
        static_assert(!std::same_as<decltype(o1), decltype(o2)>);
        static_assert(!std::same_as<decltype(o1), decltype(o3)>);
        static_assert(not o1.value);
        static_assert(o2.value.value() == 1);
        static_assert(&o2.value.value() == &o3.value.value());
    }

    {
        static int si = 0;
        X<std::tuple<int, int>(1, 0)> t1;
        X<std::tuple<int, int>(1, 0)> t2;
        X<std::tuple<int, int&>(1, si)> t3;
        X<std::tuple<int, int&>(1, si)> t4;
        static_assert(std::same_as<decltype(t1), decltype(t2)>);
        static_assert(!std::same_as<decltype(t1), decltype(t3)>);
        static_assert(std::same_as<decltype(t3), decltype(t4)>);
        static_assert(std::get<0>(t1.value) == 1);
        static_assert(std::get<1>(t1.value) == 0);
        static_assert(&std::get<1>(t3.value) == &si);

        static std::string ss;
        X<std::tuple<std::string, std::string&>("hello", ss)> t5;
        static_assert(std::get<0>(t5.value) == "hello"sv);
        static_assert(&std::get<1>(t5.value) == &ss);
    }

    {
        X<std::variant<int, std::string>(1)> v1;
        X<std::variant<int, std::string>(1)> v2;
        X<std::variant<int, std::string>("hello")> v3;
        static_assert(std::same_as<decltype(v1), decltype(v2)>);
        static_assert(std::get<int>(v1.value) == 1);
        static_assert(std::get<std::string_view>(v3.value) == "hello");
    }

    {
        X<"hello"> a;
        X<"hello"> b;
        X<"other"> c;

        static_assert(std::same_as<decltype(a), decltype(b)>);
        static_assert(!std::same_as<decltype(a), decltype(c)>);
        static_assert(!std::same_as<decltype(b), decltype(c)>);

        static_assert(a.value == "hello"sv);
        static_assert(c.value == "other"sv);

        constexpr char const* p = "hello";
        X<p> d;
        X<p+1> e;
        static_assert(std::same_as<decltype(a), decltype(d)>);
        static_assert(d.value + 1 == e.value);
    }
}