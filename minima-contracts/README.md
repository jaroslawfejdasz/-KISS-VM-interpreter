# minima-contracts

> Ready-to-use KISS VM smart contract patterns for the Minima blockchain.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A curated library of battle-tested, audited smart contract templates for [Minima](https://minima.global). Each template generates a KISS VM script you can attach to a UTxO on the Minima network.

---

## Install

```bash
npm install minima-contracts
```

---

## Quick Start

```ts
import { contracts } from 'minima-contracts';

// Compile a contract template with parameters
const compiled = contracts.compile('htlc', {
  recipientPubKey: '0xabc...',
  senderPubKey:    '0xdef...',
  secretHash:      '0x123...',
  timeoutBlock:    '2000000',
});

console.log(compiled.script);     // Ready-to-use KISS VM script
console.log(compiled.scriptHash); // SHA3-256 hash — use as the contract "address"
```

---

## Available Contracts

| Name | Description |
|------|-------------|
| `basic-signed` | Standard P2PKH — owner's key required |
| `time-lock` | Locked until block height |
| `coin-age-lock` | Locked until coin has been unspent for N blocks |
| `multisig-2of2` | Requires both parties to sign |
| `multisig-2of3` | Threshold: any 2 of 3 |
| `multisig-m-of-n` | Generic M-of-N threshold |
| `htlc` | Hash Time Locked Contract — atomic swap primitive |
| `exchange` | Atomic token swap — native DEX primitive |
| `conditional-payment` | Pays recipient or owner reclaims |
| `state-channel` | Payment channel with force-close and replay protection |
| `vault` | Time-delayed withdrawals with guardian veto |
| `dead-mans-switch` | Beneficiary gains access if owner stops checking in |

---

## API Reference

### `contracts.list()`
Returns all available `ContractTemplate[]`.

### `contracts.get(name)`
Returns a `ContractTemplate` by name, or `undefined`.

### `contracts.compile(name, params)`
Compiles a template into a `CompiledContract`:
```ts
interface CompiledContract {
  name: string;
  script: string;         // KISS VM script
  params: Record<string, string>;
  scriptHash: string;     // SHA3-256 of the script, 0x-prefixed
}
```

Throws if:
- Contract name not found
- Required param is missing
- Param type validation fails (e.g. non-hex for `hex` type)

### `contracts.register(template)`
Register a custom `ContractTemplate`. Throws if name already exists.

---

## Contract Details

### `basic-signed`
```ts
contracts.compile('basic-signed', {
  ownerPubKey: '0x...',  // 64-byte hex public key
});
```
Generates: `RETURN SIGNEDBY(ownerPubKey)`

---

### `time-lock`
```ts
contracts.compile('time-lock', {
  ownerPubKey:  '0x...',
  unlockBlock:  '1000000',  // block height
});
```
Coin is spendable only at or after `unlockBlock`.

---

### `coin-age-lock`
```ts
contracts.compile('coin-age-lock', {
  ownerPubKey: '0x...',
  minAge:      '100',  // blocks since coin was created (@COINAGE)
});
```
Uses `@COINAGE` (blocks since creation). Good for HODLer patterns.

---

### `multisig-2of2` / `multisig-2of3`
```ts
contracts.compile('multisig-2of3', {
  pubKey1: '0x...',
  pubKey2: '0x...',
  pubKey3: '0x...',
});
```

### `multisig-m-of-n`
```ts
contracts.compile('multisig-m-of-n', {
  required: '3',
  keys: '0xkey1 0xkey2 0xkey3 0xkey4',  // space-separated
});
```

---

### `htlc`
```ts
contracts.compile('htlc', {
  recipientPubKey: '0x...',
  senderPubKey:    '0x...',
  secretHash:      '0x...',  // SHA3-256 of the preimage
  timeoutBlock:    '2000000',
});
```
**Spending paths:**
1. **Claim**: recipient reveals `STATE(1)` preimage where `SHA3(preimage) == secretHash`, signed by recipient
2. **Refund**: sender reclaims after `timeoutBlock`, signed by sender

The preimage must be placed in `STATE(1)` of the spending transaction.

---

### `exchange`
```ts
contracts.compile('exchange', {
  sellerAddress: '0x...',
  tokenId:       '0x...',  // token the seller wants (0x00 = native Minima)
  tokenAmount:   '1000',
  sellerPubKey:  '0x...',
});
```
Anyone can trigger the swap by sending the exact `tokenAmount` to `sellerAddress` in `output[0]`.
Seller can cancel by signing.

---

### `vault`
```ts
contracts.compile('vault', {
  hotPubKey:   '0x...',
  coldPubKey:  '0x...',
  delayBlocks: '2880',  // default: 2880 (~2 days)
});
```
- **Guardian (cold key)**: can spend immediately
- **Hot key**: can spend only after `delayBlocks` of coin age

---

### `dead-mans-switch`
```ts
contracts.compile('dead-mans-switch', {
  ownerPubKey:       '0x...',
  beneficiaryPubKey: '0x...',
  checkInInterval:   '10080',  // default: 10080 (~1 week)
});
```
Owner must "check in" by spending and recreating the coin. If `@COINAGE >= checkInInterval`, the beneficiary can claim.

---

### `state-channel`
```ts
contracts.compile('state-channel', {
  pubKeyA:       '0x...',
  pubKeyB:       '0x...',
  timeoutBlocks: '1440',  // default: 1440 (~1 day)
});
```
**Closing paths:**
1. **Cooperative**: both sign
2. **Force-close**: one party after `timeoutBlocks` of coin age, with nonce `STATE(1) >= PREVSTATE(1)` (replay protection)

---

## Custom Contracts

```ts
import { MinimaContractLibrary } from 'minima-contracts';

const lib = new MinimaContractLibrary();

lib.register({
  name: 'my-escrow',
  description: 'Custom escrow contract',
  params: [
    { name: 'buyer',  type: 'hex', description: 'Buyer public key' },
    { name: 'seller', type: 'hex', description: 'Seller public key' },
    { name: 'arbiter', type: 'hex', description: 'Arbiter public key' },
  ],
  script: ({ buyer, seller, arbiter }) =>
    `RETURN MULTISIG(2, ${buyer}, ${seller}, ${arbiter})`,
  example: { buyer: '0xabc...', seller: '0xdef...', arbiter: '0x123...' },
});

const compiled = lib.compile('my-escrow', { buyer: '0x...', seller: '0x...', arbiter: '0x...' });
```

---

## Testing with minima-test

All contracts are tested using [minima-test](../minima-test) — the KISS VM testing framework.

```bash
npm test
```

---

## Minima UTxO Model — Important Notes

- **No contract deployment** — every UTxO carries its own script. To "deploy" a contract, attach the script when creating the coin.
- **scriptHash** — SHA3-256 of the script. Use this as the contract "address" (in `newscript` terminal command).
- **STATE variables** — the spending transaction can attach state (0–255 ports) used in the script.
- **CHECKSIG** — in the test environment, `CHECKSIG` always returns false (requires live node EC verification). Use `SIGNEDBY` for testing.

---

## License

MIT
