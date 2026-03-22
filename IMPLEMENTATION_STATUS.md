# Minima Core C++ — Implementation Status

**Last updated:** 2026-03-22  
**Build:** ✅ 0 errors, 0 warnings (non-vendor)  
**Tests:** ✅ 15/15 suites, 442 assertions, 0 failures

---

## Test Suites

| # | Suite | Tests | Assertions | Status |
|---|-------|-------|------------|--------|
| 1 | test_mini_number | 12 | 17 | ✅ |
| 2 | test_mini_data | 7 | 12 | ✅ |
| 3 | test_kissvm | 45 | 59 | ✅ |
| 4 | test_txpow | 1 | 1 | ✅ |
| 5 | test_validation | 17 | 24 | ✅ |
| 6 | test_mmr | 20 | 42 | ✅ |
| 7 | test_chain | 12 | 21 | ✅ |
| 8 | test_token | 14 | 26 | ✅ |
| 9 | test_mining | 16 | 29 | ✅ |
| 10 | test_difficulty | 8 | 13 | ✅ |
| 11 | test_network | 10 | 69 | ✅ |
| 12 | test_p2p_sync | 7 | 20 | ✅ |
| 13 | test_ibd | 8 | 21 | ✅ |
| 14 | test_persistence | 21 | 33 | ✅ |
| 15 | test_cascade | 21 | 55 | ✅ |
| **TOTAL** | | **219** | **442** | **✅ 100%** |

---

## Module Status

| Module | Files | Java parity | Notes |
|--------|-------|-------------|-------|
| **Types** | MiniNumber, MiniData, MiniString | ✅ 1:1 | Wire-exact serialise |
| **Objects** | TxPoW, TxHeader, TxBody, Coin, Witness, Transaction, Address, StateVariable | ✅ 1:1 | |
| **Objects ext.** | Token, TxBlock, Greeting, CoinProof, IBD, Magic | ✅ 1:1 | |
| **KISS VM** | Tokenizer, Parser, Interpreter, Environment, Contract, Functions (42+) | ✅ 1:1 | 1024 instr, 64 stack |
| **Crypto** | SHA2/SHA3 (pure C++), RSA-1024 verify | ✅ | Schnorr stub present |
| **MMR** | MMRSet, MMREntry, MMRProof, MMRData | ✅ 1:1 | Peaks, proof verify |
| **Chain** | ChainState, ChainProcessor, BlockStore, UTxOSet, DifficultyAdjust | ✅ 1:1 | 256-block window retarget |
| **Cascade** | CascadeNode, Cascade | ✅ 1:1 | cascadeChain, serialise |
| **Mining** | TxPoWMiner, MiningManager | ✅ | Continuous loop, difficulty |
| **Network** | NIOMessage (24 types), NIOServer/Client, P2PSync | ✅ 1:1 | Wire-exact |
| **Persistence** | BlockStore (SQLite), UTxOStore | ✅ | SQLite3 embedded |
| **Node** | main.cpp | ✅ | Full node entry point |
| **ARM toolchain** | cmake/toolchain-aarch64.cmake, toolchain-armv7.cmake | ✅ | CI cross-compile |

---

## CI Matrix

| Job | Platform | Status |
|-----|----------|--------|
| build-and-test | Ubuntu 22.04 (GCC 11) | ✅ |
| build-and-test | Ubuntu 24.04 (GCC 13) | ✅ |
| clang-build | Ubuntu 22.04 (Clang 15) | ✅ |
| cross-aarch64 | aarch64-linux-gnu | ✅ artifact |
| cross-armv7 | armv7-linux-gnueabihf | ✅ artifact |

---

## Binary

- **Format:** ELF64, PIE, x86-64
- **Size:** ~5.7 MB (Debug), ~1.5 MB expected (Release + strip)
- **Dependencies:** libstdc++, libsqlite3 (embedded), pthreads

---

## Known Limitations

1. **Schnorr signatures** — stub only; full Schnorr requires secp256k1 or custom bignum
2. **QEMU testing** — ARM binaries not executed in CI (cross-compiled only)
3. **Maxima/Omnia** — L2 layers not implemented (out of scope for core)
