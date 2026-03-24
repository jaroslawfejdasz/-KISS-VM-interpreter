# Implementation Status

**Last updated:** 2026-03-24
**Test suites:** 24 / 24 pass ✅
**Lines of C++:** ~8 500

---

## Summary

Full-node implementation of the Minima blockchain in C++20. The goal is wire-exact compatibility with the [Java reference implementation](https://github.com/minima-global/Minima) so that this node can connect to and participate in the live Minima network.

---

## Module status

| Module | Status | Test file(s) |
|--------|--------|--------------|
| `types/` — MiniNumber, MiniData, MiniString | ✅ complete | test_mini_number, test_mini_data |
| `crypto/` — WOTS, TreeKey, BIP39, Hash | ✅ complete | test_wots, test_treekey, test_bip39 |
| `serialization/` — DataStream | ✅ complete | (all modules) |
| `objects/` — Coin, TxPoW, Witness, Greeting, Pulse, Genesis | ✅ complete | test_txpow, test_integration |
| `kissvm/` — interpreter, parser, 42+ built-in functions | ✅ complete | test_kissvm |
| `mmr/` — MMRSet, MMRProof, MegaMMR (checkpoint / rollback / fast-sync) | ✅ complete | test_mmr, test_megammr |
| `chain/` — TxPowTree, BlockStore, Cascade, DifficultyAdjust | ✅ complete | test_chain, test_cascade, test_difficulty |
| `database/` — MinimaDB, Wallet | ✅ complete | test_database |
| `persistence/` — SQLite BlockStore, UTxOStore | ✅ complete | test_persistence |
| `validation/` — TxPoWValidator (incl. WOTS signature verification) | ✅ complete | test_validation |
| `network/` — NIOClient, NIOServer, NIOMessage, P2PSync | ✅ complete | test_network |
| `mining/` — TxPoWMiner, MiningManager | ✅ complete | test_mining |
| `system/` — MessageProcessor, TxPoWProcessor, TxPoWGenerator, TxPoWSearcher | ✅ complete | test_processor |
| `mempool/` — Mempool | ✅ complete | (integration) |
| **Integration** | ✅ complete | test_integration |

---

## CI matrix

| Job | Platform | Compiler | Status |
|-----|----------|----------|--------|
| build-linux-x64 | Ubuntu 22.04 | GCC 11 | ✅ |
| build-linux-arm64 | Ubuntu 22.04 | aarch64-linux-gnu-g++ | ✅ |
| build-linux-armv7 | Ubuntu 22.04 | arm-linux-gnueabihf-g++ | ✅ |

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                       P2P Network                        │
│             NIOServer ◄──► NIOClient                     │
│             NIOMessage (wire protocol)                   │
└──────────────────────┬───────────────────────────────────┘
                       │ packets
┌──────────────────────▼───────────────────────────────────┐
│                  TxPoWProcessor                          │
│          (async message queue + worker thread)           │
└──────┬────────────────────────────────────────┬──────────┘
       │ ACCEPTED                               │ SYNC
┌──────▼────────┐    ┌────────────────┐   ┌────▼──────────┐
│  TxPowTree    │    │ TxPoWGenerator │   │ TxPoWSearcher │
│  (chain +     │    │ (block template│   │ (chain query) │
│   reorg)      │    │  from mempool) │   └───────────────┘
└──────┬────────┘    └────────┬───────┘
       │                      │
┌──────▼──────────────────────▼─────────────────────────────┐
│                      MinimaDB                              │
│   TxPowTree + BlockStore + MMRSet + Wallet + Mempool       │
│   Persistence: SQLite (bootstrapFromDB on restart)         │
└────────────────────────────────────────────────────────────┘
```

---

## Next priorities

| # | Task | Priority |
|---|------|----------|
| 1 | Publish npm packages (minima-test, kiss-vm-lint) | medium |
| 2 | Live node integration test — connect to a real Minima seed node and exchange Greeting | medium |
| 3 | ARM QEMU testing in CI — execute cross-compiled binaries | low |


## Live Node Integration Test — DONE ✅
**Date:** 2026-03-24

**Wynik:** 11/11 testów przechodzi z prawdziwym węzłem Java Minima v1.0.45-TEST.15

### Co zostało zweryfikowane:
1. ✅ RPC HTTP GET `/status` — blok, balans, uptime
2. ✅ TCP connect do NIO port 9001
3. ✅ Greeting send/receive — wire format zgodny z Java 1:1
4. ✅ PING/PONG round-trip
5. ✅ TxPoW request/response (GET_SINGLE_TXPOW)
6. ✅ TxHeader + TxBody parse z realnych danych
7. ✅ Chain ID parsing z Greeting (21 bloków)

### Kluczowe fakty (weryfikacja z prawdziwym węzłem):
- Minima v1.0.45 używa `-noshutdownhook` żeby węzeł żył w tle
- P2P port: `baseport` (np. 9001), RPC: `baseport+4` (np. 9005)
- Flaga `-solo` wyłącza P2P2 — użyj `-test` dla trybu prywatnego z P2P
- Genesis block ID: `0x874B33874805FFA5C7F045736CA773EF798074B0A320516A7F01007D3FFF7733`
- Wire format Greeting nasz C++ parsuje 835 bytes 835/835 (100% poprawnie)

