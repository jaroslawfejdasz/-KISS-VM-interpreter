# minima-test — Roadmap

**Last updated:** 2026-03-24

---

## Completed

### v0.1.0 — Core framework
- [x] Full KISS VM interpreter in TypeScript (42 built-in functions)
- [x] Jest-like test API: `describe`, `it`, `expect`, `runScript`
- [x] Transaction mock: UTxO inputs/outputs, state variables, globals (`@BLOCK`, `@AMOUNT`, etc.)
- [x] CLI: `minima-test run tests/` and `minima-test eval "RETURN TRUE"`
- [x] 75 tests

### v0.1.1 — Bug fixes (audit pass)
- [x] Tokenizer whitespace preservation inside string literals `[...]`
- [x] `MULTISIG` n-of-k signature checking
- [x] Empty hex literal `0x` error handling
- [x] `EXEC` instruction counter isolation
- [x] Division and modulo by zero errors
- [x] `STATE(n)` / `PREVSTATE(n)` returning `0` for unset ports per spec
- [x] 23 regression tests added

---

## Planned

### v0.2.0 — Real signature support
- [ ] `CHECKSIG` backed by real WOTS verification (via Minima node RPC or native WOTS library)
- [ ] `SIGNEDBY` using real key material from a live or mock node

### v0.2.1 — Improved test output
- [ ] Coloured diff output on assertion failures
- [ ] Per-contract instruction count reporting
- [ ] Watch mode: `minima-test run --watch`

### v0.3.0 — Coverage
- [ ] KISS VM contract coverage report (which branches were executed)
- [ ] HTML coverage output
