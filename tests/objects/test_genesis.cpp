#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/objects/Genesis.hpp"
#include "../../src/objects/Pulse.hpp"
#include "../../src/objects/MiniByte.hpp"

using namespace minima;

// ── Genesis tests ─────────────────────────────────────────────────────────

TEST_SUITE("Genesis") {
    TEST_CASE("coin amount = 1 billion") {
        auto c = makeGenesisCoin();
        CHECK(c.amount() == MiniNumber("1000000000"));
    }
    TEST_CASE("coin coinID = 32 zero bytes") {
        auto c = makeGenesisCoin();
        REQUIRE(c.coinID().bytes().size() == 32);
        for (auto b : c.coinID().bytes()) CHECK(b == 0x00);
    }
    TEST_CASE("coin address = SHA3(MiniString('RETURN TRUE').serialise())") {
        auto c = makeGenesisCoin();
        // Java: Address = SHA3(MiniString.writeDataStream()) = SHA3(2-byte-len + utf8)
        MiniString ms("RETURN TRUE");
        auto msBytes = ms.serialise();
        auto expected = crypto::Hash::sha3_256(msBytes.data(), msBytes.size());
        CHECK(c.address().hash() == expected);
    }
    TEST_CASE("genesis MMR has 1 leaf") {
        auto gc = makeGenesisCoin();
        auto mmr = makeGenesisMMR(gc);
        CHECK(mmr.getLeafCount() == 1);
    }
    TEST_CASE("genesis MMR root non-zero") {
        auto gc = makeGenesisCoin();
        auto mmr = makeGenesisMMR(gc);
        auto root = mmr.getRoot();
        bool allZero = true;
        for (auto b : root.getData().bytes()) if (b) { allZero = false; break; }
        CHECK_FALSE(allZero);
    }
    TEST_CASE("genesis TxPoW blockNumber = 0") {
        auto txp = makeGenesisTxPoW();
        CHECK(txp.header().blockNumber == MiniNumber(int64_t(0)));
    }
    TEST_CASE("genesis TxPoW all superParents zero") {
        auto txp = makeGenesisTxPoW();
        MiniData z(std::vector<uint8_t>(32, 0x00));
        for (int i = 0; i < CASCADE_LEVELS; ++i)
            CHECK(txp.header().superParents[i] == z);
    }
    TEST_CASE("genesis TxPoW mmrRoot matches GenesisMMR") {
        auto gc  = makeGenesisCoin();
        auto mmr = makeGenesisMMR(gc);
        auto txp = makeGenesisTxPoW();
        CHECK(txp.header().mmrRoot == mmr.getRoot().getData());
    }
    TEST_CASE("isGenesisBlock TRUE for genesis") {
        CHECK(isGenesisBlock(makeGenesisTxPoW()));
    }
    TEST_CASE("isGenesisBlock FALSE for non-genesis") {
        auto txp = makeGenesisTxPoW();
        txp.header().blockNumber = MiniNumber(int64_t(1));
        CHECK_FALSE(isGenesisBlock(txp));
    }
    TEST_CASE("genesis deterministic ID") {
        CHECK(makeGenesisTxPoW().computeID() == makeGenesisTxPoW().computeID());
    }
    TEST_CASE("genesis ID non-zero") {
        auto id = makeGenesisTxPoW().computeID();
        bool allZero = true;
        for (auto b : id.bytes()) if (b) { allZero = false; break; }
        CHECK_FALSE(allZero);
    }
    TEST_CASE("genesis coin serialise/deserialise roundtrip") {
        auto c = makeGenesisCoin();
        auto bytes = c.serialise();
        size_t off = 0;
        auto c2 = Coin::deserialise(bytes.data(), off);
        CHECK(c2.amount() == c.amount());
        CHECK(c2.coinID() == c.coinID());
        CHECK(c2.address().hash() == c.address().hash());
    }
}

// ── Pulse tests ───────────────────────────────────────────────────────────

TEST_SUITE("Pulse") {
    TEST_CASE("empty pulse") {
        Pulse p;
        CHECK(p.isEmpty());
        CHECK(p.size() == 0);
        auto bytes = p.serialise();
        REQUIRE(bytes.size() == 1);
        CHECK(bytes[0] == 0);
    }
    TEST_CASE("add and contains") {
        Pulse p;
        MiniData id(std::vector<uint8_t>(32, 0xAB));
        p.addTxPoWID(id);
        CHECK(p.contains(id));
        CHECK(p.size() == 1);
    }
    TEST_CASE("roundtrip 3 IDs") {
        Pulse p;
        for (int i = 0; i < 3; ++i)
            p.addTxPoWID(MiniData(std::vector<uint8_t>(32, static_cast<uint8_t>(i+1))));
        auto bytes = p.serialise();
        size_t off = 0;
        auto p2 = Pulse::deserialise(bytes.data(), off);
        REQUIRE(p2.size() == 3);
        for (int i = 0; i < 3; ++i)
            CHECK(p2.txPoWIDs()[i] == MiniData(std::vector<uint8_t>(32, static_cast<uint8_t>(i+1))));
        CHECK(off == bytes.size());
    }
    TEST_CASE("max 60 IDs enforced") {
        Pulse p;
        for (int i = 0; i < 60; ++i)
            p.addTxPoWID(MiniData(std::vector<uint8_t>(32, static_cast<uint8_t>(i))));
        CHECK_THROWS(p.addTxPoWID(MiniData(std::vector<uint8_t>(32, 0xFF))));
    }
    TEST_CASE("does not contain unknown ID") {
        Pulse p;
        p.addTxPoWID(MiniData(std::vector<uint8_t>(32, 0x01)));
        CHECK_FALSE(p.contains(MiniData(std::vector<uint8_t>(32, 0xFF))));
    }
}

// ── MiniByte tests ────────────────────────────────────────────────────────

TEST_SUITE("MiniByte") {
    TEST_CASE("default value 0") {
        MiniByte mb;
        CHECK(mb.value() == 0);
    }
    TEST_CASE("construct with value") {
        MiniByte mb(42);
        CHECK(mb.value() == 42);
    }
    TEST_CASE("serialise/deserialise roundtrip") {
        MiniByte mb(0xAB);
        auto bytes = mb.serialise();
        REQUIRE(bytes.size() == 1);
        CHECK(bytes[0] == 0xAB);
        size_t off = 0;
        auto mb2 = MiniByte::deserialise(bytes.data(), off);
        CHECK(mb2.value() == 0xAB);
        CHECK(off == 1);
    }
    TEST_CASE("equality") {
        CHECK(MiniByte(5) == MiniByte(5));
        CHECK(MiniByte(5) != MiniByte(6));
    }
}
