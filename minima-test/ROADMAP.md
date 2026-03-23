# Minima Developer Toolkit — Roadmap

## Status ogólny
Ostatnia aktualizacja: 2026-03-12 22:25

## Fazy

### FAZA 1 — minima-test (DONE ✅)
- [x] Interpreter KISS VM (75 testów przechodzi)
- [x] Audyt + naprawione bugi (3 bugi)
- [x] CLI: minima-test run / eval

### FAZA 2 — kiss-vm-lint (DONE ✅)
- [ ] Statyczny analizator skryptów KISS VM
- [ ] Wykrywa błędy bez uruchamiania (undefined vars, unreachable code, limit warnings)
- [ ] CLI: kiss-vm-lint script.kiss
- [ ] Testy + audyt

### FAZA 3 — minima-contracts (CURRENT)
- [ ] Biblioteka gotowych wzorców kontraktów
- [ ] Wzorce: BasicSigned, TimeLock, MultiSig, HTLC, Exchange, MAST, Flashcash, HODLer
- [ ] Każdy z testami + dokumentacją
- [ ] Testy + audyt

### FAZA 4 — create-minidapp
- [ ] Scaffold CLI: npx create-minidapp my-app
- [ ] Szablony: blank, token-wallet, exchange, voting
- [ ] Testy + audyt

### FAZA 5 — Monorepo + Docs + CI
- [ ] Monorepo (packages/*)
- [ ] README.md dla każdego pakietu
- [ ] GitHub Actions CI (testy na każdy PR)
- [ ] Finalna wersja gotowa na GitHub

## Log pracy

## Log pracy
- 2026-03-12 22:25 — Start autonomicznego buildu
- 2026-03-12 22:3x — kiss-vm-lint ✅ (25 testów, 10 reguł lintowania)
