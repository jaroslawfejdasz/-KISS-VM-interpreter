# AGENT RULES — minima-core-cpp

## Mission
Rewrite Minima blockchain core 1:1 in C++20 for bare-metal ARM chips.
Reference: https://github.com/minima-global/Minima (Java)

---

## Session Start — do this FIRST, every session

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

## After EVERY compile+test pass → IMMEDIATELY push

```bash
cd /app
git add -A
git commit -m "feat: <what was done>"
git push github main
```

**This is not optional.** If the session dies before push — work is lost.

---

## cmake build command (exact)

```bash
rm -rf /tmp/build && mkdir /tmp/build
cmake /app -DCMAKE_BUILD_TYPE=Debug -DMINIMA_BUILD_TESTS=ON \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev -G "Unix Makefiles" \
  -S /app -B /tmp/build
make -C /tmp/build -j$(nproc) && ctest --test-dir /tmp/build --output-on-failure
```

`-G "Unix Makefiles"` is required — without it cmake picks Ninja and the
`make` command fails.

---

## Coding Rules

- Every new file → immediately add to `CMakeLists.txt` sources list
- Every new class → write matching test in `tests/<module>/test_<module>.cpp`
- Check Java reference before writing wire formats
- No half-finished pushes — compile+test FIRST, then push
- Small iterations: max 3-4 files per commit

---

## API Conventions

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
// then: h3.bytes() to get std::vector<uint8_t>

// Coin getters
c.coinID()           // MiniData
c.amount()           // MiniNumber
c.address().hash()   // MiniData

// TxBlock
block.txpow()        // NOT .getTxPoW()

// Namespaces
minima::            // types, objects, kissvm, crypto, mmr, validation
minima::chain::     // chain, cascade
minima::network::   // NIOServer, NIOClient, NIOMessage
minima::crypto::    // Hash, Winternitz, TreeKey
```

---

## Test Template

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

## Repository Layout

```
/app/                   ← git root, also cmake root
├── src/                ← all C++ source
├── tests/              ← one test file per module
├── monorepo/           ← TypeScript packages (separate from C++)
├── cmake/              ← ARM cross-compile toolchains
├── .github/workflows/  ← CI
├── CMakeLists.txt
├── IMPLEMENTATION_STATUS.md
├── AGENT_RULES.md      ← this file
└── docs/ARCHITECTURE.md
```

---

## Current Priorities (as of 2026-03-22)

1. **npm publish** — publish monorepo packages to npm registry
2. **Persistence integration** — wire BlockStore + UTxOStore into ChainProcessor
3. **Network I/O integration** — complete NIOServer peer handshake
4. **P2P sync loop** — IBD request/response, flood-fill propagation
5. **ARM QEMU testing** — execute cross-compiled binaries in CI
