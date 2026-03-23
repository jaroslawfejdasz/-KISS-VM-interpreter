<div align="center">
  <h1>🛠️ Minima Developer Toolkit</h1>
  <p><strong>The complete developer toolkit for building on the <a href="https://minima.global">Minima blockchain</a>.</strong></p>

  [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
  [![Tests](https://img.shields.io/badge/tests-203%20passing-brightgreen)](https://github.com/jaroslawfejdasz/-KISS-VM-interpreter/actions)
</div>

---

## Packages

| Package | Version | Description |
|---------|---------|-------------|
| [`minima-test`](./packages/minima-test) | 0.1.0 | Unit testing framework for KISS VM smart contracts |
| [`kiss-vm-lint`](./packages/kiss-vm-lint) | 0.1.0 | Static analyzer — catches errors before you run |
| [`minima-contracts`](./packages/minima-contracts) | 0.1.0 | Ready-to-use KISS VM contract patterns (12 contracts) |
| [`create-minidapp`](./packages/create-minidapp) | 0.1.0 | Scaffold CLI for MiniDapp projects |

---

## Quick Start

### Test your KISS VM contracts

```bash
npm install --save-dev minima-test

# tests/my-contract.test.js
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

// Compile a time-lock contract
const { script } = contracts.compile('time-lock', {
  ownerPubKey: '0xYourPublicKey',
  unlockBlock: 500000
});
// → KISS VM script ready to use
```

### Scaffold a new MiniDapp

```bash
npx create-minidapp my-dapp
npx create-minidapp my-swap --template exchange
npx create-minidapp my-counter --template counter
```

---

## What is Minima?

Minima is a Layer 1 blockchain where **every device is a full node** — smartphones, IoT devices, anything with 300MB RAM. There are no miners, no cloud servers — pure edge computing.

Smart contracts use **KISS VM** — a simple but powerful scripting language. Each UTxO has a script that must return `TRUE` for the transaction to be valid.

**The stack:**
- **L1 — Minima**: UTxO blockchain, flood-fill P2P consensus
- **L2 — Maxima**: Off-chain P2P messaging
- **L3 — MiniDapps**: ZIP web apps that run on your node

---

## Architecture

```
minima-developer-toolkit/
├── packages/
│   ├── minima-test/          # KISS VM test runner (75 tests)
│   ├── kiss-vm-lint/         # Static analyzer (40 tests)
│   ├── minima-contracts/     # Contract library (59 tests)
│   └── create-minidapp/      # Scaffold CLI (23 tests)
├── .github/
│   └── workflows/
│       └── ci.yml            # GitHub Actions CI
└── README.md
```

**Total: 198 automated tests across all packages.**

---

## Development

```bash
git clone https://github.com/jaroslawfejdasz/-KISS-VM-interpreter
cd minima-developer-toolkit
npm install          # installs all workspace deps
npm run build        # builds all packages
npm test             # runs all 202 tests
```

---

## Roadmap

- [x] `minima-test` — KISS VM testing framework
- [x] `kiss-vm-lint` — Static analyzer
- [x] `minima-contracts` — 12 contract patterns
- [x] `create-minidapp` — Scaffold CLI (3 templates)
- [ ] `minima-test` v0.2 — Real CHECKSIG support (via Minima node RPC)
- [ ] `kiss-vm-lint` v0.2 — Type inference, dead code detection
- [ ] More templates: multisig wallet, HTLC swap UI
- [ ] VSCode extension

---

## Contributing

PRs welcome. Each package has its own test suite — make sure `npm test` passes before submitting.

## License

MIT — see [LICENSE](LICENSE)
