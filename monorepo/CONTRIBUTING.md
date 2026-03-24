# Contributing to Minima Developer Toolkit

Contributions are welcome. This document explains how to get the project running locally and how to submit changes.

---

## Setup

```bash
git clone https://github.com/jaroslawfejdasz/minima-core-cpp
cd minima-core-cpp/monorepo
npm install
npm run build
npm test
```

All 198 tests should pass before you start making changes.

---

## Repository structure

```
packages/
├── minima-test/        # KISS VM testing framework
├── kiss-vm-lint/       # Static analyser
├── minima-contracts/   # Contract library
└── create-minidapp/    # Scaffold CLI
```

---

## Development workflow

1. Make changes in the relevant `packages/<name>/src/` directory.
2. Build the changed package: `cd packages/<name> && npm run build`
3. Test the changed package: `cd packages/<name> && npm test`
4. Run the full suite from the monorepo root: `npm test`

---

## Adding a new KISS VM built-in function

The KISS VM interpreter lives in `packages/minima-test/src/interpreter/functions.ts`.

1. Add the function to the `FUNCTIONS` map.
2. Add it to the `KEYWORDS` set in `tokenizer/index.ts`.
3. Add arity information in `kiss-vm-lint/src/rules.ts`.
4. Write tests in `packages/minima-test/tests/`.

---

## Adding a new contract pattern

1. Create a new `.ts` file in `packages/minima-contracts/src/contracts/`.
2. Register it in `packages/minima-contracts/src/index.ts`.
3. Write tests in `packages/minima-contracts/tests/run.js`.

---

## Commit convention

```
feat(minima-test): add CHECKSIG mock with real secp256k1
fix(kiss-vm-lint): handle nested ELSEIF chains correctly
docs: update README quick-start examples
release: v0.2.0
```

Use `release: vX.Y.Z` to trigger automatic npm publish via CI.

---

## Code style

- TypeScript for all source files.
- No external runtime dependencies — the KISS VM interpreter must be self-contained.
- Every new feature must include tests.
- Error messages must be clear and actionable.

---

## Questions

Open an issue or reach out on the [Minima Discord](https://discord.gg/minima).
