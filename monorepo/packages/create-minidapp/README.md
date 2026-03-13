# create-minidapp

> Scaffold CLI for [Minima](https://minima.global) MiniDapp projects. Like `create-react-app`, but for Minima.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![npm version](https://badge.fury.io/js/create-minidapp.svg)](https://www.npmjs.com/package/create-minidapp)

## Quick Start

```bash
npx create-minidapp my-dapp
cd my-dapp
npm run package   # creates my-dapp.mds.zip
```

Then install the `.mds.zip` on your Minima node.

## Templates

| Template | Description |
|----------|-------------|
| `basic` | Minimal MiniDapp — HTML + MDS API connection |
| `counter` | On-chain counter with KISS VM contract |
| `exchange` | Token swap UI with VERIFYOUT contract |

```bash
npx create-minidapp my-swap --template exchange
npx create-minidapp my-counter --template counter
```

## What's Generated

```
my-dapp/
├── dapp.conf        # MiniDapp manifest (name, version, permissions)
├── index.html       # Main UI (vanilla HTML/CSS/JS — no framework needed)
├── mds.js           # Minima MDS API (replace with real file from node)
├── service.js       # Background service worker (optional)
├── package.json     # npm scripts for packaging
└── README.md        # Getting started guide
```

For contract templates, also includes:
```
└── contract.kiss    # KISS VM smart contract script
```

## Packaging

```bash
npm run package   # → my-dapp.mds.zip
```

Then in your Minima terminal:
```
minidapp install file:///path/to/my-dapp.mds.zip
```

## Why No Framework?

MiniDapps run on edge devices with 300MB RAM. Vanilla HTML/JS loads instantly, works offline, and doesn't add megabytes of dependencies. The MDS API is your backend — no servers needed.

## Related Tools

- [`minima-test`](https://npmjs.com/package/minima-test) — Unit testing for KISS VM contracts
- [`kiss-vm-lint`](https://npmjs.com/package/kiss-vm-lint) — Static analyzer for KISS VM scripts
- [`minima-contracts`](https://npmjs.com/package/minima-contracts) — Ready-to-use contract patterns

## License

MIT
