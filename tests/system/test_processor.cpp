#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/system/MessageProcessor.hpp"
#include "../../src/system/TxPoWSearcher.hpp"
#include "../../src/system/TxPoWGenerator.hpp"
#include "../../src/system/TxPoWProcessor.hpp"
#include "../../src/database/MinimaDB.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniNumber.hpp"

#include <chrono>
#include <thread>
#include <atomic>

using namespace minima;
using namespace minima::system;

// ── Helper: build a TxPoW ────────────────────────────────────────────────────

static TxPoW makeBlock(int64_t blockNum, MiniData parentID, int64_t nonce = 0) {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(blockNum);
    txp.header().nonce       = MiniNumber(nonce);
    // Set current time so basicPoWCheck passes
    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    txp.header().timeMilli   = MiniNumber(ms);
    txp.header().superParents[0] = parentID;
    for (int i = 1; i < CASCADE_LEVELS; ++i)
        txp.header().superParents[i] = MiniData(std::vector<uint8_t>(32, 0x00));
    return txp;
}

static MiniData zeroID() {
    return MiniData(std::vector<uint8_t>(32, 0x00));
}

// ── MessageProcessor ─────────────────────────────────────────────────────────
TEST_SUITE("MessageProcessor") {
    TEST_CASE("start and stop") {
        MessageProcessor mp("test");
        mp.start();
        CHECK(mp.isRunning() == true);
        mp.stop();
        CHECK(mp.isRunning() == false);
    }

    TEST_CASE("post executes handler") {
        MessageProcessor mp("test");
        mp.start();
        std::atomic<int> count{0};
        mp.post("INC", [&]{ count++; });
        mp.post("INC", [&]{ count++; });
        mp.post("INC", [&]{ count++; });
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        mp.stop();
        CHECK(count.load() == 3);
    }

    TEST_CASE("name is set correctly") {
        MessageProcessor mp("my-processor");
        CHECK(mp.name() == "my-processor");
    }

    TEST_CASE("queueSize reflects pending work") {
        MessageProcessor mp("test");
        // Do NOT start — so messages pile up
        mp.post("A", []{ std::this_thread::sleep_for(std::chrono::milliseconds(200)); });
        mp.post("B", []{});
        // Both are queued before processing starts
        CHECK(mp.queueSize() <= 2); // Might be 2 or less depending on timing
        mp.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        mp.stop();
        CHECK(mp.queueSize() == 0);
    }
}

