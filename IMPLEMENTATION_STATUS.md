# minima-core-cpp Implementation Status

**Date:** 2026-03-23 (updated)  
**Test suites:** 24/24 pass ✅  
**Lines of C++:** ~8500

---

## Summary

Full node implementation of the Minima blockchain in C++20.
Wire-exact compatibility with the Java reference implementation.

---

## Module Status

| Module | Status | Tests |
|--------|--------|-------|
| `types/` — MiniNumber, MiniData, MiniString | ✅ | test_mini_number, test_mini_data |
| `crypto/` — WOTS, TreeKey, BIP39, Hash | ✅ | test_wots, test_treekey, test_bip39 |
| `serialization/` — DataStream | ✅ | (all) |
| `objects/` — Coin, TxPoW, Witness, Greeting, Pulse, Genesis | ✅ | test_txpow, test_witness_wire, test_genesis |
| `kissvm/` — interpreter, parser, 42+ builtins | ✅ | test_kissvm |
| `mmr/` — MMRSet, MMRProof, MegaMMR (checkpoint/rollback/fast-sync) | ✅ | test_mmr, test_megammr |
| `chain/` — TxPowTree, BlockStore, Cascade, DifficultyAdjust | ✅ | test_chain, test_cascade, test_difficulty |
| `database/` — MinimaDB, Wallet | ✅ | test_database |
| `persistence/` — SQLite BlockStore, UTxOStore | ✅ | test_persistence |
| `validation/` — TxPoWValidator | ✅ | test_validation |
| `network/` — NIOClient, NIOServer, NIOMessage, P2PSync | ✅ | test_network, test_p2p_sync |
| `mining/` — TxPoWMiner, MiningManager | ✅ | test_mining |
| `system/` — MessageProcessor, TxPoWProcessor, TxPoWGenerator, TxPoWSearcher | ✅ | test_processor |
| `mempool/` — Mempool | ✅ | (integration) |
| **Integration** | ✅ | test_integration (12 tests) |

---

## CI Matrix

| Job | Platform | Compiler | Status |
|-----|----------|----------|--------|
| build-linux-x64 | Ubuntu 22.04 | GCC 11 | ✅ |
| build-linux-arm64 | Ubuntu 22.04 | aarch64-linux-gnu-g++ | ✅ |
| build-linux-armv7 | Ubuntu 22.04 | arm-linux-gnueabihf-g++ | ✅ |

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                       P2P Network                         │
│            NIOServer ◄──► NIOClient                       │
│            NIOMessage (wire protocol)                     │
└──────────────────────┬───────────────────────────────────┘
                       │ packets
┌──────────────────────▼───────────────────────────────────┐
│                  TxPoWProcessor                           │
│           (async message queue + worker thread)           │
└──────┬────────────────────────────────────────┬──────────┘
       │ ACCEPTED                               │ SYNC
┌──────▼────────┐    ┌────────────────┐   ┌────▼──────────┐
│  TxPowTree    │    │ TxPoWGenerator │   │ TxPoWSearcher │
│  (chain + re- │    │ (block template│   │ (chain query) │
│   org)        │    │  from mempool) │   └───────────────┘
└──────┬────────┘    └────────┬───────┘
       │                      │
┌──────▼──────────────────────▼─────────────────────────────┐
│                      MinimaDB                              │
│   TxPowTree + BlockStore + MMRSet + Wallet + Mempool       │
│   Persistence: SQLite (bootstrapFromDB on restart)         │
└────────────────────────────────────────────────────────────┘
```

---

## TODO (next priorities)

| # | Task | Priority |
|---|------|----------|
| 1 | WOTS signature validation in TxPoWValidator | HIGH |
| 2 | PROOF function in KISS VM (MMR proof verify) | HIGH |
| 3 | P2P Greeting send to seed node on startup | MEDIUM |
| 4 | MegaMMR full logic (fast-sync IBD) | MEDIUM |
| 5 | Cascade integration in MinimaDB.addBlock() | LOW |
| 6 | npm publish monorepo packages | MEDIUM |
