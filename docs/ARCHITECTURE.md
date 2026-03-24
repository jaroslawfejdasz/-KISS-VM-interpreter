# Minima Core C++ — Architecture Deep-Dive

> For a quick overview see [README.md](../README.md).
> For current test status see [IMPLEMENTATION_STATUS.md](../IMPLEMENTATION_STATUS.md).

---

## Table of Contents

1. [Design Philosophy](#design-philosophy)
2. [Module Map](#module-map)
3. [Layer 1 — Primitive Types](#layer-1--primitive-types)
4. [Layer 2 — Protocol Objects](#layer-2--protocol-objects)
5. [Layer 3 — KISS VM](#layer-3--kiss-vm)
6. [Layer 4 — Cryptography](#layer-4--cryptography)
7. [Layer 5 — MMR Accumulator](#layer-5--mmr-accumulator)
8. [Layer 6 — Chain & Consensus](#layer-6--chain--consensus)
9. [Layer 7 — Mining](#layer-7--mining)
10. [Layer 8 — Network](#layer-8--network)
11. [Layer 9 — Persistence](#layer-9--persistence)
12. [Layer 10 — Validation](#layer-10--validation)
13. [Serialization — Wire Format](#serialization--wire-format)
14. [Dependency Graph](#dependency-graph)
15. [Adding a New Module](#adding-a-new-module)

---

## Design Philosophy

**1. Java parity first, optimisation later.**
Every algorithm matches the Java reference byte-for-byte. If the Java code does
something unusual (e.g. MiniNumber as a decimal string instead of a binary integer),
we do the same. Divergence from the Java reference = a bug.

**2. Header-only where possible, `.cpp` when necessary.**
Most modules are implemented entirely in `.hpp` files. `.cpp` files exist only
when the translation unit would be compiled multiple times (avoiding ODR
violations) or when compile time matters.

**3. Zero mandatory external dependencies.**
- SHA256 and SHA3 are bundled as single-header vendor libraries (`src/crypto/impl/`).
- SQLite3 is bundled as a single amalgamation (`src/vendor/sqlite3/`).
- No Boost, no OpenSSL (unless explicitly enabled with `-DMINIMA_CRYPTO_OPENSSL=ON`).

**4. Deterministic, bounded execution.**
KISS VM enforces hard limits at runtime: 1024 instructions, 64 stack depth.
The mining loop is interruptible. No unbounded allocations in hot paths.

**5. Testability.**
Every module has a matching `tests/<module>/test_<module>.cpp`.
Tests use the [doctest](https://github.com/doctest/doctest) header (vendored).

---

## Module Map

```
src/
├── types/              ← Layer 1: primitive types
│   ├── MiniNumber.hpp/cpp
│   ├── MiniData.hpp/cpp
│   └── MiniString.hpp/cpp
│
├── objects/            ← Layer 2: protocol objects
│   ├── Coin.hpp/cpp
│   ├── Address.hpp/cpp
│   ├── StateVariable.hpp/cpp
│   ├── Transaction.hpp/cpp
│   ├── Witness.hpp/cpp
│   ├── TxHeader.hpp/cpp
│   ├── TxBody.hpp/cpp
│   ├── TxPoW.hpp/cpp
│   ├── Token.hpp
│   ├── TxBlock.hpp
│   ├── CoinProof.hpp
│   ├── Greeting.hpp
│   ├── IBD.hpp
│   └── Magic.hpp/cpp
│
├── kissvm/             ← Layer 3: smart contracts
│   ├── Token.hpp/cpp   (lexer token, different from objects/Token)
│   ├── Tokenizer.hpp/cpp
│   ├── Parser.hpp/cpp
│   ├── Value.hpp/cpp
│   ├── Environment.hpp/cpp
│   ├── Interpreter.hpp/cpp
│   ├── Contract.hpp/cpp
│   └── functions/
│       └── Functions.hpp/cpp
│
├── crypto/             ← Layer 4: cryptography
│   ├── Hash.hpp/cpp    (SHA2-256, SHA3-256)
│   ├── Winternitz.hpp  (WOTS W=8, post-quantum)
│   ├── TreeKey.hpp     (Merkle tree of WOTS keys)
│   ├── Schnorr.hpp/cpp (stub — delegates to WOTS)
│   ├── RSA.hpp/cpp     (RSA-1024 verify — legacy)
│   └── impl/
│       ├── sha256.h    (vendored, header-only)
│       └── sha3.h      (vendored, header-only)
│
├── serialization/      ← Layer 4b: wire format
│   └── DataStream.hpp/cpp
│
├── mmr/                ← Layer 5: UTXO accumulator
│   ├── MMRData.hpp/cpp
│   ├── MMREntry.hpp/cpp
│   ├── MMRProof.hpp/cpp
│   └── MMRSet.hpp/cpp
│
├── chain/              ← Layer 6: consensus
│   ├── ChainState.hpp
│   ├── ChainProcessor.hpp
│   ├── DifficultyAdjust.hpp
│   ├── UTxOSet.hpp
│   ├── BlockStore.hpp
│   └── cascade/
│       ├── CascadeNode.hpp
│       └── Cascade.hpp
│
├── mining/             ← Layer 7: mining
│   ├── TxPoWMiner.hpp
│   └── MiningManager.hpp
│
├── network/            ← Layer 8: P2P networking
│   ├── NIOMessage.hpp
│   ├── NIOServer.hpp
│   ├── NIOClient.hpp
│   └── P2PSync.hpp
│
├── mempool/            ← auxiliary
│   └── Mempool.hpp
│
├── persistence/        ← Layer 9: storage
│   ├── BlockStore.hpp
│   ├── UTxOStore.hpp
│   └── Database.hpp
│
├── validation/         ← Layer 10: validation
│   └── TxPoWValidator.hpp/cpp
│
├── vendor/
│   └── sqlite3/        (embedded, ~230 KB amalgamation)
│
├── minima_core.hpp     ← umbrella include
└── main.cpp            ← full-node entry point
```

---

## Layer 1 — Primitive Types

These are the building blocks for everything else. They map 1:1 to Java.

### `MiniNumber`

Arbitrary-precision decimal number. Backed by a **decimal string** internally,
NOT a binary integer.

**Why?** The Java reference uses `BigDecimal`. The semantics of division and
rounding must match exactly — so we store the decimal string and implement
arithmetic on it directly.

```cpp
MiniNumber a("1000");
MiniNumber b = a.div(MiniNumber("3"));  // "333.3333..." (not integer division)
MiniNumber c = a.add(MiniNumber("0.5"));

a.writeToStream(ds);    // Minima wire format: length-prefixed UTF-8 string
```

Java equivalent: `MiniNumber.java`

---

### `MiniData`

Variable-length byte array. Represents the **HEX type** in KISS VM.

```cpp
MiniData d = MiniData::fromHex("0xDEADBEEF");
MiniData hash = MiniData::fromBytes({0x01, 0x02, 0x03});

const std::vector<uint8_t>& raw = d.bytes();  // always .bytes(), not .getBytes()

d.writeToStream(ds);   // wire: 2-byte length prefix + raw bytes
```

Java equivalent: `MiniData.java`

---

### `MiniString`

UTF-8 string with Minima serialization (length-prefixed).

```cpp
MiniString s("RETURN SIGNEDBY(0xABCD...)");
s.writeToStream(ds);   // wire: 2-byte length + UTF-8 bytes
const std::string& str = s.str();  // .str(), not .toString()
```

Java equivalent: `MiniString.java`

---

## Layer 2 — Protocol Objects

### TxPoW — The Fundamental Unit

Everything in Minima is a `TxPoW`. A transaction and a block are the same object.

```
TxPoW
├── TxHeader
│   ├── txpowID     MiniData  (SHA3 of serialised body)
│   ├── blkDiff     MiniData  (block difficulty target)
│   ├── txnDiff     MiniData  (transaction difficulty target)
│   ├── chainID     MiniData
│   ├── mmrRoot     MiniData  (MMR root at this block)
│   ├── mmrTotal    MiniNumber
│   ├── nonce       MiniNumber  ← only this changes during mining
│   ├── timeMilli   MiniNumber
│   ├── blockNumber MiniNumber
│   ├── superParents MiniData[SUPER_BLOCK_DEPTH]
│   └── magic       Magic
│
└── TxBody
    ├── transaction  Transaction (inputs + outputs)
    ├── witness      Witness     (scripts + signatures)
    ├── txnList[]    MiniData[]  (referenced tx IDs)
    ├── burnTransaction Transaction
    └── burnWitness  Witness
```

**TxPoW becomes a block when:**
```
SHA2-256(serialise(TxHeader)) < blockDifficulty
```

**TxPoW becomes a valid transaction when:**
```
SHA2-256(serialise(TxHeader)) < txnDifficulty
```

Java equivalent: `TxPoW.java`, `TxHeader.java`, `TxBody.java`

---

### Coin (UTxO)

```
Coin
├── coinID      MiniData   SHA3(transactionID | outputIndex)
├── address     MiniData   SHA3(lockingScript)
├── amount      MiniNumber
├── tokenID     MiniData   "0x00" = native Minima
├── storeState  bool       whether state variables persist
└── state[]     StateVariable[0..255]
```

Java equivalent: `Coin.java`

---

### Transaction

```
Transaction
├── inputs[]    Coin[]   — coins being spent
├── outputs[]   Coin[]   — coins being created
└── state[]     StateVariable[]  — global state for this tx
```

Java equivalent: `Transaction.java`

---

### Witness

Contains the cryptographic proofs that authorise spending the input coins.

```
Witness
├── signatures[]   SignatureProof[]   {pubKey, signature (WOTS)}
└── scripts[]      MiniString[]       unlocking scripts
```

Java equivalent: `Witness.java`

---

## Layer 3 — KISS VM

### Execution Pipeline

```
locking script (string)
       │
       ▼
  Tokenizer        lexical analysis → Token[]
       │
       ▼
  Parser / Executor  (merged in this impl — no separate AST)
       │
       ▼
  Interpreter      stack machine, statement-by-statement
       │
       ▼
  Environment      holds: Transaction, Witness, coinIndex, block context
       │
       ▼
  Value            TRUE / FALSE
```

### Contract

`Contract` is the top-level object. It takes a script string, a `Transaction`,
a `Witness`, and a coin index; it runs the VM and returns the result.

```cpp
Contract c(script, transaction, witness, coinIndex);
Value result = c.execute();
// throws ContractException if > 1024 instructions or > 64 stack depth
```

### Value Types

| KISS VM Type | C++ `Value` | Notes |
|---|---|---|
| `BOOLEAN` | `bool` | |
| `NUMBER` | `MiniNumber` | arbitrary-precision decimal |
| `HEX` | `MiniData` | variable-length bytes |
| `SCRIPT` | `MiniString` | script fragment |

### Selected Built-in Functions

See `src/kissvm/functions/Functions.hpp` for the full list (42+ functions).

| Function | Behaviour |
|---|---|
| `SIGNEDBY(pub)` | Check witness for WOTS sig by `pub` |
| `MULTISIG(m, k1…kN)` | m-of-n multi-sig |
| `CHECKSIG(pub, data, sig)` | Verify a single WOTS signature |
| `STATE(n)` | Value of state variable `n` on this coin |
| `PREVSTATE(n)` | Value of state variable `n` on previous coin |
| `SAMESTATE(n, m)` | Assert STATE(n) == output STATE(m) |
| `VERIFYOUT(i, addr, amt, tok)` | Assert output `i` has exact address/amount/tokenID |
| `VERIFYIN(i, addr, amt, tok)` | Same for inputs |
| `GETOUTADDR(i)` | Get address of output `i` |
| `GETOUTAMT(i)` | Get amount of output `i` |
| `SUMINPUTS(tok)` | Sum of all inputs for tokenID |
| `SUMOUTPUTS(tok)` | Sum of all outputs for tokenID |
| `@BLOCK` | Current block height |
| `@COINAGE` | Blocks since this coin was created |
| `@AMOUNT` | Amount of this coin |
| `@ADDRESS` | Address of this coin |
| `SHA2(d)` | SHA2-256 hash |
| `SHA3(d)` | SHA3-256 hash |
| `CONCAT(a, b)` | Byte concatenation |
| `SUBSET(d, s, l)` | Byte slice |
| `LEN(d)` | Length in bytes |
| `RPLVAR(script, var, val)` | Replace variable in script string |
| `MAST(hash)` | MAST branch commit (Merkelized AST) |
| `EXEC(script)` | Execute sub-script |

---

## Layer 4 — Cryptography

### Hash Functions

```cpp
// SHA3-256 (used for addresses, coin IDs)
MiniData h3 = Hash::sha3_256(data);          // returns MiniData

// SHA2-256 (used for TxPoW / mining)
MiniData h2 = Hash::sha2_256(data);
```

### Winternitz OTS — Post-Quantum Signatures

Minima uses **Winternitz One-Time Signatures**, NOT Schnorr/ECDSA.
This is post-quantum secure (hash-based, not elliptic-curve-based).

**Parameters (must match Java exactly):**
- Hash: SHA3-256
- W parameter: 8
- Key size: 2880 bytes
- Signature size: 2880 bytes

```cpp
// Key generation (from 32-byte seed)
auto [priv, pub] = Winternitz::generate(seed);

// Sign
std::vector<uint8_t> sig = Winternitz::sign(priv, message);

// Verify
bool ok = Winternitz::verify(pub, message, sig);
```

Java equivalent: `WinternitzOTSignature` from BouncyCastle PQC.

### TreeKey — Wallet Key

Because WOTS keys are one-time use, Minima uses a **Merkle tree of WOTS keys**.

- Tree depth: 12 (= 4096 leaf keys)
- Public key: Merkle root (32 bytes)
- Signing: use leaf `i`, provide Merkle proof of path to root

```cpp
TreeKey tk(seed);               // generates all 4096 WOTS keys
MiniData pubRoot = tk.getPublicKey();   // Merkle root = wallet address
auto [sig, proof] = tk.sign(leafIndex, message);
bool ok = TreeKey::verify(pubRoot, leafIndex, message, sig, proof);
```

Java equivalent: `TreeKey.java`

---

## Layer 5 — MMR Accumulator

Minima uses a **Merkle Mountain Range** as its UTxO commitment.

### Why MMR instead of a Merkle tree?
- Appending a new leaf is O(log N) — no re-hashing the whole tree
- Proving a leaf is in the set is O(log N)
- The "peak" list (at most log₂ N peaks) is the compact state commitment

### Structure

```
MMRSet
├── append(coin)    → MMREntry (with index)
├── update(index, spent=true)
├── getRoot()       → MiniData (hash of all peaks)
└── getProof(index) → MMRProof

MMREntry
├── row        int       (which MMR "mountain")
├── entry      int       (position within the row)
├── data       MMRData   (hash of the coin)
├── inBlock    MiniNumber
└── spent      bool

MMRProof
├── blockTime  MiniNumber
├── entryNumber MiniNumber
└── proofChain[] MMREntry[]   (sibling hashes from leaf to peak)
```

Java equivalent: `MMRSet.java`, `MMREntry.java`, `MMRProof.java`

---

## Layer 6 — Chain & Consensus

### ChainState

Holds the current canonical chain tip. Wraps the MMRSet and the block tree.

```cpp
ChainState chain;
chain.addBlock(txpow);     // extend chain
chain.getTip();            // current best TxPoW
chain.getMMRSet();         // current UTXO accumulator
```

### DifficultyAdjust

Minima retargets difficulty every 256 blocks using a moving average.

```
targetTime = 256 * BLOCK_TIME_MS  (e.g. 256 * 50_000 = 12.8 min)
actualTime = timestamp[current] - timestamp[current - 256]
newDiff    = oldDiff * (actualTime / targetTime)
```

Java equivalent: `DifficultyManager.java`

### Cascade — Chain Pruning

Minima keeps only the most recent `N` blocks in full. Older data is
"cascaded" — summarised into compact `CascadeNode` objects and pruned.

```
Cascade
├── tip         TxPoW         (most recent cascade point)
└── nodes[]     CascadeNode[] (summarised old blocks)

CascadeNode
├── txpow       TxPoW
└── weight      MiniNumber    (accumulated PoW weight)
```

The cascade serialises/deserialises to wire format for network propagation
(this is what new nodes receive during IBD instead of full history).

Java equivalent: `Cascade.java`, `CascadeNode.java`

---

## Layer 7 — Mining

### TxPoWMiner

Mines a single TxPoW by incrementing the nonce in `TxHeader` until the hash
meets either the transaction or block difficulty target.

```cpp
TxPoWMiner miner;
miner.setTxPoW(txpow);
miner.mine([](const TxPoW& result) {
    // called when a valid hash is found
});
miner.stop();  // interrupt the mining loop
```

### MiningManager

Orchestrates multiple `TxPoWMiner` threads. Assembles the `TxBody` from
the mempool, updates the `TxHeader`, triggers re-mining when the chain tip
advances.

Java equivalent: `TxPoWMiner.java`, `MiningManager.java`

---

## Layer 8 — Network

### NIOMessage — 24 Message Types

Every P2P message is a `NIOMessage` with a type enum and a payload.

| Type | Description |
|---|---|
| `GREETING` | Handshake, exchange of chain state |
| `TXPOW` | Broadcast a new TxPoW |
| `IBD` | Initial Block Download response |
| `GREET` | Peer greeting |
| `PING` / `PONG` | Keep-alive |
| `MAXIMA_*` | Maxima (L2) message types |
| ... (24 total) | ... |

### NIOServer / NIOClient

TCP socket layer. `NIOServer` accepts incoming connections; `NIOClient`
manages outbound connections. Both use asio-style async I/O.

### P2PSync

Orchestrates peer sync: handles IBD, broadcasts new TxPoW, routes Maxima
messages.

Java equivalent: `NIOManager.java`, `P2PMessageProcessor.java`

---

## Layer 9 — Persistence

### BlockStore (SQLite)

Stores `TxPoW` objects by their txpowID. Uses the embedded SQLite3 amalgamation.

```cpp
BlockStore db("node.db");
db.storeTxPoW(txpow);
TxPoW loaded = db.getTxPoW(id);
db.getBlocksFromHeight(height, limit);
```

### UTxOStore

Stores unspent `Coin` objects. Supports queries by address and tokenID.

Java equivalent: `TxPoWDB.java`, `CoinDB.java`

---

## Layer 10 — Validation

### TxPoWValidator

End-to-end validation of a TxPoW:

1. **PoW check** — hash meets txnDifficulty
2. **Input/output balance** — sum(inputs) == sum(outputs) + burn
3. **Script execution** — for each input coin, run KISS VM locking script
4. **MMR proofs** — each input coin's MMR proof is valid
5. **Signature verification** — WOTS signatures in Witness are valid

```cpp
TxPoWValidator validator(chainState);
ValidationResult r = validator.validate(txpow);
if (!r.valid) {
    std::cerr << r.error << "\n";
}
```

Java equivalent: `TxPoWChecker.java`

---

## Serialization — Wire Format

All objects serialise via `DataStream`. The format is **identical to Java**
(big-endian, length-prefixed, no external schema).

```cpp
DataStream ds;
txpow.writeToStream(ds);           // serialise
std::vector<uint8_t> bytes = ds.bytes();

DataStream ds2(bytes);
TxPoW txpow2;
txpow2.readFromStream(ds2);        // deserialise
```

### Primitive encodings

| Type | Wire format |
|---|---|
| `bool` | 1 byte (`0x00` / `0x01`) |
| `MiniNumber` | 2-byte length + UTF-8 decimal string |
| `MiniData` | 2-byte length + raw bytes |
| `MiniString` | 2-byte length + UTF-8 bytes |
| `std::vector<T>` | 4-byte count + N × T |

All lengths are **big-endian** (Java `DataOutputStream` convention).

---

## Dependency Graph

```
validation  ──────────────────────────────────────►  kissvm
    │                                                   │
    ▼                                                   ▼
 objects  ───────────────────────────────────────►  types
    │                                                   │
    ▼                                                   ▼
 crypto  ◄───────────────────────────────────────  serialization
    │
    ▼
  mmr  ──────────────────────────────────────────►  objects
    │
    ▼
 chain  ────────────────────────────────────────►  mmr + objects
    │
    ▼
mining  ────────────────────────────────────────►  chain + mempool
    │
    ▼
network  ───────────────────────────────────────►  objects + chain
    │
    ▼
persistence  ───────────────────────────────────►  objects
```

Rule: **no circular dependencies**. Lower layers never include from higher layers.

---

## Adding a New Module

1. Create `src/<module>/MyClass.hpp` (and `.cpp` if needed).
2. Add source files to `CMakeLists.txt` in the `minima_core` library target.
3. Create `tests/<module>/test_<module>.cpp` with doctest test cases.
4. Add test to `tests/CMakeLists.txt`.
5. Build and run: `cmake --build build && ctest --test-dir build`.
6. Commit and push — CI will verify all 5 platforms.

```cpp
// tests/mymodule/test_mymodule.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/mymodule/MyClass.hpp"

TEST_CASE("MyClass basic") {
    MyClass obj;
    CHECK(obj.doThing() == expected);
}
```
