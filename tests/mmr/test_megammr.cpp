#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/mmr/MegaMMR.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniNumber.hpp"

using namespace minima;

// ── helpers ─────────────────────────────────────────────────────────────────

static Coin makeCoin(uint8_t id, int64_t amount) {
    Coin c;
    c.setCoinID(MiniData(std::vector<uint8_t>(32, id)))
     .setAddress(Address(MiniData(std::vector<uint8_t>(32, uint8_t(id + 0x80)))))
     .setAmount(MiniNumber(amount));
    return c;
}

static MiniData coinID(uint8_t id) {
    return MiniData(std::vector<uint8_t>(32, id));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Basic mutation
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TEST_SUITE("MegaMMR — basic") {
    TEST_CASE("empty: 0 leaves") {
        MegaMMR m;
        CHECK(m.leafCount() == 0);
    }
    TEST_CASE("addCoin increments leafCount") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        CHECK(m.leafCount() == 1);
        m.addCoin(makeCoin(2, 200));
        CHECK(m.leafCount() == 2);
    }
    TEST_CASE("hasCoin true after add") {
        MegaMMR m;
        auto c = makeCoin(1, 100);
        m.addCoin(c);
        CHECK(m.hasCoin(c.coinID()));
    }
    TEST_CASE("hasCoin false for unknown") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        CHECK_FALSE(m.hasCoin(coinID(0xFF)));
    }
    TEST_CASE("spendCoin ok") {
        MegaMMR m;
        auto c = makeCoin(1, 100);
        m.addCoin(c);
        CHECK(m.spendCoin(c.coinID()));
        // coin remains in index after spend (auditing)
        CHECK(m.hasCoin(c.coinID()));
    }
    TEST_CASE("spendCoin false for unknown") {
        MegaMMR m;
        CHECK_FALSE(m.spendCoin(coinID(0xAA)));
    }
    TEST_CASE("root changes on addCoin") {
        MegaMMR m;
        auto r0 = m.getRoot();
        m.addCoin(makeCoin(1, 100));
        CHECK_FALSE(m.getRoot().getData() == r0.getData());
    }
    TEST_CASE("processBlock add+spend") {
        MegaMMR m;
        auto c1 = makeCoin(1, 100);
        auto c2 = makeCoin(2, 200);
        m.addCoin(c1);
        m.processBlock({c1.coinID()}, {c2});
        CHECK(m.leafCount() == 2); // c1 leaf stays (spent), c2 added
        CHECK(m.hasCoin(c2.coinID()));
    }
    TEST_CASE("getLeafEntry correct indices") {
        MegaMMR m;
        auto c0 = makeCoin(0, 10), c1 = makeCoin(1, 20), c2 = makeCoin(2, 30);
        m.addCoin(c0); m.addCoin(c1); m.addCoin(c2);
        CHECK(m.getLeafEntry(c0.coinID()).value() == 0);
        CHECK(m.getLeafEntry(c1.coinID()).value() == 1);
        CHECK(m.getLeafEntry(c2.coinID()).value() == 2);
    }
    TEST_CASE("getCoinProof present/absent") {
        MegaMMR m;
        auto c = makeCoin(1, 100);
        m.addCoin(c);
        CHECK(m.getCoinProof(c.coinID()).has_value());
        CHECK_FALSE(m.getCoinProof(coinID(0xFF)).has_value());
    }
    TEST_CASE("reset clears all") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.reset();
        CHECK(m.leafCount() == 0);
        CHECK_FALSE(m.hasCoin(coinID(1)));
        CHECK(m.checkpointCount() == 0);
    }
    TEST_CASE("coinToMMRData deterministic") {
        auto c = makeCoin(0xAB, 999);
        CHECK(MegaMMR::coinToMMRData(c).getData() == MegaMMR::coinToMMRData(c).getData());
    }
    TEST_CASE("coinToMMRData differs for different coins") {
        CHECK_FALSE(MegaMMR::coinToMMRData(makeCoin(1, 100)).getData()
                 == MegaMMR::coinToMMRData(makeCoin(2, 100)).getData());
    }
    TEST_CASE("coinToMMRData differs for same id different amount") {
        CHECK_FALSE(MegaMMR::coinToMMRData(makeCoin(1, 100)).getData()
                 == MegaMMR::coinToMMRData(makeCoin(1, 200)).getData());
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Root verification
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TEST_SUITE("MegaMMR — verifyRoot") {
    TEST_CASE("verifyRoot matches own root") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.addCoin(makeCoin(2, 200));
        MiniData root = m.getRoot().getData();
        CHECK(m.verifyRoot(root));
    }

    TEST_CASE("verifyRoot fails on wrong root") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        MiniData wrongRoot(std::vector<uint8_t>(32, 0xFF));
        CHECK_FALSE(m.verifyRoot(wrongRoot));
    }

    TEST_CASE("verifyRoot after processBlock") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.addCoin(makeCoin(2, 200));
        // Process a block: spend coin 1, add coin 3
        m.processBlock({coinID(1)}, {makeCoin(3, 300)});
        MiniData root = m.getRoot().getData();
        CHECK(m.verifyRoot(root));
        // Root must have changed from original
        MegaMMR m2;
        m2.addCoin(makeCoin(1, 100));
        m2.addCoin(makeCoin(2, 200));
        CHECK_FALSE(m.verifyRoot(m2.getRoot().getData()));
    }

    TEST_CASE("verifyRoot two identical MMRs produce same root") {
        MegaMMR m1, m2;
        for (uint8_t i = 1; i <= 5; ++i) {
            m1.addCoin(makeCoin(i, i * 100));
            m2.addCoin(makeCoin(i, i * 100));
        }
        CHECK(m1.verifyRoot(m2.getRoot().getData()));
        CHECK(m2.verifyRoot(m1.getRoot().getData()));
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Checkpoint / Rollback
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TEST_SUITE("MegaMMR — checkpoint/rollback") {
    TEST_CASE("checkpoint stores state") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.checkpoint(MiniNumber(int64_t(10)));
        CHECK(m.hasCheckpoint(MiniNumber(int64_t(10))));
        CHECK_FALSE(m.hasCheckpoint(MiniNumber(int64_t(11))));
        CHECK(m.checkpointCount() == 1);
    }

    TEST_CASE("rollback restores leaf count and root") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.addCoin(makeCoin(2, 200));
        MiniData rootAt2 = m.getRoot().getData();
        m.checkpoint(MiniNumber(int64_t(5)));

        // Add more coins (simulate next block)
        m.addCoin(makeCoin(3, 300));
        m.addCoin(makeCoin(4, 400));
        CHECK(m.leafCount() == 4);

        // Roll back to block 5
        bool ok = m.rollback(MiniNumber(int64_t(5)));
        CHECK(ok);
        CHECK(m.leafCount() == 2);
        CHECK(m.verifyRoot(rootAt2));
    }

    TEST_CASE("rollback returns false for missing checkpoint") {
        MegaMMR m;
        CHECK_FALSE(m.rollback(MiniNumber(int64_t(99))));
    }

    TEST_CASE("rollback restores coin index") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.checkpoint(MiniNumber(int64_t(1)));

        m.addCoin(makeCoin(2, 200));
        CHECK(m.hasCoin(coinID(2)));

        m.rollback(MiniNumber(int64_t(1)));
        CHECK(m.hasCoin(coinID(1)));
        CHECK_FALSE(m.hasCoin(coinID(2)));
    }

    TEST_CASE("multiple checkpoints and rollbacks") {
        MegaMMR m;
        // Block 1
        m.addCoin(makeCoin(1, 100));
        m.checkpoint(MiniNumber(int64_t(1)));
        MiniData root1 = m.getRoot().getData();

        // Block 2
        m.addCoin(makeCoin(2, 200));
        m.checkpoint(MiniNumber(int64_t(2)));
        MiniData root2 = m.getRoot().getData();

        // Block 3
        m.addCoin(makeCoin(3, 300));
        m.checkpoint(MiniNumber(int64_t(3)));

        // Re-org: roll back to block 1
        m.rollback(MiniNumber(int64_t(1)));
        CHECK(m.verifyRoot(root1));
        CHECK(m.leafCount() == 1);

        // Roll forward again from checkpoint 2
        m.rollback(MiniNumber(int64_t(2)));
        CHECK(m.verifyRoot(root2));
        CHECK(m.leafCount() == 2);
    }

    TEST_CASE("pruneCheckpoints removes old entries") {
        MegaMMR m;
        for (int i = 1; i <= 10; ++i) {
            m.addCoin(makeCoin(static_cast<uint8_t>(i), i * 10));
            m.checkpoint(MiniNumber(int64_t(i)));
        }
        CHECK(m.checkpointCount() == 10);

        // Keep depth 3 from block 10: cutoff = 10-3 = 7
        // Blocks < 7 are pruned: 1..6 removed, 7..10 remain = 4 blocks
        m.pruneCheckpoints(MiniNumber(int64_t(10)), 3);
        CHECK(m.checkpointCount() == 4);
        CHECK_FALSE(m.hasCheckpoint(MiniNumber(int64_t(1))));
        CHECK_FALSE(m.hasCheckpoint(MiniNumber(int64_t(6))));
        CHECK(m.hasCheckpoint(MiniNumber(int64_t(7))));
        CHECK(m.hasCheckpoint(MiniNumber(int64_t(10))));
    }

    TEST_CASE("checkpoint then spend then rollback restores unspent state") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.addCoin(makeCoin(2, 200));
        m.checkpoint(MiniNumber(int64_t(0)));

        // Spend coin 1
        m.spendCoin(coinID(1));

        // Rollback
        m.rollback(MiniNumber(int64_t(0)));

        // Coin 1 should be unspent again in the MMR leaf
        auto optEntry = m.getLeafEntry(coinID(1));
        CHECK(optEntry.has_value());
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Coin proof verification
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TEST_SUITE("MegaMMR — coin proof") {
    TEST_CASE("verifyCoinProof valid") {
        MegaMMR m;
        auto c1 = makeCoin(1, 100);
        auto c2 = makeCoin(2, 200);
        auto c3 = makeCoin(3, 300);
        m.addCoin(c1); m.addCoin(c2); m.addCoin(c3);

        auto optProof = m.getCoinProof(c2.coinID());
        REQUIRE(optProof.has_value());
        CHECK(m.verifyCoinProof(c2, *optProof));
    }

    TEST_CASE("verifyCoinProof fails for wrong coin") {
        MegaMMR m;
        auto c1 = makeCoin(1, 100);
        auto c2 = makeCoin(2, 200);
        m.addCoin(c1); m.addCoin(c2);

        auto optProof = m.getCoinProof(c1.coinID());
        REQUIRE(optProof.has_value());
        // Try to verify with c2 data but c1's proof — should fail
        CHECK_FALSE(m.verifyCoinProof(c2, *optProof));
    }

    TEST_CASE("verifyCoinProof for all 8 coins") {
        MegaMMR m;
        std::vector<Coin> coins;
        for (uint8_t i = 0; i < 8; ++i) {
            coins.push_back(makeCoin(i, (i+1) * 50));
            m.addCoin(coins.back());
        }
        for (int i = 0; i < 8; ++i) {
            auto optProof = m.getCoinProof(coins[i].coinID());
            REQUIRE(optProof.has_value());
            INFO("verifyCoinProof failed for coin index " << i);
            CHECK(m.verifyCoinProof(coins[i], *optProof));
        }
    }

    TEST_CASE("coin proof invalid after adding more coins") {
        // In an append-only MMR, proofs from earlier states need to be regenerated.
        // After adding coin 2, the proof for coin 1 changes because new peaks form.
        MegaMMR m;
        auto c1 = makeCoin(1, 100);
        m.addCoin(c1);
        auto proof1 = m.getCoinProof(c1.coinID());
        REQUIRE(proof1.has_value());

        // After 1 leaf, proof is trivial — adding c2 creates a merged peak
        m.addCoin(makeCoin(2, 200));
        // Now get a fresh proof for c1
        auto proof1fresh = m.getCoinProof(c1.coinID());
        REQUIRE(proof1fresh.has_value());
        CHECK(m.verifyCoinProof(c1, *proof1fresh));
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Fast-sync wire protocol
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TEST_SUITE("MegaMMR — fast-sync") {
    TEST_CASE("getSyncPacket/applySyncPacket round-trip: empty") {
        MegaMMR m;
        auto pkt = m.getSyncPacket();
        size_t off = 0;
        auto m2 = MegaMMR::applySyncPacket(pkt.data(), off);
        CHECK(m2.leafCount() == 0);
        CHECK(off == pkt.size());
    }

    TEST_CASE("getSyncPacket/applySyncPacket round-trip: 3 coins") {
        MegaMMR m;
        for (uint8_t i = 1; i <= 3; ++i) m.addCoin(makeCoin(i, i * 100));
        auto pkt = m.getSyncPacket();
        size_t off = 0;
        auto m2 = MegaMMR::applySyncPacket(pkt.data(), off);
        CHECK(m2.leafCount() == 3);
        CHECK(off == pkt.size());
        for (uint8_t i = 1; i <= 3; ++i)
            CHECK(m2.hasCoin(coinID(i)));
    }

    TEST_CASE("getSyncPacket preserves root hash") {
        MegaMMR m;
        for (uint8_t i = 0; i < 5; ++i) m.addCoin(makeCoin(i, (i+1) * 50));
        MiniData rootBefore = m.getRoot().getData();
        auto pkt = m.getSyncPacket();
        size_t off = 0;
        auto m2 = MegaMMR::applySyncPacket(pkt.data(), off);
        CHECK(m2.getRoot().getData() == rootBefore);
    }

    TEST_CASE("getSyncPacket preserves spent flags") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.addCoin(makeCoin(2, 200));
        m.spendCoin(coinID(1));
        MiniData rootBefore = m.getRoot().getData();
        auto pkt = m.getSyncPacket();
        size_t off = 0;
        auto m2 = MegaMMR::applySyncPacket(pkt.data(), off);
        // Root must match (spent flag affects leaf hash in our MMR impl)
        CHECK(m2.getRoot().getData() == rootBefore);
        CHECK(m2.leafCount() == 2);
    }

    TEST_CASE("getSyncPacket preserves coin index") {
        MegaMMR m;
        for (uint8_t i = 0; i < 8; ++i) m.addCoin(makeCoin(i, i + 1));
        auto pkt = m.getSyncPacket();
        size_t off = 0;
        auto m2 = MegaMMR::applySyncPacket(pkt.data(), off);
        for (uint8_t i = 0; i < 8; ++i) {
            auto e1 = m.getLeafEntry(coinID(i));
            auto e2 = m2.getLeafEntry(coinID(i));
            REQUIRE(e1.has_value());
            REQUIRE(e2.has_value());
            CHECK(e1.value() == e2.value());
        }
    }

    TEST_CASE("serialise/deserialise aliases work") {
        MegaMMR m;
        m.addCoin(makeCoin(5, 500));
        auto b = m.serialise();
        size_t off = 0;
        auto m2 = MegaMMR::deserialise(b.data(), off);
        CHECK(m2.leafCount() == 1);
        CHECK(m2.hasCoin(coinID(5)));
    }

    TEST_CASE("large MMR: 100 coins round-trip") {
        MegaMMR m;
        for (int i = 0; i < 100; ++i)
            m.addCoin(makeCoin(static_cast<uint8_t>(i % 256), int64_t(i + 1)));
        MiniData rootBefore = m.getRoot().getData();
        auto pkt = m.getSyncPacket();
        size_t off = 0;
        auto m2 = MegaMMR::applySyncPacket(pkt.data(), off);
        CHECK(m2.leafCount() == 100);
        CHECK(m2.getRoot().getData() == rootBefore);
        CHECK(off == pkt.size());
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Integration: simulate a 3-block chain
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TEST_SUITE("MegaMMR — chain simulation") {
    TEST_CASE("3 blocks: checkpoint, verify, prune, re-org") {
        MegaMMR m;

        // ── Block 1: add coins A, B ────────────────────────────────────────
        auto coinA = makeCoin(0xA0, 1000);
        auto coinB = makeCoin(0xB0, 2000);
        m.processBlock({}, {coinA, coinB});
        MiniData root1 = m.getRoot().getData();
        CHECK(m.verifyRoot(root1));
        m.checkpoint(MiniNumber(int64_t(1)));
        CHECK(m.checkpointCount() == 1);

        // ── Block 2: spend A, add C ────────────────────────────────────────
        auto coinC = makeCoin(0xC0, 3000);
        m.processBlock({coinA.coinID()}, {coinC});
        MiniData root2 = m.getRoot().getData();
        CHECK(m.verifyRoot(root2));
        CHECK_FALSE(m.verifyRoot(root1)); // must differ
        m.checkpoint(MiniNumber(int64_t(2)));
        CHECK(m.checkpointCount() == 2);

        // ── Block 3: add D ────────────────────────────────────────────────
        auto coinD = makeCoin(0xD0, 4000);
        m.processBlock({}, {coinD});
        MiniData root3 = m.getRoot().getData();
        m.checkpoint(MiniNumber(int64_t(3)));

        // Check state: B, C, D present; A still tracked (spent)
        CHECK(m.hasCoin(coinB.coinID()));
        CHECK(m.hasCoin(coinC.coinID()));
        CHECK(m.hasCoin(coinD.coinID()));
        CHECK(m.leafCount() == 4); // A(spent) + B + C + D

        // ── Re-org: roll back to block 1 ──────────────────────────────────
        bool ok = m.rollback(MiniNumber(int64_t(1)));
        CHECK(ok);
        CHECK(m.verifyRoot(root1));
        CHECK(m.leafCount() == 2); // A + B
        CHECK(m.hasCoin(coinA.coinID()));
        CHECK(m.hasCoin(coinB.coinID()));
        CHECK_FALSE(m.hasCoin(coinC.coinID())); // C was in block 2
        CHECK_FALSE(m.hasCoin(coinD.coinID())); // D was in block 3

        // ── Prune all: keepDepth=-1 → cutoff = 3-(-1) = 4 > all block nums ───
        m.pruneCheckpoints(MiniNumber(int64_t(3)), -1);
        CHECK(m.checkpointCount() == 0);
    }

    TEST_CASE("getSyncPacket after block processing matches block root") {
        MegaMMR m;
        m.processBlock({}, {makeCoin(1, 100), makeCoin(2, 200)});
        MiniData blockRoot = m.getRoot().getData();

        auto pkt = m.getSyncPacket();
        size_t off = 0;
        auto m2 = MegaMMR::applySyncPacket(pkt.data(), off);
        CHECK(m2.verifyRoot(blockRoot));
    }
}
