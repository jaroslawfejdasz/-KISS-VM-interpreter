/**
 * Tests for Chain layer: UTxOSet, BlockStore, ChainState, ChainProcessor, Mempool.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/chain/ChainState.hpp"
#include "../../src/chain/UTxOSet.hpp"
#include "../../src/chain/BlockStore.hpp"
#include "../../src/chain/ChainProcessor.hpp"
#include "../../src/mempool/Mempool.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniNumber.hpp"

using namespace minima;
using namespace minima::chain;
using namespace minima::mempool;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static MiniData makeID(uint8_t seed) {
    std::vector<uint8_t> b(32, seed);
    return MiniData(b);
}

static Coin makeCoin(uint8_t idSeed, int64_t amount = 100) {
    Coin c;
    c.setCoinID(makeID(idSeed));
    c.setAddress(Address(makeID(static_cast<uint8_t>(idSeed + 100u))));
    c.setAmount(MiniNumber(amount));
    return c;
}

static TxPoW makeGenesis() {
    TxPoW g;
    g.header().blockNumber       = MiniNumber(0LL);
    g.header().timestamp         = 1000000ULL;
    g.header().blockDifficulty   = MiniNumber("999999999");
    g.header().txnDifficulty     = MiniNumber("1");

    Coin out = makeCoin(0xAA, 1000000);
    g.body().txn.addOutput(out);
    return g;
}

// Build block that spends inSeed coin, produces outSeed coin
static TxPoW makeBlock(uint8_t inSeed, uint8_t outSeed,
                       uint64_t blockNum, const MiniData& parentID) {
    TxPoW b;
    b.header().blockNumber     = MiniNumber(static_cast<int64_t>(blockNum));
    b.header().timestamp       = 1000000ULL + blockNum * 10000;
    b.header().parentID        = parentID;
    b.header().blockDifficulty = MiniNumber("999999999");
    b.header().txnDifficulty   = MiniNumber("1");

    b.body().txn.addInput(makeCoin(inSeed, 100));
    b.body().txn.addOutput(makeCoin(outSeed, 100));
    return b;
}

// ─── ChainState ───────────────────────────────────────────────────────────────

TEST_SUITE("ChainState") {
    TEST_CASE("genesis detection") {
        ChainState cs;
        CHECK(cs.isGenesis());
        cs.blockNumber = 1;
        CHECK(!cs.isGenesis());
    }

    TEST_CASE("isBetterThan: higher blockNumber wins") {
        ChainState a, b;
        a.blockNumber = 5;
        b.blockNumber = 10;
        // a.isBetterThan(b) = "is b a better candidate than a?" = b.bn(10) > a.bn(5) = true
        CHECK(a.isBetterThan(b) == true);
        CHECK(b.isBetterThan(a) == false);
    }

    TEST_CASE("equal blockNumbers: not better") {
        ChainState a, b;
        a.blockNumber = b.blockNumber = 7;
        CHECK(a.isBetterThan(b) == false);
    }
}

// ─── UTxOSet ─────────────────────────────────────────────────────────────────

TEST_SUITE("UTxOSet") {
    TEST_CASE("add and contains") {
        UTxOSet utxos;
        Coin c = makeCoin(1);
        utxos.add(c);
        CHECK(utxos.contains(c.coinID()));
        CHECK(utxos.size() == 1);
    }

    TEST_CASE("remove existing coin") {
        UTxOSet utxos;
        Coin c = makeCoin(2);
        utxos.add(c);
        CHECK(utxos.remove(c.coinID()));
        CHECK(!utxos.contains(c.coinID()));
        CHECK(utxos.size() == 0);
    }

    TEST_CASE("remove non-existing returns false") {
        UTxOSet utxos;
        CHECK(!utxos.remove(makeID(99)));
    }

    TEST_CASE("get returns coin") {
        UTxOSet utxos;
        Coin c = makeCoin(3);
        utxos.add(c);
        auto opt = utxos.get(c.coinID());
        REQUIRE(opt.has_value());
        CHECK(opt->coinID().bytes() == c.coinID().bytes());
    }

    TEST_CASE("applyBlock removes inputs and adds outputs") {
        UTxOSet utxos;
        Coin in  = makeCoin(10, 100);
        Coin out = makeCoin(11, 100);
        utxos.add(in);

        Transaction tx;
        tx.addInput(in);
        tx.addOutput(out);

        CHECK(utxos.applyBlock(tx));
        CHECK(!utxos.contains(in.coinID()));
        CHECK(utxos.contains(out.coinID()));
    }

    TEST_CASE("applyBlock fails if input missing") {
        UTxOSet utxos;
        Coin in  = makeCoin(20, 100);
        Coin out = makeCoin(21, 100);
        // NOT adding in

        Transaction tx;
        tx.addInput(in);
        tx.addOutput(out);

        CHECK(!utxos.applyBlock(tx));
        CHECK(!utxos.contains(out.coinID())); // no side effect
    }

    TEST_CASE("revertBlock undoes applyBlock") {
        UTxOSet utxos;
        Coin in  = makeCoin(30, 100);
        Coin out = makeCoin(31, 100);
        utxos.add(in);

        Transaction tx;
        tx.addInput(in);
        tx.addOutput(out);

        utxos.applyBlock(tx);
        utxos.revertBlock(tx);

        CHECK(utxos.contains(in.coinID()));
        CHECK(!utxos.contains(out.coinID()));
    }

    TEST_CASE("coinsAt returns coins for address") {
        UTxOSet utxos;
        MiniData addr = makeID(0xBB);

        Coin c1 = makeCoin(0x01);
        c1.setAddress(Address(addr));
        Coin c2 = makeCoin(0x02);
        c2.setAddress(Address(addr));
        Coin c3 = makeCoin(0x03); // different address

        utxos.add(c1);
        utxos.add(c2);
        utxos.add(c3);

        auto coins = utxos.coinsAt(addr);
        CHECK(coins.size() == 2);
    }
}

// ─── BlockStore ───────────────────────────────────────────────────────────────

TEST_SUITE("BlockStore") {
    TEST_CASE("store and retrieve by hash") {
        BlockStore bs;
        TxPoW g = makeGenesis();
        bs.store(g);
        auto opt = bs.getByHash(g.computeID());
        REQUIRE(opt.has_value());
    }

    TEST_CASE("retrieve by block number") {
        BlockStore bs;
        TxPoW g = makeGenesis();
        bs.store(g);
        auto opt = bs.getByNumber(0);
        REQUIRE(opt.has_value());
    }

    TEST_CASE("has returns true/false correctly") {
        BlockStore bs;
        TxPoW g = makeGenesis();
        bs.store(g);
        CHECK(bs.has(g.computeID()));
        CHECK(!bs.has(makeID(0xFF)));
    }

    TEST_CASE("size tracks stored blocks") {
        BlockStore bs;
        CHECK(bs.size() == 0);
        bs.store(makeGenesis());
        CHECK(bs.size() == 1);
    }

    TEST_CASE("getAncestors walks parent chain") {
        BlockStore bs;
        TxPoW g  = makeGenesis();
        bs.store(g);
        TxPoW b1 = makeBlock(0xAA, 0x01, 1, g.computeID());
        bs.store(b1);
        TxPoW b2 = makeBlock(0x01, 0x02, 2, b1.computeID());
        bs.store(b2);

        std::vector<TxPoW> path;
        bool ok = bs.getAncestors(b2.computeID(), 3, path);
        CHECK(ok);
        CHECK(path.size() == 3);
    }
}

// ─── Mempool ─────────────────────────────────────────────────────────────────

TEST_SUITE("Mempool") {
    auto makeTx = [](uint8_t inSeed, uint8_t outSeed) {
        TxPoW tx;
        tx.header().blockNumber     = MiniNumber(0LL);
        tx.header().blockDifficulty = MiniNumber("0");
        tx.header().txnDifficulty   = MiniNumber("1");
        tx.header().timestamp       = static_cast<uint64_t>(inSeed) * 1000;
        tx.body().txn.addInput(makeCoin(inSeed, 50));
        tx.body().txn.addOutput(makeCoin(outSeed, 50));
        return tx;
    };

    TEST_CASE("add and contains") {
        Mempool mp;
        auto tx = makeTx(1, 2);
        CHECK(mp.add(tx));
        CHECK(mp.contains(tx.computeID()));
        CHECK(mp.size() == 1);
    }

    TEST_CASE("duplicate add returns false") {
        Mempool mp;
        auto tx = makeTx(3, 4);
        CHECK(mp.add(tx));
        CHECK(!mp.add(tx));
        CHECK(mp.size() == 1);
    }

    TEST_CASE("remove existing") {
        Mempool mp;
        auto tx = makeTx(5, 6);
        mp.add(tx);
        CHECK(mp.remove(tx.computeID()));
        CHECK(!mp.contains(tx.computeID()));
        CHECK(mp.size() == 0);
    }

    TEST_CASE("remove non-existing returns false") {
        Mempool mp;
        CHECK(!mp.remove(makeID(0xCC)));
    }

    TEST_CASE("double-spend detection: same input coinID rejected") {
        Mempool mp;
        auto tx1 = makeTx(10, 11); // spends coin #10
        auto tx2 = makeTx(10, 12); // also spends coin #10 — double-spend!
        CHECK(mp.add(tx1));
        CHECK(!mp.add(tx2));
        CHECK(mp.size() == 1);
    }

    TEST_CASE("capacity eviction") {
        Mempool mp(3);
        auto tx1 = makeTx(20, 21);
        auto tx2 = makeTx(22, 23);
        auto tx3 = makeTx(24, 25);
        auto tx4 = makeTx(26, 27);
        mp.add(tx1); mp.add(tx2); mp.add(tx3);
        CHECK(mp.size() == 3);
        mp.add(tx4);
        CHECK(mp.size() == 3);
        CHECK(mp.contains(tx4.computeID()));
    }

    TEST_CASE("getPending returns ordered list") {
        Mempool mp;
        auto tx1 = makeTx(30, 31);
        auto tx2 = makeTx(32, 33);
        mp.add(tx1); mp.add(tx2);
        auto pending = mp.getPending(10);
        CHECK(pending.size() == 2);
    }

    TEST_CASE("onBlockAccepted removes conflicting txns") {
        Mempool mp;
        auto tx = makeTx(40, 41); // spends coin #40
        mp.add(tx);
        CHECK(mp.size() == 1);

        TxPoW block = makeBlock(40, 50, 1, makeID(0x00));
        mp.onBlockAccepted(block);
        CHECK(mp.size() == 0);
    }
}
