/**
 * test_live_node.cpp
 *
 * TCP-level wire format tests for the Minima P2P protocol.
 *
 * Uses NIOClient + NIOServer over 127.0.0.1 loopback.
 * No external Minima Java node required — runs entirely in-process via fork().
 *
 * Wire format in this implementation:
 *   NIOMsg::encode() = [1-byte type][payload bytes]    (no length prefix)
 *   NIOClient::send() adds [4-byte big-endian length] on the wire
 *   NIOClient::receive() strips the length prefix before returning NIOMsg
 *
 * MiniNumber binary format (Java-compatible):
 *   [1-byte scale][1-byte bigint_len][bigint_len bytes of two's-complement]
 *
 * Greeting.serialise() layout:
 *   [4-byte len][version string "1.0.45"]  = 10 bytes
 *   [4-byte len][extraData "{}"]           =  6 bytes
 *   [MiniNumber topBlock]                  =  3 bytes (at offset 16)
 *   [MiniNumber chainCount]                =  3 bytes (at offset 19)
 *   total = 22 bytes (fresh install)
 *
 * Java ref: NIOClient.java, MiniNumber.writeDataStream(), Greeting.writeDataStream()
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/network/NIOMessage.hpp"
#include "../../src/network/NIOClient.hpp"
#include "../../src/network/NIOServer.hpp"
#include "../../src/objects/Greeting.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/types/MiniNumber.hpp"
#include "../../src/types/MiniData.hpp"

#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

using namespace minima;
using namespace minima::network;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void raw_write_all(int fd, const uint8_t* data, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t r = write(fd, data + sent, n - sent);
        if (r <= 0) throw std::runtime_error("raw_write_all failed");
        sent += r;
    }
}

static void raw_write_all(int fd, const std::vector<uint8_t>& v) {
    raw_write_all(fd, v.data(), v.size());
}

/** Connect a raw IPv4 TCP socket to 127.0.0.1:port */
static int raw_connect(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(port);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        throw std::runtime_error(std::string("connect() failed: ") + strerror(errno));
    }
    return fd;
}

/** Send a NIOMsg over raw socket using the wire framing:
 *  [4-byte big-endian length][encode() bytes] */
static void raw_send_msg(int fd, const NIOMsg& msg) {
    auto body = msg.encode();   // [type][payload]
    uint32_t len = (uint32_t)body.size();
    uint8_t hdr[4] = {
        (uint8_t)(len >> 24), (uint8_t)(len >> 16),
        (uint8_t)(len >>  8), (uint8_t)(len)
    };
    raw_write_all(fd, hdr, 4);
    raw_write_all(fd, body);
}

// ── Wire format tests (NIOMsg.encode() format) ─────────────────────────────

