# Minima Core C++ — Implementation Status

**Goal:** 1:1 reimplementation of Minima blockchain core in C++20, suitable for bare-metal ARM chips.

---

## ✅ DONE — builds, tests pass, wire-exact

### Types
- `MiniNumber` — BigDecimal-compatible, full arithmetic, wire format 1:1 Java
- `MiniData` — arbitrary byte array, hex encode/decode
- `MiniString` — UTF-8 string with Minima wire format

### Serialization
- `DataStream` — write/read primitives, big-endian, matches Java DataOutputStream

### Crypto
- `Hash` — SHA2-256 (PoW), SHA3-256 (IDs), SHA3-double (TxPoW ID)
- `RSA` — RSA-1024 SHA256withRSA (conditional: `-DMINIMA_OPENSSL` for real, stub otherwise)
- `Schnorr.hpp` — kept as stub (Minima uses RSA, not Schnorr)

### Objects (wire-exact)
- `Address` — SHA3(script), 32 bytes
- `StateVariable` — port + value (STRING/NUMBER/HEX)
- `Coin` — UTxO: coinID, amount, address, tokenID, stateVars, flags
- `Transaction` — inputs[] + outputs[] + stateVars[]
- `Witness` — signatures[] + scripts[]
- `Magic` — 6 consensus params (currentMax/desiredMax × 3)
- `TxHeader` — WIRE-EXACT: nonce, chainID, timeMilli, blockNumber, blockDifficulty(MiniData), superParents[32] RLE-encoded, mmrRoot/mmrTotal, Magic, customHash, bodyHash
- `TxBody` — WIRE-EXACT: prng(32B random), txnDifficulty(MiniData), txn, witness, burnTxn, burnWitness, txnList
- `TxPoW` — ID=SHA3(SHA3(header)), PoW=SHA2(header), body-present flag

### KISS VM
- `Tokenizer` + `Parser` + `Interpreter` — full KISS VM
- All 42+ built-in functions: SIGNEDBY, CHECKSIG(RSA), MULTISIG, STATE/PREVSTATE/SAMESTATE, VERIFYOUT/VERIFYIN, SHA2/SHA3, CONCAT/SUBSET/LEN, @BLOCK/@COINAGE/@AMOUNT, all math/logic/string ops
- Limits: 1024 instructions, stack depth 64

### MMR (Merkle Mountain Range)
- `MMRData` / `MMREntry` / `MMRProof` / `MMRSet`
- addLeaf, getProof, verifyProof, getRoot — 1:1 Java

### Validation
- `TxPoWValidator` — PoW check, balance check, coinID uniqueness, state vars, script execution, size check

### Chain
- `BlockStore` — hash+number indexed, ancestor walk
- `ChainState` — tip tracking
- `UTxOSet` — add/spend/balance by address+token
- `ChainProcessor` — block processing, duplicate rejection, UTxO apply

---

## 🔴 TODO — critical for full node

### P2P / Network
- [ ] `TxPoW` flood-fill propagation protocol
- [ ] Peer discovery / connection management
- [ ] Sync protocol (IBD — Initial Block Download)

### Token system
- [ ] `Token` object (Java: Token.java)
- [ ] Token creation transaction
- [ ] Token balance tracking in UTxOSet

### Mining (TxPoW production)
- [ ] TxPoW miner loop (increment nonce, check difficulty)
- [ ] Difficulty adjustment (per-block)
- [ ] Super-block cascade (32 levels)

### Pulse / Sync
- [ ] `TxPoWMiner` thread
- [ ] `TxPoWChecker` — incoming tx verification
- [ ] IBD state machine

### Persistence
- [ ] Block database (LevelDB or SQLite for embedded)
- [ ] UTxOSet persistence

### MiniDapp / Maxima
- Not in scope for chip firmware (these are L3/L2 features)

---

## 🧪 Test Coverage

| Module          | Tests | Status |
|----------------|-------|--------|
| MiniNumber      | ~20   | ✅     |
| MiniData        | ~15   | ✅     |
| KISS VM         | ~30   | ✅     |
| TxPoW objects   | ~15   | ✅     |
| Validation      | ~10   | ✅     |
| MMR             | ~15   | ✅     |
| Chain           | ~12   | ✅     |
| **TOTAL**       | **~117** | **7/7 suites pass** |

---

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

ARM cross-compile:
```bash
cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-cortex-m4.cmake \
      -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-arm -j$(nproc)
```

---

## Wire compatibility

TxHeader and TxBody serialisation matches Java `writeDataStream()` byte-for-byte.
A C++ node can parse blocks produced by a Java node and vice versa.

Key: **TxPoW ID = SHA3(SHA3(TxHeader bytes))** — double SHA3, header only.
