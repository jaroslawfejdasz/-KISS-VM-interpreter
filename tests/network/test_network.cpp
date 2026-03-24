#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/network/NIOMessage.hpp"
#include "../../src/network/NIOClient.hpp"
#include "../../src/network/NIOServer.hpp"
#include "../../src/objects/Greeting.hpp"
#include "../../src/objects/TxPoW.hpp"

#include <thread>
#include <atomic>
#include <chrono>

using namespace minima;
using namespace minima::network;

// ── NIOMessage tests (already exist, keeping them) ────────────────────────────

TEST_CASE("NIOMessage: encode/decode roundtrip PING") {
    NIOMsg msg{MsgType::PING, {}};
    auto encoded = msg.encode();
    REQUIRE_FALSE(encoded.empty());
    auto decoded = NIOMsg::decode(encoded.data(), encoded.size());
    CHECK(decoded.type == MsgType::PING);
    CHECK(decoded.payload.empty());
}

TEST_CASE("NIOMessage: encode/decode roundtrip TXPOW") {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0xAB, 0xCD};
    NIOMsg msg{MsgType::TXPOW, payload};
    auto encoded = msg.encode();
    auto decoded = NIOMsg::decode(encoded.data(), encoded.size());
    CHECK(decoded.type == MsgType::TXPOW);
    CHECK(decoded.payload == payload);
}

TEST_CASE("NIOMessage: all message types have valid type byte") {
    // encode() returns [1-byte type][payload] — length prefix added by sendFramed
    for (uint8_t t = 0; t <= 23; t++) {
        NIOMsg msg{static_cast<MsgType>(t), {}};
        auto enc = msg.encode();
        REQUIRE_FALSE(enc.empty());
        CHECK(enc[0] == t);  // byte 0 = type (length prefix NOT included in encode())
    }
}

TEST_CASE("NIOMessage: decode throws on empty input") {
    CHECK_THROWS(NIOMsg::decode(nullptr, 0));
}

TEST_CASE("NIOMessage: msgTypeName returns non-empty string") {
    CHECK(std::string(msgTypeName(MsgType::GREETING)).size() > 0);
    CHECK(std::string(msgTypeName(MsgType::TXPOW)).size() > 0);
    CHECK(std::string(msgTypeName(MsgType::IBD_REQ)).size() > 0);
}

// ── NIOClient + NIOServer loopback tests ──────────────────────────────────────

static uint16_t findFreePort() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    ::bind(fd, (sockaddr*)&addr, sizeof(addr));
    socklen_t len = sizeof(addr);
    ::getsockname(fd, (sockaddr*)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

TEST_CASE("NIOClient+NIOServer: loopback PING") {
    uint16_t port = findFreePort();

    // Server accepts one connection, reads one message, echoes it back
    NIOServer server(port);
    server.bind();

    std::thread serverThread([&]() {
        server.acceptOne([](NIOClient& peer) {
            auto msg = peer.receive();
            peer.send(msg); // echo
        });
    });

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    NIOClient client("127.0.0.1", port);
    client.connect();
    client.setRecvTimeout(2000);

    NIOMsg ping{MsgType::PING, {}};
    client.send(ping);
    auto reply = client.receive();

    CHECK(reply.type == MsgType::PING);
    CHECK(reply.payload.empty());

    serverThread.join();
    server.stop();
}

TEST_CASE("NIOClient+NIOServer: loopback TXPOW with payload") {
    uint16_t port = findFreePort();

    std::vector<uint8_t> sentPayload = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02};

    NIOServer server(port);
    server.bind();

    std::thread serverThread([&]() {
        server.acceptOne([](NIOClient& peer) {
            auto msg = peer.receive();
            peer.send(msg);
        });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    NIOClient client("127.0.0.1", port);
    client.connect();
    client.setRecvTimeout(2000);

    NIOMsg txMsg{MsgType::TXPOW, sentPayload};
    client.send(txMsg);
    auto reply = client.receive();

    CHECK(reply.type == MsgType::TXPOW);
    CHECK(reply.payload == sentPayload);

    serverThread.join();
    server.stop();
}

TEST_CASE("NIOClient+NIOServer: multiple messages in sequence") {
    uint16_t port = findFreePort();

    NIOServer server(port);
    server.bind();

    // Server: read 3 messages, echo each
    std::thread serverThread([&]() {
        server.acceptOne([](NIOClient& peer) {
            for (int i = 0; i < 3; i++) {
                auto msg = peer.receive();
                peer.send(msg);
            }
        });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    NIOClient client("127.0.0.1", port);
    client.connect();
    client.setRecvTimeout(2000);

    std::vector<MsgType> types = {MsgType::PING, MsgType::TXPOW, MsgType::GREETING};
    for (auto t : types) {
        NIOMsg msg{t, {0xAA, 0xBB}};
        client.send(msg);
        auto reply = client.receive();
        CHECK(reply.type == t);
    }

    serverThread.join();
    server.stop();
}

TEST_CASE("NIOClient: connect to refused port throws") {
    // Port 1 is always refused (requires root to bind)
    NIOClient client("127.0.0.1", 1);
    CHECK_THROWS(client.connect());
}

TEST_CASE("NIOClient+NIOServer: large payload (1MB)") {
    uint16_t port = findFreePort();

    std::vector<uint8_t> bigPayload(1024 * 1024, 0x42); // 1 MB

    NIOServer server(port);
    server.bind();

    std::thread serverThread([&]() {
        server.acceptOne([](NIOClient& peer) {
            auto msg = peer.receive();
            peer.send(msg);
        });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    NIOClient client("127.0.0.1", port);
    client.connect();
    client.setRecvTimeout(5000);

    NIOMsg bigMsg{MsgType::IBD, bigPayload};
    client.send(bigMsg);
    auto reply = client.receive();

    CHECK(reply.type == MsgType::IBD);
    CHECK(reply.payload.size() == bigPayload.size());
    CHECK(reply.payload[0] == 0x42);
    CHECK(reply.payload.back() == 0x42);

    serverThread.join();
    server.stop();
}
