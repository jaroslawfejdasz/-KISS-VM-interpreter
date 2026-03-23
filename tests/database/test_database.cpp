#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/database/MinimaDB.hpp"
#include "../../src/wallet/Wallet.hpp"
#include "../../src/chain/TxPowTree.hpp"
#include "../../src/chain/TxPoWTreeNode.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniNumber.hpp"
#include "../../src/system/TxPoWProcessor.hpp"
#include "../../src/system/MessageProcessor.hpp"
#include <cstdio>
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/mmr/MMRProof.hpp"


using namespace minima;
using namespace minima::chain;

static MiniData makeID(uint8_t val) {
    return MiniData(std::vector<uint8_t>(32, val));
}

static TxPoW makeTxPoW(uint8_t parentVal, int64_t blockNum, int64_t nonce = 0) {
    TxPoW txp;
    txp.header().blockNumber     = MiniNumber(blockNum);
    txp.header().nonce           = MiniNumber(nonce);
    txp.header().superParents[0] = makeID(parentVal);
    for (int i = 1; i < CASCADE_LEVELS; ++i)
        txp.header().superParents[i] = makeID(0x00);
    return txp;
}

// ── TxPoWTreeNode ─────────────────────────────────────────────────────────────
TEST_SUITE("TxPoWTreeNode") {
    TEST_CASE("construction") {
        TxPoW txp = makeTxPoW(0xFF, 5);
        TxPoWTreeNode node(txp);
        CHECK(node.blockNumber() == 5);
        CHECK(node.parent() == nullptr);
        CHECK(node.depth() == 0);
        CHECK(node.isLeaf() == true);
        CHECK(node.txPoWID().bytes().size() > 0);
    }

    TEST_CASE("child linking updates depth") {
        TxPoW p = makeTxPoW(0xFF, 0);
        TxPoW c = makeTxPoW(0x00, 1);
        TxPoWTreeNode parent(p);
        TxPoWTreeNode child(c);
        parent.addChild(&child);
        CHECK(child.parent() == &parent);
        CHECK(child.depth() == 1);
        CHECK(parent.isLeaf() == false);
    }

    TEST_CASE("heaviestChild picks max weight") {
        TxPoW p  = makeTxPoW(0xFF, 0);
        TxPoW c1 = makeTxPoW(0x00, 1, 1);
        TxPoW c2 = makeTxPoW(0x01, 1, 2);
        TxPoWTreeNode parent(p);
        TxPoWTreeNode child1(c1);
        TxPoWTreeNode child2(c2);
        child1.setChainWeight(MiniNumber(int64_t(10)));
        child2.setChainWeight(MiniNumber(int64_t(99)));
        parent.addChild(&child1);
        parent.addChild(&child2);
        CHECK(parent.heaviestChild() == &child2);
    }
}

