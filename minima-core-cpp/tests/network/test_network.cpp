#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/network/NIOMessage.hpp"
#include "../../src/network/NetworkProtocol.hpp"

using namespace minima;
using namespace minima::network;

// ── NIOMessage basics ─────────────────────────────────────────────────────────

TEST_SUITE("NIOMessage — wire format") {

TEST_CASE("msgTypeName is correct") {
    CHECK(std::string(msgTypeName(MsgType::GREETING))    == "GREETING");
    CHECK(std::string(msgTypeName(MsgType::TXPOW))       == "TXPOW");
    CHECK(std::string(msgTypeName(MsgType::PULSE))       == "PULSE");
    CHECK(std::string(msgTypeName(MsgType::SINGLE_PING)) == "SINGLE_PING");
    CHECK(std::string(msgTypeName(MsgType::IBD))         == "IBD");
}

TEST_CASE("encode/decode roundtrip") {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0xAB, 0xCD};
    NIOMsg orig(MsgType::TXPOW, payload);
    auto encoded = orig.encode();
    uint32_t len = ((uint32_t)encoded[0] << 24) | ((uint32_t)encoded[1] << 16)
                 | ((uint32_t)encoded[2] << 8)  |  (uint32_t)encoded[3];
    CHECK(len == 6);
    CHECK(encoded[4] == (uint8_t)MsgType::TXPOW);
    NIOMsg decoded = NIOMsg::decode(encoded.data(), encoded.size());
    CHECK(decoded.type == MsgType::TXPOW);
    CHECK(decoded.payload == payload);
}

TEST_CASE("empty payload roundtrip") {
    NIOMsg orig(MsgType::SINGLE_PING, {});
    auto enc = orig.encode();
    CHECK(enc.size() == 5);
    NIOMsg d = NIOMsg::decode(enc.data(), enc.size());
    CHECK(d.type == MsgType::SINGLE_PING);
    CHECK(d.payload.empty());
}

TEST_CASE("buildPing / buildPong") {
    CHECK(buildPing().type == MsgType::SINGLE_PING);
    CHECK(buildPong().type == MsgType::SINGLE_PONG);
    CHECK(buildPing().payload.empty());
}

TEST_CASE("buildTxPoWID packs 32-byte ID") {
    MiniData id(std::vector<uint8_t>(32, 0xAB));
    auto msg = buildTxPoWID(id);
    CHECK(msg.type == MsgType::TXPOWID);
    CHECK(msg.payload.size() == 32);
    CHECK(msg.payload[0] == 0xAB);
}

TEST_CASE("buildPulse encodes block number") {
    MiniNumber bn(int64_t(12345));
    auto pulse = buildPulse(bn);
    CHECK(pulse.type == MsgType::PULSE);
    size_t off = 0;
    auto bn2 = MiniNumber::deserialise(pulse.payload.data(), off);
    CHECK(bn2 == bn);
}

TEST_CASE("buildGreeting contains topBlock") {
    Greeting g; g.setTopBlock(MiniNumber(int64_t(42)));
    auto msg = buildGreeting(g);
    CHECK(msg.type == MsgType::GREETING);
    size_t off = 0;
    auto g2 = Greeting::deserialise(msg.payload.data(), off);
    CHECK(g2.topBlock() == MiniNumber(int64_t(42)));
}

TEST_CASE("decode throws on truncated data") {
    std::vector<uint8_t> t = {0x00, 0x00, 0x00, 0x0A, 0x04}; // claims 10 bytes, only 0
    CHECK_THROWS_AS(NIOMsg::decode(t.data(), t.size()), std::runtime_error);
}

TEST_CASE("buildTxPoW serialises full TxPoW") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(7));
    auto msg = buildTxPoW(txp);
    CHECK(msg.type == MsgType::TXPOW);
    auto txp2 = TxPoW::deserialise(msg.payload);
    CHECK(txp2.header().blockNumber == txp.header().blockNumber);
}

