// Copyright (C) 2018 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <foonathan/tiny/spare_bits.hpp>

#include <catch.hpp>

#include <foonathan/tiny/aligned_ptr.hpp>
#include <foonathan/tiny/pointer_variant_impl.hpp>
#include <foonathan/tiny/tiny_pair.hpp>

using namespace foonathan::tiny;

namespace foonathan
{
    namespace tiny
    {
        // we need an operator== for pointer_variant_impl
        template <typename... Ts>
        bool operator==(const pointer_variant_impl<Ts...>& lhs,
                        const pointer_variant_impl<Ts...>& rhs)
        {
            return lhs.tag() == rhs.tag() && lhs.get() == rhs.get();
        }

    } // namespace tiny
} // namespace foonathan

namespace
{
    template <typename T>
    void verify_extract_put(T big)
    {
        tiny_pair_impl<T, spare_bits<T>()> pair(big, 0u);
        REQUIRE(decltype(pair)::is_compressed::value);
        REQUIRE(pair.integer() == 0u);
        REQUIRE(pair.big() == big);

        auto max_value =
            std::min(std::uintmax_t(255), (static_cast<std::uintmax_t>(1) << spare_bits<T>()) - 1);
        for (auto i = static_cast<typename decltype(pair)::integer_type>(0); i != max_value; ++i)
        {
            pair.set_integer(i);
            REQUIRE(pair.integer() == i);
            REQUIRE(pair.big() == big);
        }
    }

    template <typename T>
    void verify_clear(T big)
    {
        T t(big);
        if (spare_bits<T>() > 0u)
            put_spare_bits(t, 1u);
        clear_spare_bits(t);
        REQUIRE(t == big);
    }

    template <typename T>
    void verify_spare_bits(T big)
    {
        verify_extract_put(big);
        verify_clear(big);
    }
} // namespace

TEST_CASE("spare_bits_traits default")
{
    REQUIRE(spare_bits<std::string>() == 0u);
    verify_spare_bits(std::string("hello world"));
}

TEST_CASE("spare_bits_traits bool")
{
    REQUIRE(spare_bits<bool>() == 7);
    verify_spare_bits(true);
    verify_spare_bits(false);
}

namespace
{
    template <typename T>
    void verify_pointer(std::size_t align, std::size_t spare)
    {
        REQUIRE(alignof(T) == align);
        REQUIRE(spare_bits<T*>() == spare);

        T obj;
        verify_spare_bits(&obj);
        verify_spare_bits(static_cast<T*>(nullptr));
    }
} // namespace

TEST_CASE("spare_bits_traits pointer")
{
    SECTION("min align == ?")
    {
        REQUIRE(spare_bits<void*>() == 0u);
        REQUIRE(spare_bits<const void*>() == 0u);
        REQUIRE(spare_bits<volatile void*>() == 0u);
        REQUIRE(spare_bits<const volatile void*>() == 0u);
    }
    SECTION("min align == 1")
    {
        REQUIRE(spare_bits<char*>() == 0u);
        REQUIRE(spare_bits<const unsigned char*>() == 0u);
    }
    SECTION("min align > 1")
    {
        verify_pointer<std::uint16_t>(2, 1);
        verify_pointer<std::uint32_t>(4, 2);
        verify_pointer<std::uint64_t>(8, 3);
    }
}

namespace
{
    struct member_test_type
    {
        int  i;
        bool b1;
        bool b2;

        friend bool operator==(member_test_type lhs, member_test_type rhs)
        {
            return lhs.i == rhs.i && lhs.b1 == rhs.b1 && lhs.b2 == rhs.b2;
        }
    };
} // namespace

namespace foonathan
{
    namespace tiny
    {
        template <>
        struct spare_bits_traits<member_test_type>
        : spare_bits_traits_member<member_test_type, FOONATHAN_TINY_MEMBER(&member_test_type::b1),
                                   FOONATHAN_TINY_MEMBER(&member_test_type::b2)>
        {
        };
    } // namespace tiny
} // namespace foonathan

TEST_CASE("spare_bits_traits member")
{
    REQUIRE(spare_bits<member_test_type>() == 2 * spare_bits<bool>());

    member_test_type obj{42, true, true};
    verify_spare_bits(obj);
    obj.b1 = false;
    verify_spare_bits(obj);
    obj.b2 = false;
    verify_spare_bits(obj);
}

namespace
{
    template <typename T>
    void verify_pair(std::size_t spare)
    {
        using pair = tiny_bool_pair<T*>;
        REQUIRE(spare_bits<pair>() == spare);

        verify_spare_bits(pair(nullptr, false));

        T obj;
        verify_spare_bits(pair(&obj, true));
    }
} // namespace

TEST_CASE("spare_bits_traits tiny_bool_pair")
{
    SECTION("spare_bits<T>() == 1u")
    {
        verify_pair<std::uint16_t>(0u);
    }
    SECTION("spare_bits<T>() > 1u")
    {
        verify_pair<std::uint64_t>(2u);
    }
    SECTION("spare_bits<T>() == 0u")
    {
        // now the pair uses the spare bits of the integer necessary to store the bool
        verify_pair<std::uint8_t>(7u);
    }
}

TEST_CASE("spare_bits_traits aligned_ptr")
{
    REQUIRE(spare_bits<aligned_ptr<void, 8>>() == 3u);
    verify_spare_bits(aligned_ptr<void, 8>(nullptr));

    alignas(8) int obj;
    verify_spare_bits(aligned_ptr<void, 8>(&obj));
}

TEST_CASE("spare_bits_traits pointer_variant_impl")
{
    SECTION("not compressed")
    {
        using variant = pointer_variant_impl<char, int, double>;
        REQUIRE(spare_bits<variant>() == sizeof(std::size_t) * CHAR_BIT - 2);

        verify_spare_bits(variant(nullptr));

        int i = 0;
        verify_spare_bits(variant(&i));
    }
    SECTION("compressed: 2")
    {
        using variant = pointer_variant_impl<std::int32_t>;
        REQUIRE(spare_bits<variant>() == 2);

        verify_spare_bits(variant(nullptr));

        std::int32_t i = 0;
        verify_spare_bits(variant(&i));
    }
    SECTION("compressed: 1")
    {
        using variant = pointer_variant_impl<std::int32_t, std::uint32_t>;
        REQUIRE(spare_bits<variant>() == 1);

        verify_spare_bits(variant(nullptr));

        std::int32_t i = 0;
        verify_spare_bits(variant(&i));
    }
    SECTION("compressed: 4")
    {
        using variant = pointer_variant_impl<aligned_obj<int, 32>, aligned_obj<unsigned, 64>>;
        REQUIRE(spare_bits<variant>() == 4);

        verify_spare_bits(variant(nullptr));

        alignas(32) int i = 0;
        verify_spare_bits(variant(&i));
    }
    SECTION("compressed: 0")
    {
        using variant = pointer_variant_impl<std::int32_t, std::uint64_t, std::int64_t>;
        REQUIRE(spare_bits<variant>() == 0);
    }
}
