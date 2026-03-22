/**
 * minima-core-cpp — main entry point
 *
 * Uruchamia pełny węzeł Minima:
 *   1. MinimaDB       — centralny God Object (TxPowTree, BlockStore, MMR, Mempool, Wallet)
 *   2. Genesis init   — ładuje/generuje blok #0
 *   3. TxPoWProcessor — asynchroniczny procesor bloków i transakcji
 *   4. TxPoWGenerator — składa nowy blok do wyminowania
 *   5. MiningManager  — ciągłe wydobycie (opcjonalne)
 *   6. P2PSync        — IBD + flood-fill z siecią
 *   7. SQLite persist — BlockStore / UTxOSet na dysku
 *
 * Usage:
 *   ./minima_node [--seed <host:port>] [--difficulty <0-32>] [--no-mine] [--db <path>]
 */
#include "database/MinimaDB.hpp"
#include "objects/Genesis.hpp"
#include "system/MessageProcessor.hpp"
#include "system/TxPoWProcessor.hpp"
#include "system/TxPoWGenerator.hpp"
#include "system/TxPoWSearcher.hpp"
#include "mining/MiningManager.hpp"
#include "chain/ChainProcessor.hpp"
#include "chain/UTxOSet.hpp"
#include "network/P2PSync.hpp"
#include "persistence/Database.hpp"
#include "persistence/BlockStore.hpp"
#include "persistence/UTxOStore.hpp"
#include "crypto/BIP39.hpp"
#include "types/MiniNumber.hpp"
#include "types/MiniData.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>
#include <sstream>
#include <optional>

// ── global shutdown flag ──────────────────────────────────────────────────
static std::atomic<bool> g_shutdown{false};
static void sigHandler(int) {
    std::cout << "\n[node] Shutting down...\n";
    g_shutdown = true;
}

// ── CLI config ────────────────────────────────────────────────────────────
struct NodeConfig {
    std::string seedHost    = "145.40.90.17";
    uint16_t    seedPort    = 9001;
    int         difficulty  = 3;
    bool        mine        = true;
    std::string dbPath      = "minima.db";
    bool        verbose     = false;
};

static NodeConfig parseArgs(int argc, char* argv[]) {
    NodeConfig cfg;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--no-mine"))         { cfg.mine = false; }
        else if (!std::strcmp(argv[i], "--verbose"))    { cfg.verbose = true; }
        else if (!std::strcmp(argv[i], "--seed") && i+1<argc) {
            std::string s = argv[++i];
            auto c = s.rfind(':');
            cfg.seedHost = (c!=std::string::npos) ? s.substr(0,c) : s;
            if (c!=std::string::npos) cfg.seedPort = static_cast<uint16_t>(std::stoi(s.substr(c+1)));
        }
        else if (!std::strcmp(argv[i], "--difficulty") && i+1<argc) cfg.difficulty = std::stoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--db") && i+1<argc)         cfg.dbPath     = argv[++i];
        else if (!std::strcmp(argv[i],"--help") || !std::strcmp(argv[i],"-h")) {
            std::cout <<
                "Usage: minima_node [options]\n"
                "  --seed <host:port>    Seed node (default: 145.40.90.17:9001)\n"
                "  --difficulty <n>      Leading zero bytes 0-32 (default: 3)\n"
                "  --no-mine             Disable mining (sync-only)\n"
                "  --db <path>           SQLite DB path (default: minima.db)\n"
                "  --verbose             Extra logging\n";
            std::exit(0);
        }
    }
    return cfg;
}

// ── logger ────────────────────────────────────────────────────────────────
static bool g_verbose = false;
static void log(const std::string& tag, const std::string& msg) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "[" << ms << "][" << tag << "] " << msg << "\n";
}
static void logv(const std::string& tag, const std::string& msg) {
    if (g_verbose) log(tag, msg);
}

// ── Genesis bootstrap ─────────────────────────────────────────────────────
/**
 * Załaduj genesis z DB (jeśli istnieje) lub zainicjalizuj nowy.
 * Zwraca genesis TxPoW który jest zawsze pierwszym blokiem.
 */
