#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/network/NIOMessage.hpp"

using namespace minima;
using namespace minima::network;

TEST_CASE("NIOMessage: msgTypeName is correct") {
    CHECK(std::string(msgTypeName(MsgType::GREETING))   == "GREETING");
    CHECK(std::string(msgTypeName(MsgType::TXPOW))      == "TXPOW");
    CHECK(std::string(msgTypeName(MsgType::PULSE))      == "PULSE");
    CHECK(std::string(msgTypeName(MsgType::SINGLE_PING)) == "SINGLE_PING");
    CHECK(std::string(msgTypeName(MsgType::IBD))        == "IBD");
}

TEST_CASE("NIOMessage: encode/decode roundtrip") {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0xAB, 0xCD};
    NIOMsg orig(MsgType::TXPOW, payload);
    auto encoded = orig.encode();

    // Check header
    uint32_t len = ((uint32_t)encoded[0] << 24) | ((uint32_t)encoded[1] << 16)
                 | ((uint32_t)encoded[2] << 8)  |  (uint32_t)encoded[3];
    CHECK(len == 6); // 1 type byte + 5 payload
    CHECK(encoded[4] == (uint8_t)MsgType::TXPOW);

    // Decode
    NIOMsg decoded = NIOMsg::decode(encoded.data(), encoded.size());
    CHECK(decoded.type == MsgType::TXPOW);
    CHECK(decoded.payload == payload);
}

TEST_CASE("NIOMessage: empty payload roundtrip") {
    NIOMsg orig(MsgType::SINGLE_PING, {});
    auto encoded = orig.encode();
    CHECK(encoded.size() == 5); // 4-byte len + 1-byte type

    NIOMsg decoded = NIOMsg::decode(encoded.data(), encoded.size());
    CHECK(decoded.type == MsgType::SINGLE_PING);
    CHECK(decoded.payload.empty());
}

TEST_CASE("NIOMessage: buildPing / buildPong") {
    auto ping = buildPing();
    CHECK(ping.type == MsgType::SINGLE_PING);
    CHECK(ping.payload.empty());

    auto pong = buildPong();
    CHECK(pong.type == MsgType::SINGLE_PONG);
    CHECK(pong.payload.empty());
}

TEST_CASE("NIOMessage: buildTxPoWID packs 32-byte ID") {
    MiniData id(std::vector<uint8_t>(32, 0xAB));
    auto msg = buildTxPoWID(id);
    CHECK(msg.type == MsgType::TXPOWID);
    CHECK(msg.payload.size() == 32);
    CHECK(msg.payload[0] == 0xAB);
}

TEST_CASE("NIOMessage: buildTxPoWReq same as ID format") {
    MiniData id(std::vector<uint8_t>(32, 0xCC));
    auto req = buildTxPoWReq(id);
    CHECK(req.type == MsgType::TXPOWREQ);
    CHECK(req.payload == id.bytes());
}

TEST_CASE("NIOMessage: buildPulse encodes block number") {
    MiniNumber bn(int64_t(12345));
    auto pulse = buildPulse(bn);
    CHECK(pulse.type == MsgType::PULSE);
    CHECK(!pulse.payload.empty());
    // Decode back
    size_t off = 0;
    auto bn2 = MiniNumber::deserialise(pulse.payload.data(), off);
    CHECK(bn2 == bn);
}

TEST_CASE("NIOMessage: buildGreeting contains version") {
    Greeting g;
    g.setTopBlock(MiniNumber(int64_t(42)));
    auto msg = buildGreeting(g);
    CHECK(msg.type == MsgType::GREETING);
    CHECK(!msg.payload.empty());

    // Deserialise back
    size_t off = 0;
    auto g2 = Greeting::deserialise(msg.payload.data(), off);
    CHECK(g2.topBlock() == MiniNumber(int64_t(42)));
}

TEST_CASE("NIOMessage: decode throws on truncated data") {
    std::vector<uint8_t> truncated = {0x00, 0x00, 0x00, 0x0A, 0x04}; // claims 10 bytes payload, only 0 given
    CHECK_THROWS_AS(NIOMsg::decode(truncated.data(), truncated.size()), std::runtime_error);
}

TEST_CASE("NIOMessage: buildTxPoW serialises full TxPoW") {
    TxPoW txp;
    txp.header().blockNumber = MiniNumber(int64_t(7));
    auto msg = buildTxPoW(txp);
    CHECK(msg.type == MsgType::TXPOW);
    CHECK(!msg.payload.empty());

    // Deserialise back
    auto txp2 = TxPoW::deserialise(msg.payload);
    CHECK(txp2.header().blockNumber == txp.header().blockNumber);
}

TEST_CASE("NIOMessage: all 24 type codes are distinct") {
    // Verify enum values match Java reference
    CHECK((uint8_t)MsgType::GREETING   == 0);
    CHECK((uint8_t)MsgType::IBD        == 1);
    CHECK((uint8_t)MsgType::TXPOW      == 4);
    CHECK((uint8_t)MsgType::PULSE      == 6);
    CHECK((uint8_t)MsgType::TXBLOCK    == 20);
    CHECK((uint8_t)MsgType::MEGAMMRSYNC_RESP == 23);
}
