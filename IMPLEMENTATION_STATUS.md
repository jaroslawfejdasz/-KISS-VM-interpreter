# minima-core-cpp — Implementation Status

Last updated: 2026-03-22

## Test suite: 13/13 ✅

| Suite              | Tests | Status |
|--------------------|-------|--------|
| test_mini_number   | 8     | ✅     |
| test_mini_data     | 6     | ✅     |
| test_kissvm        | 45    | ✅     |
| test_txpow         | 12    | ✅     |
| test_validation    | 4     | ✅     |
| test_mmr           | 10    | ✅     |
| test_chain         | 7     | ✅     |
| test_token         | 5     | ✅     |
| test_mining        | 4     | ✅     |
| test_difficulty    | 6     | ✅     |
| test_network       | 10    | ✅     |
| test_ibd           | 5     | ✅     |
| test_persistence   | 17    | ✅     |

## Layers

### ✅ Foundation
- `MiniNumber` — arbitrary precision arithmetic, wire-exact
- `MiniData` — byte array, SHA2/SHA3, hex encode/decode
- `MiniString` — UTF-8 string, length-prefixed wire format

### ✅ Objects (wire-exact with Java)
- `Address`, `Coin`, `StateVariable`
- `Transaction`, `Witness`
- `TxHeader`, `TxBody`, `TxPoW`
- `TxBlock` — block wrapper with MMR data
- `Magic` — protocol parameters
- `Token` — custom token definition
- `Greeting` — P2P handshake
- `CoinProof` — MMR coin proof
- `IBD` — Initial Blockchain Download payload

### ✅ KISS VM Interpreter
- Full tokenizer, parser, evaluator
- 42+ built-in functions (SIGNEDBY, CHECKSIG, MULTISIG, STATE, VERIFYOUT, etc.)
- Limits: 1024 instructions, depth 64
- 45 test cases covering all major opcodes

### ✅ Cryptography
- SHA2-256, SHA3-256 (vendored, no OpenSSL required)
- RSA-1024 verify (reference implementation)
- Schnorr stub (placeholder for future Schnorr-256k1)

### ✅ MMR (Merkle Mountain Range)
- MMREntry, MMRData, MMRProof, MMRSet
- Append, get, generate/verify proofs

### ✅ Validation
- `TxPoWValidator` — end-to-end tx validation
  - PoW check (SHA2 ≤ blockDifficulty)
  - KISS VM contract execution per input
  - RSA signature verification

### ✅ Chain
- `BlockStore` (in-memory) — chain state tracking
- `ChainProcessor` — block apply/revert
- `DifficultyAdjust` — Java-exact 256-block median window
- `UTxOSet` — in-memory unspent output set

### ✅ Mining
- `TxPoWMiner` — nonce increment + SHA2 PoW loop

### ✅ Network Protocol
- `NIOMessage` — 24 message types, wire-exact with Java
- `NIOClient` — TCP client, length-prefixed framing
- `NIOServer` — TCP server, accept loop, peer handler

### ✅ Persistence (SQLite)
- `Database` — RAII SQLite3 wrapper, WAL mode, transactions
- `BlockStore` (persistent) — blocks indexed by id/number/parent
- `UTxOStore` — coins with soft-delete (reorg-safe)

## TODO (next priorities)

1. **P2P sync loop** — connect to live Minima node, IBD request/response, flood-fill
2. **Cascade** — chain cascade/pruning (CascadeNode, CascadeTree)
3. **Mempool** — persistent mempool with UTxO validation
4. **Main entry point** — `main.cpp` that starts node (server + peer connections)
5. **ARM cross-compile** — CMake toolchain file for aarch64-linux
