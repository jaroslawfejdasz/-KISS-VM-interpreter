/**
 * minima-core-cpp — main entry point
 *
 * Spins up a full Minima node:
 *   1. ChainProcessor  — validates + stores blocks
 *   2. SQLite persist  — BlockStore / UTxOSet
 *   3. MiningManager   — continuous block production
 *   4. P2PSync         — connects to seed node, IBD, flood-fill
 *
 * Usage:
 *   ./minima [--seed <host:port>] [--difficulty <0-32>] [--no-mine] [--db <path>]
 */

#include "chain/ChainProcessor.hpp"
#include "chain/UTxOSet.hpp"
#include "mining/MiningManager.hpp"
#include "network/P2PSync.hpp"
#include "network/NIOClient.hpp"
#include "objects/Greeting.hpp"
#include "persistence/Database.hpp"
#include "persistence/BlockStore.hpp"
#include "persistence/UTxOStore.hpp"
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

// ── global shutdown flag ──────────────────────────────────────────────────
static std::atomic<bool> g_shutdown{false};

static void sigHandler(int) {
    std::cout << "\n[node] Shutting down...\n";
    g_shutdown = true;
}

// ── CLI args ──────────────────────────────────────────────────────────────
struct NodeConfig {
    std::string seedHost   = "145.40.90.17";
    uint16_t    seedPort   = 9001;
    int         difficulty = 3;    // leading zero bytes — real mainnet ~3-6
    bool        mine       = true;
    std::string dbPath     = "minima.db";
};

static NodeConfig parseArgs(int argc, char* argv[]) {
    NodeConfig cfg;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--no-mine") == 0) {
            cfg.mine = false;
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            std::string s = argv[++i];
            auto colon = s.rfind(':');
            if (colon != std::string::npos) {
                cfg.seedHost = s.substr(0, colon);
                cfg.seedPort = static_cast<uint16_t>(std::stoi(s.substr(colon + 1)));
            } else {
                cfg.seedHost = s;
            }
        } else if (std::strcmp(argv[i], "--difficulty") == 0 && i + 1 < argc) {
            cfg.difficulty = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            cfg.dbPath = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout <<
                "Usage: minima [options]\n"
                "  --seed <host:port>    Seed node (default: 145.40.90.17:9001)\n"
                "  --difficulty <n>      Leading zero bytes 0-32 (default: 3)\n"
                "  --no-mine             Disable mining (sync-only mode)\n"
                "  --db <path>           SQLite DB path (default: minima.db)\n";
            std::exit(0);
        }
    }
    return cfg;
}

// ── logger ────────────────────────────────────────────────────────────────
static void log(const std::string& tag, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()).count();
    std::cout << "[" << ms << "][" << tag << "] " << msg << "\n";
}

// ── main ──────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    NodeConfig cfg = parseArgs(argc, argv);

    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    log("node", "Starting minima-core-cpp node");
    log("node", "Seed:       " + cfg.seedHost + ":" + std::to_string(cfg.seedPort));
    log("node", "Difficulty: " + std::to_string(cfg.difficulty) + " leading zeros");
    log("node", "Mining:     " + std::string(cfg.mine ? "enabled" : "disabled"));
    log("node", "DB:         " + cfg.dbPath);

    // ── persistence ───────────────────────────────────────────────────────
    minima::persistence::Database db(cfg.dbPath);  // opens in ctor
    minima::persistence::BlockStore blockStore(db);   // creates table in ctor
    minima::persistence::UTxOStore  utxoStore(db);    // creates table in ctor
    log("db", "SQLite opened: " + cfg.dbPath);

    // ── chain ─────────────────────────────────────────────────────────────
    minima::chain::ChainProcessor chain;
    minima::chain::UTxOSet        utxo;

    // Restore tip from DB
    int64_t storedHeight = blockStore.count();
    if (storedHeight > 0) {
        log("chain", "Restoring from DB at height " + std::to_string(storedHeight));
        // TODO: replay blocks from blockStore into chain
    }
    log("chain", "Chain height: " + std::to_string(chain.getHeight()));

    // ── miner ─────────────────────────────────────────────────────────────
    std::unique_ptr<minima::mining::MiningManager> miner;
    if (cfg.mine) {
        minima::mining::MiningManager::Config mcfg;
        mcfg.defaultLeadingZeros = cfg.difficulty;
        mcfg.continuous          = true;
        mcfg.checkStopEvery      = 4096;

        miner = std::make_unique<minima::mining::MiningManager>(chain, mcfg);
        miner->setLogger([](const std::string& m){ log("miner", m); });
        miner->onMined([&](const minima::TxPoW& blk) {
            int64_t bn = blk.header().blockNumber.getAsLong();
            log("miner", "Block #" + std::to_string(bn) + " mined — saving to DB");
            blockStore.put(minima::TxBlock(blk));
        });
        miner->start();
    }

    // ── P2P sync (separate thread) ────────────────────────────────────────
    minima::network::P2PSync::Config pcfg;
    pcfg.host         = cfg.seedHost;
    pcfg.port         = cfg.seedPort;
    pcfg.recvTimeout  = 30000;
    pcfg.maxRetries   = 10;
    pcfg.ibdBatchSize = 500;

    minima::network::P2PSync p2p(pcfg, chain, utxo);
    p2p.setLogger([](const std::string& m){ log("p2p", m); });

    p2p.setLocalTip(chain.getHeight());

    p2p.onBlock([&](const minima::TxPoW& blk) {
        int64_t bn = blk.header().blockNumber.getAsLong();
        log("p2p", "New block from peer: #" + std::to_string(bn));
        blockStore.put(minima::TxBlock(blk));
        if (miner) miner->notifyNewTip();
    });

    p2p.onTx([&](const minima::TxPoW& tx) {
        log("p2p", "New tx received (id=" +
            tx.computeID().toHexString().substr(0, 16) + "...)");
    });

    p2p.onSyncDone([&](int64_t tipBlock) {
        log("p2p", "IBD complete — tip=" + std::to_string(tipBlock));
        p2p.setLocalTip(tipBlock);
    });

    std::thread p2pThread([&]() {
        p2p.run();
    });

    // ── main loop — status every 30s ──────────────────────────────────────
    log("node", "Node running. Press Ctrl+C to stop.");
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (g_shutdown) break;

        int64_t h = chain.getHeight();
        std::string minerStats = miner
            ? ("  hashes=" + std::to_string(miner->totalHashes()) +
               "  blocks=" + std::to_string(miner->blocksFound()))
            : "  mining=off";
        log("status", "height=" + std::to_string(h) +
                      "  p2pTip=" + std::to_string(p2p.peerTipBlock()) +
                      minerStats);
    }

    // ── shutdown ──────────────────────────────────────────────────────────
    log("node", "Stopping P2P...");
    p2p.stop();
    if (p2pThread.joinable()) p2pThread.join();

    if (miner) {
        log("node", "Stopping miner...");
        miner->stop();
    }

    // db closes via RAII
    log("node", "Clean shutdown. Goodbye.");
    return 0;
}