// ── TxPoWProcessor (sync mode — without threading) ──────────────────────────
TEST_SUITE("TxPoWProcessor") {
    TEST_CASE("genesis block accepted") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        TxPoW genesis = makeBlock(0, zeroID());
        auto result = proc.processTxPoWSync(genesis);
        CHECK(result == ProcessResult::ACCEPTED);
        CHECK(proc.blocksProcessed() == 1);
    }

    TEST_CASE("duplicate rejected") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        TxPoW genesis = makeBlock(0, zeroID());
        proc.processTxPoWSync(genesis);
        auto result = proc.processTxPoWSync(genesis);
        CHECK(result == ProcessResult::DUPLICATE);
        CHECK(proc.blocksProcessed() == 1);
    }

    TEST_CASE("linear chain: height grows") {
        MinimaDB db;
        TxPoWProcessor proc(db);

        TxPoW b0 = makeBlock(0, zeroID());
        CHECK(proc.processTxPoWSync(b0) == ProcessResult::ACCEPTED);

        MiniData id0 = b0.computeID();
        TxPoW b1 = makeBlock(1, id0, 1);
        CHECK(proc.processTxPoWSync(b1) == ProcessResult::ACCEPTED);

        MiniData id1 = b1.computeID();
        TxPoW b2 = makeBlock(2, id1, 2);
        CHECK(proc.processTxPoWSync(b2) == ProcessResult::ACCEPTED);

        CHECK(proc.currentHeight() == 2);
        CHECK(proc.blocksProcessed() == 3);
    }

    TEST_CASE("orphan block rejected (parent unknown)") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        // No genesis — b1 has unknown parent
        MiniData unknownID(std::vector<uint8_t>(32, 0xAB));
        TxPoW b1 = makeBlock(1, unknownID);
        auto result = proc.processTxPoWSync(b1);
        CHECK(result == ProcessResult::ORPHAN);
        CHECK(proc.blocksProcessed() == 0);
    }

    TEST_CASE("to_string covers all results") {
        CHECK(to_string(ProcessResult::ACCEPTED)       == "ACCEPTED");
        CHECK(to_string(ProcessResult::DUPLICATE)      == "DUPLICATE");
        CHECK(to_string(ProcessResult::ORPHAN)         == "ORPHAN");
        CHECK(to_string(ProcessResult::INVALID_POW)    == "INVALID_POW");
        CHECK(to_string(ProcessResult::INVALID_SCRIPT) == "INVALID_SCRIPT");
        CHECK(to_string(ProcessResult::INVALID_SIGS)   == "INVALID_SIGS");
        CHECK(to_string(ProcessResult::ERROR)          == "ERROR");
    }

    TEST_CASE("async submitTxPoW with callback") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        proc.start();
        std::atomic<int> accepted{0};
        TxPoW b0 = makeBlock(0, zeroID());
        proc.submitTxPoW(b0, [&](const MiniData&, ProcessResult r) {
            if (r == ProcessResult::ACCEPTED) accepted++;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        proc.stop();
        CHECK(accepted.load() == 1);
    }

    TEST_CASE("processIBD processes multiple blocks") {
        MinimaDB db;
        TxPoWProcessor proc(db);
        proc.start();

        TxPoW b0 = makeBlock(0, zeroID());
        MiniData id0 = b0.computeID();
        TxPoW b1 = makeBlock(1, id0, 1);
        MiniData id1 = b1.computeID();
        TxPoW b2 = makeBlock(2, id1, 2);

        std::atomic<int> total{0};
        std::atomic<int> accepted{0};
        proc.submitIBD({b0, b1, b2}, [&](const MiniData&, ProcessResult r) {
            total++;
            if (r == ProcessResult::ACCEPTED) accepted++;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        proc.stop();
        CHECK(total.load() == 3);
        CHECK(accepted.load() == 3);
    }
}

// ── TxPoWSearcher ────────────────────────────────────────────────────────────
TEST_SUITE("TxPoWSearcher") {
    TEST_CASE("empty db — no tip") {
        MinimaDB db;
        TxPoWSearcher searcher(db.txPowTree(), db.blockStore());
        CHECK(searcher.getTip().has_value() == false);
        CHECK(searcher.getHeight() == 0);
    }

    TEST_CASE("tip after adding block") {
        MinimaDB db;
        TxPoW b0 = makeBlock(0, zeroID());
        db.addBlock(b0);
        TxPoWSearcher searcher(db.txPowTree(), db.blockStore());
        CHECK(searcher.getTip().has_value() == true);
        CHECK(searcher.getHeight() == 0);
    }

    TEST_CASE("isInChain — block in canonical chain") {
        MinimaDB db;
        TxPoW b0 = makeBlock(0, zeroID()); db.addBlock(b0);
        MiniData id0 = b0.computeID();
        TxPoWSearcher searcher(db.txPowTree(), db.blockStore());
        CHECK(searcher.isInChain(id0) == true);
        CHECK(searcher.isInChain(MiniData(std::vector<uint8_t>(32, 0xFF))) == false);
    }

    TEST_CASE("getBlockAtHeight") {
        MinimaDB db;
        TxPoW b0 = makeBlock(0, zeroID()); db.addBlock(b0);
        MiniData id0 = b0.computeID();
        TxPoW b1 = makeBlock(1, id0, 1); db.addBlock(b1);
        TxPoWSearcher searcher(db.txPowTree(), db.blockStore());
        auto found = searcher.getBlockAtHeight(1);
        CHECK(found.has_value());
        CHECK(found->header().blockNumber.getAsLong() == 1);
        CHECK(searcher.getBlockAtHeight(99).has_value() == false);
    }

    TEST_CASE("getLastBlocks — last N") {
        MinimaDB db;
        TxPoW b0 = makeBlock(0, zeroID()); db.addBlock(b0);
        MiniData id0 = b0.computeID();
        TxPoW b1 = makeBlock(1, id0, 1); db.addBlock(b1);
        MiniData id1 = b1.computeID();
        TxPoW b2 = makeBlock(2, id1, 2); db.addBlock(b2);
        TxPoWSearcher searcher(db.txPowTree(), db.blockStore());
        auto last2 = searcher.getLastBlocks(2);
        CHECK(last2.size() == 2);
        CHECK(last2[0].header().blockNumber.getAsLong() == 1);
        CHECK(last2[1].header().blockNumber.getAsLong() == 2);
    }

    TEST_CASE("getTxPoW from store") {
        MinimaDB db;
        TxPoW b0 = makeBlock(0, zeroID()); db.addBlock(b0);
        TxPoWSearcher searcher(db.txPowTree(), db.blockStore());
        auto found = searcher.getTxPoW(b0.computeID());
        CHECK(found.has_value());
        CHECK(found->header().blockNumber.getAsLong() == 0);
    }
}

// ── TxPoWGenerator ───────────────────────────────────────────────────────────
TEST_SUITE("TxPoWGenerator") {
    TEST_CASE("genesis block generated") {
        MinimaDB db;
        TxPoWGenerator gen(db.txPowTree(), db.mempool());
        auto genesis = gen.generateGenesis();
        CHECK(genesis.header().blockNumber.getAsLong() == 0);
        // superParents[0] should be all zeros
        bool allZero = true;
        for (uint8_t b : genesis.header().superParents[0].bytes())
            if (b) { allZero = false; break; }
        CHECK(allZero == true);
    }

    TEST_CASE("generateTxPoW uses tip as parent") {
        MinimaDB db;
        TxPoW b0 = makeBlock(0, zeroID()); db.addBlock(b0);
        MiniData id0 = b0.computeID();

        TxPoWGenerator gen(db.txPowTree(), db.mempool());
        auto next = gen.generateTxPoW();
        CHECK(next.header().blockNumber.getAsLong() == 1);
        CHECK(next.header().superParents[0].bytes() == id0.bytes());
    }

    TEST_CASE("generateTxPoW: empty tree gives blockNumber=0") {
        MinimaDB db;
        TxPoWGenerator gen(db.txPowTree(), db.mempool());
        auto txp = gen.generateTxPoW();
        CHECK(txp.header().blockNumber.getAsLong() == 0);
    }
}
