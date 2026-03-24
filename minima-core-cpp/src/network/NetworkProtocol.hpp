#pragma once
/**
 * NetworkProtocol — Minima P2P protocol state machine.
 *
 * Java ref: NIOClientInfo.java, NIOManager.java, NIOReader.java
 *
 * Models the per-connection state and message handling logic.
 * Does NOT use real sockets — uses byte pipes for testability.
 *
 * Handshake sequence:
 *   → GREETING (us)
 *   ← GREETING (peer)
 *   → IBD_REQ  (if peer is ahead)
 *   ← IBD      (peer sends chain)
 *
 * After handshake, the connection enters READY state and handles:
 *   TXPOWID → request TXPOW if unknown
 *   TXPOW   → validate + store
 *   PULSE   → update known tip
 *   PING    → reply PONG
 */
#include "NIOMessage.hpp"
#include "../objects/Greeting.hpp"
#include "../objects/IBD.hpp"
#include "../types/MiniNumber.hpp"
#include <vector>
#include <functional>
#include <string>
#include <queue>
#include <optional>
#include <cstdint>

namespace minima {
namespace network {

// ── Connection state ──────────────────────────────────────────────────────────
enum class ConnState {
    HANDSHAKE_WAIT_GREETING,  // sent our greeting, waiting for peer's
    HANDSHAKE_WAIT_IBD,       // sent IBD_REQ, waiting for IBD
    READY,                    // fully connected, normal message flow
    CLOSED,
};

inline const char* connStateName(ConnState s) {
    switch (s) {
        case ConnState::HANDSHAKE_WAIT_GREETING: return "WAIT_GREETING";
        case ConnState::HANDSHAKE_WAIT_IBD:      return "WAIT_IBD";
        case ConnState::READY:                   return "READY";
        case ConnState::CLOSED:                  return "CLOSED";
        default:                                  return "UNKNOWN";
    }
}

// ── Events emitted to the application layer ───────────────────────────────────
struct NetworkEvent {
    enum class Kind {
        CONNECTED,       // handshake done
        GREETING_RECV,   // received peer greeting
        IBD_RECV,        // received IBD data
        TXPOW_RECV,      // received a TxPoW
        TXPOWID_RECV,    // received a TxPoW ID (unknown tx advertisement)
        PULSE_RECV,      // received a pulse (block height update)
        PING_RECV,       // received a ping
        MSG_UNKNOWN,     // unknown message type
        ERROR,           // protocol error
    };

    Kind        kind;
    std::string detail;

    // Optional data payloads
    std::optional<Greeting>  greeting;
    std::optional<MiniNumber> blockHeight;
    std::optional<MiniData>   txpowId;
    std::optional<IBD>        ibd;

    NetworkEvent() = default;
    NetworkEvent(const NetworkEvent&) = delete;
    NetworkEvent& operator=(const NetworkEvent&) = delete;
    NetworkEvent(NetworkEvent&&) = default;
    NetworkEvent& operator=(NetworkEvent&&) = default;
};

// ── Protocol state machine for one connection ─────────────────────────────────
class ProtocolConnection {
public:
    using EventCallback = std::function<void(const NetworkEvent&)>;

    // outBuffer: bytes to send to peer
    std::vector<uint8_t> outBuffer;

    explicit ProtocolConnection(MiniNumber localTip, EventCallback cb = nullptr)
        : m_localTip(localTip)
        , m_state(ConnState::HANDSHAKE_WAIT_GREETING)
        , m_cb(std::move(cb))
    {
        // Immediately send our Greeting
        _sendGreeting();
    }

    ConnState state() const { return m_state; }
    bool isReady()    const { return m_state == ConnState::READY; }
    bool isClosed()   const { return m_state == ConnState::CLOSED; }

    const MiniNumber& peerTip() const { return m_peerTip; }

    // ── Feed incoming bytes ───────────────────────────────────────────────────
    // Call this whenever bytes arrive from the peer.
    // Returns false if connection should be closed.
    bool receive(const std::vector<uint8_t>& bytes) {
        m_recvBuf.insert(m_recvBuf.end(), bytes.begin(), bytes.end());
        return _processRecvBuf();
    }

