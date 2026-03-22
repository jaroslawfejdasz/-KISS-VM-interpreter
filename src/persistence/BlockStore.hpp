#pragma once
#include <string>
#include <vector>
#include <optional>
#include "Database.hpp"
#include "../objects/TxPoW.hpp"
#include "../objects/TxBlock.hpp"

namespace minima::persistence {

using namespace minima;

/**
 * Persistent block store backed by SQLite.
 *
 * Schema:
 *   blocks(txpow_id TEXT PK, block_number INTEGER, parent_id TEXT,
 *          block_time INTEGER, serialized BLOB)
 *
 * Java reference: com.minima.database.minima.TxPoWDB
 */
class BlockStore {
public:
    explicit BlockStore(Database& db) : db_(db) {
        createSchema();
        prepareStatements();
    }

    ~BlockStore() {
        sqlite3_finalize(stmtInsert_);
        sqlite3_finalize(stmtGetById_);
        sqlite3_finalize(stmtGetByNumber_);
        sqlite3_finalize(stmtGetTip_);
        sqlite3_finalize(stmtGetRange_);
        sqlite3_finalize(stmtDelete_);
        sqlite3_finalize(stmtCount_);
    }

    /** Store a TxBlock. Overwrites if txpow_id already exists. */
    void put(const TxBlock& block) {
        const TxPoW& tx = block.txpow();
        std::string id      = tx.computeID().toHexString(false);
        int64_t blockNum    = static_cast<int64_t>(tx.header().blockNumber.getAsLong());
        std::string parentId = tx.header().superParents[0].toHexString(false);
        int64_t blockTime   = static_cast<int64_t>(tx.header().timeMilli.getAsLong());

        auto bytes = tx.serialise();

        sqlite3_bind_text(stmtInsert_, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmtInsert_, 2, blockNum);
        sqlite3_bind_text(stmtInsert_, 3, parentId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmtInsert_, 4, blockTime);
        sqlite3_bind_blob(stmtInsert_, 5, bytes.data(), (int)bytes.size(), SQLITE_TRANSIENT);
        db_.stepOnce(stmtInsert_);
    }

    /** Retrieve by txpow_id hex string. */
    std::optional<TxBlock> getById(const std::string& txpowId) {
        sqlite3_bind_text(stmtGetById_, 1, txpowId.c_str(), -1, SQLITE_TRANSIENT);
        std::optional<TxBlock> result;
        db_.step(stmtGetById_, [&](sqlite3_stmt* s) {
            result = blobToBlock(s, 0);
        });
        return result;
    }

    /** Get block at a specific block number (main chain). */
    std::optional<TxBlock> getByNumber(int64_t blockNumber) {
        sqlite3_bind_int64(stmtGetByNumber_, 1, blockNumber);
        std::optional<TxBlock> result;
        db_.step(stmtGetByNumber_, [&](sqlite3_stmt* s) {
            result = blobToBlock(s, 0);
        });
        return result;
    }

    /** Get the block with the highest block_number (chain tip). */
    std::optional<TxBlock> getTip() {
        std::optional<TxBlock> result;
        db_.step(stmtGetTip_, [&](sqlite3_stmt* s) {
            result = blobToBlock(s, 0);
        });
        return result;
    }

    /** Get blocks in [fromBlock, toBlock] range, ordered ascending. */
    std::vector<TxBlock> getRange(int64_t fromBlock, int64_t toBlock) {
        sqlite3_bind_int64(stmtGetRange_, 1, fromBlock);
        sqlite3_bind_int64(stmtGetRange_, 2, toBlock);
        std::vector<TxBlock> blocks;
        db_.step(stmtGetRange_, [&](sqlite3_stmt* s) {
            blocks.push_back(blobToBlock(s, 0));
        });
        return blocks;
    }

    /** Delete block by txpow_id. */
    void remove(const std::string& txpowId) {
        sqlite3_bind_text(stmtDelete_, 1, txpowId.c_str(), -1, SQLITE_TRANSIENT);
        db_.stepOnce(stmtDelete_);
    }

    /** Total number of blocks stored. */
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
            CREATE TABLE IF NOT EXISTS blocks (
                txpow_id     TEXT PRIMARY KEY,
                block_number INTEGER NOT NULL,
                parent_id    TEXT NOT NULL,
                block_time   INTEGER NOT NULL,
                serialized   BLOB NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_blocks_number ON blocks(block_number);
            CREATE INDEX IF NOT EXISTS idx_blocks_parent ON blocks(parent_id);
        )");
    }

    void prepareStatements() {
        stmtInsert_ = db_.prepare(
            "INSERT OR REPLACE INTO blocks(txpow_id, block_number, parent_id, block_time, serialized)"
            " VALUES(?,?,?,?,?);");
        stmtGetById_ = db_.prepare(
            "SELECT serialized FROM blocks WHERE txpow_id=? LIMIT 1;");
        stmtGetByNumber_ = db_.prepare(
            "SELECT serialized FROM blocks WHERE block_number=? ORDER BY block_time DESC LIMIT 1;");
        stmtGetTip_ = db_.prepare(
            "SELECT serialized FROM blocks ORDER BY block_number DESC LIMIT 1;");
        stmtGetRange_ = db_.prepare(
            "SELECT serialized FROM blocks WHERE block_number>=? AND block_number<=?"
            " ORDER BY block_number ASC;");
        stmtDelete_ = db_.prepare(
            "DELETE FROM blocks WHERE txpow_id=?;");
        stmtCount_ = db_.prepare(
            "SELECT COUNT(*) FROM blocks;");
    }

    TxBlock blobToBlock(sqlite3_stmt* s, int col) {
        const void* blob = sqlite3_column_blob(s, col);
        int sz = sqlite3_column_bytes(s, col);
        std::vector<uint8_t> bytes(
            static_cast<const uint8_t*>(blob),
            static_cast<const uint8_t*>(blob) + sz);
        return TxBlock(TxPoW::deserialise(bytes));
    }

    Database& db_;
    sqlite3_stmt* stmtInsert_      = nullptr;
    sqlite3_stmt* stmtGetById_     = nullptr;
    sqlite3_stmt* stmtGetByNumber_ = nullptr;
    sqlite3_stmt* stmtGetTip_      = nullptr;
    sqlite3_stmt* stmtGetRange_    = nullptr;
    sqlite3_stmt* stmtDelete_      = nullptr;
    sqlite3_stmt* stmtCount_       = nullptr;
};

} // namespace minima::persistence
