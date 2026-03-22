#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/persistence/Database.hpp"
#include "../../src/persistence/BlockStore.hpp"
#include "../../src/persistence/UTxOStore.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/objects/TxBlock.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/types/MiniNumber.hpp"
#include "../../src/types/MiniData.hpp"

using namespace minima;
using namespace minima::persistence;

// ── helpers ──────────────────────────────────────────────────────────────────

static TxBlock makeBlock(int64_t blockNum, const std::string& idHex = "") {
    TxPoW tx;
    tx.header().blockNumber = MiniNumber(blockNum);
    tx.header().timeMilli   = MiniNumber(blockNum * 10000);
    if (!idHex.empty()) {
        std::vector<uint8_t> bytes(32, 0);
        // encode blockNum into last 8 bytes so IDs differ
        for (int i = 0; i < 8; i++)
            bytes[24 + i] = static_cast<uint8_t>((blockNum >> (i * 8)) & 0xFF);
        // txpowID is computed from header hash, not stored directly
    }
    // parent = zero
    tx.header().superParents[0] = MiniData(std::vector<uint8_t>(32, 0));
    return TxBlock(tx);
}

static Coin makeCoin(int idx) {
    std::vector<uint8_t> idBytes(32, static_cast<uint8_t>(idx));
    std::vector<uint8_t> addrBytes(32, static_cast<uint8_t>(idx + 100));

    Coin c;
    c.setCoinID(MiniData(idBytes));
    c.setAddress(Address(MiniData(addrBytes)));
    c.setAmount(MiniNumber(int64_t(100 + idx)));
    c.setTokenID(MiniData(std::vector<uint8_t>(32, 0))); // native Minima
    return c;
}

// ── Database tests ────────────────────────────────────────────────────────────

TEST_CASE("Database: open in-memory") {
    CHECK_NOTHROW(Database(":memory:"));
}

TEST_CASE("Database: exec and basic query") {
    Database db(":memory:");
    db.exec("CREATE TABLE t(id INTEGER, val TEXT);");
    db.exec("INSERT INTO t VALUES(1,'hello');");

    auto* stmt = db.prepare("SELECT id, val FROM t;");
    int rows = 0;
    db.step(stmt, [&](sqlite3_stmt* s) {
        CHECK(sqlite3_column_int(s, 0) == 1);
        CHECK(std::string(reinterpret_cast<const char*>(sqlite3_column_text(s, 1))) == "hello");
        rows++;
    });
    sqlite3_finalize(stmt);
    CHECK(rows == 1);
}

TEST_CASE("Database: transaction commit") {
    Database db(":memory:");
    db.exec("CREATE TABLE t(v INTEGER);");
    db.beginTransaction();
    db.exec("INSERT INTO t VALUES(42);");
    db.commit();

    auto* stmt = db.prepare("SELECT v FROM t;");
    int val = 0;
    db.step(stmt, [&](sqlite3_stmt* s) { val = sqlite3_column_int(s, 0); });
    sqlite3_finalize(stmt);
    CHECK(val == 42);
}

TEST_CASE("Database: transaction rollback") {
    Database db(":memory:");
    db.exec("CREATE TABLE t(v INTEGER);");
    db.beginTransaction();
    db.exec("INSERT INTO t VALUES(99);");
    db.rollback();

    auto* stmt = db.prepare("SELECT COUNT(*) FROM t;");
    int cnt = 0;
    db.step(stmt, [&](sqlite3_stmt* s) { cnt = sqlite3_column_int(s, 0); });
    sqlite3_finalize(stmt);
    CHECK(cnt == 0);
}

// ── BlockStore tests ──────────────────────────────────────────────────────────

TEST_CASE("BlockStore: empty on init") {
    Database db(":memory:");
    BlockStore bs(db);
    CHECK(bs.count() == 0);
    CHECK_FALSE(bs.getTip().has_value());
}

TEST_CASE("BlockStore: put and count") {
    Database db(":memory:");
    BlockStore bs(db);
    bs.put(makeBlock(1, "a"));
    bs.put(makeBlock(2, "b"));
    CHECK(bs.count() == 2);
}

TEST_CASE("BlockStore: getTip returns highest block") {
    Database db(":memory:");
    BlockStore bs(db);
    bs.put(makeBlock(1, "a"));
    bs.put(makeBlock(5, "b"));
    bs.put(makeBlock(3, "c"));
    auto tip = bs.getTip();
    REQUIRE(tip.has_value());
    CHECK(tip->txpow().header().blockNumber.getAsLong() == 5);
}

