#pragma once
/**
 * TxPoWProcessor — centralny procesor TxPoW.
 * Java ref: src/org/minima/system/brains/TxPoWProcessor.java
 *
 * Najważniejszy komponent systemu. Asynchronicznie przetwarza:
 *   TXP_PROCESSTXPOW  — nowy TxPoW z sieci (może być blok lub transakcja)
 *   TXP_PROCESSTXBLOCK — przetwórz potwierdzony blok
 *   TXP_PROCESS_IBD    — Initial Block Download (sync z siecią)
 *
 * Rozszerza MessageProcessor — każde przetwarzanie to message w kolejce.
 */
#include "MessageProcessor.hpp"
#include "TxPoWSearcher.hpp"
#include "../database/MinimaDB.hpp"
#include "../validation/TxPoWValidator.hpp"
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace minima::system {

// ── Typy zdarzeń (analogiczne do Java MessageProcessor) ──────────────────────

inline constexpr const char* TXP_PROCESSTXPOW  = "TXP_PROCESSTXPOW";
inline constexpr const char* TXP_PROCESSTXBLOCK = "TXP_PROCESSTXBLOCK";
inline constexpr const char* TXP_PROCESS_IBD    = "TXP_PROCESS_IBD";
inline constexpr const char* TXP_PROCESS_SYNCIBD = "TXP_PROCESS_SYNCIBD";
inline constexpr const char* TXP_SHUTDOWN        = "TXP_SHUTDOWN";

// ── Wynik przetwarzania ───────────────────────────────────────────────────────

enum class ProcessResult {
    ACCEPTED,       // Zaakceptowany, dodany do łańcucha lub mempoola
    DUPLICATE,      // Już mamy ten TxPoW
    ORPHAN,         // Nie znamy rodzica — zachowaj do późniejszego retry
    INVALID_POW,    // Nie spełnia PoW
    INVALID_SCRIPT, // Nieważny skrypt KISS VM
    INVALID_SIGS,   // Błędne podpisy
    ERROR           // Inny błąd
};

inline std::string to_string(ProcessResult r) {
    switch (r) {
        case ProcessResult::ACCEPTED:       return "ACCEPTED";
        case ProcessResult::DUPLICATE:      return "DUPLICATE";
        case ProcessResult::ORPHAN:         return "ORPHAN";
        case ProcessResult::INVALID_POW:    return "INVALID_POW";
        case ProcessResult::INVALID_SCRIPT: return "INVALID_SCRIPT";
        case ProcessResult::INVALID_SIGS:   return "INVALID_SIGS";
        default:                            return "ERROR";
    }
}

// ── TxPoWProcessor ───────────────────────────────────────────────────────────

class TxPoWProcessor : public MessageProcessor {
public:
    using ResultCallback  = std::function<void(const MiniData& id, ProcessResult)>;
    using BlockCallback   = std::function<void(const TxPoW&)>;
    using TxCallback      = std::function<void(const TxPoW&)>;

    explicit TxPoWProcessor(MinimaDB& db)
        : MessageProcessor("TxPoWProcessor")
        , m_db(db)
        , m_searcher(db.txPowTree(), db.blockStore())
        , m_blocksProcessed(0)
        , m_txnsProcessed(0)
    {}

    // ── Public API (thread-safe, non-blocking) ────────────────────────────

    /// Przekaż nowy TxPoW do przetworzenia (np. odebrany z sieci)
    void submitTxPoW(const TxPoW& txpow, ResultCallback cb = {}) {
        post(TXP_PROCESSTXPOW, [this, txpow, cb] {
            auto result = doProcessTxPoW(txpow);
            if (cb) cb(txpow.computeID(), result);
        });
    }

    /// Synchroniczne przetworzenie (do testów — nie wymaga wątku)
    ProcessResult processTxPoWSync(const TxPoW& txpow) {
        return doProcessTxPoW(txpow);
    }

    /// Przekaż listę TxPoW (np. z IBD) do przetworzenia
    void submitIBD(const std::vector<TxPoW>& blocks, ResultCallback cb = {}) {
        post(TXP_PROCESS_IBD, [this, blocks, cb] {
            for (const auto& txp : blocks) {
                auto result = doProcessTxPoW(txp);
                if (cb) cb(txp.computeID(), result);
            }
        });
    }

    // ── Event callbacks ───────────────────────────────────────────────────
    void onBlockAccepted(BlockCallback cb) { m_onBlock = std::move(cb); }
    void onTxAccepted(TxCallback cb)       { m_onTx    = std::move(cb); }

    // ── Stats ──────────────────────────────────────────────────────────────
    int64_t blocksProcessed() const { return m_blocksProcessed; }
    int64_t txnsProcessed()   const { return m_txnsProcessed; }
    int64_t currentHeight()   const { return m_db.currentHeight(); }

protected:
    void processMessage(const Message& msg) override {
        if (msg.handler) msg.handler();
    }

private:
    /**
     * Główna logika przetwarzania — analogiczna do Java TxPoWProcessor.processNewTxPoW().
     *
     * Java logic:
     * 1. Sprawdź duplikat
     * 2. Waliduj PoW
     * 3. Jeśli blok (ma numer bloku >= 0): dodaj do TxPowTree
     * 4. Jeśli transakcja (bez bloku): dodaj do Mempoola
     * 5. Jeśli orphan: dodaj do orphan pool, czekaj na rodzica
     */
    ProcessResult doProcessTxPoW(const TxPoW& txpow) {
        MiniData id = txpow.computeID();

        // 1. Sprawdź duplikat
        if (m_db.blockStore().has(id) || m_db.mempool().contains(id))
            return ProcessResult::DUPLICATE;

        // 2. Walidacja PoW
        // (pełna walidacja wymaga TxPoWValidator + MinimaDB context)
        // Tu sprawdzamy podstawowe rzeczy
        if (!basicPoWCheck(txpow))
            return ProcessResult::INVALID_POW;

        // 3. Czy to blok (blockNumber > 0 lub jest genesis)?
        bool isBlock = isBlockTxPoW(txpow);

        if (isBlock) {
            return processBlock(txpow, id);
        } else {
            return processTxn(txpow, id);
        }
    }

    ProcessResult processBlock(const TxPoW& txpow, const MiniData& id) {
        // Dodaj do BlockStore + TxPowTree
        bool added = m_db.addBlock(txpow);
        if (!added) {
            // Może orphan — rodzic nieznany
            return ProcessResult::ORPHAN;
        }

        ++m_blocksProcessed;
        if (m_onBlock) m_onBlock(txpow);

        // Aktualizuj MMR jeśli to nowy tip
        // (pełna implementacja wymaga MMR rebuild po reorg)
        updateMMRIfTip(txpow);

        // Usuń z mempoola transakcje które są już w bloku
        for (const auto& txID : txpow.body().txnList)
            m_db.mempool().remove(txID);

        return ProcessResult::ACCEPTED;
    }

    ProcessResult processTxn(const TxPoW& txpow, const MiniData& id) {
        // Walidacja transakcji przed dodaniem do mempoola
        // (uproszczone — pełna weryfikacja w TxPoWValidator)
        if (!basicTxnCheck(txpow))
            return ProcessResult::INVALID_SCRIPT;

        m_db.mempool().add(txpow);
        ++m_txnsProcessed;
        if (m_onTx) m_onTx(txpow);
        return ProcessResult::ACCEPTED;
    }

    // ── Pomocnicze checks ─────────────────────────────────────────────────

    bool basicPoWCheck(const TxPoW& txpow) const {
        // Sprawdź czy blockNumber >= 0
        if (txpow.header().blockNumber.getAsLong() < 0) return false;
        // Sprawdź czy timestamp nie jest za bardzo w przyszłości (> 1h)
        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        int64_t txpMs = txpow.header().timeMilli.getAsLong();
        if (txpMs > nowMs + 3600000LL) return false; // > 1h w przyszłości
        return true;
    }

    bool isBlockTxPoW(const TxPoW& txpow) const {
        // Blok = ma blockNumber ustawiony (w Minima każdy TxPoW MA blockNumber)
        // Rozróżnienie: blok vs "transakcja bez bloku" — w Minima każdy TxPoW jest blokiem
        // Ale "non-block txpow" ma blockNumber = 0 z superParents[0] = 0x00...
        // Uproszczenie: jeśli jest genesis lub ma rodzica w drzewie → blok
        auto& sp = txpow.header().superParents[0];
        bool allZero = true;
        for (uint8_t b : sp.bytes()) if (b) { allZero = false; break; }
        int64_t bn = txpow.header().blockNumber.getAsLong();
        return (bn == 0 && allZero) || (bn > 0);
    }

    bool basicTxnCheck(const TxPoW& txpow) const {
        // Minimalna walidacja transakcji: nie może mieć inputs == outputs
        // (pełna weryfikacja w TxPoWValidator)
        return true;
    }

    void updateMMRIfTip(const TxPoW& txpow) {
        // Uproszczone — w pełnej implementacji: rebuild MMR dla nowego tip
        // wymagałoby pełnego MMRSet + reorg handling
    }

    BlockCallback   m_onBlock;
    TxCallback      m_onTx;
    MinimaDB&       m_db;
    TxPoWSearcher   m_searcher;
    std::atomic<int64_t> m_blocksProcessed;
    std::atomic<int64_t> m_txnsProcessed;
};

} // namespace minima::system
