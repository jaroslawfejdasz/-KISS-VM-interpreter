#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/chain/cascade/Cascade.hpp"
#include "../../src/chain/cascade/CascadeNode.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/mining/TxPoWMiner.hpp"

using namespace minima;
using namespace minima::cascade;
using namespace minima::mining;

// Helper: build a trivially-mined TxPoW at given height
static TxPoW makeMined(int64_t height) {
    TxPoW txp;
    txp.header().blockNumber     = MiniNumber(height);
    txp.header().blockDifficulty = makeDifficulty(0);
    txp.body().txnDifficulty     = makeDifficulty(0);
    mineBlock(txp, 1);
    return txp;
}

// ── CascadeNode ───────────────────────────────────────────────────────────

TEST_CASE("CascadeNode: construct from TxPoW") {
    auto txp = makeMined(42);
    CascadeNode node(txp);
    CHECK(node.blockNumber() == 42);
    CHECK(node.level()       == 0);
    CHECK(node.parent()      == nullptr);
}

TEST_CASE("CascadeNode: setLevel / getLevel") {
    auto txp = makeMined(1);
    CascadeNode node(txp);
    node.setLevel(5);
    CHECK(node.level() == 5);
}

TEST_CASE("CascadeNode: weight increases with level") {
    auto txp = makeMined(1);
    CascadeNode a(txp), b(txp);
    a.setLevel(0);
    b.setLevel(3);
    CHECK(b.weight() > a.weight());
    // 2^3 = 8x
    CHECK(b.weight() == doctest::Approx(a.weight() * 8.0).epsilon(0.001));
}

TEST_CASE("CascadeNode: serialise → deserialise roundtrip") {
    auto txp = makeMined(99);
    CascadeNode orig(txp);
    orig.setLevel(4);

    auto bytes = orig.serialise();
    size_t offset = 0;
    auto copy = CascadeNode::deserialise(bytes, offset);

    CHECK(copy.blockNumber() == 99);
    CHECK(copy.level()       == 4);
    CHECK(offset             == bytes.size());
}

TEST_CASE("CascadeNode: toString non-empty") {
    auto txp = makeMined(7);
    CascadeNode node(txp);
    CHECK(node.toString().size() > 0);
}

// ── Cascade ───────────────────────────────────────────────────────────────

TEST_CASE("Cascade: empty on construction") {
    Cascade c;
    CHECK(c.empty());
    CHECK(c.length() == 0);
    CHECK(c.tip()    == nullptr);
    CHECK(c.root()   == nullptr);
}

TEST_CASE("Cascade: addToTip — single block") {
    Cascade c;
    c.addToTip(makeMined(1));

    CHECK(c.length()   == 1);
    CHECK(c.tipBlock() == 1);
    CHECK(c.tip()->parent() == nullptr);
}

TEST_CASE("Cascade: addToTip — multiple blocks, tip is newest") {
    Cascade c;
    for (int64_t i = 1; i <= 5; i++) c.addToTip(makeMined(i));

    CHECK(c.length()    == 5);
    CHECK(c.tipBlock()  == 5);
    CHECK(c.rootBlock() == 1);
}

TEST_CASE("Cascade: parent links are correct") {
    Cascade c;
    c.addToTip(makeMined(10));
    c.addToTip(makeMined(11));
    c.addToTip(makeMined(12));

    // tip(12) → parent(11) → parent(10) → null
    auto* tip = c.tip();
    REQUIRE(tip != nullptr);
    CHECK(tip->blockNumber() == 12);
    REQUIRE(tip->parent() != nullptr);
    CHECK(tip->parent()->blockNumber() == 11);
    REQUIRE(tip->parent()->parent() != nullptr);
    CHECK(tip->parent()->parent()->blockNumber() == 10);
    CHECK(tip->parent()->parent()->parent() == nullptr);
}

TEST_CASE("Cascade: cascadeChain on small chain doesn't crash") {
    Cascade c;
    for (int64_t i = 1; i <= 10; i++) c.addToTip(makeMined(i));
    REQUIRE_NOTHROW(c.cascadeChain());
    CHECK(c.length() <= 10);
    CHECK(c.tipBlock() == 10);
}

