#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/database/MinimaDB.hpp"
#include "../../src/wallet/Wallet.hpp"
#include "../../src/chain/TxPowTree.hpp"
#include "../../src/chain/TxPoWTreeNode.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniNumber.hpp"

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