TEST_SUITE("NIOMsg::encode() — in-memory format") {

TEST_CASE("SINGLE_PING encode: [0B][00 00 00 01 00]") {
    // buildPing() has Java-compatible payload {0x00,0x00,0x00,0x01,0x00}
    auto enc = buildPing().encode();
    REQUIRE(enc.size() == 6);
    CHECK(enc[0] == 0x0B);  // SINGLE_PING = 11
    // payload = MiniData.ZERO_TXPOWID = [4-byte len=1][0x00]
    CHECK(enc[1] == 0x00);
    CHECK(enc[2] == 0x00);
    CHECK(enc[3] == 0x00);
    CHECK(enc[4] == 0x01);
    CHECK(enc[5] == 0x00);
}

TEST_CASE("SINGLE_PONG encode: [0C]") {
    auto enc = buildPong().encode();
    REQUIRE(enc.size() == 1);
    CHECK(enc[0] == 0x0C);  // SINGLE_PONG = 12
}

TEST_CASE("GREETING encode: first byte is 0x00") {
    Greeting g;
    auto enc = buildGreeting(g).encode();
    REQUIRE(enc.size() >= 1);
    CHECK(enc[0] == 0x00);  // GREETING = 0
}

TEST_CASE("TXPOWID 32-byte: [02][32 bytes payload]") {
    MiniData id(std::vector<uint8_t>(32, 0xFF));
    auto enc = buildTxPoWID(id).encode();
    REQUIRE(enc.size() == 33);   // 1(type) + 32(id)
    CHECK(enc[0] == 0x02);       // TXPOWID = 2
    for (int i = 1; i <= 32; ++i) CHECK(enc[i] == 0xFF);
}

TEST_CASE("TXPOWID 4-byte: [02][4 bytes payload]") {
    MiniData id(std::vector<uint8_t>(4, 0xAA));
    auto enc = buildTxPoWID(id).encode();
    REQUIRE(enc.size() == 5);
    CHECK(enc[0] == 0x02);
    for (int i = 1; i <= 4; ++i) CHECK(enc[i] == 0xAA);
}

TEST_CASE("PULSE MiniNumber(0): [06][00 01 00]") {
    auto enc = buildPulse(MiniNumber(int64_t(0))).encode();
    REQUIRE(enc.size() == 4);
    CHECK(enc[0] == 0x06);  // PULSE = 6
    CHECK(enc[1] == 0x00);  // scale = 0
    CHECK(enc[2] == 0x01);  // bigint len = 1
    CHECK(enc[3] == 0x00);  // BigInteger(0) = [0x00]
}

TEST_CASE("PULSE MiniNumber(42): [06][00 01 2A]") {
    auto enc = buildPulse(MiniNumber(int64_t(42))).encode();
    REQUIRE(enc.size() == 4);
    CHECK(enc[0] == 0x06);
    CHECK(enc[1] == 0x00);  // scale=0
    CHECK(enc[2] == 0x01);  // len=1
    CHECK(enc[3] == 0x2A);  // 42=0x2A
}

TEST_CASE("PULSE MiniNumber(500): [06][00 02 01 F4]") {
    auto enc = buildPulse(MiniNumber(int64_t(500))).encode();
    REQUIRE(enc.size() == 5);
    CHECK(enc[0] == 0x06);
    CHECK(enc[1] == 0x00);  // scale=0
    CHECK(enc[2] == 0x02);  // len=2
    CHECK(enc[3] == 0x01);
    CHECK(enc[4] == 0xF4);  // 0x01F4 = 500
}

TEST_CASE("PULSE MiniNumber(200): [06][00 02 00 C8]") {
    // 200 = 0xC8; MSB set → needs leading 0x00 for positive BigInt
    auto enc = buildPulse(MiniNumber(int64_t(200))).encode();
    REQUIRE(enc.size() == 5);
    CHECK(enc[0] == 0x06);
    CHECK(enc[2] == 0x02);  // len=2
    CHECK(enc[3] == 0x00);  // leading zero
    CHECK(enc[4] == 0xC8);  // 200
}

TEST_CASE("TXPOW type byte is 0x04") {
    TxPoW txp;
    auto enc = buildTxPoW(txp).encode();
    REQUIRE(enc.size() >= 1);
    CHECK(enc[0] == 0x04);  // TXPOW = 4
}

TEST_CASE("encode/decode roundtrip — basic message types") {
    for (int t = 0; t <= 13; ++t) {
        std::vector<uint8_t> pl = {0x01, (uint8_t)t, 0x03};
        NIOMsg orig((MsgType)t, pl);
        auto enc = orig.encode();
        CHECK(enc[0] == (uint8_t)t);
        NIOMsg got = NIOMsg::decode(enc.data(), enc.size());
        CHECK(got.type == orig.type);
        CHECK(got.payload == orig.payload);
    }
}

} // TEST_SUITE NIOMsg::encode()

// ── MiniNumber binary format ──────────────────────────────────────────────────

