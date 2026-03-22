#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/objects/Token.hpp"
#include "../../src/objects/Greeting.hpp"
#include "../../src/objects/TxBlock.hpp"

using namespace minima;

// ── Token ─────────────────────────────────────────────────────────────────────

TEST_CASE("Token: basic construction and tokenID computed") {
    Token tok(
        MiniData(std::vector<uint8_t>(32, 0x01)),
        MiniNumber(int64_t(2)),
        MiniNumber(int64_t(1000000)),
        MiniString("TestToken"),
        MiniString("RETURN TRUE")
    );
    CHECK(tok.tokenID().size() == 32);
    // tokenID must not be all zeros
    CHECK(tok.tokenID().bytes() != std::vector<uint8_t>(32, 0x00));
}

TEST_CASE("Token: totalTokens = amount * 10^scale") {
    Token tok(
        MiniData(std::vector<uint8_t>(32, 0x01)),
        MiniNumber(int64_t(3)),
        MiniNumber(int64_t(100)),
        MiniString("T"),
        MiniString("RETURN TRUE")
    );
    CHECK(tok.totalTokens() == MiniNumber(int64_t(100000)));
}

TEST_CASE("Token: scale=0 → totalTokens == minimaAmount") {
    Token tok(
        MiniData(std::vector<uint8_t>(32, 0x02)),
        MiniNumber(int64_t(0)),
        MiniNumber(int64_t(777)),
        MiniString("NoScale"),
        MiniString("RETURN TRUE")
    );
    CHECK(tok.totalTokens() == MiniNumber(int64_t(777)));
}

TEST_CASE("Token: serialise / deserialise roundtrip") {
    Token orig(
        MiniData(std::vector<uint8_t>(32, 0xAB)),
        MiniNumber(int64_t(1)),
        MiniNumber(int64_t(500)),
        MiniString("RoundtripToken"),
        MiniString("RETURN SIGNEDBY(0xFF)"),
        MiniNumber(int64_t(42))
    );
    auto bytes = orig.serialise();
    size_t offset = 0;
    Token restored = Token::deserialise(bytes.data(), offset);

    CHECK(orig.tokenID().bytes()   == restored.tokenID().bytes());
    CHECK(orig.scale()             == restored.scale());
    CHECK(orig.minimaAmount()      == restored.minimaAmount());
    CHECK(orig.createdBlock()      == restored.createdBlock());
    CHECK(orig.name().str()   == restored.name().str());
    CHECK(orig.script().str() == restored.script().str());
}

TEST_CASE("Token: deterministic tokenID - same params → same ID") {
    auto make = []() {
        return Token(
            MiniData(std::vector<uint8_t>(32, 0x07)),
            MiniNumber(int64_t(0)),
            MiniNumber(int64_t(1000)),
            MiniString("Identical"),
            MiniString("RETURN TRUE")
        );
    };
    CHECK(make().tokenID().bytes() == make().tokenID().bytes());
}

TEST_CASE("Token: different params → different tokenID") {
    Token t1(
        MiniData(std::vector<uint8_t>(32, 0x01)),
        MiniNumber(int64_t(1)), MiniNumber(int64_t(100)),
        MiniString("A"), MiniString("RETURN TRUE")
    );
    Token t2(
        MiniData(std::vector<uint8_t>(32, 0x02)),
        MiniNumber(int64_t(1)), MiniNumber(int64_t(100)),
        MiniString("A"), MiniString("RETURN TRUE")
    );
    CHECK(t1.tokenID().bytes() != t2.tokenID().bytes());
}

TEST_CASE("Token: TOKENID_MINIMA is 0x00") {
    CHECK(Token::TOKENID_MINIMA().bytes() == std::vector<uint8_t>{0x00});
}

TEST_CASE("Token: TOKENID_CREATE is 0xFF") {
    CHECK(Token::TOKENID_CREATE().bytes() == std::vector<uint8_t>{0xFF});
}

// ── Greeting ──────────────────────────────────────────────────────────────────

TEST_CASE("Greeting: default is fresh install") {
    Greeting g;
    CHECK(g.isFreshInstall());
}

TEST_CASE("Greeting: setTopBlock clears fresh install") {
    Greeting g;
    g.setTopBlock(MiniNumber(int64_t(100)));
    CHECK_FALSE(g.isFreshInstall());
}

TEST_CASE("Greeting: serialise / deserialise") {
    Greeting orig;
    orig.setTopBlock(MiniNumber(int64_t(100)));
    orig.addChainID(MiniData(std::vector<uint8_t>(32, 0xAA)));
    orig.addChainID(MiniData(std::vector<uint8_t>(32, 0xBB)));

    auto bytes = orig.serialise();
    size_t off = 0;
    Greeting res = Greeting::deserialise(bytes.data(), off);

    CHECK(res.topBlock()      == MiniNumber(int64_t(100)));
    CHECK(res.chain().size()  == 2);
    CHECK(res.isFreshInstall() == false);
}

TEST_CASE("Greeting: version string is set") {
    Greeting g;
    CHECK(g.version().str() == std::string(minima::MINIMA_VERSION));
}

// ── TxBlock ───────────────────────────────────────────────────────────────────

TEST_CASE("TxBlock: create from TxPoW") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(7));
    TxBlock tb(txp);
    CHECK(tb.txpow().header().blockNumber == MiniNumber(int64_t(7)));
    CHECK(tb.newCoins().empty());
    CHECK(tb.spentCoins().empty());
    CHECK(tb.previousPeaks().empty());
}

TEST_CASE("TxBlock: serialise / deserialise basic") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(5));
    TxBlock orig(txp);

    auto bytes = orig.serialise();
    size_t off = 0;
    TxBlock res = TxBlock::deserialise(bytes.data(), off, bytes.size());
    CHECK(res.txpow().header().blockNumber == MiniNumber(int64_t(5)));
    CHECK(res.newCoins().empty());
}
