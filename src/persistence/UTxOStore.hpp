#pragma once
#include <string>
#include <vector>
#include <optional>
#include "Database.hpp"
#include "../objects/Coin.hpp"

namespace minima::persistence {

using namespace minima;

/**
 * Persistent UTxO set backed by SQLite.
 *
 * Schema:
 *   utxos(coin_id TEXT PK, address TEXT, amount TEXT,
 *         token_id TEXT, block_created INTEGER, block_spent INTEGER,
 *         spent INTEGER, serialized BLOB)
 *
 * Soft-delete: coins are marked spent, not removed — needed for reorg.
 * Java reference: com.minima.database.minima.TxPoWDB (spent/unspent tracking)
 */
class UTxOStore {
public:
    explicit UTxOStore(Database& db) : db_(db) {
        createSchema();
        prepareStatements();
    }

    ~UTxOStore() {
        sqlite3_finalize(stmtInsert_);
        sqlite3_finalize(stmtMarkSpent_);
        sqlite3_finalize(stmtGetById_);
        sqlite3_finalize(stmtGetByAddress_);
        sqlite3_finalize(stmtGetUnspent_);
        sqlite3_finalize(stmtDeleteByBlock_);
        sqlite3_finalize(stmtCount_);
    }

    /** Add a new UTxO (coin output). Ignored if coin_id already exists. */
    void put(const Coin& coin, int64_t blockCreated) {
        std::string coinId  = coin.coinID().toHexString(false);
        std::string addr    = coin.address().hash().toHexString(false);
        std::string amount  = coin.amount().toString();
        std::string tokenId = coin.tokenID().toHexString(false);

        auto bytes = coin.serialise();

        sqlite3_bind_text(stmtInsert_, 1, coinId.c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmtInsert_, 2, addr.c_str(),    -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmtInsert_, 3, amount.c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmtInsert_, 4, tokenId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmtInsert_, 5, blockCreated);
        sqlite3_bind_int(stmtInsert_,  6, 0);    // not spent
        sqlite3_bind_blob(stmtInsert_, 7, bytes.data(), (int)bytes.size(), SQLITE_TRANSIENT);
        db_.stepOnce(stmtInsert_);
    }

    /** Mark a coin as spent (soft delete). */
    void markSpent(const std::string& coinId, int64_t blockSpent) {
        sqlite3_bind_int(stmtMarkSpent_,   1, 1);
        sqlite3_bind_int64(stmtMarkSpent_, 2, blockSpent);
        sqlite3_bind_text(stmtMarkSpent_,  3, coinId.c_str(), -1, SQLITE_TRANSIENT);
        db_.stepOnce(stmtMarkSpent_);
    }

    /** Get coin by ID. */
    std::optional<Coin> getById(const std::string& coinId) {
        sqlite3_bind_text(stmtGetById_, 1, coinId.c_str(), -1, SQLITE_TRANSIENT);
        std::optional<Coin> result;
        db_.step(stmtGetById_, [&](sqlite3_stmt* s) {
            result = blobToCoin(s, 0);
        });
        return result;
    }

    /** Get all coins (spent + unspent) belonging to an address. */
    std::vector<Coin> getByAddress(const std::string& addressHex) {
        sqlite3_bind_text(stmtGetByAddress_, 1, addressHex.c_str(), -1, SQLITE_TRANSIENT);
        std::vector<Coin> coins;
        db_.step(stmtGetByAddress_, [&](sqlite3_stmt* s) {
            coins.push_back(blobToCoin(s, 0));
        });
        return coins;
    }

    /** Get all unspent coins. */
    std::vector<Coin> getUnspent() {
        std::vector<Coin> coins;
        db_.step(stmtGetUnspent_, [&](sqlite3_stmt* s) {
            coins.push_back(blobToCoin(s, 0));
        });
        return coins;
    }

    /** Remove all coins created at blockNumber (for reorg rollback). */
    void deleteByBlock(int64_t blockNumber) {
        sqlite3_bind_int64(stmtDeleteByBlock_, 1, blockNumber);
        db_.stepOnce(stmtDeleteByBlock_);
    }

    /** Total coin count (spent + unspent). */
    int64_t count() {
        int64_t n = 0;
        db_.step(stmtCount_, [&](sqlite3_stmt* s) {
            n = sqlite3_column_int64(s, 0);
        });
        return n;
    }

private:
    void createSchema() {
        db_.exec(R"(
            CREATE TABLE IF NOT EXISTS utxos (
                coin_id       TEXT PRIMARY KEY,
                address       TEXT NOT NULL,
                amount        TEXT NOT NULL,
                token_id      TEXT NOT NULL,
                block_created INTEGER NOT NULL,
                block_spent   INTEGER NOT NULL DEFAULT -1,
                spent         INTEGER NOT NULL DEFAULT 0,
                serialized    BLOB NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_utxos_address ON utxos(address);
            CREATE INDEX IF NOT EXISTS idx_utxos_spent   ON utxos(spent);
            CREATE INDEX IF NOT EXISTS idx_utxos_block   ON utxos(block_created);
        )");
    }

    void prepareStatements() {
        stmtInsert_ = db_.prepare(
            "INSERT OR IGNORE INTO utxos"
            "(coin_id,address,amount,token_id,block_created,spent,serialized)"
            " VALUES(?,?,?,?,?,?,?);");
        stmtMarkSpent_ = db_.prepare(
            "UPDATE utxos SET spent=?, block_spent=? WHERE coin_id=?;");
        stmtGetById_ = db_.prepare(
            "SELECT serialized FROM utxos WHERE coin_id=? LIMIT 1;");
        stmtGetByAddress_ = db_.prepare(
            "SELECT serialized FROM utxos WHERE address=? ORDER BY block_created ASC;");
        stmtGetUnspent_ = db_.prepare(
            "SELECT serialized FROM utxos WHERE spent=0 ORDER BY block_created ASC;");
        stmtDeleteByBlock_ = db_.prepare(
            "DELETE FROM utxos WHERE block_created=?;");
        stmtCount_ = db_.prepare(
            "SELECT COUNT(*) FROM utxos;");
    }

    Coin blobToCoin(sqlite3_stmt* s, int col) {
        const void* blob = sqlite3_column_blob(s, col);
        int sz = sqlite3_column_bytes(s, col);
        size_t offset = 0;
        return Coin::deserialise(static_cast<const uint8_t*>(blob), offset);
    }

    Database& db_;
    sqlite3_stmt* stmtInsert_        = nullptr;
    sqlite3_stmt* stmtMarkSpent_     = nullptr;
    sqlite3_stmt* stmtGetById_       = nullptr;
    sqlite3_stmt* stmtGetByAddress_  = nullptr;
    sqlite3_stmt* stmtGetUnspent_    = nullptr;
    sqlite3_stmt* stmtDeleteByBlock_ = nullptr;
    sqlite3_stmt* stmtCount_         = nullptr;
};

} // namespace minima::persistence