TEST_SUITE("MiniNumber — binary format (Java parity)") {

TEST_CASE("MiniNumber(0): [00][01][00]") {
    auto b = MiniNumber(int64_t(0)).serialise();
    REQUIRE(b.size() == 3);
    CHECK(b[0] == 0x00);  // scale=0
    CHECK(b[1] == 0x01);  // BigInt len=1
    CHECK(b[2] == 0x00);
}

TEST_CASE("MiniNumber(-1): [00][01][FF]") {
    auto b = MiniNumber(int64_t(-1)).serialise();
    REQUIRE(b.size() == 3);
    CHECK(b[0] == 0x00);
    CHECK(b[1] == 0x01);
    CHECK(b[2] == 0xFF);
}

TEST_CASE("MiniNumber(42): [00][01][2A]") {
    auto b = MiniNumber(int64_t(42)).serialise();
    REQUIRE(b.size() == 3);
    CHECK(b[2] == 0x2A);
}

TEST_CASE("MiniNumber(100): [00][01][64]") {
    auto b = MiniNumber(int64_t(100)).serialise();
    REQUIRE(b.size() == 3);
    CHECK(b[2] == 0x64);
}

TEST_CASE("MiniNumber(127): [00][01][7F] — max positive single-byte BigInt") {
    auto b = MiniNumber(int64_t(127)).serialise();
    REQUIRE(b.size() == 3);
    CHECK(b[2] == 0x7F);
}

TEST_CASE("MiniNumber(128): [00][02][00 80] — MSB set needs leading 0x00") {
    auto b = MiniNumber(int64_t(128)).serialise();
    REQUIRE(b.size() == 4);
    CHECK(b[1] == 0x02);
    CHECK(b[2] == 0x00);
    CHECK(b[3] == 0x80);
}

TEST_CASE("MiniNumber(-128): [00][01][80]") {
    auto b = MiniNumber(int64_t(-128)).serialise();
    REQUIRE(b.size() == 3);
    CHECK(b[1] == 0x01);
    CHECK(b[2] == 0x80);
}

TEST_CASE("MiniNumber(-256): [00][02][FF 00]") {
    // -256 in two's complement = 0xFF00
    auto b = MiniNumber(int64_t(-256)).serialise();
    REQUIRE(b.size() == 4);
    CHECK(b[1] == 0x02);
    CHECK(b[2] == 0xFF);
    CHECK(b[3] == 0x00);
}

TEST_CASE("MiniNumber roundtrip for representative values") {
    for (int64_t v : {int64_t(0), int64_t(1), int64_t(-1),
                      int64_t(42), int64_t(100), int64_t(127),
                      int64_t(128), int64_t(-128), int64_t(256), int64_t(-256),
                      int64_t(12345), int64_t(1000000), int64_t(-999999)}) {
        auto bytes = MiniNumber(v).serialise();
        size_t off = 0;
        auto got = MiniNumber::deserialise(bytes.data(), off);
        CHECK(got.getAsLong() == v);
        CHECK(off == bytes.size());
    }
}

} // TEST_SUITE MiniNumber

// ── Greeting wire format ──────────────────────────────────────────────────────

TEST_SUITE("Greeting — serialise() byte layout") {

TEST_CASE("fresh install: exactly 22 bytes") {
    // Layout:
    //   [4-byte len=6]["1.0.45"]       = 10 bytes  (version MiniString)
    //   [4-byte len=2]["{}"]           =  6 bytes  (extraData MiniString)
    //   [00][01][FF]                   =  3 bytes  (topBlock = MiniNumber(-1))
    //   [00][01][00]                   =  3 bytes  (chainCount = MiniNumber(0))
    //   total = 22 bytes
    Greeting g;
    auto b = g.serialise();
    REQUIRE(b.size() == 22);

    // version = MiniString: [4-byte big-endian len][utf8 bytes]
    uint32_t vlen = ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];
    CHECK(vlen == 6);
    CHECK(b[4]=='1'); CHECK(b[5]=='.'); CHECK(b[6]=='0');
    CHECK(b[7]=='.'); CHECK(b[8]=='4'); CHECK(b[9]=='5');

    // extraData = "{}" at offset 10
    uint32_t elen = ((uint32_t)b[10]<<24)|((uint32_t)b[11]<<16)|((uint32_t)b[12]<<8)|b[13];
    CHECK(elen == 2);
    CHECK(b[14] == '{');
    CHECK(b[15] == '}');

    // topBlock = MiniNumber(-1) at offset 16
    CHECK(b[16] == 0x00);  // scale=0
    CHECK(b[17] == 0x01);  // len=1
    CHECK(b[18] == 0xFF);  // -1

    // chainCount = MiniNumber(0) at offset 19
    CHECK(b[19] == 0x00);
    CHECK(b[20] == 0x01);
    CHECK(b[21] == 0x00);
}