    // ── Send helpers (called by application layer) ────────────────────────────
    void sendTxPoWID(const MiniData& id) {
        _enqueue(buildTxPoWID(id));
    }
    void sendTxPoW(const TxPoW& txpow) {
        _enqueue(buildTxPoW(txpow));
    }
    void sendPulse(const MiniNumber& blockNum) {
        _enqueue(buildPulse(blockNum));
    }
    void sendTxPoWReq(const MiniData& id) {
        _enqueue(buildTxPoWReq(id));
    }

private:
    MiniNumber      m_localTip;
    MiniNumber      m_peerTip  { int64_t(-1) };
    ConnState       m_state;
    EventCallback   m_cb;

    std::vector<uint8_t> m_recvBuf;

    void _emit(NetworkEvent ev) {
        if (m_cb) m_cb(ev);
    }

    void _enqueue(const NIOMsg& msg) {
        auto enc = msg.encode();
        outBuffer.insert(outBuffer.end(), enc.begin(), enc.end());
    }

    void _sendGreeting() {
        Greeting g;
        g.setTopBlock(m_localTip);
        _enqueue(buildGreeting(g));
    }

    bool _processRecvBuf() {
        while (m_recvBuf.size() >= 5) {
            // Peek at length header
            uint32_t len = ((uint32_t)m_recvBuf[0] << 24)
                         | ((uint32_t)m_recvBuf[1] << 16)
                         | ((uint32_t)m_recvBuf[2] << 8)
                         |  (uint32_t)m_recvBuf[3];
            if (m_recvBuf.size() < 4 + len) break; // wait for more data

            // Extract message
            NIOMsg msg = NIOMsg::decode(m_recvBuf.data(), 4 + len);
            m_recvBuf.erase(m_recvBuf.begin(), m_recvBuf.begin() + 4 + len);

            if (!_dispatch(msg)) return false;
        }
        return true;
    }

    bool _dispatch(const NIOMsg& msg) {
        switch (m_state) {
            case ConnState::HANDSHAKE_WAIT_GREETING:
                return _handleHandshakeGreeting(msg);
            case ConnState::HANDSHAKE_WAIT_IBD:
                return _handleHandshakeIBD(msg);
            case ConnState::READY:
                return _handleReady(msg);
            default:
                return false;
        }
    }

    bool _handleHandshakeGreeting(const NIOMsg& msg) {
        if (msg.type != MsgType::GREETING) {
            _emit({ NetworkEvent::Kind::ERROR, "Expected GREETING, got " + std::string(msgTypeName(msg.type)) });
            m_state = ConnState::CLOSED;
            return false;
        }
        try {
            size_t off = 0;
            auto g = Greeting::deserialise(msg.payload.data(), off);
            m_peerTip = g.topBlock();

            NetworkEvent ev;
            ev.kind = NetworkEvent::Kind::GREETING_RECV;
            ev.greeting = g;
            _emit(ev);

            // If peer is ahead of us, request IBD
            if (m_peerTip.getAsLong() > m_localTip.getAsLong()) {
                // Send IBD_REQ (payload: our top block as MiniNumber)
                auto reqBytes = m_localTip.serialise();
                _enqueue(NIOMsg(MsgType::IBD_REQ, reqBytes));
                m_state = ConnState::HANDSHAKE_WAIT_IBD;
            } else {
                // We're equal or ahead — go straight to READY
                m_state = ConnState::READY;
                _emit({ NetworkEvent::Kind::CONNECTED, "handshake complete (no IBD needed)" });
            }
        } catch (const std::exception& e) {
            _emit({ NetworkEvent::Kind::ERROR, std::string("Greeting parse error: ") + e.what() });
            m_state = ConnState::CLOSED;
            return false;
        }
        return true;
    }

