# Minima Core C++ — Architecture

## Overview

`minima-core-cpp` is a clean-room C++ implementation of the Minima blockchain
core protocol, extracted from the reference Java implementation
([minima-global/Minima](https://github.com/minima-global/Minima)).

The goal is a **modular, dependency-free library** suitable for:
- Embedded devices and IoT (300 MB RAM constraint)
- FPGA synthesis (hardware-level nodes)
- High-performance validators
- Protocol research and formal verification

---

## Module Map

```
src/
├── types/              Primitive types (MiniNumber, MiniData, MiniString)
├── objects/            Protocol objects (Coin, TxPoW, Transaction, ...)
├── kissvm/             KISS VM interpreter
│   └── functions/      42+ built-in functions
├── crypto/             SHA2/SHA3 + Schnorr signatures
├── serialization/      Minima wire format (DataStream)
└── validation/         TxPoW + script validation
```

---

## Module Descriptions

### 1. `types/` — Primitive Types

| File | Description |
|------|-------------|
| `MiniNumber.hpp` | Arbitrary-precision decimal (replaces Java `BigDecimal`) |
| `MiniData.hpp` | Variable-length byte array — HEX type in KISS VM |
| `MiniString.hpp` | UTF-8 string with Minima serialisation |

**Key design decision:** `MiniNumber` is backed by a decimal string, not a
binary integer. This preserves exact semantics of the Java reference
implementation and avoids floating-point precision issues.

---

### 2. `objects/` — Protocol Objects

The core data structures of the Minima protocol.

```
Address        = SHA3(script)
Coin           = UTxO (coinID, address, amount, stateVars)
StateVariable  = typed slot 0–255 on a coin
Transaction    = inputs[] + outputs[] + stateVars[]
Witness        = scripts[] + signatures[]
TxHeader       = PoW header (what gets hashed during mining)
TxBody         = Transaction + Witness + txnList
TxPoW          = TxHeader + TxBody  ← the fundamental unit
```

**Key insight: TxPoW unification**

In Minima, every transaction IS a potential block. When a user submits a
transaction, they spend ~1 second performing PoW. If the resulting hash
crosses the `blockDifficulty` threshold, the TxPoW becomes a block.
This eliminates the miner/user distinction.

---

### 3. `kissvm/` — KISS VM Interpreter

KISS VM is Minima's Turing-complete smart contract language.

**Execution model:**
```
Script string → Tokenizer → Token[] → Executor → Value (TRUE/FALSE)
```

**Contract execution flow:**
1. Look up the script for the input coin's address (from Witness)
2. Inject transaction context into Environment
3. Execute statements sequentially
4. Final stack value must be TRUE for the spend to be valid

**Limits enforced by Contract:**
- Max **1024 instructions** (throws `ContractException`)
- Max **64 stack depth** (throws `ContractException`)

---

### 4. `crypto/` — Cryptographic Primitives

| Component | Algorithm |
|-----------|-----------|
| Hashing | SHA3-256 (addresses, IDs), SHA2-256 (PoW) |
| Signatures | Schnorr over Minima's custom curve |

**Backend selection** (CMake option `-DMINIMA_CRYPTO_OPENSSL=ON`):
- `OFF` (default): pure C++ reference implementation (portable, no deps)
- `ON`: OpenSSL backend (production performance)

---

### 5. `serialization/` — Wire Format

`DataStream` provides read/write primitives for the Minima binary protocol.

All protocol objects implement:
```cpp
std::vector<uint8_t> serialise() const;
static T             deserialise(const uint8_t* data, size_t& offset);
```

---

### 6. `validation/` — TxPoW Validation

`TxPoWValidator` performs all stateless validation checks:

| Check | Description |
|-------|-------------|
| PoW | Hash meets minimum difficulty |
| Scripts | All input scripts return TRUE via KISS VM |
| Balance | sum(inputs) >= sum(outputs) |
| CoinIDs | Output coin IDs correctly derived |
| StateVars | Well-formed state variable ports |
| Size | Within magic number limits |

Chain-level validation (MMR proofs, block ordering, longest-chain) is
intentionally out of scope — it belongs in a `ChainProcessor` module
to be added in a later phase.

---

## Dependency Policy

**Zero mandatory external dependencies.**

- C++20 standard library only for core logic
- OpenSSL: optional (crypto backend, off by default)
- doctest: test framework (fetched automatically by CMake, test builds only)

This ensures the library can be compiled for bare-metal targets and FPGAs
without a package manager.

---

## Build

```bash
cmake -B build -DMINIMA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

With OpenSSL:
```bash
cmake -B build -DMINIMA_CRYPTO_OPENSSL=ON
cmake --build build
```

---

## Development Roadmap

### Phase 1 (this repo — scaffold)
- [x] Type system: MiniNumber, MiniData, MiniString
- [x] Protocol objects: Coin, TxPoW, Transaction, Witness
- [x] KISS VM: Tokenizer, Environment, Contract, 42 functions
- [x] Crypto interface: Hash, Schnorr
- [x] Serialisation: DataStream
- [x] Validation: TxPoWValidator

### Phase 2 (implementation)
- [ ] MiniNumber arithmetic (string-based BigDecimal)
- [ ] DataStream read/write implementation
- [ ] Tokenizer implementation (KISS VM lexer)
- [ ] KISS VM executor (statement evaluator)
- [ ] All 42 built-in functions
- [ ] SHA3-256 reference implementation
- [ ] Schnorr reference implementation

### Phase 3 (chain)
- [ ] MMR (Merkle Mountain Range) accumulator
- [ ] ChainProcessor (block ordering, longest chain, reorg)
- [ ] P2P message types (Greeting, IBD, Pulse)
- [ ] Mempool

### Phase 4 (hardening)
- [ ] Fuzz testing (libFuzzer / AFL++)
- [ ] Formal spec alignment (cross-check vs Java reference)
- [ ] FPGA synthesis verification
