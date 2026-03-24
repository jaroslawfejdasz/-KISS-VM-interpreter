#pragma once
/**
 * P2PSync — Minima P2P synchronization loop.
 *
 * Java ref: org.minima.system.network.minima.NIOManager
 *
 * Flow:
 *  1. connect() to a seed node
 *  2. sendGreeting() with our chain state
 *  3. receive peer Greeting — learn their topBlock
 *  4. if behind → send IBD_REQ in batches, receive IBD responses
 *  5. flood-fill loop: TXPOWID → request TXPOW → process → (re)broadcast
 */

#include "NIOClient.hpp"
#include "NIOMessage.hpp"
#include "../objects/Greeting.hpp"
#include "../objects/IBD.hpp"
#include "../objects/TxPoW.hpp"
#include "../objects/TxBlock.hpp"
#include "../chain/ChainProcessor.hpp"
#include "../chain/UTxOSet.hpp"
#include "../types/MiniNumber.hpp"
#include "../types/MiniData.hpp"

#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_map>

namespace minima {
namespace network {

using OnBlockCallback    = std::function<void(const TxPoW&)>;
using OnTxCallback       = std::function<void(const TxPoW&)>;
using OnSyncDoneCallback = std::function<void(int64_t tipBlock)>;

class P2PSync {
public:
    struct Config {
        std::string host         = "145.40.90.17";
        uint16_t    port         = 9001;
        int         recvTimeout  = 30000;
        int         maxRetries   = 5;
        int64_t     ibdBatchSize = 500;
    };

    P2PSync(Config cfg,
            chain::ChainProcessor& chain,
            chain::UTxOSet&        utxo)
        : cfg_(std::move(cfg))
        , chain_(chain)
        , utxo_(utxo)
        , client_(cfg_.host, cfg_.port)
    {}

    void onBlock   (OnBlockCallback    cb) { onBlock_    = std::move(cb); }
    void onTx      (OnTxCallback       cb) { onTx_       = std::move(cb); }
    void onSyncDone(OnSyncDoneCallback cb) { onSyncDone_ = std::move(cb); }
    void setLogger (std::function<void(const std::string&)> l) { logger_ = std::move(l); }

    // ── state ─────────────────────────────────────────────────────────────
    void    setLocalTip(int64_t n) { localTip_ = MiniNumber(n); }
    void    setChainIDs(std::vector<MiniData> ids) { chainIDs_ = std::move(ids); }
    int64_t localTipBlock()  const { return localTip_.getAsLong(); }
    int64_t peerTipBlock()   const { return peerTipBlock_; }
    const   Greeting& peerGreeting() const { return peerGreeting_; }
    bool    isConnected()    const { return client_.isConnected(); }

    // ── connect / disconnect ───────────────────────────────────────────────
    void connect() {
        log("Connecting to " + cfg_.host + ":" + std::to_string(cfg_.port) + "...");
        client_.connect();
        client_.setRecvTimeout(cfg_.recvTimeout);
        log("Connected.");
    }

    void disconnect() {
        client_.disconnect();
        log("Disconnected.");
    }

    // ── handshake ─────────────────────────────────────────────────────────
    // Send our Greeting, receive peer Greeting. Returns peer topBlock.
    int64_t doHandshake() {
        Greeting mine;
        mine.setTopBlock(localTip_);
        for (const auto& id : chainIDs_)
            mine.addChainID(id);

        log("Sending Greeting (ourTip=" + std::to_string(localTip_.getAsLong()) + ")...");
        client_.sendGreeting(mine);

        auto msg = client_.receive();
        if (msg.type != MsgType::GREETING)
            throw std::runtime_error("P2PSync: expected GREETING, got type " +
                                     std::to_string((int)msg.type));

        size_t off = 0;
        peerGreeting_ = Greeting::deserialise(msg.payload.data(), off);
        peerTipBlock_ = peerGreeting_.topBlock().getAsLong();

        log("Peer Greeting: version=" + peerGreeting_.version().str() +
            " topBlock=" + std::to_string(peerTipBlock_));
        return peerTipBlock_;
    }

    // ── IBD ───────────────────────────────────────────────────────────────
    // Download blocks in batches until we reach peer tip.
    int64_t doIBD() {
        int64_t ourTip  = localTip_.getAsLong();
        int64_t peerTip = peerTipBlock_;

        if (peerTip <= ourTip) {
            log("Already up to date (tip=" + std::to_string(ourTip) + ")");
            if (onSyncDone_) onSyncDone_(ourTip);
            return 0;
        }

        int64_t from      = ourTip + 1;
        int64_t processed = 0;

        log("IBD: blocks " + std::to_string(from) + "→" + std::to_string(peerTip));

        while (from <= peerTip) {
            int64_t to = std::min(from + cfg_.ibdBatchSize - 1, peerTip);
            log("  batch " + std::to_string(from) + "→" + std::to_string(to));

            client_.sendIBDRequest(from, to);

            auto resp = client_.receive();
            if (resp.type == MsgType::IBD)
                processed += processIBDPayload(resp.payload);
            else
                log("  unexpected msg type " + std::to_string((int)resp.type) + " — skipping");

            from = to + 1;
        }

        localTip_ = MiniNumber(ourTip + processed);
        log("IBD done. Processed=" + std::to_string(processed));
        if (onSyncDone_) onSyncDone_(localTip_.getAsLong());
        return processed;
    }

