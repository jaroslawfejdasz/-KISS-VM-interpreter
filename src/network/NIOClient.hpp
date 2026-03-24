#pragma once
/**
 * NIOClient — TCP client that connects to a Minima node.
 *
 * Java ref: org.minima.system.network.minima.NIOClientInfo
 *           org.minima.system.network.minima.NIOManager
 *
 * Wire framing (Java DataOutputStream):
 *   Each message is length-prefixed:
 *     [4 bytes big-endian: payload length] [payload bytes]
 *   Payload = NIOMessage::encode() output (1 byte type + body).
 *
 * Default Minima P2P port: 9001 (mainnet), 7001 (testnet).
 *
 * Usage:
 *   NIOClient client("185.16.38.116", 9001);
 *   client.connect();
 *   client.sendGreeting(myGreeting);
 *   NIOMsg msg = client.receive();
 */

#include "NIOMessage.hpp"
#include "../objects/Greeting.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <functional>

// POSIX sockets
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace minima {
namespace network {

class NIOClient {
public:
    NIOClient(std::string host, uint16_t port)
        : host_(std::move(host)), port_(port) {}

    ~NIOClient() { disconnect(); }

    // Non-copyable
    NIOClient(const NIOClient&) = delete;
    NIOClient& operator=(const NIOClient&) = delete;

    /** Connect to the remote node. Throws on failure. */
    void connect() {
        if (fd_ >= 0) return; // already connected

        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags    = AI_ADDRCONFIG;

        std::string portStr = std::to_string(port_);
        int err = getaddrinfo(host_.c_str(), portStr.c_str(), &hints, &res);
        if (err != 0)
            throw std::runtime_error("NIOClient::connect getaddrinfo: " +
                                     std::string(gai_strerror(err)));

        struct addrinfo* p = res;
        for (; p != nullptr; p = p->ai_next) {
            fd_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd_ < 0) continue;
            if (::connect(fd_, p->ai_addr, p->ai_addrlen) == 0) break;
            ::close(fd_);
            fd_ = -1;
        }
        freeaddrinfo(res);

        if (fd_ < 0)
            throw std::runtime_error("NIOClient::connect failed to " +
                                     host_ + ":" + portStr +
                                     " — " + strerror(errno));
        connected_ = true;
    }

    /** Disconnect and close socket. */
    void disconnect() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        connected_ = false;
    }

    bool isConnected() const { return connected_; }

    /**
     * Send a NIOMsg over the wire.
     * Wire format: [uint32 BE length][payload bytes]
     */
    void send(const NIOMsg& msg) {
        auto payload = msg.encode();
        sendFramed(payload);
    }

    /**
     * Receive one framed NIOMsg.
     * Blocks until a full message is received.
     */
    NIOMsg receive() {
        // Read 4-byte length prefix
        uint8_t lenBuf[4];
        recvFull(lenBuf, 4);
        uint32_t length = (static_cast<uint32_t>(lenBuf[0]) << 24) |
                          (static_cast<uint32_t>(lenBuf[1]) << 16) |
                          (static_cast<uint32_t>(lenBuf[2]) <<  8) |
                           static_cast<uint32_t>(lenBuf[3]);

        if (length == 0 || length > kMaxMessageSize)
            throw std::runtime_error("NIOClient::receive: invalid length " +
                                     std::to_string(length));

        std::vector<uint8_t> payload(length);
        recvFull(payload.data(), length);
        return NIOMsg::decode(payload.data(), payload.size());
    }

    /** Set receive timeout in milliseconds (0 = no timeout). */
    void setRecvTimeout(int milliseconds) {
        struct timeval tv{};
        tv.tv_sec  = milliseconds / 1000;
        tv.tv_usec = (milliseconds % 1000) * 1000;
        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    const std::string& host() const { return host_; }
    uint16_t           port() const { return port_; }

    // ── Convenience send helpers ──────────────────────────────────────────

    void sendPing() {
        send(NIOMsg{MsgType::PING, {}});
    }

    void sendGreeting(const Greeting& g) {
        auto bytes = g.serialise();
        send(NIOMsg{MsgType::GREETING, bytes});
    }

    void sendTxPoWIDRequest(const MiniData& txpowId) {
        auto bytes = txpowId.serialise();
        send(NIOMsg{MsgType::TXPOWREQ, bytes});
    }

    void sendIBDRequest(int64_t fromBlock, int64_t toBlock) {
        // IBD_REQ payload: [fromBlock MiniNumber][toBlock MiniNumber]
        DataStream ds;
        ds.writeBytes(MiniNumber(fromBlock).serialise());
        ds.writeBytes(MiniNumber(toBlock).serialise());
        send(NIOMsg{MsgType::IBD_REQ, ds.buffer()});
    }


    /**
     * Inject an already-connected file descriptor (used by NIOServer).
     * Takes ownership — will close on disconnect/destroy.
     */
    void injectFd(int fd) {
        if (fd_ >= 0) ::close(fd_);
        fd_        = fd;
        connected_ = true;
    }

private:
    static constexpr uint32_t kMaxMessageSize = 64 * 1024 * 1024; // 64 MB

    std::string host_;
    uint16_t    port_;
    int         fd_        = -1;
    bool        connected_ = false;

    /** Send length-prefixed frame. */
    void sendFramed(const std::vector<uint8_t>& payload) {
        uint32_t length = static_cast<uint32_t>(payload.size());
        uint8_t  lenBuf[4] = {
            static_cast<uint8_t>((length >> 24) & 0xFF),
            static_cast<uint8_t>((length >> 16) & 0xFF),
            static_cast<uint8_t>((length >>  8) & 0xFF),
            static_cast<uint8_t>( length        & 0xFF),
        };
        sendFull(lenBuf, 4);
        sendFull(payload.data(), payload.size());
    }

    void sendFull(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        size_t sent = 0;
        while (sent < size) {
            ssize_t n = ::send(fd_, ptr + sent, size - sent, MSG_NOSIGNAL);
            if (n <= 0) {
                connected_ = false;
                throw std::runtime_error("NIOClient::send failed: " +
                                         std::string(strerror(errno)));
            }
            sent += static_cast<size_t>(n);
        }
    }

    void recvFull(uint8_t* buf, size_t size) {
        size_t received = 0;
        while (received < size) {
            ssize_t n = ::recv(fd_, buf + received, size - received, 0);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) {
                    // Receive timeout — connection may still be alive
                    throw std::runtime_error("NIOClient::recv timeout");
                }
                connected_ = false;
                throw std::runtime_error("NIOClient::recv failed: " +
                                         std::string(strerror(errno)));
            }
            if (n == 0) {
                connected_ = false;
                throw std::runtime_error("NIOClient::recv: connection closed by peer");
            }
            received += static_cast<size_t>(n);
        }
    }
};

} // namespace network
} // namespace minima
