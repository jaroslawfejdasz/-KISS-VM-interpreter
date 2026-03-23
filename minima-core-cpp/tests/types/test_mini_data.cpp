/**
 * MiniData unit tests
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "types/MiniData.hpp"

using namespace minima;

TEST_SUITE("MiniData") {

    TEST_CASE("construction from hex string") {
        MiniData d = MiniData::fromHex("0xDEADBEEF");
        CHECK(d.size() == 4);
        CHECK(d.bytes()[0] == 0xDE);
        CHECK(d.bytes()[3] == 0xEF);
    }

    TEST_CASE("hex string roundtrip") {
        MiniData d = MiniData::fromHex("0xCAFEBABE");
        CHECK(d.toHexString() == "0xCAFEBABE");
    }

    TEST_CASE("concat") {
        MiniData a = MiniData::fromHex("0x0102");
        MiniData b = MiniData::fromHex("0x0304");
        MiniData c = a.concat(b);
        CHECK(c.size() == 4);
        CHECK(c.toHexString() == "0x01020304");
    }

    TEST_CASE("subset") {
        MiniData d = MiniData::fromHex("0x01020304");
        MiniData s = d.subset(1, 2);
        CHECK(s.size() == 2);
        CHECK(s.bytes()[0] == 0x02);
        CHECK(s.bytes()[1] == 0x03);
    }

    TEST_CASE("subset out of range throws") {
        MiniData d = MiniData::fromHex("0x0102");
        CHECK_THROWS(d.subset(1, 5));
    }

    TEST_CASE("sha3 produces 32 bytes") {
        MiniData d = MiniData::fromHex("0x01");
        MiniData h = d.sha3();
        CHECK(h.size() == 32);
    }

    TEST_CASE("serialise / deserialise roundtrip") {
        MiniData original = MiniData::fromHex("0xDEADBEEFCAFEBABE");
        auto bytes = original.serialise();
        size_t offset = 0;
        MiniData restored = MiniData::deserialise(bytes.data(), offset);
        CHECK(original == restored);
    }
}