TEST_CASE("Cascade: cascadeChain — tip always at level 0") {
    Cascade c;
    for (int64_t i = 1; i <= 20; i++) c.addToTip(makeMined(i));
    c.cascadeChain();

    REQUIRE(c.tip() != nullptr);
    CHECK(c.tip()->level() == 0);
}

TEST_CASE("Cascade: cascadeChain — valid structure") {
    Cascade c;
    for (int64_t i = 1; i <= 50; i++) c.addToTip(makeMined(i));
    c.cascadeChain();

    std::string err;
    bool valid = c.isValid(&err);
    INFO("Validation error: " << err);
    CHECK(valid);
}

TEST_CASE("Cascade: totalWeight positive after cascade") {
    Cascade c;
    for (int64_t i = 1; i <= 10; i++) c.addToTip(makeMined(i));
    c.cascadeChain();
    CHECK(c.totalWeight() > 0.0);
}

TEST_CASE("Cascade: totalWeight increases as chain grows") {
    Cascade c1, c2;
    for (int64_t i = 1; i <= 5;  i++) c1.addToTip(makeMined(i));
    for (int64_t i = 1; i <= 10; i++) c2.addToTip(makeMined(i));
    c1.cascadeChain();
    c2.cascadeChain();
    CHECK(c2.totalWeight() >= c1.totalWeight());
}

TEST_CASE("Cascade: serialise → deserialise roundtrip") {
    Cascade orig;
    for (int64_t i = 1; i <= 5; i++) orig.addToTip(makeMined(i));
    orig.cascadeChain();

    auto bytes = orig.serialise();
    size_t offset = 0;
    auto copy = Cascade::deserialise(bytes, offset);

    CHECK(copy.length()    == orig.length());
    CHECK(copy.tipBlock()  == orig.tipBlock());
    CHECK(copy.rootBlock() == orig.rootBlock());
    CHECK(offset           == bytes.size());
}

TEST_CASE("Cascade: serialise → deserialise preserves levels") {
    Cascade orig;
    for (int64_t i = 1; i <= 8; i++) orig.addToTip(makeMined(i));
    orig.cascadeChain();

    auto bytes = orig.serialise();
    size_t offset = 0;
    auto copy = Cascade::deserialise(bytes, offset);

    // Walk both and compare levels
    const CascadeNode* a = orig.tip();
    const CascadeNode* b = copy.tip();
    int checked = 0;
    while (a && b) {
        CHECK(a->level()       == b->level());
        CHECK(a->blockNumber() == b->blockNumber());
        a = a->parent();
        b = b->parent();
        checked++;
    }
    CHECK(a == nullptr);
    CHECK(b == nullptr);
    CHECK(checked >= 1);
}

TEST_CASE("Cascade: empty cascade serialise → deserialise") {
    Cascade orig;
    auto bytes = orig.serialise();
    size_t offset = 0;
    auto copy = Cascade::deserialise(bytes, offset);
    CHECK(copy.empty());
    CHECK(offset == bytes.size());
}

TEST_CASE("Cascade: cascadeChain on empty doesn't crash") {
    Cascade c;
    REQUIRE_NOTHROW(c.cascadeChain());
    CHECK(c.empty());
}

TEST_CASE("Cascade: isValid on empty returns true") {
    Cascade c;
    CHECK(c.isValid());
}

TEST_CASE("Cascade: toString non-empty") {
    Cascade c;
    for (int64_t i = 1; i <= 3; i++) c.addToTip(makeMined(i));
    auto s = c.toString();
    CHECK(s.find("Cascade") != std::string::npos);
    CHECK(s.find("tip=3")   != std::string::npos);
}

TEST_CASE("Cascade: 100-block cascade prunes to manageable size") {
    Cascade c;
    for (int64_t i = 1; i <= 100; i++) c.addToTip(makeMined(i));
    c.cascadeChain();

    // With CASCADE_LEVEL_NODES=1, we keep at most CASCADE_LEVELS_MAX=32 nodes
    CHECK(c.length() <= CASCADE_LEVELS_MAX + 1);
    CHECK(c.tipBlock() == 100);
}
