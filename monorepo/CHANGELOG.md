# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.2] — 2026-03-14

### Security

#### `minima-contracts` — security audit

- **SECURITY:** The `state-channel` contract had a nonce replay vulnerability — `currentNonce GTE prevNonce` allowed the same state to be submitted twice. Fixed to `currentNonce GT prevNonce` (strict greater than).
- Added regression test: `[runtime] same-nonce replay blocked`.

### Fixed

- Repository URLs in all `package.json` files pointed to a placeholder (`minima-global/developer-toolkit`) — updated to the correct repository.
- Test count badge updated to 198.

---

## [0.1.1] — 2026-03-14

### Fixed

#### `minima-test` — bug fixes (audit pass)

- **Bug #1:** The tokenizer was trimming whitespace inside string literals `[...]` — spaces are now preserved exactly as written.
- **Bug #2:** `MULTISIG` always returned `FALSE` — real n-of-k signature checking against `env.signatures` is now implemented.
- **Bug #3:** An empty hex literal `0x` (without digits) silently produced a broken token — now throws a clear tokenizer error.
- `EXEC` sub-scripts were resetting the instruction counter, allowing bypass of the 1024-instruction limit — fixed.
- Division and modulo by zero produced `Infinity` — now throws a runtime error.
- `STATE(n)` and `PREVSTATE(n)` threw for unset ports — now return `0` per the Minima specification.
- `globals` in `runScript()` options only accepted `MiniValue` — now also accepts `string` and `number` (auto-coerced).
- Added `tests/06-audit-fixes.test.js` (23 new tests covering all fixes and edge cases).

#### CI

- Fixed `ci.yml` to trigger on the `main` branch.

### Added

- 17 new regression tests covering all audit findings (92 total in `minima-test`).

---

## [0.1.0] — 2026-03-14

### Added

#### `minima-test` v0.1.0

- Complete KISS VM interpreter in TypeScript (42 built-in functions).
- Jest-like test API: `describe`, `it`, `expect`, `runScript`.
- Full transaction mock: UTxO inputs/outputs, state variables, globals (`@BLOCK`, `@AMOUNT`, etc.).
- CLI: `minima-test run tests/` and `minima-test eval "RETURN TRUE"`.
- 75 tests covering: basic scripts, math functions, contract patterns, HEX operations, and instruction limits.

#### `kiss-vm-lint` v0.1.0

- Static analyser for KISS VM scripts (no execution required).
- 20+ error codes: tokenizer errors, missing RETURN, IF/WHILE structure violations, function arity mismatches.
- 6+ warning codes: unused variables, dead code, unknown globals, `=` vs `EQ`.
- Instruction count estimate.
- CLI: `kiss-vm-lint script.kiss` with optional `--json` output.
- 40 tests.

#### `minima-contracts` v0.1.0

- 12 ready-to-use KISS VM contract templates: `basic-signed`, `time-lock`, `coin-age`, `multi-sig`, `htlc`, `exchange`, `conditional-payment`, `hodl-vault`, `split-payment`, `recurring-payment`, `state-channel`, `mast-contract`.
- `ContractLibrary.compile(name, params)` — injects parameters and returns a ready-to-use script.
- `ContractLibrary.list()`, `.get(name)`, `.validate(name, params)`.
- 59 tests (structural + runtime via `minima-test`).

#### `create-minidapp` v0.1.0

- Scaffold CLI for MiniDapp projects.
- 3 templates: `default`, `exchange`, `counter`.
- Generates: `dapp.conf`, `icon.png` placeholder, `index.html`, `mds.js`, `service.js`.
- Validates output: no unresolved `{{placeholder}}` tokens.
- 23 tests.

#### Monorepo infrastructure

- npm workspaces.
- GitHub Actions CI: Node 18 / 20 / 22 matrix, auto-publish on `release:` commits.
- PR checks: per-package changed-file detection.
