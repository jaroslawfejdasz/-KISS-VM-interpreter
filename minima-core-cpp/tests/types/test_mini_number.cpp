/**
 * MiniNumber unit tests
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "types/MiniNumber.hpp"

using namespace minima;

TEST_SUITE("MiniNumber") {

    TEST_CASE("construction from string") {
        MiniNumber n("42");
        CHECK(n.toString() == "42");
    }

    TEST_CASE("construction from integer") {
        MiniNumber n(100LL);
        CHECK(n.toString() == "100");
    }

    TEST_CASE("zero constant") {
        CHECK(MiniNumber::zero().isZero());
    }

    TEST_CASE("isPositive / isNegative") {
        CHECK(MiniNumber("1").isPositive());
        CHECK(MiniNumber("-1").isNegative());
        CHECK_FALSE(MiniNumber("0").isPositive());
        CHECK_FALSE(MiniNumber("0").isNegative());
    }

    TEST_CASE("comparison") {
        MiniNumber a("10"), b("20");
        CHECK(a.isLessThan(b));
        CHECK(b.isGreaterThan(a));
        CHECK(a.isEqual(MiniNumber("10")));
    }

    TEST_CASE("arithmetic: add") {
        MiniNumber a("15"), b("7");
        CHECK(a.add(b).toString() == "22");
    }

    TEST_CASE("arithmetic: sub") {
        MiniNumber a("15"), b("7");
        CHECK(a.sub(b).toString() == "8");
    }

    TEST_CASE("arithmetic: mul") {
        MiniNumber a("6"), b("7");
        CHECK(a.mul(b).toString() == "42");
    }

    TEST_CASE("arithmetic: div") {
        MiniNumber a("100"), b("4");
        CHECK(a.div(b).toString() == "25");
    }

    TEST_CASE("arithmetic: mod") {
        MiniNumber a("10"), b("3");
        CHECK(a.mod(b).toString() == "1");
    }

    TEST_CASE("division by zero throws") {
        MiniNumber a("10"), b("0");
        CHECK_THROWS(a.div(b));
    }

    TEST_CASE("serialise / deserialise roundtrip") {
        MiniNumber original("123456789");
        auto bytes = original.serialise();
        size_t offset = 0;
        MiniNumber restored = MiniNumber::deserialise(bytes.data(), offset);
        CHECK(original.isEqual(restored));
    }
}