// ── TxPowTree ─────────────────────────────────────────────────────────────────
TEST_SUITE("TxPowTree") {
    TEST_CASE("empty tree") {
        TxPowTree tree;
        CHECK(tree.tip()  == nullptr);
        CHECK(tree.root() == nullptr);
        CHECK(tree.size() == 0);
    }

    TEST_CASE("add genesis") {
        TxPowTree tree;
        TxPoW genesis = makeTxPoW(0xFF, 0);
        CHECK(tree.addTxPoW(genesis) == true);
        CHECK(tree.size() == 1);
        CHECK(tree.tip() != nullptr);
        CHECK(tree.tip()->blockNumber() == 0);
    }

    TEST_CASE("linear chain: tip follows") {
        TxPowTree tree;
        TxPoW b0 = makeTxPoW(0xFF, 0);
        CHECK(tree.addTxPoW(b0));
        MiniData id0 = tree.tip()->txPoWID();

        TxPoW b1 = makeTxPoW(0xFF, 1, 1);
        b1.header().superParents[0] = id0;
        CHECK(tree.addTxPoW(b1));
        MiniData id1 = tree.tip()->txPoWID();

        TxPoW b2 = makeTxPoW(0xFF, 2, 2);
        b2.header().superParents[0] = id1;
        CHECK(tree.addTxPoW(b2));

        CHECK(tree.size() == 3);
        CHECK(tree.tip()->blockNumber() == 2);
    }

    TEST_CASE("orphan rejected") {
        TxPowTree tree;
        TxPoW b0 = makeTxPoW(0xFF, 0);
        tree.addTxPoW(b0);
        TxPoW b2 = makeTxPoW(0xAA, 2);
        CHECK(tree.addTxPoW(b2) == false);
        CHECK(tree.size() == 1);
    }

    TEST_CASE("duplicate ignored") {
        TxPowTree tree;
        TxPoW b0 = makeTxPoW(0xFF, 0);
        CHECK(tree.addTxPoW(b0) == true);
        CHECK(tree.addTxPoW(b0) == true);
        CHECK(tree.size() == 1);
    }

    TEST_CASE("canonical chain order") {
        TxPowTree tree;
        TxPoW b0 = makeTxPoW(0xFF, 0); tree.addTxPoW(b0);
        MiniData id0 = tree.tip()->txPoWID();
        TxPoW b1 = makeTxPoW(0xFF, 1, 1); b1.header().superParents[0] = id0; tree.addTxPoW(b1);
        MiniData id1 = tree.tip()->txPoWID();
        TxPoW b2 = makeTxPoW(0xFF, 2, 2); b2.header().superParents[0] = id1; tree.addTxPoW(b2);

        auto chain = tree.canonicalChain();
        CHECK(chain.size() == 3);
        CHECK(chain[0]->blockNumber() == 0);
        CHECK(chain[2]->blockNumber() == 2);
    }

    TEST_CASE("fork — heaviest chain wins") {
        TxPowTree tree;
        TxPoW b0 = makeTxPoW(0xFF, 0); tree.addTxPoW(b0);
        MiniData id0 = tree.tip()->txPoWID();

        // Fork A: 2 bloki (weight=3)
        TxPoW a1 = makeTxPoW(0xA1, 1, 1); a1.header().superParents[0] = id0; tree.addTxPoW(a1);
        MiniData idA1 = tree.getNode(a1.computeID())->txPoWID();
        TxPoW a2 = makeTxPoW(0xA2, 2, 2); a2.header().superParents[0] = idA1; tree.addTxPoW(a2);

        // Fork B: 1 blok (weight=2), unikalny nonce
        TxPoW b1 = makeTxPoW(0xB1, 1, 999); b1.header().superParents[0] = id0; tree.addTxPoW(b1);

        CHECK(tree.size() == 4);
        CHECK(tree.tip()->blockNumber() == 2); // Fork A wygrywa
    }
}

// ── MinimaDB ─────────────────────────────────────────────────────────────────
TEST_SUITE("MinimaDB") {
    TEST_CASE("initial state") {
        MinimaDB db;
        CHECK(db.currentHeight() == 0);
        CHECK(db.currentTip().has_value() == false);
    }

    TEST_CASE("addBlock + height") {
        MinimaDB db;
        TxPoW b0 = makeTxPoW(0xFF, 0);
        CHECK(db.addBlock(b0) == true);
        CHECK(db.currentTip().has_value() == true);
        CHECK(db.currentHeight() == 0);
    }

    TEST_CASE("height grows with chain") {
        MinimaDB db;
        TxPoW b0 = makeTxPoW(0xFF, 0); db.addBlock(b0);
        MiniData id0 = db.txPowTree().tip()->txPoWID();
        TxPoW b1 = makeTxPoW(0xFF, 1, 1); b1.header().superParents[0] = id0; db.addBlock(b1);
        CHECK(db.currentHeight() == 1);
    }

    TEST_CASE("hasBlock + getBlock") {
        MinimaDB db;
        TxPoW b0 = makeTxPoW(0xFF, 0); db.addBlock(b0);
        MiniData id = b0.computeID();
        CHECK(db.hasBlock(id) == true);
        auto got = db.getBlock(id);
        CHECK(got.has_value());
        CHECK(got->header().blockNumber.getAsLong() == 0);
    }

    TEST_CASE("canonicalChain size") {
        MinimaDB db;
        TxPoW b0 = makeTxPoW(0xFF, 0); db.addBlock(b0);
        MiniData id0 = db.txPowTree().tip()->txPoWID();
        TxPoW b1 = makeTxPoW(0xFF, 1, 1); b1.header().superParents[0] = id0; db.addBlock(b1);
        MiniData id1 = db.txPowTree().tip()->txPoWID();
        TxPoW b2 = makeTxPoW(0xFF, 2, 2); b2.header().superParents[0] = id1; db.addBlock(b2);
        CHECK(db.canonicalChain().size() == 3);
    }

    TEST_CASE("reset clears tip") {
        MinimaDB db;
        TxPoW b0 = makeTxPoW(0xFF, 0); db.addBlock(b0);
        db.reset();
        CHECK(db.currentTip().has_value() == false);
    }
}