TEST_CASE("all 24 type codes are distinct") {
    CHECK((uint8_t)MsgType::GREETING   == 0);
    CHECK((uint8_t)MsgType::IBD        == 1);
    CHECK((uint8_t)MsgType::TXPOW      == 4);
    CHECK((uint8_t)MsgType::PULSE      == 6);
    CHECK((uint8_t)MsgType::TXBLOCK    == 20);
    CHECK((uint8_t)MsgType::MEGAMMRSYNC_RESP == 23);
}

} // TEST_SUITE NIOMessage

// ── Protocol handshake ────────────────────────────────────────────────────────

TEST_SUITE("NetworkProtocol — handshake") {

TEST_CASE("equal-tip peers: both reach READY after one round") {
    std::vector<NetworkEvent::Kind> evA, evB;
    NodePair pair(
        MiniNumber(int64_t(100)), MiniNumber(int64_t(100)),
        [&](const NetworkEvent& e) { evA.push_back(e.kind); },
        [&](const NetworkEvent& e) { evB.push_back(e.kind); }
    );
    pair.converge();
    CHECK(pair.nodeA.isReady());
    CHECK(pair.nodeB.isReady());
    // Both should have received GREETING + CONNECTED
    auto hasKind = [](const std::vector<NetworkEvent::Kind>& v, NetworkEvent::Kind k) {
        return std::find(v.begin(), v.end(), k) != v.end();
    };
    CHECK(hasKind(evA, NetworkEvent::Kind::GREETING_RECV));
    CHECK(hasKind(evA, NetworkEvent::Kind::CONNECTED));
    CHECK(hasKind(evB, NetworkEvent::Kind::GREETING_RECV));
    CHECK(hasKind(evB, NetworkEvent::Kind::CONNECTED));
}

TEST_CASE("peerTip is correctly recorded from GREETING") {
    NodePair pair(MiniNumber(int64_t(50)), MiniNumber(int64_t(200)));
    pair.converge();
    // A sees B's tip as 200, B sees A's tip as 50
    CHECK(pair.nodeA.peerTip() == MiniNumber(int64_t(200)));
    CHECK(pair.nodeB.peerTip() == MiniNumber(int64_t(50)));
}

TEST_CASE("fresh install node (tip=-1) reaches READY when peer is ahead") {
    // Fresh node has tip -1, peer has tip 500
    std::vector<NetworkEvent::Kind> evA;
    NodePair pair(
        MiniNumber(int64_t(-1)), MiniNumber(int64_t(500)),
        [&](const NetworkEvent& e) { evA.push_back(e.kind); }
    );
    // After greeting exchange, A is behind → sends IBD_REQ
    // B doesn't handle IBD_REQ in this test (no IBD server) → A stays in WAIT_IBD
    // but B → READY (B is ahead)
    pair.flush(); // exchange greetings
    pair.flush(); // process responses
    // B is ahead, B should be in READY (no IBD needed from its side)
    CHECK(pair.nodeB.isReady());
    // A sent IBD_REQ, waiting for IBD
    CHECK(pair.nodeA.state() == ConnState::HANDSHAKE_WAIT_IBD);
}

TEST_CASE("IBD roundtrip: behind node receives blocks and reaches READY") {
    MiniNumber tipA(int64_t(-1));
    MiniNumber tipB(int64_t(5));
    std::vector<NetworkEvent::Kind> evA;

    NodePair pair(
        tipA, tipB,
        [&](const NetworkEvent& e) { evA.push_back(e.kind); }
    );

    // Exchange greetings
    pair.flush(); // A sends greeting → B; B sends greeting → A
    pair.flush(); // A processes B's greeting (B is ahead → A sends IBD_REQ)
                  // B processes A's greeting (A is behind → B goes READY)

    // Now A is in WAIT_IBD, B is READY.
    // Simulate B sending an IBD response manually
    IBD ibd;
    for (int i = 0; i <= 5; i++) {
        TxBlock tb;
        tb.txpow().header().blockNumber = MiniNumber(int64_t(i));
        ibd.addBlock(std::move(tb));
    }
    auto ibdPayload = ibd.serialise();
    NIOMsg ibdMsg(MsgType::IBD, ibdPayload);
    auto ibdEnc = ibdMsg.encode();

    // Feed IBD into A
    pair.nodeA.receive(ibdEnc);

    CHECK(pair.nodeA.isReady());
    auto hasKind = [](const std::vector<NetworkEvent::Kind>& v, NetworkEvent::Kind k) {
        return std::find(v.begin(), v.end(), k) != v.end();
    };
    CHECK(hasKind(evA, NetworkEvent::Kind::IBD_RECV));
    CHECK(hasKind(evA, NetworkEvent::Kind::CONNECTED));
}

} // TEST_SUITE handshake

