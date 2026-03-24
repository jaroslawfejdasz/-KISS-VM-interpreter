/**
 * test_loopback_handshake.cpp
 *
 * Full end-to-end Greeting handshake over a real TCP loopback socket.
 *
 * Two threads — server and client — each own a ProtocolConnection.
 * They communicate through a real 127.0.0.1:PORT socket pair.
 * This verifies the wire framing, state machine transitions, and
 * IBD exchange under real-world I/O conditions (including fragmentation).
 *
 * Test cases:
 *   1. Equal tips → both reach READY, no IBD needed
 *   2. Client behind → IBD_REQ sent, server sends IBD, client reaches READY
 *   3. Large IBD (50 blocks) over loopback — fragmentation stress
 *   4. Peer tips are correctly recorded after handshake
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/network/NIOMessage.hpp"
#include "../../src/network/NetworkProtocol.hpp"
#include "../../src/objects/IBD.hpp"
#include "../../src/objects/TxBlock.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <algorithm>

using namespace minima;
using namespace minima::network;
using namespace std::chrono_literals;

// ── Low-level socket helpers ──────────────────────────────────────────────────

static void sock_send_all(int fd, const std::vector<uint8_t>& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) throw std::runtime_error("send() failed");
        sent += (size_t)n;
    }
}

// Non-blocking read. Returns {} when nothing available. Throws on close/error.
static std::vector<uint8_t> sock_recv_avail(int fd) {
    char buf[8192];
    ssize_t n = ::recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
    if (n > 0)  return std::vector<uint8_t>(buf, buf + n);
    if (n == 0) throw std::runtime_error("peer closed connection");
    if (errno == EAGAIN || errno == EWOULDBLOCK) return {};
    throw std::runtime_error(std::string("recv() error: ") + strerror(errno));
}

// ── Side result (thread-safe event log) ──────────────────────────────────────

struct SideResult {
    std::vector<NetworkEvent::Kind> events;
    ConnState                       finalState { ConnState::CLOSED };
    MiniNumber                      peerTip    { int64_t(-999) };

    bool has(NetworkEvent::Kind k) const {
        return std::find(events.begin(), events.end(), k) != events.end();
    }
};

// ── Protocol loop for one side ────────────────────────────────────────────────

/**
 * Runs ProtocolConnection on fd until READY/CLOSED or timeout.
 *
 * @param fd          Connected socket fd (already connect()-ed or accept()-ed)
 * @param tip         Local block tip
 * @param result      Output: events + final state
 * @param ibdToSend   If non-null: when we detect an incoming IBD_REQ, we send
 *                    this IBD back.  Simulates a sync-capable server.
 * @param timeoutMs   Hard timeout (ms)
 */
static void run_side(int fd,
                     MiniNumber tip,
                     SideResult& result,
                     const IBD* ibdToSend = nullptr,
                     int timeoutMs = 4000)
{
    ProtocolConnection conn(tip, [&](NetworkEvent ev) {
        result.events.push_back(ev.kind);
    });

    // Send initial greeting
    if (!conn.outBuffer.empty()) {
        sock_send_all(fd, conn.outBuffer);
        conn.outBuffer.clear();
    }

    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        // 1. Read incoming bytes
        std::vector<uint8_t> incoming;
        try {
            incoming = sock_recv_avail(fd);
        } catch (...) {
            break; // peer closed
        }

        if (!incoming.empty()) {
            // If we can serve IBD, scan for IBD_REQ before feeding to conn
            if (ibdToSend) {
                size_t i = 0;
                while (i + 4 < incoming.size()) {
                    uint32_t len = ((uint32_t)incoming[i]   << 24)
                                 | ((uint32_t)incoming[i+1] << 16)
                                 | ((uint32_t)incoming[i+2] <<  8)
                                 |  (uint32_t)incoming[i+3];
                    if (i + 4 + len > incoming.size()) break;
                    uint8_t t = incoming[i + 4];
                    if (t == (uint8_t)MsgType::IBD_REQ) {
                        // Reply with IBD
                        auto payload = ibdToSend->serialise();
                        NIOMsg ibdMsg(MsgType::IBD, payload);
                        sock_send_all(fd, ibdMsg.encode());
                    }
                    i += 4 + len;
                }
            }

            conn.receive(incoming);
        }

        // 2. Flush outgoing
        if (!conn.outBuffer.empty()) {
            try {
                sock_send_all(fd, conn.outBuffer);
            } catch (...) {
                break;
            }
            conn.outBuffer.clear();
        }

        // 3. Done?
        // IMPORTANT: When READY, we must keep the socket open if we're the server
        // (ibdToSend != nullptr), because the client may still need to send IBD_REQ.
        // Linger until both: READY and no more traffic to handle.
        if (conn.isClosed()) break;
        if (conn.isReady()) {
            // If we can serve IBD, linger to catch incoming IBD_REQ.
            // Exit only when no bytes came in this iteration.
            if (!ibdToSend || incoming.empty()) break;
        }

        std::this_thread::sleep_for(1ms);
    }

    result.finalState = conn.state();
    result.peerTip    = conn.peerTip();
}