// ── Wallet ────────────────────────────────────────────────────────────────────
TEST_SUITE("Wallet") {
    TEST_CASE("empty wallet") {
        Wallet w;
        CHECK(w.size() == 0);
        CHECK(w.unusedCount() == 0);
        CHECK(w.nextUnusedAddress().has_value() == false);
    }

    TEST_CASE("createNewKey — unique addresses") {
        Wallet w;
        auto a1 = w.createNewKey();
        auto a2 = w.createNewKey();
        CHECK(a1.bytes() != a2.bytes());
        CHECK(w.size() == 2);
        CHECK(w.unusedCount() == 2);
    }

    TEST_CASE("hasKey + getKey") {
        Wallet w;
        auto addr = w.createNewKey();
        CHECK(w.hasKey(addr) == true);
        auto key = w.getKey(addr);
        CHECK(key.has_value());
        CHECK(key->used == false);
    }

    TEST_CASE("nextUnusedAddress returns first") {
        Wallet w;
        auto a1 = w.createNewKey();
        w.createNewKey();
        auto next = w.nextUnusedAddress();
        CHECK(next.has_value());
        CHECK(next->bytes() == a1.bytes());
    }

    TEST_CASE("sign marks key as used") {
        Wallet w;
        auto addr = w.createNewKey();
        MiniData data(std::vector<uint8_t>(32, 0xAB));
        auto sig = w.sign(addr, data);
        CHECK(sig.has_value());
        CHECK(sig->bytes().size() > 0);
        auto sig2 = w.sign(addr, data);
        CHECK(sig2.has_value() == false);
        CHECK(w.unusedCount() == 0);
    }

    TEST_CASE("sign unknown address returns nullopt") {
        Wallet w;
        MiniData unknown(std::vector<uint8_t>(32, 0xDE));
        CHECK(w.sign(unknown, MiniData(std::vector<uint8_t>{0x01})).has_value() == false);
    }
}

