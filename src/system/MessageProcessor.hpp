#pragma once
/**
 * MessageProcessor — asynchroniczna kolejka zdarzeń.
 * Java ref: src/org/minima/utils/messages/MessageProcessor.java
 *
 * Każdy komponent (TxPoWProcessor, NIOManager itp.) rozszerza tę klasę.
 * Wiadomości wchodzą do kolejki, worker thread je przetwarza po kolei.
 */
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <string>
#include <atomic>
#include <memory>

namespace minima::system {

// ── Message ──────────────────────────────────────────────────────────────────

struct Message {
    std::string type;
    std::function<void()> handler;

    explicit Message(std::string t, std::function<void()> h = {})
        : type(std::move(t)), handler(std::move(h)) {}
};

// ── MessageProcessor ─────────────────────────────────────────────────────────

class MessageProcessor {
public:
    explicit MessageProcessor(std::string name)
        : m_name(std::move(name))
        , m_running(false)
    {}

    virtual ~MessageProcessor() { stop(); }

    void start() {
        m_running = true;
        m_thread = std::thread([this] { run(); });
    }

    void stop() {
        if (!m_running) return;
        m_running = false;
        m_cv.notify_all();
        if (m_thread.joinable()) m_thread.join();
    }

    bool isRunning() const { return m_running; }
    const std::string& name() const { return m_name; }

    /// Post a message to the queue (thread-safe)
    void post(Message msg) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(msg));
        }
        m_cv.notify_one();
    }

    /// Post with just a type string and handler
    void post(std::string type, std::function<void()> handler) {
        post(Message{std::move(type), std::move(handler)});
    }

    size_t queueSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

protected:
    /// Override to handle messages. Default calls msg.handler() if set.
    virtual void processMessage(const Message& msg) {
        if (msg.handler) msg.handler();
    }

private:
    void run() {
        while (m_running) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });
            while (!m_queue.empty() && m_running) {
                Message msg = std::move(m_queue.front());
                m_queue.pop();
                lock.unlock();
                try { processMessage(msg); } catch (...) {}
                lock.lock();
            }
        }
    }

    std::string              m_name;
    std::atomic<bool>        m_running;
    std::queue<Message>      m_queue;
    mutable std::mutex       m_mutex;
    std::condition_variable  m_cv;
    std::thread              m_thread;
};

} // namespace minima::system