    bool _handleHandshakeIBD(const NIOMsg& msg) {
        if (msg.type == MsgType::IBD || msg.type == MsgType::IBD_RESP) {
            try {
                auto ibd = IBD::deserialise(msg.payload);
                NetworkEvent ev;
                ev.kind = NetworkEvent::Kind::IBD_RECV;
                ev.ibd  = std::move(ibd);
                ev.detail = "IBD: " + std::to_string(ev.ibd->blockCount()) + " blocks";
                _emit(ev);
            } catch (const std::exception& e) {
                _emit({ NetworkEvent::Kind::ERROR, std::string("IBD parse error: ") + e.what() });
            }
            m_state = ConnState::READY;
            _emit({ NetworkEvent::Kind::CONNECTED, "handshake complete (IBD received)" });
            return true;
        }
        // Other messages during IBD wait — queue or handle
        return _handleReady(msg);
    }

    bool _handleReady(const NIOMsg& msg) {
        switch (msg.type) {
            case MsgType::SINGLE_PING: {
                _enqueue(buildPong());
                _emit({ NetworkEvent::Kind::PING_RECV, "ping" });
                break;
            }
            case MsgType::SINGLE_PONG:
                break; // ignore pongs

            case MsgType::PULSE: {
                try {
                    size_t off = 0;
                    auto bn = MiniNumber::deserialise(msg.payload.data(), off);
                    m_peerTip = bn;
                    NetworkEvent ev;
                    ev.kind = NetworkEvent::Kind::PULSE_RECV;
                    ev.blockHeight = bn;
                    ev.detail = "pulse: block " + bn.toString();
                    _emit(ev);
                } catch (...) {}
                break;
            }

            case MsgType::TXPOWID: {
                MiniData id(msg.payload);
                NetworkEvent ev;
                ev.kind = NetworkEvent::Kind::TXPOWID_RECV;
                ev.txpowId = id;
                ev.detail = "txpow_id: " + id.toHexString();
                _emit(ev);
                break;
            }

            case MsgType::TXPOW: {
                try {
                    auto txp = TxPoW::deserialise(msg.payload);
                    NetworkEvent ev;
                    ev.kind = NetworkEvent::Kind::TXPOW_RECV;
                    ev.detail = "txpow received";
                    _emit(ev);
                } catch (const std::exception& e) {
                    _emit({ NetworkEvent::Kind::ERROR, std::string("TxPoW parse: ") + e.what() });
                }
                break;
            }

            default: {
                NetworkEvent ev;
                ev.kind = NetworkEvent::Kind::MSG_UNKNOWN;
                ev.detail = std::string("unknown msg: ") + msgTypeName(msg.type);
                _emit(ev);
                break;
            }
        }
        return true;
    }
};

// ── Simulated TCP pipe (for testing) ─────────────────────────────────────────
// Two ProtocolConnections connected together in memory.
struct NodePair {
    ProtocolConnection nodeA;
    ProtocolConnection nodeB;

    NodePair(MiniNumber tipA, MiniNumber tipB,
             ProtocolConnection::EventCallback cbA = nullptr,
             ProtocolConnection::EventCallback cbB = nullptr)
        : nodeA(tipA, std::move(cbA))
        , nodeB(tipB, std::move(cbB))
    {}

    // Flush pending output from both nodes to each other.
    // Returns total bytes exchanged this round.
    size_t flush() {
        size_t total = 0;
        // A → B
        if (!nodeA.outBuffer.empty()) {
            nodeB.receive(nodeA.outBuffer);
            total += nodeA.outBuffer.size();
            nodeA.outBuffer.clear();
        }
        // B → A
        if (!nodeB.outBuffer.empty()) {
            nodeA.receive(nodeB.outBuffer);
            total += nodeB.outBuffer.size();
            nodeB.outBuffer.clear();
        }
        return total;
    }

    // Run flush until no more data flows (converged).
    void converge(int maxRounds = 20) {
        for (int i = 0; i < maxRounds; i++)
            if (flush() == 0) break;
    }
};

} // namespace network
} // namespace minima