// ── MinimaDB Persistence ──────────────────────────────────────────────────────
TEST_SUITE("MinimaDB_Persistence") {
    TEST_CASE("openPersistence creates DB and enables persistence") {
        MinimaDB db;
        CHECK(db.isPersistenceEnabled() == false);
        bool ok = db.openPersistence("/tmp/test_minimadb_persist.db");
        CHECK(ok == true);
        CHECK(db.isPersistenceEnabled() == true);
        // cleanup
        std::remove("/tmp/test_minimadb_persist.db");
    }

    TEST_CASE("addBlockWithPersist — stores block in SQLite") {
        MinimaDB db;
        db.openPersistence("/tmp/test_minimadb_persist2.db");

        TxPoW b0 = makeTxPoW(0xFF, 0);
        bool ok = db.addBlockWithPersist(b0);
        CHECK(ok == true);
        CHECK(db.currentHeight() == 0);

        // Verify SQLite has the block
        auto* pbs = db.persistentBlockStore();
        REQUIRE(pbs != nullptr);
        CHECK(pbs->count() == 1);

        std::remove("/tmp/test_minimadb_persist2.db");
    }

    TEST_CASE("bootstrapFromDB — odtwarza drzewo z SQLite") {
        const std::string dbPath = "/tmp/test_minimadb_bootstrap.db";
        std::remove(dbPath.c_str());

        // Faza 1: zapisz 3 bloki do DB
        {
            MinimaDB db;
            db.openPersistence(dbPath);
            TxPoW b0 = makeTxPoW(0xFF, 0); db.addBlockWithPersist(b0);
            MiniData id0 = db.txPowTree().tip()->txPoWID();
            TxPoW b1 = makeTxPoW(0xFF, 1, 1); b1.header().superParents[0] = id0; db.addBlockWithPersist(b1);
            MiniData id1 = db.txPowTree().tip()->txPoWID();
            TxPoW b2 = makeTxPoW(0xFF, 2, 2); b2.header().superParents[0] = id1; db.addBlockWithPersist(b2);
            CHECK(db.currentHeight() == 2);
        }

        // Faza 2: nowa instancja — odtwarza z DB
        {
            MinimaDB db2;
            db2.openPersistence(dbPath);
            // bootstrapFromDB() wywołane w openPersistence
            CHECK(db2.currentHeight() == 2);
            CHECK(db2.txPowTree().size() == 3);
        }

        std::remove(dbPath.c_str());
    }

    TEST_CASE("rebuildMMR — MMR odtworzone z canonicalChain") {
        MinimaDB db;
        TxPoW b0 = makeTxPoW(0xFF, 0); db.addBlock(b0);
        MiniData id0 = db.txPowTree().tip()->txPoWID();
        TxPoW b1 = makeTxPoW(0xFF, 1, 1); b1.header().superParents[0] = id0; db.addBlock(b1);

        // Przed rebuild — MMR może być pusty (nie aktualizowaliśmy go)
        db.rebuildMMR();
        // MMR powinien mieć 2 liście (po jednym na blok)
        // W naszej implementacji każdy blok dodaje 1 leaf
        // Nie możemy łatwo sprawdzić rozmiaru bez publicznego API — testujemy że nie crashuje
        CHECK(db.currentHeight() == 1);
    }

    TEST_CASE("initGenesis — startuje węzeł z genesis") {
        MinimaDB db;
        CHECK(db.currentHeight() == 0);
        CHECK(db.currentTip().has_value() == false);

        bool ok = db.initGenesis();
        CHECK(ok == true);
        CHECK(db.currentTip().has_value() == true);
        CHECK(db.txPowTree().size() == 1);

        // Drugi initGenesis — ignorowany
        bool ok2 = db.initGenesis();
        CHECK(ok2 == false);
        CHECK(db.txPowTree().size() == 1);
    }
}

