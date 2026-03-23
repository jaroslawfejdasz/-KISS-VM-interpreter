# Contributing to Minima Developer Toolkit

Thank you for your interest in contributing! This document explains how to get started.

## Setup

```bash
git clone https://github.com/jaroslawfejdasz/-KISS-VM-interpreter
cd minima-developer-toolkit
npm install
npm run build
npm test
```

All 198 tests should pass.

## Repository Structure

```
packages/
├── minima-test/        # KISS VM testing framework
├── kiss-vm-lint/       # Static analyzer
├── minima-contracts/   # Contract library
└── create-minidapp/    # Scaffold CLI
```

## Development Workflow

1. Make changes in the relevant `packages/<name>/src/` directory
2. Build: `cd packages/<name> && npm run build`
3. Test: `cd packages/<name> && npm test`
4. Run all tests: `npm test` (from root)

## Adding a new KISS VM function

The KISS VM interpreter is in `packages/minima-test/src/interpreter/functions.ts`.

1. Add the function to the `FUNCTIONS` map
2. Add it to the `KEYWORDS` set in `tokenizer/index.ts`
3. Add arity info in `kiss-vm-lint/src/rules.ts`
4. Write tests in `packages/minima-test/tests/`

## Adding a new contract pattern

1. Add to `packages/minima-contracts/src/contracts/` as a `.ts` file
2. Register in `packages/minima-contracts/src/index.ts`
3. Write tests in `packages/minima-contracts/tests/run.js`

## Commit Convention

```
feat(minima-test): add CHECKSIG mock with real secp256k1
fix(kiss-vm-lint): handle nested ELSEIF chains correctly
docs: update README quick start examples
release: v0.2.0
```

Use `release: vX.Y.Z` to trigger automatic npm publish via CI.

## Code Style

- TypeScript for all source files
- No external runtime dependencies (KISS VM interpreter must be self-contained)
- Every new feature must have tests
- Error messages must be human-readable

## Questions?

Open an issue or reach out on the [Minima Discord](https://discord.gg/minima).
