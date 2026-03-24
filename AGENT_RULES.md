# Development Rules — minima-core-cpp

## Mission

Re-implement the Minima blockchain core 1:1 in C++20, targeting bare-metal ARM chips.
Reference: https://github.com/minima-global/Minima (Java)

---

## Session start — do this first, every session

```bash
cd /app
git config user.email "jaroslawfejdasz@gmail.com"
git config user.name "Jaroslaw Fejdasz"
git remote set-url github "https://$GITHUB_ACCESS_TOKEN@github.com/jaroslawfejdasz/minima-core-cpp.git"
git fetch github && git reset --hard github/main

# Verify baseline build
rm -rf /tmp/build && mkdir /tmp/build
cmake /app -DCMAKE_BUILD_TYPE=Debug -DMINIMA_BUILD_TESTS=ON \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev -G "Unix Makefiles" \
  -S /app -B /tmp/build
make -C /tmp/build -j$(nproc) && ctest --test-dir /tmp/build --output-on-failure

# Check recent history
git log --oneline -5
```

Then read `IMPLEMENTATION_STATUS.md` and pick the next TODO item.

---

## After every compile + test pass → push immediately

```bash
cd /app
git add -A
git commit -m "feat: <what was done>"
git push github main
```

Do not skip this step. If the session ends before a push, the work is lost.

---

## cmake build command (exact)

```bash
rm -rf /tmp/build && mkdir /tmp/build
cmake /app -DCMAKE_BUILD_TYPE=Debug -DMINIMA_BUILD_TESTS=ON \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev -G "Unix Makefiles" \
  -S /app -B /tmp/build
make -C /tmp/build -j$(nproc) && ctest --test-dir /tmp/build --output-on-failure
```

`-G "Unix Makefiles"` is required — without it CMake picks Ninja and `make` fails.

---

## Coding rules

- Every new file must be added to `CMakeLists.txt` sources immediately.
- Every new class must have a matching test in `tests/<module>/test_<module>.cpp`.
- Always check the Java reference before writing wire format code.
- Never push code that does not compile and pass tests.
- Keep commits small: max 3–4 files per commit.

---

## API conventions

```cpp
// MiniData
MiniData d = MiniData::fromHex("0xABCD");
const std::vector<uint8_t>& raw = d.bytes();   // .bytes(), NOT .getBytes()

// MiniNumber
MiniNumber n(int64_t(42));                      // int64_t cast required
MiniNumber s("1000.5");                         // from string also fine

// MiniString
MiniString ms("hello");
const std::string& str = ms.str();              // .str(), NOT .toString()

// Hash
MiniData h3 = Hash::sha3_256(data);            // returns MiniData

// Coin getters
c.coinID()           // MiniData
c.amount()           // MiniNumber
c.address().hash()   // MiniData

// TxPoW identity
txpow.computeID()              // SHA3(SHA3(header)) — NOT a field in the header
txpow.header().superParents[0] // direct parent ID

// TxBlock
block.txpow()        // NOT .getTxPoW()

// Namespaces
minima::            // types, objects, kissvm, crypto, mmr, validation
minima::chain::     // chain, cascade
minima::network::   // NIOServer, NIOClient, NIOMessage
minima::crypto::    // Hash, Winternitz, TreeKey
```

---

## Test template

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/<module>/<Class>.hpp"

TEST_CASE("<Class> basic") {
    // arrange
    // act
    // assert
    CHECK(result == expected);
}
```

---

## Repository layout

```
/app/                   ← git root and cmake root
├── src/                ← all C++ source
├── tests/              ← one test file per module
├── monorepo/           ← TypeScript packages (separate project)
├── cmake/              ← ARM cross-compilation toolchains
├── .github/workflows/  ← CI
├── CMakeLists.txt
├── IMPLEMENTATION_STATUS.md
├── AGENT_RULES.md      ← this file
└── docs/ARCHITECTURE.md
```

---

## Current priorities

1. **npm publish** — publish monorepo packages to the npm registry
2. **Live node test** — connect to a real Minima seed node and exchange a Greeting message
3. **ARM QEMU testing in CI** — execute cross-compiled binaries in GitHub Actions