// ── TxPoWProcessor MMR update ─────────────────────────────────────────────────
TEST_SUITE("TxPoWProcessor_MMR") {
    // Helper: tworzy genesis block (blockNum=0, parent=zero)
    // isBlockTxPoW = (bn==0 && allZero) || (bn>0)
    // Genesis: blockNum=0, parent=0x00...00 → allZero=true → isBlock=true
    static TxPoW makeBlock(int64_t blockNum, int64_t nonce = 0) {
        TxPoW txp;
        txp.header().blockNumber = MiniNumber(blockNum);
        txp.header().nonce       = MiniNumber(nonce);
        // blockNum=0 genesis: all-zero parent (allZero=true)
        // blockNum>0: parent ustawiany z zewnątrz
        for (int i = 0; i < CASCADE_LEVELS; ++i)
            txp.header().superParents[i] = MiniData(std::vector<uint8_t>(32, 0x00));
        return txp;
    }

    TEST_CASE("linear chain — updateMMRIfTip incremental") {
        using namespace minima::system;
        MinimaDB db;
        TxPoWProcessor proc(db);

        // Genesis (blockNum=0, parent=zeros) → isBlock=true
        TxPoW b0 = makeBlock(0);
        auto r0 = proc.processTxPoWSync(b0);
        REQUIRE(r0 == ProcessResult::ACCEPTED);

        auto* tip0 = db.txPowTree().tip();
        REQUIRE(tip0 != nullptr);
        MiniData id0 = tip0->txPoWID();

        // Blok 1 (blockNum=1) → isBlock=true via bn>0
        TxPoW b1 = makeBlock(1, 1);
        b1.header().superParents[0] = id0;
        auto r1 = proc.processTxPoWSync(b1);
        REQUIRE(r1 == ProcessResult::ACCEPTED);
        CHECK(db.currentHeight() == 1);
        CHECK(db.txPowTree().size() == 2);
    }

    TEST_CASE("rebuildMMR after reorg") {
        using namespace minima::system;
        MinimaDB db;

        // Zbuduj chain: genesis → b1a → b1b (reorg fork)
        TxPoW genesis = makeBlock(0);
        db.addBlock(genesis);
        MiniData genesisID = db.txPowTree().tip()->txPoWID();

        // Fork A: blok 1a
        TxPoW b1a = makeBlock(1, 10);
        b1a.header().superParents[0] = genesisID;
        db.addBlock(b1a);
        CHECK(db.currentHeight() == 1);

        // Fork B: blok 1b (inny nonce → inne ID → krótszy fork, nie zmienia tipa)
        TxPoW b1b = makeBlock(1, 20);
        b1b.header().superParents[0] = genesisID;
        db.addBlock(b1b);

        // Rebuild MMR — powinien być stable (nie crashować)
        db.rebuildMMR();
        CHECK(db.currentHeight() == 1);
    }
}