    // ── flood-fill event loop ─────────────────────────────────────────────
    void eventLoop() {
        log("Entering event loop...");
        while (client_.isConnected()) {
            try {
                auto msg = client_.receive();
                handleMsg(msg);
            } catch (const std::exception& e) {
                log("Event loop exit: " + std::string(e.what()));
                break;
            }
        }
    }

    // ── full run ──────────────────────────────────────────────────────────
    void run() {
        running_ = true;
        int retries = 0;
        while (running_) {
            try {
                connect();
                doHandshake();
                doIBD();
                eventLoop();
            } catch (const std::exception& e) {
                log("Error: " + std::string(e.what()));
                disconnect();
                if (++retries >= cfg_.maxRetries) { running_ = false; break; }
                int backoff = retries * 2;
                log("Retry in " + std::to_string(backoff) + "s...");
                std::this_thread::sleep_for(std::chrono::seconds(backoff));
            }
        }
    }

    void stop() { running_ = false; }

private:
    Config               cfg_;
    chain::ChainProcessor& chain_;
    chain::UTxOSet&        utxo_;
    NIOClient            client_;

    MiniNumber           localTip_    {int64_t(-1)};
    int64_t              peerTipBlock_{-1};
    Greeting             peerGreeting_;
    std::vector<MiniData> chainIDs_;

    std::atomic<bool>    running_{false};

    OnBlockCallback      onBlock_;
    OnTxCallback         onTx_;
    OnSyncDoneCallback   onSyncDone_;
    std::function<void(const std::string&)> logger_;

    std::unordered_map<std::string,bool> seen_;

    // ── message dispatch ──────────────────────────────────────────────────
    void handleMsg(const NIOMsg& msg) {
        switch (msg.type) {
            case MsgType::TXPOWID:   onTxPoWID(msg.payload);  break;
            case MsgType::TXPOW:     onTxPoW(msg.payload);    break;
            case MsgType::TXBLOCK:   onTxBlock(msg.payload);  break;
            case MsgType::PULSE:     log("PULSE (heartbeat)"); break;
            case MsgType::PING:      client_.sendPing();       break;
            case MsgType::GREETING:  log("Re-greeting (peer reconnect)"); break;
            default:
                log("Unhandled type=" + std::to_string((int)msg.type));
                break;
        }
    }

    void onTxPoWID(const std::vector<uint8_t>& payload) {
        size_t off = 0;
        try {
            MiniData id    = MiniData::deserialise(payload.data(), off);
            std::string hex = id.toHexString();
            if (seen_.count(hex)) return;
            seen_[hex] = true;
            log("TXPOWID " + hex.substr(0, 16) + "... — requesting");
            client_.sendTxPoWIDRequest(id);
        } catch (...) { log("onTxPoWID: parse error"); }
    }

    void onTxPoW(const std::vector<uint8_t>& payload) {
        size_t off = 0;
        try {
            TxPoW tx = TxPoW::deserialise(payload.data(), off);
            bool isBlk = tx.isBlock();
            log("TXPOW " + std::string(isBlk ? "BLOCK #" : "TX") +
                (isBlk ? std::to_string(tx.header().blockNumber.getAsLong()) : ""));
            if (isBlk) {
                chain_.processBlock(tx);
                localTip_ = tx.header().blockNumber;
                if (onBlock_) onBlock_(tx);
            } else {
                if (onTx_) onTx_(tx);
            }
        } catch (const std::exception& e) {
            log("onTxPoW error: " + std::string(e.what()));
        }
    }

    void onTxBlock(const std::vector<uint8_t>& payload) {
        size_t off = 0;
        try {
            TxBlock blk = TxBlock::deserialise(payload.data(), off, payload.size());
            TxPoW& tx   = blk.txpow();
            log("TXBLOCK #" + std::to_string(tx.header().blockNumber.getAsLong()));
            chain_.processBlock(tx);
            localTip_ = tx.header().blockNumber;
            if (onBlock_) onBlock_(tx);
        } catch (const std::exception& e) {
            log("onTxBlock error: " + std::string(e.what()));
        }
    }

    int64_t processIBDPayload(const std::vector<uint8_t>& payload) {
        size_t off = 0;
        try {
            IBD ibd = IBD::deserialise(payload.data(), off, payload.size());
            int64_t n = 0;
            for (auto& blk : ibd.txBlocks()) {
                chain_.processBlock(blk.txpow());
                if (onBlock_) onBlock_(blk.txpow());
                n++;
            }
            log("  applied " + std::to_string(n) + " blocks");
            return n;
        } catch (const std::exception& e) {
            log("processIBD error: " + std::string(e.what()));
            return 0;
        }
    }

    void log(const std::string& msg) {
        if (logger_) logger_(msg);
        else std::cout << "[P2PSync] " << msg << "\n";
    }
};

} // namespace network
} // namespace minima