// ── LoopbackTest harness ──────────────────────────────────────────────────────

/**
 * Creates a TCP server/client pair on 127.0.0.1, runs both sides in threads,
 * waits for completion, stores results.
 */
struct LoopbackTest {
    SideResult server;
    SideResult client;

    void run(MiniNumber serverTip, MiniNumber clientTip,
             const IBD* ibdForServer = nullptr,
             int timeoutMs = 4000)
    {
        // ── listening socket ──────────────────────────────────────────────────
        int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) throw std::runtime_error("socket() failed");

        int opt = 1;
        ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = ::inet_addr("127.0.0.1");
        addr.sin_port        = 0; // OS picks free port

        if (::bind(listenFd, (sockaddr*)&addr, sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed");
        if (::listen(listenFd, 1) < 0)
            throw std::runtime_error("listen() failed");

        socklen_t addrLen = sizeof(addr);
        ::getsockname(listenFd, (sockaddr*)&addr, &addrLen);
        uint16_t port = ntohs(addr.sin_port);

        // ── server thread ─────────────────────────────────────────────────────
        std::thread serverThread([&]() {
            sockaddr_in peerAddr{};
            socklen_t   peerLen = sizeof(peerAddr);
            int connFd = ::accept(listenFd, (sockaddr*)&peerAddr, &peerLen);
            if (connFd < 0) return;
            run_side(connFd, serverTip, server, ibdForServer, timeoutMs);
            ::close(connFd);
        });

        // ── client thread ─────────────────────────────────────────────────────
        std::thread clientThread([&]() {
            int connFd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (connFd < 0) return;

            sockaddr_in srvAddr{};
            srvAddr.sin_family      = AF_INET;
            srvAddr.sin_addr.s_addr = ::inet_addr("127.0.0.1");
            srvAddr.sin_port        = htons(port);

            // Retry connect for up to 500ms (server may not be in accept() yet)
            bool connected = false;
            for (int i = 0; i < 50 && !connected; i++) {
                if (::connect(connFd, (sockaddr*)&srvAddr, sizeof(srvAddr)) == 0)
                    connected = true;
                else
                    std::this_thread::sleep_for(10ms);
            }
            if (!connected) { ::close(connFd); return; }

            run_side(connFd, clientTip, client, nullptr, timeoutMs);
            ::close(connFd);
        });

        serverThread.join();
        clientThread.join();
        ::close(listenFd);
    }
};

// Helper: build a simple TxBlock with a given block number
static TxBlock makeBlock(int64_t num) {
    TxBlock tb;
    tb.txpow().header().blockNumber = MiniNumber(num);
    return tb;
}

// Helper: build an IBD with N sequential blocks starting from block 0
static IBD makeIBD(int blockCount, int64_t startBlock = 0) {
    IBD ibd;
    for (int64_t i = startBlock; i < startBlock + blockCount; i++)
        ibd.addBlock(makeBlock(i));
    return ibd;
}

// ─────────────────────────────────────────────────────────────────────────────
// TESTS
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("Loopback Handshake — real TCP socket") {

// ── 1. Equal tips ─────────────────────────────────────────────────────────────
TEST_CASE("equal tips: both sides reach READY without IBD") {
    LoopbackTest t;
    t.run(MiniNumber(int64_t(100)), MiniNumber(int64_t(100)));

    // Both peers exchanged Greetings and went READY
    CHECK(t.server.finalState == ConnState::READY);
    CHECK(t.client.finalState == ConnState::READY);

    // Both received GREETING and CONNECTED events
    CHECK(t.server.has(NetworkEvent::Kind::GREETING_RECV));
    CHECK(t.client.has(NetworkEvent::Kind::GREETING_RECV));
    CHECK(t.server.has(NetworkEvent::Kind::CONNECTED));
    CHECK(t.client.has(NetworkEvent::Kind::CONNECTED));

    // Tips are correctly recorded
    CHECK(t.server.peerTip == MiniNumber(int64_t(100)));
    CHECK(t.client.peerTip == MiniNumber(int64_t(100)));
}

// ── 2. Client behind → full IBD exchange ──────────────────────────────────────
TEST_CASE("client behind server: IBD_REQ → IBD → client READY") {
    auto ibd = makeIBD(6, 0); // blocks 0–5

    LoopbackTest t;
    t.run(MiniNumber(int64_t(5)),   // server tip
          MiniNumber(int64_t(-1)),  // client: fresh install
          &ibd);

    // Server is ahead → goes READY immediately after Greeting exchange
    CHECK(t.server.finalState == ConnState::READY);

    // Client was behind → sent IBD_REQ → received IBD → READY
    CHECK(t.client.finalState == ConnState::READY);
    CHECK(t.client.has(NetworkEvent::Kind::IBD_RECV));
    CHECK(t.client.has(NetworkEvent::Kind::CONNECTED));

    // Tips
    CHECK(t.server.peerTip == MiniNumber(int64_t(-1)));
    CHECK(t.client.peerTip == MiniNumber(int64_t(5)));
}

// ── 3. Asymmetric tips, no IBD server ─────────────────────────────────────────
TEST_CASE("client behind server, no IBD: server READY, client stuck WAIT_IBD") {
    LoopbackTest t;
    // Short timeout so test runs fast
    t.run(MiniNumber(int64_t(200)), MiniNumber(int64_t(50)), nullptr, 400);

    // Server is ahead, processes client greeting → READY straight away
    CHECK(t.server.finalState == ConnState::READY);

    // Client is behind: sends IBD_REQ, server doesn't respond → stays WAIT_IBD
    CHECK(t.client.finalState == ConnState::HANDSHAKE_WAIT_IBD);

    // Peer tips from the greeting are still correctly recorded
    CHECK(t.server.peerTip == MiniNumber(int64_t(50)));
    CHECK(t.client.peerTip == MiniNumber(int64_t(200)));
}

// ── 4. Large IBD (50 blocks) ─────────────────────────────────────────────────
TEST_CASE("large IBD (50 blocks) delivered correctly over loopback") {
    auto bigIBD = makeIBD(50, 0); // blocks 0–49

    LoopbackTest t;
    t.run(MiniNumber(int64_t(49)),
          MiniNumber(int64_t(-1)),
          &bigIBD);

    CHECK(t.client.finalState == ConnState::READY);
    CHECK(t.client.has(NetworkEvent::Kind::IBD_RECV));
}

// ── 5. Both sides are fresh installs (tip=-1) ─────────────────────────────────
TEST_CASE("both fresh install (tip=-1): exchange greetings and reach READY") {
    LoopbackTest t;
    t.run(MiniNumber(int64_t(-1)), MiniNumber(int64_t(-1)));

    // Neither is behind the other → both go READY without IBD
    CHECK(t.server.finalState == ConnState::READY);
    CHECK(t.client.finalState == ConnState::READY);
    CHECK(t.server.has(NetworkEvent::Kind::CONNECTED));
    CHECK(t.client.has(NetworkEvent::Kind::CONNECTED));
}

// ── 6. Multiple concurrent runs (no interference) ────────────────────────────
TEST_CASE("three independent loopback handshakes run concurrently without interference") {
    LoopbackTest t1, t2, t3;
    std::thread th1([&]{ t1.run(MiniNumber(int64_t(10)), MiniNumber(int64_t(10))); });
    std::thread th2([&]{ t2.run(MiniNumber(int64_t(20)), MiniNumber(int64_t(20))); });
    std::thread th3([&]{ t3.run(MiniNumber(int64_t(30)), MiniNumber(int64_t(30))); });
    th1.join(); th2.join(); th3.join();

    CHECK(t1.server.finalState == ConnState::READY);
    CHECK(t2.server.finalState == ConnState::READY);
    CHECK(t3.server.finalState == ConnState::READY);
    CHECK(t1.client.finalState == ConnState::READY);
    CHECK(t2.client.finalState == ConnState::READY);
    CHECK(t3.client.finalState == ConnState::READY);
}

} // TEST_SUITE Loopback Handshake