TEST_CASE("topBlock=100: byte 0x64 at offset 18") {
    Greeting g;
    g.setTopBlock(MiniNumber(int64_t(100)));
    auto b = g.serialise();
    REQUIRE(b.size() >= 19);
    CHECK(b[16] == 0x00);
    CHECK(b[17] == 0x01);
    CHECK(b[18] == 0x64);  // 100 = 0x64
}

TEST_CASE("MINIMA_VERSION == 1.0.45") {
    CHECK(std::string(MINIMA_VERSION) == "1.0.45");
    Greeting g;
    CHECK(g.version().str() == "1.0.45");
}

TEST_CASE("greeting with 2 chain IDs: full roundtrip, no leftover bytes") {
    Greeting orig;
    orig.setTopBlock(MiniNumber(int64_t(42)));
    orig.addChainID(MiniData(std::vector<uint8_t>(32, 0xCC)));
    orig.addChainID(MiniData(std::vector<uint8_t>(32, 0xDD)));

    auto bytes = orig.serialise();
    size_t off = 0;
    auto got = Greeting::deserialise(bytes.data(), off);

    CHECK(off == bytes.size());
    CHECK(got.topBlock() == orig.topBlock());
    CHECK(got.chain().size() == 2);
    CHECK(got.chain()[0].bytes() == std::vector<uint8_t>(32, 0xCC));
    CHECK(got.chain()[1].bytes() == std::vector<uint8_t>(32, 0xDD));
    CHECK(got.version().str() == std::string(MINIMA_VERSION));
}

TEST_CASE("greeting isFreshInstall when topBlock < 0") {
    Greeting g;
    CHECK(g.isFreshInstall());  // topBlock = -1
    g.setTopBlock(MiniNumber(int64_t(0)));
    CHECK(!g.isFreshInstall());
    g.setTopBlock(MiniNumber(int64_t(100)));
    CHECK(!g.isFreshInstall());
}

} // TEST_SUITE Greeting

// ── TCP loopback handshake tests ──────────────────────────────────────────────

