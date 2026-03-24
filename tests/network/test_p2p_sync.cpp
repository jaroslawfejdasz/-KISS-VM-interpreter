#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/network/P2PSync.hpp"
#include "../../src/network/NIOServer.hpp"
#include "../../src/network/NIOClient.hpp"
#include "../../src/network/NIOMessage.hpp"
#include "../../src/objects/Greeting.hpp"
#include "../../src/objects/IBD.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/objects/TxBlock.hpp"
#include "../../src/chain/ChainProcessor.hpp"
#include "../../src/chain/UTxOSet.hpp"

#include <thread>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace minima;
using namespace minima::network;
using namespace minima::chain;

// ── helpers ───────────────────────────────────────────────────────────────

static uint16_t findFreePort() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    ::bind(fd, (sockaddr*)&addr, sizeof(addr));
    socklen_t len = sizeof(addr);
    ::getsockname(fd, (sockaddr*)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

// Build a Greeting payload for given tip
static std::vector<uint8_t> greetingPayload(int64_t tip) {
    Greeting g;
    g.setTopBlock(MiniNumber(tip));
    return g.serialise();
}

// Build an IBD payload with fake blocks [from..to]
static std::vector<uint8_t> ibdPayload(int64_t from, int64_t to) {
    IBD ibd;
    for (int64_t i = from; i <= to; i++) {
        TxPoW tx;
        tx.header().blockNumber = MiniNumber(i);
        tx.header().timeMilli   = MiniNumber(int64_t(1700000000000LL + i * 1000));
        ibd.addBlock(TxBlock(tx));
    }
    return ibd.serialise();
}

// ── tests ─────────────────────────────────────────────────────────────────

TEST_CASE("P2PSync: handshake — receive peer Greeting") {
    uint16_t port = findFreePort();
    NIOServer server(port);
    server.bind();

    std::thread srv([&]() {
        server.acceptOne([](NIOClient& peer) {
            peer.receive();  // consume client greeting
            // send back greeting with topBlock=100
            peer.send(NIOMsg{MsgType::GREETING, greetingPayload(100)});
        });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ChainProcessor chain; UTxOSet utxo;
    P2PSync::Config cfg; cfg.host="127.0.0.1"; cfg.port=port; cfg.recvTimeout=2000;
    P2PSync sync(cfg, chain, utxo);
    sync.setLogger([](const std::string&){});

    sync.connect();
    int64_t peerTip = sync.doHandshake();

    CHECK(peerTip == 100);
    CHECK(sync.peerGreeting().topBlock().getAsLong() == 100);
    CHECK(sync.peerGreeting().version().str() == std::string(MINIMA_VERSION));

    srv.join();
    server.stop();
}

TEST_CASE("P2PSync: IBD — already up to date (localTip >= peerTip)") {
    uint16_t port = findFreePort();
    NIOServer server(port);
    server.bind();

    std::thread srv([&]() {
        server.acceptOne([](NIOClient& peer) {
            peer.receive();
            peer.send(NIOMsg{MsgType::GREETING, greetingPayload(5)});
            // no IBD request should come
        });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ChainProcessor chain; UTxOSet utxo;
    P2PSync::Config cfg; cfg.host="127.0.0.1"; cfg.port=port; cfg.recvTimeout=2000;
    P2PSync sync(cfg, chain, utxo);
    sync.setLocalTip(10);  // we're ahead
    sync.setLogger([](const std::string&){});

    sync.connect();
    sync.doHandshake();  // peer at 5
    int64_t processed = sync.doIBD();

    CHECK(processed == 0);
    CHECK(sync.localTipBlock() == 10);

    srv.join();
    server.stop();
}

TEST_CASE("P2PSync: IBD — download single batch (10 blocks)") {
    uint16_t port = findFreePort();
    NIOServer server(port);
    server.bind();

    std::thread srv([&]() {
        server.acceptOne([](NIOClient& peer) {
            peer.receive();
            peer.send(NIOMsg{MsgType::GREETING, greetingPayload(10)});

            auto req = peer.receive();
            CHECK(req.type == MsgType::IBD_REQ);
            peer.send(NIOMsg{MsgType::IBD, ibdPayload(0, 9)});
        });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ChainProcessor chain; UTxOSet utxo;
    std::vector<int64_t> blocks;

    P2PSync::Config cfg; cfg.host="127.0.0.1"; cfg.port=port;
    cfg.recvTimeout=2000; cfg.ibdBatchSize=500;
    P2PSync sync(cfg, chain, utxo);
    sync.setLocalTip(-1);
    sync.setLogger([](const std::string&){});
    sync.onBlock([&](const TxPoW& tx){
        blocks.push_back(tx.header().blockNumber.getAsLong());
    });

    sync.connect();
    sync.doHandshake();
    int64_t processed = sync.doIBD();

    CHECK(processed == 10);
    CHECK((int)blocks.size() == 10);
    CHECK(blocks.front() == 0);
    CHECK(blocks.back()  == 9);

    srv.join();
    server.stop();
}

TEST_CASE("P2PSync: IBD — two batches (batchSize=10, peerTip=19)") {
    uint16_t port = findFreePort();
    NIOServer server(port);
    server.bind();

    std::thread srv([&]() {
        server.acceptOne([](NIOClient& peer) {
            peer.receive();
            peer.send(NIOMsg{MsgType::GREETING, greetingPayload(19)});

            // batch 1: 0..9
            auto r1 = peer.receive();
            CHECK(r1.type == MsgType::IBD_REQ);
            peer.send(NIOMsg{MsgType::IBD, ibdPayload(0, 9)});

            // batch 2: 10..19
            auto r2 = peer.receive();
            CHECK(r2.type == MsgType::IBD_REQ);
            peer.send(NIOMsg{MsgType::IBD, ibdPayload(10, 19)});
        });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ChainProcessor chain; UTxOSet utxo;
    P2PSync::Config cfg; cfg.host="127.0.0.1"; cfg.port=port;
    cfg.recvTimeout=2000; cfg.ibdBatchSize=10;
    P2PSync sync(cfg, chain, utxo);
    sync.setLocalTip(-1);
    sync.setLogger([](const std::string&){});

    sync.connect();
    sync.doHandshake();
    int64_t processed = sync.doIBD();

    CHECK(processed == 20);
    CHECK(sync.localTipBlock() == 19);

    srv.join();
    server.stop();
}

TEST_CASE("P2PSync: flood-fill — TXPOWID triggers TXPOWREQ") {
    uint16_t port = findFreePort();
    NIOServer server(port);
    server.bind();

    std::thread srv([&]() {
        server.acceptOne([](NIOClient& peer) {
            peer.receive();
            // peer at 0, client at 0 → no IBD
            peer.send(NIOMsg{MsgType::GREETING, greetingPayload(0)});

            // Send a TXPOWID
            MiniData fakeID(std::vector<uint8_t>(32, 0xAB));
            peer.send(NIOMsg{MsgType::TXPOWID, fakeID.serialise()});

            // Expect TXPOWREQ back
            auto req = peer.receive();
            CHECK(req.type == MsgType::TXPOWREQ);

            // Send the TxPoW
            TxPoW tx;
            tx.header().blockNumber = MiniNumber(int64_t(1));
            tx.header().timeMilli   = MiniNumber(int64_t(1700000001000LL));
            peer.send(NIOMsg{MsgType::TXPOW, tx.serialise()});

            // give client time to process, then disconnect
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ChainProcessor chain; UTxOSet utxo;
    P2PSync::Config cfg; cfg.host="127.0.0.1"; cfg.port=port; cfg.recvTimeout=300;
    P2PSync sync(cfg, chain, utxo);
    sync.setLocalTip(0);  // already at peer tip → no IBD
    sync.setLogger([](const std::string&){});

    std::atomic<int> blockCount{0};
    sync.onBlock([&](const TxPoW&){ blockCount++; });

    sync.connect();
    sync.doHandshake();
    sync.doIBD();       // no blocks needed
    sync.eventLoop();   // runs until timeout/disconnect

    // The TXPOW was received; isBlock() checks actual PoW — may or may not pass
    // but the loop completed without crash
    CHECK(sync.isConnected() == false);  // server closed connection

    srv.join();
    server.stop();
}

TEST_CASE("P2PSync: dedup — same TXPOWID not requested twice") {
    // Test dedup logic directly via the seen_ map — send two identical
    // TXPOWID messages and verify only one TXPOWREQ is sent by counting
    // messages on the server side with a short-timeout server loop.

    uint16_t port = findFreePort();
    NIOServer server(port);
    server.bind();

    std::atomic<int> txpowReqCount{0};
    std::atomic<bool> serverDone{false};

    std::thread srv([&](){
        server.acceptOne([&](NIOClient& peer) {
            peer.setRecvTimeout(300);
            peer.receive();
            peer.send(NIOMsg{MsgType::GREETING, greetingPayload(0)});

            MiniData fakeID(std::vector<uint8_t>(32, 0xCD));
            auto payload = fakeID.serialise();

            peer.send(NIOMsg{MsgType::TXPOWID, payload});
            peer.send(NIOMsg{MsgType::TXPOWID, payload});

            // drain incoming for a short window
            for (int i = 0; i < 5; i++) {
                try {
                    auto r = peer.receive();
                    if (r.type == MsgType::TXPOWREQ) txpowReqCount++;
                } catch (...) { break; }
            }
            serverDone = true;
        });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ChainProcessor chain; UTxOSet utxo;
    P2PSync::Config cfg; cfg.host="127.0.0.1"; cfg.port=port; cfg.recvTimeout=400;
    P2PSync sync(cfg, chain, utxo);
    sync.setLocalTip(0);
    sync.setLogger([](const std::string&){});

    sync.connect();
    sync.doHandshake();
    sync.doIBD();
    sync.eventLoop();  // exits on timeout

    // Wait for server to finish counting
    for (int i = 0; i < 20 && !serverDone; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    CHECK(txpowReqCount == 1);

    srv.join();
    server.stop();
}

TEST_CASE("P2PSync: setLocalTip and localTipBlock") {
    ChainProcessor chain; UTxOSet utxo;
    P2PSync::Config cfg; cfg.host="127.0.0.1"; cfg.port=9999;
    P2PSync sync(cfg, chain, utxo);

    CHECK(sync.localTipBlock() == -1);
    sync.setLocalTip(42);
    CHECK(sync.localTipBlock() == 42);
    sync.setLocalTip(0);
    CHECK(sync.localTipBlock() == 0);
}
