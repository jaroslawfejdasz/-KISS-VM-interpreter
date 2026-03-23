# Changelog

## [0.1.1] - 2026-03-14 — Post-Audit Fixes

### Fixed
- **minima-test**: Tokenizer incorrectly trimmed whitespace from string literals `[...]` — spaces now preserved
- **minima-test**: `MULTISIG` always returned FALSE — now correctly implements n-of-k signature checking
- **minima-test**: HEX literal `0x` without digits silently produced empty token — now throws parse error
- **minima-test**: `EXEC` sub-script reset instruction counter, allowing bypass of 1024-instruction limit — fixed
- **minima-test**: Division and modulo by zero produced `Infinity` — now throws a runtime error
- **minima-test**: `STATE(n)` and `PREVSTATE(n)` threw for unset ports — now return `0` per Minima spec
- **minima-test**: `globals` in `runScript()` options only accepted `MiniValue` — now also accepts `string` and `number` (auto-coerced)

### Added
- **minima-test**: 17 new regression tests covering all audit findings (92 total in minima-test package)


All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] - 2026-03-14

### Security

#### `minima-contracts` security audit
- **SECURITY:** `state-channel` contract had a nonce replay vulnerability — `currentNonce GTE prevNonce` allowed submitting the same state twice. Fixed to `currentNonce GT prevNonce` (strict greater than).
- Added test: `[runtime] same-nonce replay blocked` to cover this exact case.

### Fixed
- Repository URLs in all `package.json` files pointed to placeholder `minima-global/developer-toolkit` — updated to actual repo `jaroslawfejdasz/-KISS-VM-interpreter`
- Test count badge updated to 198

## [0.1.1] - 2026-03-14

### Fixed

#### `minima-test` bug fixes (audit pass)
- **BUG #1:** Tokenizer was trimming whitespace inside string literals `[...]` — now preserves spaces exactly
- **BUG #2:** `MULTISIG` always returned `FALSE` — implemented real n-of-k signature checking against `env.signatures`
- **BUG #3:** Empty hex literal `0x` (without digits) silently produced a broken token — now throws a clear tokenizer error
- Added `tests/06-audit-fixes.test.js` (23 new tests covering all fixes and edge cases)

#### CI
- Fixed GitHub Actions `ci.yml` to trigger on `master` branch (was `main` only)
- `publish` job now triggers on both `main` and `master`

## [0.1.0] - 2026-03-14

### Added

#### `minima-test` v0.1.0
- Complete KISS VM interpreter in TypeScript (42 built-in functions)
- Jest-like test API: `describe`, `it`, `expect`, `runScript`
- Full transaction mock: UTxO inputs/outputs, state variables, globals (`@BLOCK`, `@AMOUNT`, etc.)
- CLI: `minima-test run tests/` and `minima-test eval "RETURN TRUE"`
- 75 tests covering: basic scripts, math functions, contract patterns, HEX operations, limits

#### `kiss-vm-lint` v0.1.0
- Static analyzer for KISS VM scripts (no execution required)
- 20+ error codes: tokenizer errors, missing RETURN, IF/WHILE structure, function arity
- 6+ warning codes: unused variables, dead code, unknown globals, `=` vs `EQ`
- Instruction count estimate
- CLI: `kiss-vm-lint script.kiss` with `--json` output
- 40 tests

#### `minima-contracts` v0.1.0
- 12 ready-to-use KISS VM contract templates:
  - `basic-signed`, `time-lock`, `coin-age`, `multi-sig`
  - `htlc`, `exchange`, `conditional-payment`
  - `hodl-vault`, `split-payment`, `recurring-payment`
  - `state-channel`, `mast-contract`
- `ContractLibrary.compile(name, params)` — injects params, returns ready-to-use script
- `ContractLibrary.list()`, `.get(name)`, `.validate(name, params)`
- 59 tests (structural + runtime via minima-test)

#### `create-minidapp` v0.1.0
- Scaffold CLI for MiniDapp projects
- 3 templates: `default`, `exchange`, `counter`
- Generates: `dapp.conf`, `icon.png` placeholder, `index.html`, `mds.js`, `service.js`
- Validates output: no unresolved `{{placeholder}}` tokens
- 23 tests

#### Monorepo infrastructure
- npm workspaces
- GitHub Actions CI: Node 18/20/22 matrix, auto-publish on `release:` commits
- PR checks: per-package changed-file detection
