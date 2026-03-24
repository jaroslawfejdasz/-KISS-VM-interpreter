<div align="center">
  <h1>Minima Developer Toolkit</h1>
  <p><strong>TypeScript tooling for building smart contracts and MiniDapps on <a href="https://minima.global">Minima</a>.</strong></p>

  [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
  [![Tests](https://img.shields.io/badge/tests-203%20passing-brightgreen)](https://github.com/jaroslawfejdasz/minima-core-cpp/actions)
</div>

---

## Packages

| Package | Version | Description |
|---------|---------|-------------|
| [`minima-test`](./packages/minima-test) | 0.1.0 | Unit testing framework for KISS VM smart contracts |
| [`kiss-vm-lint`](./packages/kiss-vm-lint) | 0.1.0 | Static analyser — catches errors before you run |
| [`minima-contracts`](./packages/minima-contracts) | 0.1.0 | Library of 12 audited KISS VM contract patterns |
| [`create-minidapp`](./packages/create-minidapp) | 0.1.0 | Scaffold CLI for new MiniDapp projects |

---

## Quick start

### Test your KISS VM contracts

```bash
npm install --save-dev minima-test
```

```js
// tests/my-contract.test.js
const { describe, it, expect, runScript } = require('minima-test');

describe('My Contract', () => {
  it('passes when signed by owner', () => {
    expect(runScript(
      'RETURN SIGNEDBY(0xOwnerPubKey)',
      { signatures: ['0xOwnerPubKey'] }
    )).toPass();
  });
});
```

```bash
npx minima-test run tests/
```

### Lint your scripts

```bash
npm install --save-dev kiss-vm-lint
npx kiss-vm-lint my-contract.kiss
```

### Use pre-built contract patterns

```bash
npm install minima-contracts
```

```js
const { contracts } = require('minima-contracts');

const { script } = contracts.compile('time-lock', {
  ownerPubKey: '0xYourPublicKey',
  unlockBlock: 500000
});
// → ready-to-use KISS VM script
```

### Scaffold a new MiniDapp

```bash
npx create-minidapp my-dapp
npx create-minidapp my-swap --template exchange
npx create-minidapp my-counter --template counter
```

---

## What is Minima?

Minima is a Layer 1 blockchain where every device — smartphone, IoT board, desktop — runs a full node. There are no dedicated miners and no cloud servers.

Smart contracts use **KISS VM**: each UTxO carries a locking script that must return `TRUE` for the transaction to be valid. The scripting language is deterministic and bounded (1024 instruction limit, 64 stack depth).

**The stack:**
- **L1 — Minima**: UTxO blockchain, flood-fill P2P consensus
- **L2 — Maxima**: Off-chain P2P messaging
- **L3 — MiniDapps**: ZIP web apps that run directly on your node

---

## Repository structure

```
packages/
├── minima-test/          # KISS VM test runner (75 tests)
├── kiss-vm-lint/         # Static analyser (40 tests)
├── minima-contracts/     # Contract library (59 tests)
└── create-minidapp/      # Scaffold CLI (23 tests)
.github/
└── workflows/
    └── ci.yml            # GitHub Actions CI (Node 18 / 20 / 22)
```

**Total: 198 automated tests across all packages.**

---

## Development

```bash
git clone https://github.com/jaroslawfejdasz/minima-core-cpp
cd minima-core-cpp/monorepo
npm install        # installs all workspace dependencies
npm run build      # builds all packages
npm test           # runs all tests
```

---

## Roadmap

- [x] `minima-test` — KISS VM testing framework
- [x] `kiss-vm-lint` — static analyser
- [x] `minima-contracts` — 12 contract patterns
- [x] `create-minidapp` — scaffold CLI with 3 templates
- [ ] `minima-test` v0.2 — real CHECKSIG support via Minima node RPC
- [ ] `kiss-vm-lint` v0.2 — type inference, dead code detection
- [ ] Additional MiniDapp templates: multisig wallet, HTLC swap UI
- [ ] VSCode extension

---

## Contributing

PRs are welcome. Each package has its own test suite — make sure `npm test` passes before submitting. See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## License

MIT — see [LICENSE](LICENSE)