TEST_CASE("BlockStore: getByNumber") {
    Database db(":memory:");
    BlockStore bs(db);
    bs.put(makeBlock(10, "x"));
    auto blk = bs.getByNumber(10);
    REQUIRE(blk.has_value());
    CHECK(blk->txpow().header().blockNumber.getAsLong() == 10);
}

TEST_CASE("BlockStore: getByNumber missing returns nullopt") {
    Database db(":memory:");
    BlockStore bs(db);
    CHECK_FALSE(bs.getByNumber(999).has_value());
}

TEST_CASE("BlockStore: getRange") {
    Database db(":memory:");
    BlockStore bs(db);
    for (int i = 1; i <= 10; i++) bs.put(makeBlock(i));
    auto range = bs.getRange(3, 7);
    CHECK(range.size() == 5);
    CHECK(range[0].txpow().header().blockNumber.getAsLong() == 3);
    CHECK(range[4].txpow().header().blockNumber.getAsLong() == 7);
}

TEST_CASE("BlockStore: remove") {
    Database db(":memory:");
    BlockStore bs(db);
    auto blk = makeBlock(1);
    bs.put(blk);
    CHECK(bs.count() == 1);
    std::string id = blk.txpow().computeID().toHexString(false);
    bs.remove(id);
    CHECK(bs.count() == 0);
}

TEST_CASE("BlockStore: put overwrites duplicate") {
    Database db(":memory:");
    BlockStore bs(db);
    auto blk = makeBlock(1);
    bs.put(blk);
    bs.put(blk); // same txpow_id
    CHECK(bs.count() == 1);
}

// ── UTxOStore tests ───────────────────────────────────────────────────────────

TEST_CASE("UTxOStore: empty on init") {
    Database db(":memory:");
    UTxOStore us(db);
    CHECK(us.count() == 0);
    CHECK(us.getUnspent().empty());
}

TEST_CASE("UTxOStore: put and count") {
    Database db(":memory:");
    UTxOStore us(db);
    us.put(makeCoin(0), 1);
    us.put(makeCoin(1), 1);
    CHECK(us.count() == 2);
}

TEST_CASE("UTxOStore: getUnspent") {
    Database db(":memory:");
    UTxOStore us(db);
    us.put(makeCoin(0), 1);
    us.put(makeCoin(1), 1);
    auto unspent = us.getUnspent();
    CHECK(unspent.size() == 2);
}

TEST_CASE("UTxOStore: markSpent reduces unspent") {
    Database db(":memory:");
    UTxOStore us(db);
    auto c0 = makeCoin(0);
    auto c1 = makeCoin(1);
    us.put(c0, 1);
    us.put(c1, 1);
    us.markSpent(c0.coinID().toHexString(false), 2);
    auto unspent = us.getUnspent();
    CHECK(unspent.size() == 1);
    CHECK(unspent[0].coinID().toHexString(false) == c1.coinID().toHexString(false));
}

TEST_CASE("UTxOStore: getById") {
    Database db(":memory:");
    UTxOStore us(db);
    auto c = makeCoin(42);
    us.put(c, 5);
    auto found = us.getById(c.coinID().toHexString(false));
    REQUIRE(found.has_value());
    CHECK(found->amount().getAsLong() == c.amount().getAsLong());
}

TEST_CASE("UTxOStore: getById missing returns nullopt") {
    Database db(":memory:");
    UTxOStore us(db);
    std::string fakeId(64, '0');
    CHECK_FALSE(us.getById(fakeId).has_value());
}

TEST_CASE("UTxOStore: getByAddress") {
    Database db(":memory:");
    UTxOStore us(db);
    auto c0 = makeCoin(0);
    auto c1 = makeCoin(1);
    us.put(c0, 1);
    us.put(c1, 1);
    std::string addr0 = c0.address().hash().toHexString(false);
    auto result = us.getByAddress(addr0);
    CHECK(result.size() == 1);
    CHECK(result[0].coinID().toHexString(false) == c0.coinID().toHexString(false));
}

TEST_CASE("UTxOStore: deleteByBlock (reorg rollback)") {
    Database db(":memory:");
    UTxOStore us(db);
    us.put(makeCoin(0), 5);
    us.put(makeCoin(1), 5);
    us.put(makeCoin(2), 6);
    us.deleteByBlock(5);
    CHECK(us.count() == 1); // only coin at block 6 remains
}

TEST_CASE("UTxOStore: duplicate put is ignored") {
    Database db(":memory:");
    UTxOStore us(db);
    auto c = makeCoin(0);
    us.put(c, 1);
    us.put(c, 1); // duplicate
    CHECK(us.count() == 1);
}
