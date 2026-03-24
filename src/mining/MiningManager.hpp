#pragma once
/**
 * MiningManager — continuous Minima block mining loop.
 *
 * Java ref: org.minima.system.brains.TxPoWMiner
 *
 * Responsibilities:
 *  1. Build a new TxPoW template from the current chain tip
 *  2. Mine in a background thread (increment nonce until isBlock())
 *  3. On success → add to ChainProcessor + broadcast via P2PSyncBroadcast
 *  4. Roll prng on each new template (prevents duplicate work)
 *  5. Restart when chain tip advances (new block received from P2P)
 *
 * Usage:
 *   MiningManager miner(chain, utxo, difficulty);
 *   miner.onBlock([](const TxPoW& blk) { broadcast(blk); });
 *   miner.start();
 *   // ... later ...
 *   miner.stop();
 */

#include "TxPoWMiner.hpp"
#include "../objects/TxPoW.hpp"
#include "../objects/TxBody.hpp"
#include "../objects/TxHeader.hpp"
#include "../chain/ChainProcessor.hpp"
#include "../chain/DifficultyAdjust.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../crypto/Hash.hpp"

#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <cstring>

namespace minima {
namespace mining {

using OnMinedCallback = std::function<void(const TxPoW&)>;

class MiningManager {
public:
    struct Config {
        int      defaultLeadingZeros;
        uint64_t checkStopEvery;
        bool     continuous;
        Config() : defaultLeadingZeros(0), checkStopEvery(4096), continuous(true) {}
    };

    MiningManager(chain::ChainProcessor& chain, Config cfg = {})
        : chain_(chain)
        , cfg_(std::move(cfg))
    {}

    ~MiningManager() { stop(); }

    void onMined(OnMinedCallback cb) { onMined_ = std::move(cb); }

    void setLogger(std::function<void(const std::string&)> l) { logger_ = std::move(l); }

    // ── start / stop ──────────────────────────────────────────────────────

    void start() {
        if (running_) return;
        running_ = true;
        stopFlag_ = false;
        thread_ = std::thread(&MiningManager::mineLoop, this);
        log("Mining started.");
    }

    void stop() {
        if (!running_) return;
        stopFlag_ = true;
        if (thread_.joinable()) thread_.join();
        running_ = false;
        log("Mining stopped.");
    }

    bool isRunning() const { return running_; }

    // Call this when a new block arrives from P2P — restarts mine loop
    // with a fresh template based on the new tip.
    void notifyNewTip() {
        tipChanged_ = true;
        log("Tip changed — rebuilding template.");
    }

    // Set a pre-built template (from TxPoWGenerator) as the next block to mine
    void setNextBlock(const TxPoW& tmpl) {
        std::lock_guard<std::mutex> lock(nextBlockMu_);
        m_nextBlock = tmpl;
        m_hasNextBlock = true;
        tipChanged_ = true; // trigger template rebuild in mine loop
    }

    // Stats
    uint64_t totalHashes()  const { return totalHashes_; }
    uint64_t blocksFound()  const { return blocksFound_; }

private:
    chain::ChainProcessor& chain_;
    Config                 cfg_;

    std::thread            thread_;
    std::atomic<bool>      running_  {false};
    std::atomic<bool>      stopFlag_ {false};
    std::atomic<bool>      tipChanged_{false};

    std::atomic<uint64_t>  totalHashes_{0};
    std::atomic<uint64_t>  blocksFound_{0};

    OnMinedCallback        onMined_;
    std::mutex             nextBlockMu_;
    TxPoW                  m_nextBlock;
    bool                   m_hasNextBlock{false};
    std::function<void(const std::string&)> logger_;

    // ── mining loop ───────────────────────────────────────────────────────

    void mineLoop() {
        while (!stopFlag_) {
            TxPoW tmpl = buildTemplate();
            bool found = mine(tmpl);

            if (found) {
                blocksFound_++;
                int64_t bn = tmpl.header().blockNumber.getAsLong();
                log("⛏  Block found! #" + std::to_string(bn) +
                    "  nonce=" + std::to_string(tmpl.header().nonce.getAsLong()));

                chain_.processBlock(tmpl);

                if (onMined_) onMined_(tmpl);
            }

            if (!cfg_.continuous) break;
        }
    }

    // Mine a single template — returns true if block found, false if stopped
    bool mine(TxPoW& tmpl) {
        uint64_t n = 0;
        while (!stopFlag_) {
            // Roll prng + set nonce
            tmpl.header().nonce = MiniNumber(static_cast<int64_t>(n));

            if (tmpl.isBlock()) {
                totalHashes_ += n + 1;
                return true;
            }

            n++;
            totalHashes_++;

            // Check if tip changed — rebuild template
            if ((n & (cfg_.checkStopEvery - 1)) == 0) {
                if (tipChanged_.exchange(false)) {
                    log("  tip changed at n=" + std::to_string(n) + " — rebuilding");
                    return false;  // caller will rebuild
                }
            }
        }
        return false;
    }

    // ── template builder ──────────────────────────────────────────────────

    TxPoW buildTemplate() {
        // If TxPoWGenerator provided a template, use it
        {
            std::lock_guard<std::mutex> lock(nextBlockMu_);
            if (m_hasNextBlock) {
                m_hasNextBlock = false;
                return m_nextBlock;
            }
        }
        TxPoW tmpl;

        int64_t currentHeight = chain_.getHeight();
        int64_t newHeight     = currentHeight + 1;

        // Block number
        tmpl.header().blockNumber = MiniNumber(newHeight);

        // Timestamp (ms since epoch)
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        tmpl.header().timeMilli = MiniNumber(static_cast<int64_t>(now));

        // Parent hash as first superParent slot
        if (currentHeight >= 0) {
            MiniData tip = chain_.getTip();
            tmpl.header().superParents[0] = tip;
        }

        // Difficulty
        MiniData diff = computeDifficulty();
        tmpl.header().blockDifficulty = diff;
        tmpl.body().txnDifficulty     = diff;

        // PRNG — fresh random 32 bytes each template
        tmpl.body().prng = randomBytes32();

        // Nonce starts at 0
        tmpl.header().nonce = MiniNumber(int64_t(0));

        return tmpl;
    }

    MiniData computeDifficulty() {
        // Use default difficulty (no difficulty adjustment yet without history)
        return makeDifficulty(cfg_.defaultLeadingZeros);
    }

    static MiniData randomBytes32() {
        static std::mt19937_64 rng(
            static_cast<uint64_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::vector<uint8_t> buf(32);
        for (int i = 0; i < 4; i++) {
            uint64_t v = rng();
            std::memcpy(buf.data() + i * 8, &v, 8);
        }
        return MiniData(buf);
    }

    void log(const std::string& msg) {
        if (logger_) logger_(msg);
        else std::cout << "[MiningManager] " << msg << "\n";
    }
};

} // namespace mining
} // namespace minima