static bool bootstrapGenesis(minima::MinimaDB& db,
                              minima::persistence::BlockStore& sqlStore)
{
    // Sprawdź czy mamy już genesis w SQLite
    if (sqlStore.count() > 0) {
        log("genesis", "Restoring from DB (" + std::to_string(sqlStore.count()) + " blocks)");
        // Załaduj wszystkie bloki z SQLite do TxPowTree (replay)
        auto allBlocks = sqlStore.getRange(0, sqlStore.count());
        int loaded = 0;
        for (auto& tb : allBlocks) {
            if (db.addBlock(tb.txpow())) ++loaded;
        }
        log("genesis", "Replayed " + std::to_string(loaded) + " blocks from DB");
        return true;
    }

    // Twórz genesis od zera
    minima::TxPoW genesis = minima::makeGenesisTxPoW();
    minima::MiniData gid = genesis.computeID();
    log("genesis", "Creating genesis block, id=" + gid.toHexString().substr(0,16) + "...");

    db.addBlock(genesis);
    sqlStore.put(minima::TxBlock(genesis));
    log("genesis", "Genesis #0 stored OK, MMR root=" +
        genesis.header().mmrRoot.toHexString().substr(0,12) + "...");
    return true;
}

// ── main ──────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    NodeConfig cfg = parseArgs(argc, argv);
    g_verbose = cfg.verbose;

    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    log("node", "============================================================");
    log("node", "  minima-core-cpp  |  Minima edge node");
    log("node", "============================================================");
    log("node", "Seed:       " + cfg.seedHost + ":" + std::to_string(cfg.seedPort));
    log("node", "Difficulty: " + std::to_string(cfg.difficulty) + " leading zeros");
    log("node", "Mining:     " + std::string(cfg.mine ? "enabled" : "disabled"));
    log("node", "DB:         " + cfg.dbPath);

    // ── 1. Persistence (SQLite) ───────────────────────────────────────────
    minima::persistence::Database   sqlDb(cfg.dbPath);
    minima::persistence::BlockStore sqlBlockStore(sqlDb);
    minima::persistence::UTxOStore  sqlUtxoStore(sqlDb);
    log("db", "SQLite opened: " + cfg.dbPath);

    // ── 2. MinimaDB (in-memory God Object) ───────────────────────────────
    minima::MinimaDB db;

    // ── 3. System layer ───────────────────────────────────────────────────
    minima::system::TxPoWProcessor  processor(db);
    minima::system::TxPoWGenerator  generator(db.txPowTree(), db.mempool());
    minima::system::TxPoWSearcher   searcher(db.txPowTree(), db.blockStore());

    // Callback: gdy blok zaakceptowany → zapisz do SQLite
    processor.onBlockAccepted([&](const minima::TxPoW& blk) {
        sqlBlockStore.put(minima::TxBlock(blk));
        logv("processor", "Block #" +
            std::to_string(blk.header().blockNumber.getAsLong()) + " saved to SQLite");
    });

    // Callback: gdy TX zaakceptowany → do mempoola (już jest, ale logujemy)
    processor.onTxAccepted([&](const minima::TxPoW& tx) {
        logv("processor", "TX " + tx.computeID().toHexString().substr(0,12) + "... in mempool");
    });

    processor.start();
    log("system", "TxPoWProcessor started");

    // ── 4. Genesis / DB restore ───────────────────────────────────────────
    bootstrapGenesis(db, sqlBlockStore);
    log("chain", "Current height: " + std::to_string(db.currentHeight()));

    // ── 5. Miner ──────────────────────────────────────────────────────────
    // Używamy nowego ChainProcessor (wrapper na stary interfejs miner-a)
    minima::chain::ChainProcessor legacyChain;  // miner wymaga ChainProcessor
    minima::chain::UTxOSet        legacyUtxo;

    std::unique_ptr<minima::mining::MiningManager> miner;
    if (cfg.mine) {
        minima::mining::MiningManager::Config mcfg;
        mcfg.defaultLeadingZeros = cfg.difficulty;
        mcfg.continuous          = true;
        mcfg.checkStopEvery      = 4096;

        miner = std::make_unique<minima::mining::MiningManager>(legacyChain, mcfg);
        miner->setLogger([](const std::string& m){ logv("miner", m); });

        miner->onMined([&](const minima::TxPoW& blk) {
            int64_t bn = blk.header().blockNumber.getAsLong();
            log("miner", "Block #" + std::to_string(bn) + " mined — submitting to processor");
            // Przekaż przez TxPoWProcessor (single source of truth)
            processor.submitTxPoW(blk, [bn](const minima::MiniData&,
                                             minima::system::ProcessResult r) {
                log("processor", "Mined block #" + std::to_string(bn) +
                    " → " + minima::system::to_string(r));
            });
        });

        miner->start();
        log("miner", "Mining started (difficulty=" + std::to_string(cfg.difficulty) + ")");
    }

    // ── 6. P2P Sync ───────────────────────────────────────────────────────
    minima::network::P2PSync::Config pcfg;
    pcfg.host         = cfg.seedHost;
    pcfg.port         = cfg.seedPort;
    pcfg.recvTimeout  = 30000;
    pcfg.maxRetries   = 10;
    pcfg.ibdBatchSize = 500;

    minima::network::P2PSync p2p(pcfg, legacyChain, legacyUtxo);
    p2p.setLogger([](const std::string& m){ log("p2p", m); });
    p2p.setLocalTip(db.currentHeight());

    // Nowy blok z sieci → przez TxPoWProcessor
    p2p.onBlock([&](const minima::TxPoW& blk) {
        int64_t bn = blk.header().blockNumber.getAsLong();
        log("p2p", "Block #" + std::to_string(bn) + " from peer — submitting");
        processor.submitTxPoW(blk, [bn](const minima::MiniData&,
                                         minima::system::ProcessResult r) {
            log("processor", "Peer block #" + std::to_string(bn) +
                " → " + minima::system::to_string(r));
        });
        if (miner) miner->notifyNewTip();
    });

    p2p.onTx([&](const minima::TxPoW& tx) {
        processor.submitTxPoW(tx); // TX do mempoola
    });

    p2p.onSyncDone([&](int64_t tipBlock) {
        log("p2p", "IBD done — tip=" + std::to_string(tipBlock));
        p2p.setLocalTip(tipBlock);
        if (miner) {
            // Wygeneruj nowy blok na podstawie aktualnego tipu
            auto candidate = generator.generateTxPoW();
            miner->setNextBlock(candidate);
        }
    });

    std::thread p2pThread([&]{ p2p.run(); });
    log("p2p", "P2P started → " + cfg.seedHost + ":" + std::to_string(cfg.seedPort));

    // ── 7. Wallet info ────────────────────────────────────────────────────
    {
        auto& wallet = db.wallet();
        if (wallet.addressCount() == 0) {
            wallet.createAddress();
            log("wallet", "Created default address: " +
                wallet.defaultAddress().toHexString().substr(0,16) + "...");
        } else {
            log("wallet", "Wallet has " + std::to_string(wallet.addressCount()) +
                " address(es), default: " +
                wallet.defaultAddress().toHexString().substr(0,16) + "...");
        }
    }

    // ── 8. Status loop ────────────────────────────────────────────────────
    log("node", "Node running. Press Ctrl+C to stop.");
    int64_t lastHeight = -1;
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (g_shutdown) break;

        int64_t h    = db.currentHeight();
        int64_t peer = p2p.peerTipBlock();

        if (h != lastHeight || peer > h) {
            std::string gap = (peer > h) ? (" [" + std::to_string(peer - h) + " behind]") : "";
            std::string minerStats = miner
                ? ("  hashes=" + std::to_string(miner->totalHashes()) +
                   "  found="  + std::to_string(miner->blocksFound()))
                : "  mining=off";
            std::string mpool = "  mempool=" + std::to_string(db.mempool().size());
            log("status", "height=" + std::to_string(h) +
                          "  peer=" + std::to_string(peer) + gap +
                          minerStats + mpool);
            lastHeight = h;
        }
    }

    // ── Shutdown ──────────────────────────────────────────────────────────
    log("node", "Stopping...");
    processor.stop();
    p2p.stop();
    if (p2pThread.joinable()) p2pThread.join();
    if (miner) miner->stop();

    log("node", "Final height: " + std::to_string(db.currentHeight()));
    log("node", "SQLite blocks: " + std::to_string(sqlBlockStore.count()));
    log("node", "Clean shutdown. Goodbye.");
    return 0;
}
