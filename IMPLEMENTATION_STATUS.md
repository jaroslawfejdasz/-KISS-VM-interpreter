# Implementation Status — minima-core-cpp

Last updated: 2026-03-22

## Test suites: 12/12 pass ✅

| Suite           | Tests | Status |
|----------------|-------|--------|
| test_mini_number | types | ✅ |
| test_mini_data   | types | ✅ |
| test_kissvm      | KISS VM interpreter | ✅ |
| test_txpow       | TxPoW / TxHeader / TxBody | ✅ |
| test_validation  | TxPoWValidator | ✅ |
| test_mmr         | Merkle Mountain Range | ✅ |
| test_chain       | BlockStore / ChainProcessor | ✅ |
| test_token       | Token / Greeting / TxBlock | ✅ |
| test_mining      | TxPoWMiner | ✅ |
| test_difficulty  | DifficultyAdjust | ✅ |
| test_network     | NIOMessage (P2P wire) | ✅ |
| test_ibd         | IBD (Initial Blockchain Download) | ✅ |

## Implemented modules

### Core types
- [x] MiniNumber (BigDecimal-compatible, full arithmetic)
- [x] MiniData (byte array, HEX encoding)
- [x] MiniString (UTF-8)

### Objects
- [x] TxHeader (wire-exact: superParents[32] RLE, Magic, blockDifficulty)
- [x] TxBody (wire-exact: prng, txnDifficulty, burnTxn)
- [x] TxPoW (ID = SHA3(SHA3(header)), getPoWHash = SHA2(header))
- [x] Transaction (inputs/outputs/state variables)
- [x] Witness (signatures, scripts, proofs)
- [x] Coin (UTxO model)
- [x] CoinProof (MMR proof for coin)
- [x] Address (script → SHA3 hash)
- [x] StateVariable (key-value, 255 slots)
- [x] Token (custom tokens, tokenID = SHA3(coinID+scale+name+script))
- [x] Magic (network parameters, calculateNewCurrent)
- [x] Greeting (initial handshake, version, topBlock, chain IDs)
- [x] TxBlock (block + new/spent coins + MMR peaks)
- [x] IBD (Initial Blockchain Download, serialise/deserialise)

### Cryptography
- [x] SHA2-256 (pure C++)
- [x] SHA3-256 (pure C++)
- [x] RSA-1024 SHA256withRSA (signature verify)
- [ ] Schnorr (stub — Java uses RSA not Schnorr)

### KISS VM
- [x] Tokenizer (all token types)
- [x] Parser (full grammar)
- [x] Interpreter (42+ functions)
- [x] Environment (stack, state variables, transaction context)
- [x] All built-ins: SIGNEDBY, CHECKSIG, MULTISIG, VERIFYOUT, VERIFYIN,
      SHA2, SHA3, CONCAT, SUBSET, GETOUTADDR, GETOUTAMT,
      SUMINPUTS, STATE, PREVSTATE, SAMESTATE, @BLOCK, @COINAGE, etc.

### MMR (Merkle Mountain Range)
- [x] MMRData, MMREntry, MMRProof
- [x] MMRSet (add, get, getProof, verifyProof)

### Chain layer
- [x] BlockStore (in-memory block storage)
- [x] ChainState (UTxO set management)
- [x] UTxOSet (add/spend/query coins)
- [x] ChainProcessor (process blocks, apply transactions)
- [x] DifficultyAdjust (Java-exact retarget algorithm)

### Validation
- [x] TxPoWValidator (PoW check, script execution, signature verify)

### Mining
- [x] TxPoWMiner (nonce increment, difficulty check, cancel flag)

### Network (P2P wire protocol)
- [x] NIOMessage (24 message types, encode/decode)
- [x] IBD (Initial Blockchain Download)

### Mempool
- [x] Mempool (add/remove/query pending TxPoW)

### Serialization
- [x] DataStream (read/write primitives)

## TODO (next priority)

1. **Persistence** — LevelDB or SQLite for BlockStore/UTxOSet
2. **Network I/O** — actual TCP socket layer (NIOServer/NIOClient)
3. **P2P sync loop** — IBD request/response, flood-fill TxPoW propagation
4. **Cascade** — chain cascade/pruning (CascadeNode, Cascade class)
5. **ARM cross-compile** — CMake toolchain file for aarch64
6. **npm publish** — minima-test framework

## Architecture

```
L1: Minima (UTxO flood-fill P2P TxPoW)
    ↓
    Objects: TxPoW → TxBlock → IBD
    Chain:   BlockStore + UTxOSet + ChainProcessor
    Crypto:  SHA2/SHA3/RSA
    KISSVM:  Interpreter + all built-ins
    Network: NIOMessage (24 types) + IBD
    Mining:  TxPoWMiner + DifficultyAdjust
```