TEST_SUITE("TCP loopback — NIOClient/NIOServer") {

TEST_CASE("GREETING exchange: client verifies peer greeting content") {
    NIOServer server(0);
    server.bind();
    uint16_t port = server.port();
    CHECK(port > 0);

    pid_t pid = fork();
    REQUIRE(pid >= 0);

    if (pid == 0) {
        // ── SERVER CHILD ──────────────────────────────────────────────────
        server.acceptOne([](NIOClient& peer) {
            peer.setRecvTimeout(2000);
            auto msg = peer.receive();
            if (msg.type != MsgType::GREETING) _exit(1);

            // Verify client greeting is parseable and has correct version
            size_t off = 0;
            auto cg = Greeting::deserialise(msg.payload.data(), off);
            if (cg.version().str() != MINIMA_VERSION) _exit(2);

            // Reply with tip=100
            Greeting srv;
            srv.setTopBlock(MiniNumber(int64_t(100)));
            peer.sendGreeting(srv);
        });
        _exit(0);
    }

    NIOClient client("127.0.0.1", port);
    client.setRecvTimeout(2000);
    client.connect();

    Greeting myGreet;
    myGreet.setTopBlock(MiniNumber(int64_t(50)));
    client.sendGreeting(myGreet);

    auto resp = client.receive();
    REQUIRE(resp.type == MsgType::GREETING);

    size_t off = 0;
    auto peerGreet = Greeting::deserialise(resp.payload.data(), off);
    CHECK(peerGreet.topBlock() == MiniNumber(int64_t(100)));
    CHECK(peerGreet.version().str() == MINIMA_VERSION);

    int status; waitpid(pid, &status, 0);
    CHECK(WEXITSTATUS(status) == 0);
}

TEST_CASE("PING/PONG via NIOClient over real TCP") {
    NIOServer server(0);
    server.bind();
    uint16_t port = server.port();

    pid_t pid = fork();
    REQUIRE(pid >= 0);

    if (pid == 0) {
        server.acceptOne([](NIOClient& peer) {
            peer.setRecvTimeout(2000);
            auto msg = peer.receive();
            if (msg.type != MsgType::SINGLE_PING) _exit(2);
            peer.send(buildPong());
        });
        _exit(0);
    }

    NIOClient client("127.0.0.1", port);
    client.setRecvTimeout(2000);
    client.connect();
    client.send(buildPing());

    auto pong = client.receive();
    CHECK(pong.type == MsgType::SINGLE_PONG);
    CHECK(pong.payload.empty());

    int status; waitpid(pid, &status, 0);
    CHECK(WEXITSTATUS(status) == 0);
}

TEST_CASE("TXPOWID 32-byte via NIOClient") {
    NIOServer server(0);
    server.bind();
    uint16_t port = server.port();

    pid_t pid = fork();
    REQUIRE(pid >= 0);

    if (pid == 0) {
        server.acceptOne([](NIOClient& peer) {
            peer.setRecvTimeout(2000);
            auto msg = peer.receive();
            if (msg.type != MsgType::TXPOWID) _exit(3);
            if (msg.payload.size() != 32)      _exit(4);
            for (auto b : msg.payload)
                if (b != 0xAB) _exit(5);
        });
        _exit(0);
    }

    NIOClient client("127.0.0.1", port);
    client.setRecvTimeout(2000);
    client.connect();
    client.send(buildTxPoWID(MiniData(std::vector<uint8_t>(32, 0xAB))));

    int status; waitpid(pid, &status, 0);
    CHECK(WEXITSTATUS(status) == 0);
}

TEST_CASE("PULSE MiniNumber(42) survives TCP roundtrip") {
    NIOServer server(0);
    server.bind();
    uint16_t port = server.port();

    pid_t pid = fork();
    REQUIRE(pid >= 0);

    if (pid == 0) {
        server.acceptOne([](NIOClient& peer) {
            peer.setRecvTimeout(2000);
            auto msg = peer.receive();
            if (msg.type != MsgType::PULSE) _exit(6);
            if (msg.payload.size() < 3) _exit(7);
            size_t off = 0;
            auto bn = MiniNumber::deserialise(msg.payload.data(), off);
            if (bn.getAsLong() != 42) _exit(8);
        });
        _exit(0);
    }

    NIOClient client("127.0.0.1", port);
    client.setRecvTimeout(2000);
    client.connect();
    client.send(buildPulse(MiniNumber(int64_t(42))));

    int status; waitpid(pid, &status, 0);
    CHECK(WEXITSTATUS(status) == 0);
}

TEST_CASE("fragmented TCP: message split into two write()s") {
    NIOServer server(0);
    server.bind();
    uint16_t port = server.port();

    pid_t pid = fork();
    REQUIRE(pid >= 0);

    if (pid == 0) {
        server.acceptOne([](NIOClient& peer) {
            peer.setRecvTimeout(2000);
            auto msg = peer.receive();
            if (msg.type != MsgType::PULSE) _exit(9);
            size_t off = 0;
            auto bn = MiniNumber::deserialise(msg.payload.data(), off);
            if (bn.getAsLong() != 777) _exit(10);
        });
        _exit(0);
    }

    int raw_fd = raw_connect(port);
    // Send as NIOClient would: [4-byte length][type][payload]
    auto body = buildPulse(MiniNumber(int64_t(777))).encode();
    uint32_t len = (uint32_t)body.size();
    uint8_t hdr[4] = {(uint8_t)(len>>24),(uint8_t)(len>>16),(uint8_t)(len>>8),(uint8_t)len};
    // Split: send header + first byte, pause, send rest
    std::vector<uint8_t> part1(hdr, hdr+4);
    part1.push_back(body[0]);
    raw_write_all(raw_fd, part1);
    usleep(2000);
    raw_write_all(raw_fd, body.data()+1, body.size()-1);
    close(raw_fd);

    int status; waitpid(pid, &status, 0);
    CHECK(WEXITSTATUS(status) == 0);
}

TEST_CASE("batch delivery: 3 messages coalesced in one raw write()") {
    NIOServer server(0);
    server.bind();
    uint16_t port = server.port();

    pid_t pid = fork();
    REQUIRE(pid >= 0);

    if (pid == 0) {
        server.acceptOne([](NIOClient& peer) {
            peer.setRecvTimeout(2000);
            auto m1 = peer.receive();
            if (m1.type != MsgType::SINGLE_PING) _exit(11);
            auto m2 = peer.receive();
            if (m2.type != MsgType::TXPOWID)     _exit(12);
            if (m2.payload.size() != 32)          _exit(13);
            auto m3 = peer.receive();
            if (m3.type != MsgType::SINGLE_PONG) _exit(14);
        });
        _exit(0);
    }

    int raw_fd = raw_connect(port);
    std::vector<uint8_t> batch;
    // Each framed as [4-byte len][type][payload]
    auto frame = [](const NIOMsg& msg) {
        auto body = msg.encode();
        uint32_t len = (uint32_t)body.size();
        std::vector<uint8_t> r;
        r.push_back(len>>24); r.push_back(len>>16);
        r.push_back(len>>8);  r.push_back(len);
        r.insert(r.end(), body.begin(), body.end());
        return r;
    };
    auto f1 = frame(buildPing());
    auto f2 = frame(buildTxPoWID(MiniData(std::vector<uint8_t>(32, 0x42))));
    auto f3 = frame(buildPong());
    batch.insert(batch.end(), f1.begin(), f1.end());
    batch.insert(batch.end(), f2.begin(), f2.end());
    batch.insert(batch.end(), f3.begin(), f3.end());
    raw_write_all(raw_fd, batch);
    close(raw_fd);

    int status; waitpid(pid, &status, 0);
    CHECK(WEXITSTATUS(status) == 0);
}

TEST_CASE("bidirectional: 3-message exchange") {
    NIOServer server(0);
    server.bind();
    uint16_t port = server.port();

    pid_t pid = fork();
    REQUIRE(pid >= 0);

    if (pid == 0) {
        server.acceptOne([](NIOClient& peer) {
            peer.setRecvTimeout(2000);
            auto m1 = peer.receive(); if (m1.type != MsgType::SINGLE_PING) _exit(20);
            auto m2 = peer.receive(); if (m2.type != MsgType::PULSE)       _exit(21);
            auto m3 = peer.receive(); if (m3.type != MsgType::TXPOWID)     _exit(22);

            peer.send(buildPong());
            peer.send(buildPulse(MiniNumber(int64_t(999))));
            peer.send(buildTxPoWID(MiniData(std::vector<uint8_t>(32, 0xBB))));
        });
        _exit(0);
    }

    NIOClient client("127.0.0.1", port);
    client.setRecvTimeout(2000);
    client.connect();

    client.send(buildPing());
    client.send(buildPulse(MiniNumber(int64_t(123))));
    client.send(buildTxPoWID(MiniData(std::vector<uint8_t>(32, 0xAA))));

    auto r1 = client.receive(); CHECK(r1.type == MsgType::SINGLE_PONG);
    auto r2 = client.receive(); CHECK(r2.type == MsgType::PULSE);
    auto r3 = client.receive(); CHECK(r3.type == MsgType::TXPOWID);

    size_t off = 0;
    auto pulse_val = MiniNumber::deserialise(r2.payload.data(), off);
    CHECK(pulse_val.getAsLong() == 999);
    CHECK(r3.payload == std::vector<uint8_t>(32, 0xBB));

    int status; waitpid(pid, &status, 0);
    CHECK(WEXITSTATUS(status) == 0);
}

TEST_CASE("NIOServer port=0 gets ephemeral port > 1024") {
    NIOServer server(0);
    server.bind();
    CHECK(server.port() > 1024);
}

} // TEST_SUITE TCP loopback
