# Parity Gap Analysis — C++ vs Java Reference

**Last updated:** 2026-03-24
**Test suites:** 24 / 24 pass
**Goal:** Bit-for-bit, wire-exact compatibility with the [Java reference implementation](https://github.com/minima-global/Minima).

---

## Resolved gaps

All previously identified gaps have been closed. The table below records what was done for reference.

| Gap | Description | Status |
|-----|-------------|--------|
| 1 | Witness wire format — SignatureProof / ScriptProof / CoinProof | ✅ resolved |
| 2 | KISS VM — 13 missing built-in functions | ✅ resolved |
| 3 | TxPowTree + MinimaDB + Wallet | ✅ resolved |
| 4 | MessageProcessor + TxPoWProcessor + Generator + Searcher | ✅ resolved |
| 5 | Genesis block (GenesisCoin, GenesisMMR, GenesisTxPoW) | ✅ resolved |

---

## Full parity map

| Area | Status | Notes |
|------|--------|-------|
| Wire format (DataStream) | ✅ 1:1 | DataOutputStream-compatible byte order |
| Primitive types (MiniNumber / MiniData / MiniString) | ✅ 1:1 | Byte-exact serialisation |
| Protocol objects (Coin, TxPoW, TxHeader, TxBody, Witness) | ✅ 1:1 | Wire-compatible |
| Signature / SignatureProof / ScriptProof / CoinProof | ✅ 1:1 | Matches Java structure exactly |
| KISS VM interpreter (42+ functions) | ✅ ~95% | All functions used in production contracts |
| Cryptography (WOTS / TreeKey / BIP39) | ✅ 1:1 | Byte-exact against BouncyCastle |
| MMR accumulator + MegaMMR | ✅ 1:1 | Checkpoint, rollback, fast-sync IBD |
| TxPowTree + chain reorganisation | ✅ 1:1 | |
| MinimaDB (central coordinator) | ✅ complete | All sub-systems wired |
| Wallet (WOTS key management) | ✅ complete | |
| Processing pipeline (Processor / Generator / Searcher) | ✅ complete | Sync + async paths |
| Genesis block | ✅ 1:1 | SHA3(MiniString.serialise()) address |
| Persistence (SQLite, bootstrap replay) | ✅ complete | |
| P2P network (NIOClient / NIOServer / NIOMessage) | ✅ complete | Wire-compatible |
| P2P sync (P2PSync, Greeting exchange) | ✅ complete | |
| Mining (TxPoWMiner, MiningManager) | ✅ complete | |
| Cascade (chain pruning) | ✅ complete | |
| CI (GitHub Actions, 3 platforms) | ✅ complete | x64 + arm64 + armv7 |

---

## Remaining items (non-blocking)

| # | Description | Priority |
|---|-------------|----------|
| 1 | Live node integration test — connect to a real seed node and exchange Greeting | medium |
| 2 | Publish npm packages (monorepo) | medium |
| 3 | ARM QEMU smoke test in CI | low |

---

## Key implementation notes

**MiniNumber** is stored as a decimal string internally, not a binary integer. The Java reference uses `BigDecimal`; matching its rounding semantics requires the same representation.

**TxPoW ID** is not a field in the header — it is computed as `SHA3(SHA3(serialised TxHeader))` and cached. The parent ID is `txpow.header().superParents[0]`.

**Cryptography** — Minima does not use Schnorr / secp256k1. It uses **Winternitz OTS** (SHA3-256, W=8), which is post-quantum. The `Schnorr` stub in this codebase delegates to WOTS.

**MegaMMR checkpoint depth** is 64 blocks. Checkpoints older than `currentBlock - 64` are pruned automatically in `MinimaDB.addBlock()`. The `pruneBelow()` call is intentionally deferred to `TxPoWProcessor.trimTree()` to match the Java sequencing.