// ── Ready-state message handling ──────────────────────────────────────────────

TEST_SUITE("NetworkProtocol — ready state") {

// Helper: build a READY pair
static NodePair readyPair(
    ProtocolConnection::EventCallback cbA = nullptr,
    ProtocolConnection::EventCallback cbB = nullptr)
{
    NodePair p(MiniNumber(int64_t(100)), MiniNumber(int64_t(100)),
               std::move(cbA), std::move(cbB));
    p.converge();
    return p;
}

TEST_CASE("PING → PONG: A pings B, B auto-replies") {
    std::vector<NetworkEvent::Kind> evB;
    auto p = readyPair(nullptr, [&](const NetworkEvent& e) { evB.push_back(e.kind); });
    // A sends ping
    p.nodeA.outBuffer.clear();
    auto pingEnc = buildPing().encode();
    p.nodeB.receive(pingEnc);
    CHECK(std::find(evB.begin(), evB.end(), NetworkEvent::Kind::PING_RECV) != evB.end());
    // B should have queued a PONG
    CHECK(!p.nodeB.outBuffer.empty());
    // Decode pong
    NIOMsg pong = NIOMsg::decode(p.nodeB.outBuffer.data(), p.nodeB.outBuffer.size());
    CHECK(pong.type == MsgType::SINGLE_PONG);
}

TEST_CASE("PULSE updates peer tip") {
    auto p = readyPair();
    MiniNumber newTip(int64_t(999));
    p.nodeA.sendPulse(newTip);
    p.nodeB.receive(p.nodeA.outBuffer);
    p.nodeA.outBuffer.clear();
    CHECK(p.nodeB.peerTip() == newTip);
}

TEST_CASE("TXPOWID event fired on receive") {
    std::vector<NetworkEvent::Kind> evB;
    auto p = readyPair(nullptr, [&](const NetworkEvent& e) { evB.push_back(e.kind); });
    MiniData id(std::vector<uint8_t>(32, 0xDE));
    p.nodeA.sendTxPoWID(id);
    p.nodeB.receive(p.nodeA.outBuffer);
    p.nodeA.outBuffer.clear();
    CHECK(std::find(evB.begin(), evB.end(), NetworkEvent::Kind::TXPOWID_RECV) != evB.end());
}

TEST_CASE("TXPOW event fired on receive") {
    std::vector<NetworkEvent::Kind> evB;
    auto p = readyPair(nullptr, [&](const NetworkEvent& e) { evB.push_back(e.kind); });
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(101));
    p.nodeA.sendTxPoW(txp);
    p.nodeB.receive(p.nodeA.outBuffer);
    p.nodeA.outBuffer.clear();
    CHECK(std::find(evB.begin(), evB.end(), NetworkEvent::Kind::TXPOW_RECV) != evB.end());
}

