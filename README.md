# minima-core-cpp

A C++20 implementation of the Minima blockchain core, targeting embedded and mobile devices (ARM, 300 MB RAM). The goal is wire-exact compatibility with the [Java reference implementation](https://github.com/minima-global/Minima) so that this node can participate in the live Minima network as a first-class peer.

---

## What this is

Minima is a Layer 1 blockchain designed to run as a full node on every device — phones, IoT, bare-metal ARM boards. Every participant validates and produces blocks; there are no dedicated miners and no cloud servers. The protocol uses a UTxO model (similar to Bitcoin) and a deterministic scripting language called KISS VM for smart contracts.

This repository contains:

- **`src/`** — C++20 full-node implementation (types, objects, KISS VM interpreter, MMR accumulator, chain, network, mining, persistence)
- **`tests/`** — [doctest](https://github.com/doctest/doctest) unit tests for every module (24 test suites)
- **`monorepo/packages/`** — TypeScript developer tools: testing framework, linter, contract library, MiniDapp scaffold CLI
- **`cmake/`** — cross-compilation toolchains for ARM64 and ARMv7

---

## Building

### Requirements

- CMake 3.16+
- GCC 11+ or Clang 14+ with C++20 support
- For ARM cross-compilation: `aarch64-linux-gnu-g++` or `arm-linux-gnueabihf-g++`

### Linux / macOS

```bash
git clone https://github.com/jaroslawfejdasz/minima-core-cpp
cd minima-core-cpp

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMINIMA_BUILD_TESTS=ON \
  -G "Unix Makefiles"

make -C build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### ARM cross-compilation

```bash
# ARM64 (e.g. Raspberry Pi 4, Android)
cmake -S . -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release

# ARMv7 (e.g. older IoT boards)
cmake -S . -B build-armv7 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-armv7.cmake \
  -DCMAKE_BUILD_TYPE=Release
```

---

## Test status

```
24/24 test suites pass
```

| Module | Tests |
|--------|-------|
| Primitive types (MiniNumber, MiniData, MiniString) | test_mini_number, test_mini_data |
| Cryptography (WOTS, TreeKey, BIP39, Hash) | test_wots, test_treekey, test_bip39 |
| Protocol objects (Coin, TxPoW, Witness, Genesis) | test_txpow, test_integration |
| KISS VM interpreter (42+ built-in functions) | test_kissvm |
| MMR accumulator + MegaMMR | test_mmr, test_megammr |
| Chain (TxPowTree, Cascade, DifficultyAdjust) | test_chain, test_cascade, test_difficulty |
| Database (MinimaDB, Wallet) | test_database |
| Persistence (SQLite BlockStore, UTxOStore) | test_persistence |
| Validation (TxPoWValidator, WOTS signatures) | test_validation |
| Network (NIOClient, NIOServer, P2PSync) | test_network |
| Mining (TxPoWMiner, MiningManager) | test_mining |

---

## Architecture overview

```
┌───────────────────────────────────────────────┐
│              P2P Network Layer                 │
│         NIOServer ◄──► NIOClient               │
│         NIOMessage (wire protocol)             │
└──────────────────┬────────────────────────────┘
                   │
┌──────────────────▼────────────────────────────┐
│            TxPoWProcessor                     │
│     (async message queue + worker thread)     │
└──────┬───────────────────────────┬────────────┘
       │ accepted                  │ sync
┌──────▼──────────┐   ┌────────────▼───────────┐
│   TxPowTree     │   │  TxPoWGenerator        │
│ (chain + reorg) │   │  TxPoWSearcher         │
└──────┬──────────┘   └────────────────────────┘
       │
┌──────▼──────────────────────────────────────┐
│                 MinimaDB                    │
│  TxPowTree + BlockStore + MMRSet + Wallet   │
│  Persistence: SQLite (bootstrap on restart) │
└─────────────────────────────────────────────┘
```

For a detailed description of every layer see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

---

## Developer toolkit (TypeScript)

The `monorepo/` directory contains four npm packages for working with KISS VM and MiniDapps. See [monorepo/README.md](monorepo/README.md) for full documentation.

```
monorepo/packages/
├── minima-test/          # Jest-like test runner for KISS VM contracts
├── kiss-vm-lint/         # Static analyser and linter
├── minima-contracts/     # Library of 12 audited contract patterns
└── create-minidapp/      # Scaffold CLI for new MiniDapp projects
```

---

## What is Minima?

- **Every device is a full node** — 300 MB RAM, runs on a smartphone or IoT board
- **UTxO model** — similar to Bitcoin, not account-based
- **KISS VM** — deterministic smart contract language, 1024 instruction limit
- **MiniDapps** — ZIP web apps that run directly on your node, no cloud required
- **Maxima** — off-chain P2P messaging layer
- **TxPoW** — every transaction carries a small proof-of-work; no block-level mining race

---

## Implementation parity

The implementation aims for **bit-for-bit wire compatibility** with the Java reference. Where the Java code makes unusual choices (e.g. `MiniNumber` stored as a decimal string rather than a binary integer), this implementation does the same. Divergence from the reference is treated as a bug.

See [docs/PARITY_GAP_ANALYSIS.md](docs/PARITY_GAP_ANALYSIS.md) for the current parity status.

---

## CI

GitHub Actions runs the full test suite on three targets:

| Job | Platform | Compiler |
|-----|----------|----------|
| build-linux-x64 | Ubuntu 22.04 | GCC 11 |
| build-linux-arm64 | Ubuntu 22.04 | aarch64-linux-gnu-g++ |
| build-linux-armv7 | Ubuntu 22.04 | arm-linux-gnueabihf-g++ |

---

## License

MIT — see [LICENSE](LICENSE)
