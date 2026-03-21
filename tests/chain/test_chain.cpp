#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/chain/BlockStore.hpp"
#include "../../src/chain/ChainState.hpp"
#include "../../src/chain/UTxOSet.hpp"
#include "../../src/chain/ChainProcessor.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"

using namespace minima;
using namespace minima::chain;

static MiniData makeHash(uint8_t b) {
    return MiniData(std::vector<uint8_t>(32, b));
}

static TxPoW makeTxPoW(int64_t blockNum) {
    TxPoW t;
    t.header().blockNumber = MiniNumber(blockNum);
    t.header().timeMilli   = MiniNumber(int64_t(1000000 + blockNum));
    return t;
}

// ── BlockStore ────────────────────────────────────────────────────────────────

TEST_CASE("BlockStore: add and retrieve") {
    BlockStore store;
    TxPoW b = makeTxPoW(1);
    MiniData id = b.computeID();
    store.add(id, b);
    CHECK(store.has(id));
    auto opt = store.get(id);
    CHECK(opt.has_value());
    CHECK(opt->header().blockNumber == MiniNumber(int64_t(1)));
}

TEST_CASE("BlockStore: missing key returns nullopt") {
    BlockStore store;
    CHECK_FALSE(store.has(makeHash(0xAB)));
    CHECK_FALSE(store.get(makeHash(0xAB)).has_value());
}

TEST_CASE("BlockStore: multiple blocks") {
    BlockStore store;
    for (int i = 1; i <= 10; ++i) {
        TxPoW b = makeTxPoW(i);
        store.add(b.computeID(), b);
    }
    CHECK(store.size() == 10);
}

TEST_CASE("BlockStore: getByNumber") {
    BlockStore store;
    TxPoW b = makeTxPoW(7);
    MiniData id = b.computeID();
    store.add(id, b);
    auto opt = store.getByNumber(7);
    CHECK(opt.has_value());
    CHECK(opt->header().blockNumber == MiniNumber(int64_t(7)));
}

// ── ChainState ────────────────────────────────────────────────────────────────

TEST_CASE("ChainState: initial state") {
    ChainState cs;
    CHECK(cs.getHeight() == 0);
    CHECK(cs.isGenesis());
}

TEST_CASE("ChainState: update tip") {
    ChainState cs;
    MiniData h = makeHash(0xBE);
    cs.setTip(h, 42);
    CHECK(cs.getHeight() == 42);
    CHECK(cs.getTip() == h);
}

// ── UTxOSet ───────────────────────────────────────────────────────────────────

TEST_CASE("UTxOSet: add and spend") {
    UTxOSet utxo;
    Coin c;
    c.setCoinID(makeHash(0x01));
    c.setAmount(MiniNumber(int64_t(100)));
    c.setAddress(Address(makeHash(0x02)));

    utxo.addCoin(c);
    CHECK(utxo.hasCoin(c.coinID()));
    utxo.spendCoin(c.coinID());
    CHECK_FALSE(utxo.hasCoin(c.coinID()));
}

TEST_CASE("UTxOSet: double spend is safe") {
    UTxOSet utxo;
    Coin c;
    c.setCoinID(makeHash(0x11));
    c.setAmount(MiniNumber(int64_t(10)));
    c.setAddress(Address(makeHash(0x22)));
    utxo.addCoin(c);
    utxo.spendCoin(c.coinID());
    utxo.spendCoin(c.coinID()); // second spend: no throw
    CHECK_FALSE(utxo.hasCoin(c.coinID()));
}

// ── ChainProcessor ────────────────────────────────────────────────────────────

TEST_CASE("ChainProcessor: process genesis") {
    ChainProcessor proc;
    TxPoW g = makeTxPoW(0);
    CHECK(proc.processBlock(g));
    CHECK(proc.getHeight() == 0);
}

TEST_CASE("ChainProcessor: chain of 5 blocks") {
    ChainProcessor proc;
    for (int h = 0; h <= 5; ++h) {
        TxPoW b = makeTxPoW(h);
        proc.processBlock(b);
    }
    CHECK(proc.getHeight() == 5);
}

TEST_CASE("ChainProcessor: reject duplicate") {
    ChainProcessor proc;
    TxPoW b = makeTxPoW(1);
    CHECK(proc.processBlock(b));
    CHECK_FALSE(proc.processBlock(b));
}

TEST_CASE("ChainProcessor: tip tracks highest block") {
    ChainProcessor proc;
    proc.processBlock(makeTxPoW(0));
    proc.processBlock(makeTxPoW(1));
    proc.processBlock(makeTxPoW(2));
    CHECK(proc.getHeight() == 2);
}
