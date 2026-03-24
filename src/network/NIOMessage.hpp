#pragma once
/**
 * NIOMessage — Minima P2P wire protocol message types.
 *
 * Java ref: NIOMessage.java (org.minima.system.network.minima)
 *
 * Wire format for each message on the TCP stream:
 *   [4 bytes big-endian: payload length] [1 byte: msgType] [payload bytes...]
 *
 * NIOMsg::encode() returns ONLY [1-byte type][body] — the length prefix
 * is added by NIOClient::sendFramed().
 * NIOMsg::decode() expects ONLY [1-byte type][body] — length has already
 * been read by NIOClient::receive().
 */
#include "../serialization/DataStream.hpp"
#include "../objects/TxPoW.hpp"
#include "../objects/Greeting.hpp"
#include "../objects/TxBlock.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

namespace minima {
namespace network {

// ── Message type codes (Java ref: NIOMessage.java) ────────────────────────────
enum class MsgType : uint8_t {
    GREETING        = 0,
    IBD             = 1,   // Initial Blockchain Download
    TXPOWID         = 2,
    TXPOWREQ        = 3,
    TXPOW           = 4,
    GENMESSAGE      = 5,
    PULSE           = 6,
    P2P             = 7,
    PING            = 8,
    MAXIMA_CTRL     = 9,
    MAXIMA_TXPOW    = 10,
    SINGLE_PING     = 11,
    SINGLE_PONG     = 12,
    IBD_REQ         = 13,
    IBD_RESP        = 14,
    ARCHIVE_REQ     = 15,
    ARCHIVE_DATA    = 16,
    ARCHIVE_SINGLE_REQ = 17,
    TXBLOCKID       = 18,
    TXBLOCKREQ      = 19,
    TXBLOCK         = 20,
    TXBLOCKMINE     = 21,
    MEGAMMRSYNC_REQ  = 22,
    MEGAMMRSYNC_RESP = 23,
};

inline const char* msgTypeName(MsgType t) {
    switch (t) {
        case MsgType::GREETING:         return "GREETING";
        case MsgType::IBD:              return "IBD";
        case MsgType::TXPOWID:          return "TXPOWID";
        case MsgType::TXPOWREQ:         return "TXPOWREQ";
        case MsgType::TXPOW:            return "TXPOW";
        case MsgType::GENMESSAGE:       return "GENMESSAGE";
        case MsgType::PULSE:            return "PULSE";
        case MsgType::P2P:              return "P2P";
        case MsgType::PING:             return "PING";
        case MsgType::MAXIMA_CTRL:      return "MAXIMA_CTRL";
        case MsgType::MAXIMA_TXPOW:     return "MAXIMA_TXPOW";
        case MsgType::SINGLE_PING:      return "SINGLE_PING";
        case MsgType::SINGLE_PONG:      return "SINGLE_PONG";
        case MsgType::IBD_REQ:          return "IBD_REQ";
        case MsgType::IBD_RESP:         return "IBD_RESP";
        case MsgType::ARCHIVE_REQ:      return "ARCHIVE_REQ";
        case MsgType::ARCHIVE_DATA:     return "ARCHIVE_DATA";
        case MsgType::TXBLOCKID:        return "TXBLOCKID";
        case MsgType::TXBLOCKREQ:       return "TXBLOCKREQ";
        case MsgType::TXBLOCK:          return "TXBLOCK";
        case MsgType::TXBLOCKMINE:      return "TXBLOCKMINE";
        case MsgType::MEGAMMRSYNC_REQ:  return "MEGAMMRSYNC_REQ";
        case MsgType::MEGAMMRSYNC_RESP: return "MEGAMMRSYNC_RESP";
        default:                         return "UNKNOWN";
    }
}

// ── Wire message envelope ─────────────────────────────────────────────────────
struct NIOMsg {
    MsgType              type;
    std::vector<uint8_t> payload;

    NIOMsg() : type(MsgType::PING) {}
    NIOMsg(MsgType t, std::vector<uint8_t> p)
        : type(t), payload(std::move(p)) {}

    /**
     * Encode to [1-byte type][payload].
     * The 4-byte length prefix is added by NIOClient::sendFramed().
     */
    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> out;
        out.reserve(1 + payload.size());
        out.push_back(static_cast<uint8_t>(type));
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    /**
     * Decode from [1-byte type][payload].
     * Caller has already read and consumed the 4-byte length prefix.
     */
    static NIOMsg decode(const uint8_t* data, size_t size) {
        if (size < 1) throw std::runtime_error("NIOMsg: empty message");
        MsgType t = static_cast<MsgType>(data[0]);
        std::vector<uint8_t> p(data + 1, data + size);
        return NIOMsg(t, std::move(p));
    }
};

// ── Message builders ──────────────────────────────────────────────────────────

// GREETING: send our Greeting object
inline NIOMsg buildGreeting(const Greeting& g) {
    auto bytes = g.serialise();
    return NIOMsg(MsgType::GREETING, bytes);
}

// TXPOWID: broadcast a new TxPoW ID (flood-fill propagation)
inline NIOMsg buildTxPoWID(const MiniData& txpowid) {
    auto bytes = txpowid.bytes();
    return NIOMsg(MsgType::TXPOWID, bytes);
}

// TXPOWREQ: request a TxPoW by ID
inline NIOMsg buildTxPoWReq(const MiniData& txpowid) {
    auto bytes = txpowid.bytes();
    return NIOMsg(MsgType::TXPOWREQ, bytes);
}

// TXPOW: send full TxPoW (response to TXPOWREQ or new block)
inline NIOMsg buildTxPoW(const TxPoW& txpow) {
    auto bytes = txpow.serialise();
    return NIOMsg(MsgType::TXPOW, bytes);
}

// PING / PONG (single)
inline NIOMsg buildPing() {
    // Java NIOMessage expects MiniData.ReadFromStream(dis) as SINGLE_PING payload.
    // MiniData.ZERO_TXPOWID = 4-byte length (=1) + 0x00
    std::vector<uint8_t> payload = {0x00, 0x00, 0x00, 0x01, 0x00};
    return NIOMsg(MsgType::SINGLE_PING, payload);
}
inline NIOMsg buildPong()      { return NIOMsg(MsgType::SINGLE_PONG, {}); }

// PULSE: keep-alive with current block number
inline NIOMsg buildPulse(const MiniNumber& blockNum) {
    auto bytes = blockNum.serialise();
    return NIOMsg(MsgType::PULSE, bytes);
}

// TXBLOCKID: advertise a TxBlock ID
inline NIOMsg buildTxBlockID(const MiniData& txblockid) {
    return NIOMsg(MsgType::TXBLOCKID, txblockid.bytes());
}

// TXBLOCKREQ: request a TxBlock by ID
inline NIOMsg buildTxBlockReq(const MiniData& txblockid) {
    return NIOMsg(MsgType::TXBLOCKREQ, txblockid.bytes());
}

} // namespace network
} // namespace minima
