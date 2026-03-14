# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- 45 tests

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
