# Minima Developer Toolkit

> The missing developer toolchain for building on [Minima blockchain](https://docs.minima.global)

A monorepo of TypeScript packages that bring modern developer experience to Minima smart contract and MiniDapp development.

---

## Packages

| Package | Description | Version |
|---------|-------------|---------|
| [`minima-test`](./minima-test) | Testing framework for KISS VM smart contracts | 0.1.0 |
| [`kiss-vm-lint`](./kiss-vm-lint) | Static analyzer and linter for KISS VM scripts | 0.1.0 |
| [`minima-contracts`](./minima-contracts) | Library of audited contract patterns | 0.1.0 |
| [`create-minidapp`](./create-minidapp) | Scaffold CLI for new MiniDapp projects | 0.1.0 |

---

## Quick Start

### Test your KISS VM contracts

```bash
npm install -g minima-test
minima-test run tests/
```

### Lint your contracts

```bash
npm install -g kiss-vm-lint
kiss-vm-lint contracts/my-contract.kiss
```

### Use audited contract patterns

```js
const { timeLock, multisig, exchange } = require('minima-contracts');

const contract = timeLock({ lockBlock: 1000000, ownerKey: '0xabc...' });
```

### Scaffold a new MiniDapp

```bash
npx create-minidapp my-dapp --template wallet
cd my-dapp
# Open index.html on your Minima node
```

---

## What is Minima?

Minima is a Layer 1 blockchain where **every user runs a full node** — on mobile, IoT, or desktop. No dedicated miners. No cloud servers. True decentralization.

- **UTxO model** (like Bitcoin, not Ethereum)
- **KISS VM** — smart contract scripting language (1024 instruction limit, deterministic)
- **MiniDapps** — decentralized apps installed directly on user nodes
- **Maxima** — P2P messaging layer for off-chain communication

---

## Architecture

```
minima-developer-toolkit/
├── minima-test/          # Testing framework (Jest-like API)
│   ├── src/
│   │   ├── tokenizer/    # KISS VM tokenizer
│   │   ├── interpreter/  # Full KISS VM interpreter
│   │   ├── mock/         # Transaction mocking
│   │   └── api/          # Test API (describe/it/expect)
│   └── tests/            # 75 tests
│
├── kiss-vm-lint/         # Static analyzer
│   ├── src/
│   │   ├── rules/        # Lint rules
│   │   └── cli.ts        # CLI
│   └── tests/            # 40 tests
│
├── minima-contracts/     # Contract library
│   ├── src/
│   │   └── contracts/    # 12 contract patterns
│   └── tests/            # 59 tests
│
└── create-minidapp/      # Scaffold CLI
    ├── src/
    │   ├── templates/    # wallet / token / exchange
    │   └── cli.ts        # CLI
    └── tests/            # 23 tests
```

---

## Development

Each package is independent — install and build separately:

```bash
cd minima-test && npm install && npm run build && npm test
cd kiss-vm-lint && npm install && npm run build && npm test
cd minima-contracts && npm install && npm run build && npm test
cd create-minidapp && npm install && npm run build && npm test
```

---

## CI/CD

GitHub Actions runs tests on Node 18, 20, 22. Publishing to npm happens automatically on `v*` tags.

See [`.github/workflows/ci.yml`](./.github/workflows/ci.yml).

---

## License

MIT — see [LICENSE](./LICENSE)

---

*Built with ❤️ for the Minima ecosystem*
