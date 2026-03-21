# AGENT RULES — minima-core-cpp

## Mission
Rewrite Minima blockchain core 1:1 in C++20 for bare-metal ARM chips.
Reference: https://github.com/minima-global/MinimaCore (Java)

## After EVERY compile+test pass → IMMEDIATELY push to GitHub
```bash
cd /app/minima-core-cpp
git add -A
git commit -m "feat: <what was done>"
git push github main
```
This is NOT optional. If the session dies before push — work is lost.

## Session start checklist (do this FIRST, every session)
1. Read IMPLEMENTATION_STATUS.md — know what's done and what's TODO
2. `cd /tmp && mkdir build && cmake /app/minima-core-cpp ... && make && ctest` — verify baseline
3. Check last git log: `git -C /app/minima-core-cpp log --oneline -5`
4. Pick the next TODO item from IMPLEMENTATION_STATUS.md and execute it

## Coding rules
- Every new file → immediately add to CMakeLists.txt sources list
- Every new class → write matching test in tests/<module>/
- Check Java reference before writing wire formats
- Build with: `cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev`
- No half-finished pushes — compile+test FIRST, then push

## API conventions (C++ side)
- `MiniData` fields: always `MiniData(std::vector<uint8_t>{...})`  
- `MiniNumber` from int: `MiniNumber(int64_t(42))`
- Coin getters: `c.coinID()`, `c.amount()`, `c.address().hash()`
- TxHeader: `timeMilli` (not timestamp), `superParents[0]` (parent), `blockDifficulty` (MiniData)
- TxBody: `txnDifficulty` (MiniData, default 0xFF = any tx accepted)
- Chain namespace: `minima::chain::BlockStore`, `ChainProcessor`, etc.

## What this is NOT
- NOT a MiniDapp server
- NOT Maxima (P2P messaging)
- NOT a wallet UI
- Core protocol only: consensus, UTXO, KISS VM, MMR, P2P sync

## Priority order for next TODO items
1. Token object + token transactions
2. Mining loop (nonce increment + difficulty check)
3. Difficulty adjustment
4. Network / P2P sync protocol
5. Persistence (LevelDB or SQLite)
6. ARM cross-compile toolchain CMake file
