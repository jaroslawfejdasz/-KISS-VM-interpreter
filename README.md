<div align="center">
  <h1>⛓️ minima-core-cpp</h1>
  <p><strong>C++ implementation of the Minima blockchain core protocol</strong></p>
  <p>UTxO model · TxPoW consensus · KISS VM smart contracts · Schnorr signatures</p>

  [![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
  [![C++](https://img.shields.io/badge/C++-20-blue.svg)](CMakeLists.txt)
  [![Status](https://img.shields.io/badge/status-scaffold-orange.svg)](docs/ARCHITECTURE.md)
</div>

---

## What is this?

A clean-room C++ port of the [Minima blockchain](https://minima.global) core protocol,
extracted from the reference [Java implementation](https://github.com/minima-global/Minima).

**Goals:**
- Native-level performance for edge devices (IoT, mobile, 300 MB RAM)
- FPGA-ready (no dynamic allocation in hot paths)
- Zero mandatory external dependencies
- Modular — use only the parts you need

> Minima itself [is migrating from Java to C++](https://www.linkedin.com/posts/minimaglobal_minima-has-nearly-completed-the-full-conversion-activity-7394416482753040384-hWOG)
> and has already run on FPGA. This repo is a community implementation aligned with that direction.

---

## Architecture

```
src/
├── types/          MiniNumber · MiniData · MiniString
├── objects/        Coin · TxPoW · Transaction · Witness · TxHeader · TxBody
├── kissvm/         KISS VM interpreter (tokenizer + executor + 42 functions)
├── crypto/         SHA2/SHA3 · Schnorr signatures
├── serialization/  Minima wire format (DataStream)
└── validation/     TxPoW validation · script execution
```

Full architecture doc: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

---

## Build

**Requirements:** CMake 3.16+, C++20 compiler (GCC 11+, Clang 13+, MSVC 2022+)

```bash
git clone https://github.com/jaroslawfejdasz/minima-core-cpp
cd minima-core-cpp

cmake -B build -DMINIMA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

With OpenSSL crypto backend:
```bash
cmake -B build -DMINIMA_CRYPTO_OPENSSL=ON -DMINIMA_BUILD_TESTS=ON
cmake --build build
```

---

## Quick Example

```cpp
#include "minima_core.hpp"

using namespace minima;
using namespace minima::kissvm;

// Validate a KISS VM script
Transaction txn;
Witness     wit;

// Add a signature to the witness
SignatureProof sp;
sp.pubKey    = MiniData::fromHex("0xABCD...");
sp.signature = MiniData::fromHex("0xSIG...");
wit.addSignature(sp);

// Execute the contract
Contract c("RETURN SIGNEDBY(0xABCD...)", txn, wit, 0);
Value result = c.execute();

if (result.isTrue()) {
    // Spend is valid
}
```

---

## Key Protocol Concepts

### TxPoW — Transaction Proof of Work

Every transaction in Minima IS a potential block. Users spend ~1 second
performing PoW when submitting a transaction. If the hash crosses the block
difficulty threshold, their transaction becomes a block — no dedicated miners.

### UTxO Model

Like Bitcoin: coins are unspent outputs, spending requires a valid script proof.
Unlike Bitcoin: scripts are KISS VM (richer, but still deterministic and bounded).

### KISS VM

Minima's smart contract language. Every coin has a script that must return `TRUE`
for the coin to be spent. Default: `RETURN SIGNEDBY(pubKey)`.

Limits: **1024 instructions**, **64 stack depth** — enforced at runtime.

---

## Status

> **Phase 1 complete: scaffold + interfaces**
> All headers defined, architecture documented, test harness in place.
> Phase 2 is implementing the `.cpp` bodies. See [ARCHITECTURE.md](docs/ARCHITECTURE.md).

| Module | Headers | Implementation | Tests |
|--------|---------|---------------|-------|
| types/ | ✅ | 🔲 | 🔲 |
| objects/ | ✅ | 🔲 | 🔲 |
| kissvm/ | ✅ | 🔲 | 🔲 |
| crypto/ | ✅ | 🔲 | 🔲 |
| serialization/ | ✅ | 🔲 | 🔲 |
| validation/ | ✅ | 🔲 | 🔲 |

---

## Reference

- [Minima Docs](https://docs.minima.global)
- [Java Reference Implementation](https://github.com/minima-global/Minima)
- [KISS VM Scripting](https://docs.minima.global/docs/development/contracts-basics)