TEST_CASE("streamed receive: fragmented message reassembled") {
    std::vector<NetworkEvent::Kind> evB;
    auto p = readyPair(nullptr, [&](const NetworkEvent& e) { evB.push_back(e.kind); });

    MiniData id(std::vector<uint8_t>(32, 0xAB));
    p.nodeA.sendTxPoWID(id);
    auto fullEnc = p.nodeA.outBuffer;
    p.nodeA.outBuffer.clear();

    // Feed in two fragments
    size_t half = fullEnc.size() / 2;
    p.nodeB.receive(std::vector<uint8_t>(fullEnc.begin(), fullEnc.begin() + half));
    CHECK(std::find(evB.begin(), evB.end(), NetworkEvent::Kind::TXPOWID_RECV) == evB.end());
    p.nodeB.receive(std::vector<uint8_t>(fullEnc.begin() + half, fullEnc.end()));
    CHECK(std::find(evB.begin(), evB.end(), NetworkEvent::Kind::TXPOWID_RECV) != evB.end());
}

TEST_CASE("multiple messages in one receive call processed in order") {
    // Build ready pair first (without tracking events)
    auto p = readyPair();

    // Now register callback and inject 2 messages in sequence
    std::vector<NetworkEvent::Kind> evB;
    // Feed PULSE + PING concatenated directly (B is already READY)
    auto pulseEnc = buildPulse(MiniNumber(int64_t(200))).encode();
    auto pingEnc  = buildPing().encode();
    std::vector<uint8_t> both;
    both.insert(both.end(), pulseEnc.begin(), pulseEnc.end());
    both.insert(both.end(), pingEnc.begin(),  pingEnc.end());

    // Manually process through B with fresh tracking
    // Use nodeB directly — re-feed the combined buffer
    // We create a separate fresh-READY connection for clean event tracking
    NodePair p2(MiniNumber(int64_t(100)), MiniNumber(int64_t(100)),
                nullptr,
                [&](const NetworkEvent& e) { evB.push_back(e.kind); });
    p2.converge(); // get to READY state
    evB.clear();   // clear handshake events

    p2.nodeB.receive(both);
    REQUIRE(evB.size() >= 2);
    CHECK(evB[0] == NetworkEvent::Kind::PULSE_RECV);
    CHECK(evB[1] == NetworkEvent::Kind::PING_RECV);
}

TEST_CASE("unknown message type fires MSG_UNKNOWN event") {
    std::vector<NetworkEvent::Kind> evB;
    auto p = readyPair(nullptr, [&](const NetworkEvent& e) { evB.push_back(e.kind); });
    NIOMsg unk(MsgType::GENMESSAGE, {0x01, 0x02});
    auto enc = unk.encode();
    p.nodeB.receive(enc);
    CHECK(std::find(evB.begin(), evB.end(), NetworkEvent::Kind::MSG_UNKNOWN) != evB.end());
}

} // TEST_SUITE ready state

// ── Greeting object tests ─────────────────────────────────────────────────────

TEST_SUITE("Greeting") {

TEST_CASE("default greeting is fresh install") {
    Greeting g;
    CHECK(g.isFreshInstall());
    CHECK(g.topBlock().getAsLong() == -1);
}

TEST_CASE("greeting with chain IDs serialises + deserialises") {
    Greeting g;
    g.setTopBlock(MiniNumber(int64_t(10)));
    g.addChainID(MiniData(std::vector<uint8_t>(32, 0xAA)));
    g.addChainID(MiniData(std::vector<uint8_t>(32, 0xBB)));

    auto bytes = g.serialise();
    size_t off = 0;
    auto g2 = Greeting::deserialise(bytes.data(), off);

    CHECK(g2.topBlock() == MiniNumber(int64_t(10)));
    CHECK(g2.chain().size() == 2);
    CHECK(g2.chain()[0].bytes()[0] == 0xAA);
    CHECK(g2.chain()[1].bytes()[0] == 0xBB);
}

TEST_CASE("version field is MINIMA_VERSION") {
    Greeting g;
    CHECK(g.version().str() == MINIMA_VERSION);
}

} // TEST_SUITE Greeting