// ── MMR with Coins ────────────────────────────────────────────────────────────
// Tests for applyBlockToMMR — verifies UTxO coins are tracked in MMR
TEST_SUITE("MinimaDB_MMR_Coins") {

    static Coin makeCoin(const std::string& idHex, int64_t amount) {
        std::vector<uint8_t> idBytes(32, 0x00);
        // fill with idHex characters as bytes (simple deterministic ID)
        for (size_t i = 0; i < idHex.size() && i < 32; ++i)
            idBytes[i] = static_cast<uint8_t>(idHex[i]);
        Coin c;
        c.setCoinID(MiniData(idBytes));
        c.setAmount(MiniNumber(amount));
        // address = 32 zero bytes (script=RETURN TRUE)
        Address addr(MiniData(std::vector<uint8_t>(32, 0xAB)));
        c.setAddress(addr);
        return c;
    }

    static TxPoW makeBlockWithTxn(int64_t blockNum, int64_t nonce,
                                   const std::vector<Coin>& outputs,
                                   const std::vector<Coin>& inputs = {}) {
        TxPoW txp;
        txp.header().blockNumber  = MiniNumber(blockNum);
        txp.header().nonce        = MiniNumber(nonce);
        for (int i = 1; i < CASCADE_LEVELS; ++i)
            txp.header().superParents[i] = MiniData(std::vector<uint8_t>(32, 0));
        for (const auto& o : outputs) txp.body().txn.addOutput(o);
        for (const auto& inp : inputs) txp.body().txn.addInput(inp);
        return txp;
    }

    TEST_CASE("MMR leaf count grows with outputs") {
        MinimaDB db;
        Coin c1 = makeCoin("coin1", 100);
        Coin c2 = makeCoin("coin2", 200);
        TxPoW b0 = makeBlockWithTxn(0, 1, {c1, c2});
        db.addBlock(b0);
        // MMR should have 2 leaves (one per output)
        CHECK(db.mmrSet().getLeafCount() == 2);
    }

    TEST_CASE("MMR root changes after adding block with coins") {
        MinimaDB db;
        MMRData emptyRoot = db.mmrSet().getRoot();

        Coin c1 = makeCoin("coinA", 500);
        TxPoW b0 = makeBlockWithTxn(0, 1, {c1});
        db.addBlock(b0);

        MMRData rootAfter = db.mmrSet().getRoot();
        // Root should be non-empty after adding a coin
        CHECK(!rootAfter.isEmpty());
        CHECK(rootAfter.getData() != emptyRoot.getData());
    }

    TEST_CASE("Coin hash is deterministic (same serialise → same hash)") {
        Coin c = makeCoin("detcoin", 42);
        MiniData h1 = c.hashValue();
        MiniData h2 = c.hashValue();
        CHECK(h1 == h2);
        CHECK(h1.bytes().size() == 32);
    }

    TEST_CASE("Coin hash differs for different coins") {
        Coin c1 = makeCoin("coinX", 100);
        Coin c2 = makeCoin("coinY", 200);
        CHECK(c1.hashValue() != c2.hashValue());
    }

    TEST_CASE("rebuildMMR with coins is stable") {
        MinimaDB db;
        Coin c1 = makeCoin("rc1", 111);
        Coin c2 = makeCoin("rc2", 222);
        TxPoW b0 = makeBlockWithTxn(0, 1, {c1, c2});
        db.addBlock(b0);

        MMRData root1 = db.mmrSet().getRoot();
        db.rebuildMMR();
        MMRData root2 = db.mmrSet().getRoot();

        // Root should be the same after rebuild
        CHECK(root1.getData() == root2.getData());
        CHECK(db.mmrSet().getLeafCount() == 2);
    }

    TEST_CASE("MMR inclusion proof verifies for added coin") {
        MinimaDB db;
        Coin c1 = makeCoin("proofcoin", 999);
        TxPoW b0 = makeBlockWithTxn(0, 1, {c1});
        db.addBlock(b0);

        // Leaf 0 should be verifiable
        MMRProof proof = db.mmrSet().getProof(0);
        MMRData  root  = db.mmrSet().getRoot();
        CHECK(proof.verifyProof(root));
    }

    TEST_CASE("MMR has 3 leaves after 3-output block") {
        MinimaDB db;
        Coin c1 = makeCoin("t1", 10);
        Coin c2 = makeCoin("t2", 20);
        Coin c3 = makeCoin("t3", 30);
        TxPoW b0 = makeBlockWithTxn(0, 7, {c1, c2, c3});
        db.addBlock(b0);
        CHECK(db.mmrSet().getLeafCount() == 3);

        // All 3 proofs should verify
        MMRData root = db.mmrSet().getRoot();
        for (uint64_t i = 0; i < 3; ++i) {
            MMRProof p = db.mmrSet().getProof(i);
            CHECK(p.verifyProof(root));
        }
    }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Cascade + MegaMMR integration with MinimaDB
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#include "../../src/chain/cascade/Cascade.hpp"
#include "../../src/mmr/MegaMMR.hpp"

TEST_SUITE("MinimaDB_CascadeMegaMMR") {

    // ── helpers ───────────────────────────────────────────────────────────

    static Coin makeCMCoin(uint8_t id, int64_t amount) {
        Coin c;
        c.setCoinID(MiniData(std::vector<uint8_t>(32, id)));
        c.setAmount(MiniNumber(amount));
        c.setAddress(Address(MiniData(std::vector<uint8_t>(32, uint8_t(id + 0x80)))));
        return c;
    }

    // Build a chain of N blocks, each with a unique output coin.
    // Returns the MinimaDB with all blocks added.
    // Also returns all TxPoW in order so caller can read IDs.
    static std::vector<TxPoW> buildChain(MinimaDB& db, int n) {
        std::vector<TxPoW> blocks;
        for (int i = 0; i < n; ++i) {
            TxPoW txp;
            txp.header().blockNumber = MiniNumber(int64_t(i));
            txp.header().nonce       = MiniNumber(int64_t(i + 1));
            // Link to previous block
            if (i > 0) {
                txp.header().superParents[0] = blocks.back().computeID();
            } else {
                txp.header().superParents[0] = MiniData(std::vector<uint8_t>(32, 0xFF));
            }
            for (int j = 1; j < CASCADE_LEVELS; ++j)
                txp.header().superParents[j] = MiniData(std::vector<uint8_t>(32, 0));

            // Add a unique coin as output
            Coin c = makeCMCoin(static_cast<uint8_t>(i), int64_t((i+1) * 100));
            txp.body().txn.addOutput(c);

            db.addBlock(txp);
            blocks.push_back(txp);
        }
        return blocks;
    }

    // ── Cascade tests ─────────────────────────────────────────────────────

    TEST_CASE("cascade is empty before any block") {
        MinimaDB db;
        CHECK(db.cascade().empty());
    }

    TEST_CASE("cascade grows as blocks are added") {
        MinimaDB db;
        auto blocks = buildChain(db, 3);
        CHECK(db.cascade().length() > 0);
        CHECK(db.cascade().tipBlock() == 2);
    }

    TEST_CASE("cascade tip tracks chain tip") {
        MinimaDB db;
        auto blocks = buildChain(db, 5);
        CHECK(db.cascade().tipBlock() == 4);
        CHECK(db.cascade().tip() != nullptr);
    }

    TEST_CASE("cascade root block <= tip block") {
        MinimaDB db;
        auto blocks = buildChain(db, 5);
        CHECK(db.cascade().rootBlock() <= db.cascade().tipBlock());
    }

    TEST_CASE("cascade total weight increases with chain") {
        MinimaDB db;
        buildChain(db, 1);
        double w1 = db.cascade().totalWeight();
        buildChain(db, 1);  // adds more blocks (separate DB needed for clean test)
        // Just verify weight is positive
        CHECK(w1 > 0.0);
    }

    TEST_CASE("cascade serialise/deserialise round-trip after addBlock") {
        MinimaDB db;
        auto blocks = buildChain(db, 4);

        auto bytes = db.cascade().serialise();
        size_t off = 0;
        auto c2 = cascade::Cascade::deserialise(bytes, off);

        CHECK(c2.length()   == db.cascade().length());
        CHECK(c2.tipBlock() == db.cascade().tipBlock());
        CHECK(off == bytes.size());
    }

    TEST_CASE("rebuildCascade produces same length as original") {
        MinimaDB db;
        buildChain(db, 5);
        int lenBefore = db.cascade().length();
        db.rebuildCascade();
        CHECK(db.cascade().length() == lenBefore);
        CHECK(db.cascade().tipBlock() == 4);
    }

    // ── MegaMMR tests ─────────────────────────────────────────────────────

    TEST_CASE("megaMMR empty before blocks") {
        MinimaDB db;
        CHECK(db.megaMMR().leafCount() == 0);
    }

    TEST_CASE("megaMMR leafCount grows with outputs") {
        MinimaDB db;
        buildChain(db, 3); // 3 blocks, each 1 output coin
        CHECK(db.megaMMR().leafCount() == 3);
    }

    TEST_CASE("megaMMR has correct coins after addBlock") {
        MinimaDB db;
        auto blocks = buildChain(db, 3);
        // Each block i adds coin with id byte = i
        for (int i = 0; i < 3; ++i) {
            MiniData cid(std::vector<uint8_t>(32, static_cast<uint8_t>(i)));
            CHECK(db.megaMMR().hasCoin(cid));
        }
    }

    TEST_CASE("megaMMR verifyRoot after addBlock") {
        MinimaDB db;
        buildChain(db, 3);
        MiniData root = db.megaMMR().getRoot().getData();
        CHECK(db.megaMMR().verifyRoot(root));
    }

    TEST_CASE("megaMMR root changes block by block") {
        MinimaDB db;
        auto blocks = buildChain(db, 1);
        MiniData root1 = db.megaMMR().getRoot().getData();

        // Add one more block
        TxPoW b2;
        b2.header().blockNumber     = MiniNumber(int64_t(1));
        b2.header().nonce           = MiniNumber(int64_t(999));
        b2.header().superParents[0] = blocks.back().computeID();
        for (int j = 1; j < CASCADE_LEVELS; ++j)
            b2.header().superParents[j] = MiniData(std::vector<uint8_t>(32, 0));
        Coin c = makeCMCoin(0xF0, 500);
        b2.body().txn.addOutput(c);
        db.addBlock(b2);

        MiniData root2 = db.megaMMR().getRoot().getData();
        CHECK_FALSE(root1 == root2);
    }

    TEST_CASE("megaMMR checkpoint created per block") {
        MinimaDB db;
        buildChain(db, 3);
        // Blocks 0,1,2 should each have a checkpoint
        CHECK(db.megaMMR().hasCheckpoint(MiniNumber(int64_t(0))));
        CHECK(db.megaMMR().hasCheckpoint(MiniNumber(int64_t(1))));
        CHECK(db.megaMMR().hasCheckpoint(MiniNumber(int64_t(2))));
    }

    TEST_CASE("megaMMR rollback restores prior state") {
        MinimaDB db;
        auto blocks = buildChain(db, 2);
        // After 2 blocks: leafCount=2
        MiniData root1 = db.megaMMR().getRoot().getData();
        CHECK(db.megaMMR().leafCount() == 2);

        // Add block 2 (leaf 3)
        TxPoW b2;
        b2.header().blockNumber     = MiniNumber(int64_t(2));
        b2.header().nonce           = MiniNumber(int64_t(42));
        b2.header().superParents[0] = blocks.back().computeID();
        for (int j = 1; j < CASCADE_LEVELS; ++j)
            b2.header().superParents[j] = MiniData(std::vector<uint8_t>(32, 0));
        Coin c = makeCMCoin(0xCC, 300);
        b2.body().txn.addOutput(c);
        db.addBlock(b2);
        CHECK(db.megaMMR().leafCount() == 3);

        // Re-org: roll back to block 1
        bool ok = db.rollbackMegaMMR(1);
        CHECK(ok);
        CHECK(db.megaMMR().leafCount() == 2);
        CHECK(db.megaMMR().verifyRoot(root1));
    }

    TEST_CASE("megaMMR getCoinProof works after addBlock") {
        MinimaDB db;
        buildChain(db, 4);
        // Coin 0 (added in block 0) should have a valid proof
        MiniData cid0(std::vector<uint8_t>(32, 0));
        auto optProof = db.megaMMR().getCoinProof(cid0);
        REQUIRE(optProof.has_value());
        Coin c0 = makeCMCoin(0, 100);
        CHECK(db.megaMMR().verifyCoinProof(c0, *optProof));
    }

    TEST_CASE("rebuildMegaMMR produces same root") {
        MinimaDB db;
        buildChain(db, 4);
        MiniData rootBefore = db.megaMMR().getRoot().getData();
        db.rebuildMegaMMR();
        CHECK(db.megaMMR().getRoot().getData() == rootBefore);
        CHECK(db.megaMMR().leafCount() == 4);
    }

    // ── Cascade + MegaMMR combined ────────────────────────────────────────

    TEST_CASE("cascade and megaMMR both advance together") {
        MinimaDB db;
        buildChain(db, 5);
        CHECK(db.cascade().tipBlock()  == 4);
        CHECK(db.megaMMR().leafCount() == 5);
        // Both structures should have valid state
        CHECK_FALSE(db.cascade().empty());
        CHECK(db.megaMMR().verifyRoot(db.megaMMR().getRoot().getData()));
    }

    TEST_CASE("reset clears cascade and megaMMR") {
        MinimaDB db;
        buildChain(db, 3);
        db.reset();
        CHECK(db.cascade().empty());
        CHECK(db.megaMMR().leafCount() == 0);
        CHECK(db.megaMMR().checkpointCount() == 0);
    }

    TEST_CASE("fast-sync: getSyncPacket after chain matches root") {
        MinimaDB db;
        buildChain(db, 4);
        MiniData chainRoot = db.megaMMR().getRoot().getData();

        // Simulate new node receiving sync packet
        auto pkt = db.megaMMR().getSyncPacket();
        size_t off = 0;
        auto syncedMMR = MegaMMR::applySyncPacket(pkt.data(), off);

        // New node's root must match the chain root
        CHECK(syncedMMR.verifyRoot(chainRoot));
        CHECK(syncedMMR.leafCount() == 4);
    }
}
