#pragma once
/**
 * NIOServer — TCP server that accepts incoming Minima node connections.
 *
 * Java ref: org.minima.system.network.minima.NIOServer
 *           org.minima.system.network.minima.NIOManager
 *
 * Design:
 *   - Single-threaded accept loop (call acceptOne() or runForever())
 *   - Each accepted connection is wrapped in NIOClient for send/receive
 *   - Caller provides a handler callback: void(NIOClient&)
 *
 * Usage:
 *   NIOServer server(9001);
 *   server.bind();
 *   server.runForever([](NIOClient& peer) {
 *       auto msg = peer.receive();
 *       // handle msg
 *   });
 */

#include "NIOClient.hpp"
#include <functional>
#include <memory>
#include <string>
#include <cstring>
#include <stdexcept>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace minima {
namespace network {

using PeerHandler = std::function<void(NIOClient&)>;

class NIOServer {
public:
    explicit NIOServer(uint16_t port, std::string bindAddr = "0.0.0.0")
        : port_(port), bindAddr_(std::move(bindAddr)) {}

    ~NIOServer() { stop(); }

    NIOServer(const NIOServer&) = delete;
    NIOServer& operator=(const NIOServer&) = delete;

    /** Bind and listen. Throws on failure. */
    void bind() {
        listenFd_ = ::socket(AF_INET6, SOCK_STREAM, 0);
        bool v6ok = listenFd_ >= 0;

        if (!v6ok) {
            // Fallback to IPv4
            listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (listenFd_ < 0)
                throw std::runtime_error("NIOServer::bind socket(): " +
                                         std::string(strerror(errno)));
            bindIPv4();
        } else {
            // Try dual-stack IPv6
            int off = 0;
            setsockopt(listenFd_, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
            bindIPv6();
        }

        int reuseAddr = 1;
        setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

        if (::listen(listenFd_, kBacklog) < 0)
            throw std::runtime_error("NIOServer::bind listen(): " +
                                     std::string(strerror(errno)));
        running_ = true;
    }

    /** Accept one incoming connection, call handler, return. */
    void acceptOne(const PeerHandler& handler) {
        struct sockaddr_storage peerAddr{};
        socklen_t addrLen = sizeof(peerAddr);
        int peerFd = ::accept(listenFd_,
                              reinterpret_cast<sockaddr*>(&peerAddr), &addrLen);
        if (peerFd < 0) {
            if (errno == EINTR || errno == EAGAIN) return;
            throw std::runtime_error("NIOServer::acceptOne accept(): " +
                                     std::string(strerror(errno)));
        }

        // Wrap in NIOClient
        std::string peerHost = getPeerHost(peerAddr);
        NIOClient peer(peerHost, port_);
        // Inject the accepted fd by subclassing trick — we expose via friend
        peer.injectFd(peerFd);

        try {
            handler(peer);
        } catch (const std::exception& e) {
            // Don't propagate — just close this connection
            (void)e;
        }
    }

    /** Run accept loop until stop() is called. */
    void runForever(const PeerHandler& handler) {
        while (running_) {
            acceptOne(handler);
        }
    }

    void stop() {
        running_ = false;
        if (listenFd_ >= 0) {
            ::close(listenFd_);
            listenFd_ = -1;
        }
    }

    uint16_t port() const { return port_; }

private:
    static constexpr int kBacklog = 64;

    uint16_t    port_;
    std::string bindAddr_;
    int         listenFd_ = -1;
    bool        running_  = false;

    void bindIPv4() {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port_);
        inet_pton(AF_INET, bindAddr_.c_str(), &addr.sin_addr);
        if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("NIOServer::bind bind() IPv4: " +
                                     std::string(strerror(errno)));
    }

    void bindIPv6() {
        struct sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port   = htons(port_);
        addr.sin6_addr   = in6addr_any;
        if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("NIOServer::bind bind() IPv6: " +
                                     std::string(strerror(errno)));
    }

    static std::string getPeerHost(const struct sockaddr_storage& addr) {
        char buf[128] = {};
        if (addr.ss_family == AF_INET) {
            inet_ntop(AF_INET,
                      &reinterpret_cast<const sockaddr_in&>(addr).sin_addr,
                      buf, sizeof(buf));
        } else if (addr.ss_family == AF_INET6) {
            inet_ntop(AF_INET6,
                      &reinterpret_cast<const sockaddr_in6&>(addr).sin6_addr,
                      buf, sizeof(buf));
        }
        return buf;
    }
};

} // namespace network
} // namespace minima
