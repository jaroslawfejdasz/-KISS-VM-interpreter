<p align="center">
  <h1 align="center">minima-test</h1>
  <p align="center">Testing framework for Minima KISS VM smart contracts</p>
</p>

## What is this?

`minima-test` is a **Jest-like testing framework** for [Minima](https://minima.global) smart contracts written in KISS VM. It lets you write unit tests for your contracts **without a running Minima node** — everything runs locally.

Think of it as Hardhat/Foundry for Minima.

```bash
npx minima-test run tests/
```

```
  ✓ passes when signed by owner          1ms
  ✓ fails when not signed                0ms
  ✓ fails before lock expires            0ms
  ✓ passes exactly at lock block         0ms

  ✓ All 12 tests passed!  (21ms)
```

## Installation

```bash
npm install --save-dev @minima-global/minima-test
```

Or run without installing:
```bash
npx @minima-global/minima-test run tests/
```

## Quick Start

Create a test file `tests/my-contract.test.js`:

```js
const { describe, it, expect, runScript } = require('@minima-global/minima-test');

describe('My Contract', () => {

  it('allows spending when signed by owner', () => {
    expect(runScript(
      'RETURN SIGNEDBY(0xaabbccdd)',
      { signatures: ['0xaabbccdd'] }
    )).toPass();
  });

  it('blocks spending when not signed', () => {
    expect(runScript(
      'RETURN SIGNEDBY(0xaabbccdd)',
      { signatures: [] }
    )).toFail();
  });

});
```

Run it:
```bash
npx minima-test run tests/
```

## API

### `runScript(script, options?)`

Executes a KISS VM script and returns a result object.

```js
const result = runScript('RETURN TRUE AND SIGNEDBY(0xaabbccdd)', {
  signatures: ['0xaabbccdd'],
  transaction: {
    blockNumber: 1500,
    inputs: [{
      coinId: '0xabcd',
      address: '0x1234',
      amount: 100,
      tokenId: '0x00',
      blockCreated: 1000,
    }],
    outputs: [{
      address: '0x5678',
      amount: 100,
      tokenId: '0x00',
    }],
  },
});

result.success      // boolean — did script return TRUE?
result.passed       // alias for success
result.failed       // alias for !success
result.error        // string | null — parse/runtime error
result.instructions // number — how many instructions were executed
result.duration     // number — milliseconds
```

#### Options

| Option | Type | Description |
|--------|------|-------------|
| `transaction` | `Partial<MockTransaction>` | Transaction data (uses defaults if not provided) |
| `inputIndex` | `number` | Which input coin this script runs for (default: 0) |
| `signatures` | `string[]` | Public keys that have signed this transaction |
| `variables` | `Record<string, MiniValue>` | Pre-set script variables |
| `globals` | `Record<string, MiniValue>` | Override global variables |
| `mastScripts` | `Record<string, string>` | MAST scripts (hash → script) |

### `expect(result)`

Chainable assertions:

```js
expect(result).toPass()                    // script returned TRUE
expect(result).toFail()                    // script returned FALSE
expect(result).toThrow('ASSERT failed')    // script threw an error
expect(result).toBeWithinInstructions(100) // used ≤ 100 instructions
expect(result).toBe(true)                  // result.success === true
```

### `describe(name, fn)` / `it(name, fn)` / `test(name, fn)`

Jest-compatible test structure:
```js
describe('Contract Name', () => {
  it('test case', () => {
    // ...
  });
});
```

### `defaultTransaction(overrides?)`

Returns a complete `MockTransaction` with sensible defaults, optionally merged with your overrides:

```js
const tx = defaultTransaction({
  blockNumber: 2000,
  stateVars: { 0: 'counter', 1: '42' },
});
```

### `MiniValue`

Helper to create typed values for `variables`/`globals` options:

```js
MiniValue.boolean(true)
MiniValue.number(42)
MiniValue.hex('0xdeadbeef')
MiniValue.string('hello world')
```

## Contract Examples

### Basic Signed (default Minima contract)
```js
it('basic signed', () => {
  expect(runScript(
    'RETURN SIGNEDBY(0xmypubkey)',
    { signatures: ['0xmypubkey'] }
  )).toPass();
});
```

### Time Lock
```js
const LOCK_BLOCK = 5000;
const script = `RETURN @BLOCK GTE ${LOCK_BLOCK}`;

it('locked before block 5000', () => {
  expect(runScript(script, { transaction: { blockNumber: 4999 } })).toFail();
});

it('unlocked at block 5000', () => {
  expect(runScript(script, { transaction: { blockNumber: 5000 } })).toPass();
});
```

### Coin Age Lock (HODLer contract)
```js
it('coin must be 500 blocks old', () => {
  expect(runScript('RETURN @COINAGE GTE 500', {
    transaction: {
      blockNumber: 2000,
      inputs: [{ coinId: '0x01', address: '0x01', amount: 100, tokenId: '0x00', blockCreated: 1000 }],
    }
  })).toPass(); // coinage = 2000 - 1000 = 1000 ✓
});
```

### Exchange / Atomic Swap
```js
it('verifies correct token output', () => {
  expect(runScript(
    'RETURN VERIFYOUT(0, 100, 0xaliceaddr, 0xtokenid)',
    {
      transaction: {
        inputs: [{ coinId: '0x1', address: '0xaliceaddr', amount: 100, tokenId: '0x00', blockCreated: 1 }],
        outputs: [{ address: '0xaliceaddr', amount: 100, tokenId: '0xtokenid' }],
        blockNumber: 100, blockTime: Date.now(), signatures: [],
      }
    }
  )).toPass();
});
```

### Multi-sig (2 of 3)
```js
it('2-of-3 multisig', () => {
  const key1 = '0xkey1'; const key2 = '0xkey2'; const key3 = '0xkey3';
  const script = `RETURN MULTISIG(2, ${key1}, ${key2}, ${key3})`;
  
  expect(runScript(script, { signatures: [key1, key2] })).toPass();    // 2 sigs ✓
  expect(runScript(script, { signatures: [key1] })).toFail();          // 1 sig ✗
});
```

## CLI

```bash
# Run all tests in ./tests directory
npx minima-test run

# Run tests in specific directory
npx minima-test run path/to/tests

# Quickly evaluate a script
npx minima-test eval "RETURN @BLOCK GTE 1000"

# Help
npx minima-test help
```

## GitHub Actions CI

```yaml
name: Contract Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
      - run: npm install
      - run: npx minima-test run tests/
```

## KISS VM Support

| Feature | Status |
|---------|--------|
| `LET`, `IF/THEN/ELSE/ENDIF` | ✅ |
| `WHILE/DO/ENDWHILE` | ✅ |
| `RETURN`, `ASSERT`, `EXEC`, `MAST` | ✅ |
| `AND`, `OR`, `NOT`, `XOR`, `NAND`, `NOR` | ✅ |
| `EQ`, `NEQ`, `LT`, `GT`, `LTE`, `GTE` | ✅ |
| Math: `ABS`, `CEIL`, `FLOOR`, `MIN`, `MAX`, `POW`, `SQRT`... | ✅ |
| HEX: `CONCAT`, `SUBSET`, `REV`, `LEN`, `BITGET`, `BITSET`... | ✅ |
| Hash: `SHA2`, `SHA3` | ✅ |
| Sigs: `SIGNEDBY`, `CHECKSIG`, `MULTISIG` | ✅ (mock) |
| State: `STATE`, `PREVSTATE`, `SAMESTATE` | ✅ |
| Txn: `GETINADDR`, `GETOUTAMT`, `VERIFYOUT`, `SUMINPUTS`... | ✅ |
| Globals: `@BLOCK`, `@AMOUNT`, `@COINAGE`, `@ADDRESS`... | ✅ |
| MAX_INSTRUCTIONS (1024) | ✅ |
| MAX_STACK_DEPTH (64) | ✅ |

## License

MIT
