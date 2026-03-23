# Parity Gap Analysis — C++ vs Java Reference

**Date:** 2026-03-23  
**Current state:** 24/24 test suites pass.  
**Goal:** Bit-for-bit, wire-exact 1:1 z Java referencją.

---

## Status GAP-ów (2026-03-23)

| GAP | Opis | Status |
|-----|------|--------|
| GAP 1 | Witness wire format + SignatureProof/ScriptProof/CoinProof | ✅ DONE |
| GAP 2 | KISS VM brakujące 13 funkcji | ✅ DONE |
| GAP 3 | TxPowTree + MinimaDB + Wallet | ✅ DONE |
| GAP 4 | MessageProcessor + TxPoWProcessor + Generator + Searcher | ✅ DONE |
| GAP 5 | Genesis block (GenesisCoin, GenesisMMR, GenesisTxPoW) | ✅ DONE |

---

## Pełna mapa implementacji

| Obszar | Stan | Uwagi |
|--------|------|-------|
| Wire format (DataStream) | ✅ 1:1 | DataOutputStream-compatible |
| Primitive types (MiniNumber/MiniData/MiniString) | ✅ 1:1 | Byte-exact |
| Protocol objects (Coin, TxPoW, TxHeader/Body, Witness) | ✅ 1:1 | Wire-compatible |
| Signature / SignatureProof / ScriptProof / CoinProof | ✅ 1:1 | Java-exact structure |
| KISS VM interpreter (42+ functions) | ✅ ~95% | Wszystkie kluczowe funkcje |
| Cryptography (WOTS/TreeKey/BIP39) | ✅ 1:1 | Byte-exact z BouncyCastle |
| MMR accumulator + MegaMMR | ✅ 1:1 | |
| TxPowTree + chain reorganization | ✅ 1:1 | |
| MinimaDB (central coordinator) | ✅ done | Wszystkie sub-systemy powiązane |
| Wallet (WOTS key mgmt) | ✅ done | |
| Processing pipeline (Processor/Generator/Searcher) | ✅ done | Sync + async |
| Genesis block | ✅ 1:1 | SHA3(MiniString.serialise()) address |
| Persistence (SQLite, bootstrap replay) | ✅ done | |
| P2P network (NIOClient/NIOServer/NIOMessage) | ✅ done | Wire-compatible |
| P2P sync (P2PSync, Greeting exchange) | ✅ done | |
| Mining (TxPoWMiner, MiningManager) | ✅ done | |
| Cascade (chain pruning) | ✅ done | |
| CI (GitHub Actions, 3 platforms) | ✅ done | x64 + arm64 + armv7 |

---

## Pozostałe małe GAP-y (nie blokujące)

| # | Opis | Priorytet |
|---|------|-----------|
| 1 | `MegaMMR` — pełny fast-sync IBD (szkielet jest, brak pełnej logiki) | ✅ DONE |
| 2 | `Cascade` integracja z `MinimaDB.addBlock()` — auto-trim starych bloków | LOW |
| 3 | `P2P Greeting` — wysyłanie do seed node przy starcie | MEDIUM |
| 4 | `PROOF` funkcja KISS VM — MMR proof weryfikacja w skrypcie | ✅ DONE |
| 5 | npm publish — monorepo packages (minima-test, kiss-vm-lint) | MEDIUM |
| 6 | Pełna walidacja WOTS podpisów w TxPoWValidator | ✅ DONE |

---

## Priorytetowana lista (następne kroki)

### 🔴 Krytyczne dla działającego węzła

1. **WOTS signature validation** w `TxPoWValidator` — aktualnie stub
2. **PROOF** funkcja KISS VM — pozwala kontraktom weryfikować MMR proofs
3. **P2P Greeting** — wysyłanie własnego Greeting przy połączeniu do seed node

### 🟡 Ważne

4. **MegaMMR pełna logika** — fast sync dla nowych węzłów
5. **Cascade integration** — automatyczne przycinanie

### 🟢 Uzupełniające

6. **npm publish** monorepo packages
7. **Live node integration test** — połącz się z prawdziwym węzłem Minima i wymień Greeting
