#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/mmr/MMRData.hpp"
#include "../../src/mmr/MMREntry.hpp"
#include "../../src/mmr/MMRProof.hpp"
#include "../../src/mmr/MMRSet.hpp"
#include "../../src/crypto/Hash.hpp"

using namespace minima;
using namespace minima::crypto;

static MMRData makeLeaf(const std::string& s) {
    MiniData hash = Hash::sha3_256(std::vector<uint8_t>(s.begin(), s.end()));
    return MMRData(hash, MiniNumber::ZERO, false);
}

TEST_CASE("MMRData: combine deterministic") {
    MMRData a = makeLeaf("alice"), b = makeLeaf("bob");
    CHECK(MMRData::combine(a,b).getData() == MMRData::combine(a,b).getData());
}

TEST_CASE("MMRData: combine not commutative") {
    MMRData a = makeLeaf("L"), b = makeLeaf("R");
    CHECK_FALSE(MMRData::combine(a,b).getData() == MMRData::combine(b,a).getData());
}

TEST_CASE("MMRData: spent flag") {
    MMRData leaf = makeLeaf("coin");
    CHECK_FALSE(leaf.isSpent());
    leaf.setSpent(true);
    CHECK(leaf.isSpent());
}

TEST_CASE("MMRData: isEmpty default") {
    CHECK(MMRData().isEmpty());
}

TEST_CASE("MMRSet: addLeaf single") {
    MMRSet mmr;
    auto e = mmr.addLeaf(makeLeaf("A"));
    CHECK(e.getRow() == 0);
    CHECK(e.getEntry() == 0);
    CHECK(mmr.getLeafCount() == 1);
}

TEST_CASE("MMRSet: two leaves create parent") {
    MMRSet mmr;
    mmr.addLeaf(makeLeaf("A"));
    mmr.addLeaf(makeLeaf("B"));
    CHECK(mmr.getLeafCount() == 2);
    auto parent = mmr.getEntry(1, 0);
    REQUIRE(parent.has_value());
    CHECK_FALSE(parent->getData().isEmpty());
}

TEST_CASE("MMRSet: parent matches manual combine") {
    MMRSet mmr;
    MMRData a = makeLeaf("A"), b = makeLeaf("B");
    mmr.addLeaf(a); mmr.addLeaf(b);
    MMRData expected = MMRData::combine(a, b);
    auto parent = mmr.getEntry(1, 0);
    REQUIRE(parent.has_value());
    CHECK(parent->getData().getData() == expected.getData());
}

TEST_CASE("MMRSet: four leaves tree structure") {
    MMRSet mmr;
    for (int i = 0; i < 4; ++i) mmr.addLeaf(makeLeaf(std::to_string(i)));
    CHECK(mmr.getLeafCount() == 4);
    auto root = mmr.getEntry(2, 0);
    CHECK(root.has_value());
}

TEST_CASE("MMRSet: root single leaf") {
    MMRSet mmr;
    MMRData leaf = makeLeaf("solo");
    mmr.addLeaf(leaf);
    MMRData root = mmr.getRoot();
    CHECK(root.getData() == leaf.getData());
}

TEST_CASE("MMRSet: root two leaves") {
    MMRSet mmr;
    MMRData a = makeLeaf("A"), b = makeLeaf("B");
    mmr.addLeaf(a); mmr.addLeaf(b);
    CHECK(mmr.getRoot().getData() == MMRData::combine(a,b).getData());
}

TEST_CASE("MMRSet: root deterministic") {
    MMRSet m1, m2;
    for (int i = 0; i < 5; ++i) {
        auto lf = makeLeaf(std::to_string(i));
        m1.addLeaf(lf); m2.addLeaf(lf);
    }
    CHECK(m1.getRoot().getData() == m2.getRoot().getData());
}

TEST_CASE("MMRSet: different input → different root") {
    MMRSet m1, m2;
    m1.addLeaf(makeLeaf("A")); m2.addLeaf(makeLeaf("B"));
    CHECK_FALSE(m1.getRoot().getData() == m2.getRoot().getData());
}

TEST_CASE("MMRProof: two-leaf proofs verify") {
    MMRSet mmr;
    mmr.addLeaf(makeLeaf("A")); mmr.addLeaf(makeLeaf("B"));
    MMRData root = mmr.getRoot();
    CHECK(mmr.getProof(0).verifyProof(root));
    CHECK(mmr.getProof(1).verifyProof(root));
}

TEST_CASE("MMRProof: four-leaf all verify") {
    MMRSet mmr;
    for (int i = 0; i < 4; ++i) mmr.addLeaf(makeLeaf(std::to_string(i)));
    MMRData root = mmr.getRoot();
    for (uint64_t i = 0; i < 4; ++i)
        CHECK_MESSAGE(mmr.getProof(i).verifyProof(root), "leaf", i);
}

TEST_CASE("MMRProof: eight-leaf all verify") {
    MMRSet mmr;
    for (int i = 0; i < 8; ++i) mmr.addLeaf(makeLeaf("leaf" + std::to_string(i)));
    MMRData root = mmr.getRoot();
    for (uint64_t i = 0; i < 8; ++i)
        CHECK_MESSAGE(mmr.getProof(i).verifyProof(root), "leaf", i);
}

TEST_CASE("MMRProof: tampered root fails") {
    MMRSet mmr;
    mmr.addLeaf(makeLeaf("A")); mmr.addLeaf(makeLeaf("B"));
    CHECK_FALSE(mmr.getProof(0).verifyProof(makeLeaf("wrong")));
}

TEST_CASE("MMRSet: updateLeaf with different data changes root") {
    MMRSet mmr;
    mmr.addLeaf(makeLeaf("A")); mmr.addLeaf(makeLeaf("B"));
    MMRData root1 = mmr.getRoot();
    // Update with genuinely different data (different hash)
    mmr.updateLeaf(0, makeLeaf("A_modified"));
    CHECK_FALSE(root1.getData() == mmr.getRoot().getData());
}

TEST_CASE("MMRSet: proof after updateLeaf verifies") {
    MMRSet mmr;
    mmr.addLeaf(makeLeaf("A")); mmr.addLeaf(makeLeaf("B"));
    // In Minima, a spent coin gets a new data hash (the spending tx hash)
    MMRData spentCoin = makeLeaf("A_spent_by_tx123");
    spentCoin.setSpent(true);
    mmr.updateLeaf(0, spentCoin);
    MMRData newRoot = mmr.getRoot();
    MMRProof proof = mmr.getProof(0);
    CHECK(proof.verifyProof(newRoot));
    CHECK(proof.getData().isSpent());
}

TEST_CASE("MMRSet: 3 leaves odd peaks") {
    MMRSet mmr;
    for (int i = 0; i < 3; ++i) mmr.addLeaf(makeLeaf(std::to_string(i)));
    CHECK_FALSE(mmr.getRoot().isEmpty());
}

TEST_CASE("MMRSet: 5 leaves lower 4 proofs verify") {
    MMRSet mmr;
    for (int i = 0; i < 5; ++i) mmr.addLeaf(makeLeaf(std::to_string(i)));
    MMRData root = mmr.getRoot();
    for (uint64_t i = 0; i < 4; ++i)
        CHECK_MESSAGE(mmr.getProof(i).verifyProof(root), "leaf", i);
}
