/**
 * minima-contracts test suite
 * Tests the ContractLibrary API + runtime verification via minima-test.
 */
const { contracts, MinimaContractLibrary } = require('../dist/index');
const { createHash } = require('crypto');

let passed = 0, failed = 0;

function test(name, fn) {
  try { fn(); console.log('\x1b[32m  ✓\x1b[0m ' + name); passed++; }
  catch(e) { console.log('\x1b[31m  ✗\x1b[0m ' + name + '\n    \x1b[31m→ ' + e.message + '\x1b[0m'); failed++; }
}
function assert(cond, msg) { if (!cond) throw new Error(msg || 'Assertion failed'); }
function assertThrows(fn, msgContains) {
  try { fn(); throw new Error('Expected to throw but did not'); }
  catch(e) {
    if (e.message === 'Expected to throw but did not') throw e;
    if (msgContains && !e.message.includes(msgContains))
      throw new Error(`Expected error containing "${msgContains}", got: ${e.message}`);
  }
}
function assertContains(str, sub) {
  if (!str.includes(sub)) throw new Error(`Expected to contain "${sub}", got:\n${str}`);
}

// Helpers for building mock transactions
function withBlock(blockNumber, extra = {}) {
  return { transaction: { blockNumber, ...extra } };
}
function withCoinAge(coinAge, extra = {}) {
  // coinAge = blockNumber - blockCreated
  return { transaction: { blockNumber: 1000 + coinAge, inputs: [{ coinId: '0xabc', address: '0xabc', amount: 100, tokenId: '0x00', blockCreated: 1000 }], ...extra } };
}
function withState(stateVars, prevStateVars = {}, extra = {}) {
  return { transaction: { stateVars, prevStateVars, ...extra } };
}
function withStateAndAge(stateVars, prevStateVars, coinAge, extra = {}) {
  return { transaction: { stateVars, prevStateVars, blockNumber: 1000 + coinAge, inputs: [{ coinId: '0xabc', address: '0xabc', amount: 100, tokenId: '0x00', blockCreated: 1000 }], ...extra } };
}

// Load minima-test
let runScript = null;
try {
  runScript = require('../../minima-test/dist/api').runScript;
  console.log('\x1b[90m  (minima-test available — runtime tests enabled)\x1b[0m');
} catch {
  console.log('\x1b[90m  (minima-test not available — runtime tests skipped)\x1b[0m');
}

const KEY_A = '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd';
const KEY_B = '0x1122334411223344112233441122334411223344112233441122334411223344';
const KEY_C = '0x5566778855667788556677885566778855667788556677885566778855667788';
const ADDR  = '0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef';
const TOKEN = '0x' + 'ab'.repeat(32);

// SHA3 helper
function sha3hex(data) {
  return '0x' + createHash('sha3-256').update(Buffer.from(data, 'utf8')).digest('hex');
}

// =====================================================================
console.log('\n\x1b[1m  ContractLibrary — core API\x1b[0m');

test('contracts.list() returns all 12 contracts', () => {
  assert(contracts.list().length === 12, `Expected 12, got ${contracts.list().length}`);
});
test('contracts.get() returns a template', () => {
  const t = contracts.get('basic-signed');
  assert(t && t.name === 'basic-signed');
});
test('contracts.get() returns undefined for unknown', () => {
  assert(contracts.get('unknown') === undefined);
});
test('contracts.compile() throws for unknown contract', () => {
  assertThrows(() => contracts.compile('nope', {}), 'not found');
});
test('contracts.compile() fills defaults (vault delayBlocks)', () => {
  const c = contracts.compile('vault', { hotPubKey: KEY_A, coldPubKey: KEY_B });
  assertContains(c.script, '2880');
});
test('contracts.compile() throws on missing required param', () => {
  assertThrows(() => contracts.compile('basic-signed', {}), "Missing required param 'ownerPubKey'");
});
test('contracts.compile() throws on invalid hex param', () => {
  assertThrows(() => contracts.compile('basic-signed', { ownerPubKey: 'not-hex' }), 'must be a hex value');
});
test('scriptHash is 0x + 64 hex chars (SHA3-256)', () => {
  const c = contracts.compile('basic-signed', { ownerPubKey: KEY_A });
  assert(c.scriptHash.startsWith('0x') && c.scriptHash.length === 66,
    `Bad scriptHash: ${c.scriptHash}`);
});
test('scriptHash is deterministic', () => {
  const a = contracts.compile('basic-signed', { ownerPubKey: KEY_A });
  const b = contracts.compile('basic-signed', { ownerPubKey: KEY_A });
  assert(a.scriptHash === b.scriptHash);
});
test('different params → different scriptHash', () => {
  const a = contracts.compile('basic-signed', { ownerPubKey: KEY_A });
  const b = contracts.compile('basic-signed', { ownerPubKey: KEY_B });
  assert(a.scriptHash !== b.scriptHash);
});
test('custom contract can be registered', () => {
  const lib = new MinimaContractLibrary();
  lib.register({
    name: 'my-custom', description: 'test',
    params: [{ name: 'key', type: 'hex', description: 'key' }],
    script: ({ key }) => `RETURN SIGNEDBY(${key})`,
    example: { key: KEY_A },
  });
  assertContains(lib.compile('my-custom', { key: KEY_A }).script, KEY_A);
});
test('registering duplicate name throws', () => {
  const lib = new MinimaContractLibrary();
  const t = { name: 'dup', description: '', params: [], script: () => 'RETURN TRUE', example: {} };
  lib.register(t);
  assertThrows(() => lib.register(t), 'already registered');
});

// =====================================================================
console.log('\n\x1b[1m  basic-signed\x1b[0m');

test('script contains SIGNEDBY(key)', () => {
  assertContains(contracts.compile('basic-signed', { ownerPubKey: KEY_A }).script, `SIGNEDBY(${KEY_A})`);
});
if (runScript) {
  test('[runtime] passes when signed by owner', () => {
    const c = contracts.compile('basic-signed', { ownerPubKey: KEY_A });
    assert(runScript(c.script, { signatures: [KEY_A] }).success);
  });
  test('[runtime] fails without signature', () => {
    const c = contracts.compile('basic-signed', { ownerPubKey: KEY_A });
    assert(!runScript(c.script, { signatures: [] }).success);
  });
  test('[runtime] fails with wrong key', () => {
    const c = contracts.compile('basic-signed', { ownerPubKey: KEY_A });
    assert(!runScript(c.script, { signatures: [KEY_B] }).success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  time-lock\x1b[0m');

test('script contains @BLOCK GTE and SIGNEDBY', () => {
  const c = contracts.compile('time-lock', { ownerPubKey: KEY_A, unlockBlock: '1000' });
  assertContains(c.script, '@BLOCK'); assertContains(c.script, 'SIGNEDBY');
});
if (runScript) {
  test('[runtime] fails before unlock block', () => {
    const c = contracts.compile('time-lock', { ownerPubKey: KEY_A, unlockBlock: '1000' });
    assert(!runScript(c.script, { signatures: [KEY_A], ...withBlock(999) }).success);
  });
  test('[runtime] passes at unlock block', () => {
    const c = contracts.compile('time-lock', { ownerPubKey: KEY_A, unlockBlock: '1000' });
    assert(runScript(c.script, { signatures: [KEY_A], ...withBlock(1000) }).success);
  });
  test('[runtime] passes after unlock block', () => {
    const c = contracts.compile('time-lock', { ownerPubKey: KEY_A, unlockBlock: '1000' });
    assert(runScript(c.script, { signatures: [KEY_A], ...withBlock(9999) }).success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  coin-age-lock\x1b[0m');

test('script contains @COINAGE', () => {
  assertContains(contracts.compile('coin-age-lock', { ownerPubKey: KEY_A, minAge: '100' }).script, '@COINAGE');
});
if (runScript) {
  test('[runtime] fails if coin too young', () => {
    const c = contracts.compile('coin-age-lock', { ownerPubKey: KEY_A, minAge: '100' });
    assert(!runScript(c.script, { signatures: [KEY_A], ...withCoinAge(50) }).success);
  });
  test('[runtime] passes when coin old enough', () => {
    const c = contracts.compile('coin-age-lock', { ownerPubKey: KEY_A, minAge: '100' });
    assert(runScript(c.script, { signatures: [KEY_A], ...withCoinAge(100) }).success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  multisig variants\x1b[0m');

test('2of2 script contains MULTISIG(2, key1, key2)', () => {
  assertContains(
    contracts.compile('multisig-2of2', { pubKey1: KEY_A, pubKey2: KEY_B }).script,
    `MULTISIG(2, ${KEY_A}, ${KEY_B})`
  );
});
test('2of3 script contains MULTISIG(2, k1, k2, k3)', () => {
  assertContains(
    contracts.compile('multisig-2of3', { pubKey1: KEY_A, pubKey2: KEY_B, pubKey3: KEY_C }).script,
    `MULTISIG(2, ${KEY_A}, ${KEY_B}, ${KEY_C})`
  );
});
test('m-of-n generates correct MULTISIG call', () => {
  assertContains(
    contracts.compile('multisig-m-of-n', { required: '2', keys: `${KEY_A} ${KEY_B} ${KEY_C}` }).script,
    'MULTISIG(2,'
  );
});
if (runScript) {
  test('[runtime] 2of2 passes with both sigs', () => {
    const c = contracts.compile('multisig-2of2', { pubKey1: KEY_A, pubKey2: KEY_B });
    assert(runScript(c.script, { signatures: [KEY_A, KEY_B] }).success);
  });
  test('[runtime] 2of2 fails with one sig', () => {
    const c = contracts.compile('multisig-2of2', { pubKey1: KEY_A, pubKey2: KEY_B });
    assert(!runScript(c.script, { signatures: [KEY_A] }).success);
  });
  test('[runtime] 2of3 passes with any 2', () => {
    const c = contracts.compile('multisig-2of3', { pubKey1: KEY_A, pubKey2: KEY_B, pubKey3: KEY_C });
    assert(runScript(c.script, { signatures: [KEY_B, KEY_C] }).success);
  });
  test('[runtime] 2of3 fails with 1 of 3', () => {
    const c = contracts.compile('multisig-2of3', { pubKey1: KEY_A, pubKey2: KEY_B, pubKey3: KEY_C });
    assert(!runScript(c.script, { signatures: [KEY_A] }).success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  htlc\x1b[0m');

// Use string preimage (STATE returns string, SHA3 on string uses UTF8)
const PREIMAGE = 'supersecret_preimage';
const HTLC_HASH = sha3hex(PREIMAGE);

test('script contains SHA3, STATE(1), @BLOCK, timeout', () => {
  const c = contracts.compile('htlc', { recipientPubKey: KEY_A, senderPubKey: KEY_B,
    secretHash: HTLC_HASH, timeoutBlock: '5000' });
  assertContains(c.script, 'SHA3');
  assertContains(c.script, 'STATE(1)');
  assertContains(c.script, '@BLOCK');
});
if (runScript) {
  test('[runtime] claim: correct preimage + recipient sig', () => {
    const c = contracts.compile('htlc', { recipientPubKey: KEY_A, senderPubKey: KEY_B,
      secretHash: HTLC_HASH, timeoutBlock: '9999999' });
    const r = runScript(c.script, { signatures: [KEY_A],
      ...withBlock(100, { stateVars: { 1: PREIMAGE } }) });
    assert(r.success, r.error);
  });
  test('[runtime] claim fails with wrong preimage', () => {
    const c = contracts.compile('htlc', { recipientPubKey: KEY_A, senderPubKey: KEY_B,
      secretHash: HTLC_HASH, timeoutBlock: '9999999' });
    const r = runScript(c.script, { signatures: [KEY_A],
      ...withBlock(100, { stateVars: { 1: 'wrong' } }) });
    assert(!r.success);
  });
  test('[runtime] refund after timeout + sender sig', () => {
    const c = contracts.compile('htlc', { recipientPubKey: KEY_A, senderPubKey: KEY_B,
      secretHash: HTLC_HASH, timeoutBlock: '100' });
    const r = runScript(c.script, { signatures: [KEY_B],
      ...withBlock(200, { stateVars: { 1: 'wrong' } }) });
    assert(r.success, r.error);
  });
  test('[runtime] refund fails before timeout', () => {
    const c = contracts.compile('htlc', { recipientPubKey: KEY_A, senderPubKey: KEY_B,
      secretHash: HTLC_HASH, timeoutBlock: '9999' });
    const r = runScript(c.script, { signatures: [KEY_B],
      ...withBlock(100, { stateVars: { 1: 'wrong' } }) });
    assert(!r.success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  exchange\x1b[0m');

test('script contains VERIFYOUT, amount, seller address', () => {
  const c = contracts.compile('exchange', { sellerAddress: ADDR, tokenId: TOKEN,
    tokenAmount: '1000', sellerPubKey: KEY_A });
  assertContains(c.script, 'VERIFYOUT');
  assertContains(c.script, '1000');
  assertContains(c.script, ADDR);
});
if (runScript) {
  test('[runtime] passes when correct payment output exists', () => {
    const c = contracts.compile('exchange', { sellerAddress: ADDR, tokenId: TOKEN,
      tokenAmount: '500', sellerPubKey: KEY_A });
    const r = runScript(c.script, { signatures: [],
      transaction: { outputs: [{ address: ADDR, amount: 500, tokenId: TOKEN }] } });
    assert(r.success, r.error);
  });
  test('[runtime] fails with wrong amount', () => {
    const c = contracts.compile('exchange', { sellerAddress: ADDR, tokenId: TOKEN,
      tokenAmount: '500', sellerPubKey: KEY_A });
    assert(!runScript(c.script, { signatures: [],
      transaction: { outputs: [{ address: ADDR, amount: 400, tokenId: TOKEN }] } }).success);
  });
  test('[runtime] fails with wrong address', () => {
    const c = contracts.compile('exchange', { sellerAddress: ADDR, tokenId: TOKEN,
      tokenAmount: '500', sellerPubKey: KEY_A });
    assert(!runScript(c.script, { signatures: [],
      transaction: { outputs: [{ address: '0xbadaddr', amount: 500, tokenId: TOKEN }] } }).success);
  });
  test('[runtime] seller can cancel by signing', () => {
    const c = contracts.compile('exchange', { sellerAddress: ADDR, tokenId: TOKEN,
      tokenAmount: '500', sellerPubKey: KEY_A });
    assert(runScript(c.script, { signatures: [KEY_A], transaction: { outputs: [] } }).success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  vault\x1b[0m');

test('script contains hot key, cold key, @COINAGE', () => {
  const c = contracts.compile('vault', { hotPubKey: KEY_A, coldPubKey: KEY_B });
  assertContains(c.script, KEY_A); assertContains(c.script, KEY_B); assertContains(c.script, '@COINAGE');
});
if (runScript) {
  test('[runtime] guardian (cold key) can spend immediately', () => {
    const c = contracts.compile('vault', { hotPubKey: KEY_A, coldPubKey: KEY_B, delayBlocks: '100' });
    assert(runScript(c.script, { signatures: [KEY_B], ...withCoinAge(0) }).success);
  });
  test('[runtime] hot key fails before delay', () => {
    const c = contracts.compile('vault', { hotPubKey: KEY_A, coldPubKey: KEY_B, delayBlocks: '100' });
    assert(!runScript(c.script, { signatures: [KEY_A], ...withCoinAge(50) }).success);
  });
  test('[runtime] hot key passes after delay', () => {
    const c = contracts.compile('vault', { hotPubKey: KEY_A, coldPubKey: KEY_B, delayBlocks: '100' });
    assert(runScript(c.script, { signatures: [KEY_A], ...withCoinAge(100) }).success);
  });
  test('[runtime] hot key fails without signature', () => {
    const c = contracts.compile('vault', { hotPubKey: KEY_A, coldPubKey: KEY_B, delayBlocks: '100' });
    assert(!runScript(c.script, { signatures: [], ...withCoinAge(200) }).success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  dead-mans-switch\x1b[0m');

test('script contains @COINAGE, checkInInterval, both keys', () => {
  const c = contracts.compile('dead-mans-switch', { ownerPubKey: KEY_A,
    beneficiaryPubKey: KEY_B, checkInInterval: '500' });
  assertContains(c.script, '@COINAGE'); assertContains(c.script, '500');
  assertContains(c.script, KEY_A); assertContains(c.script, KEY_B);
});
if (runScript) {
  test('[runtime] owner can always spend (age 0)', () => {
    const c = contracts.compile('dead-mans-switch', { ownerPubKey: KEY_A,
      beneficiaryPubKey: KEY_B, checkInInterval: '1000' });
    assert(runScript(c.script, { signatures: [KEY_A], ...withCoinAge(0) }).success);
  });
  test('[runtime] beneficiary fails before interval', () => {
    const c = contracts.compile('dead-mans-switch', { ownerPubKey: KEY_A,
      beneficiaryPubKey: KEY_B, checkInInterval: '1000' });
    assert(!runScript(c.script, { signatures: [KEY_B], ...withCoinAge(500) }).success);
  });
  test('[runtime] beneficiary passes after interval', () => {
    const c = contracts.compile('dead-mans-switch', { ownerPubKey: KEY_A,
      beneficiaryPubKey: KEY_B, checkInInterval: '1000' });
    assert(runScript(c.script, { signatures: [KEY_B], ...withCoinAge(1000) }).success);
  });
  test('[runtime] nobody else can spend', () => {
    const c = contracts.compile('dead-mans-switch', { ownerPubKey: KEY_A,
      beneficiaryPubKey: KEY_B, checkInInterval: '1000' });
    assert(!runScript(c.script, { signatures: [KEY_C], ...withCoinAge(9999) }).success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  state-channel\x1b[0m');

test('script contains MULTISIG, STATE(1), PREVSTATE(1)', () => {
  const c = contracts.compile('state-channel', { pubKeyA: KEY_A, pubKeyB: KEY_B });
  assertContains(c.script, 'MULTISIG'); assertContains(c.script, 'STATE(1)'); assertContains(c.script, 'PREVSTATE(1)');
});
if (runScript) {
  test('[runtime] cooperative close (both sign)', () => {
    const c = contracts.compile('state-channel', { pubKeyA: KEY_A, pubKeyB: KEY_B, timeoutBlocks: '100' });
    assert(runScript(c.script, { signatures: [KEY_A, KEY_B] }).success);
  });
  test('[runtime] force-close: signed + timeout + nonce advances', () => {
    const c = contracts.compile('state-channel', { pubKeyA: KEY_A, pubKeyB: KEY_B, timeoutBlocks: '100' });
    assert(runScript(c.script, {
      signatures: [KEY_A],
      ...withStateAndAge({ 1: '5' }, { 1: '3' }, 100)
    }).success);
  });
  test('[runtime] force-close fails before timeout', () => {
    const c = contracts.compile('state-channel', { pubKeyA: KEY_A, pubKeyB: KEY_B, timeoutBlocks: '100' });
    assert(!runScript(c.script, {
      signatures: [KEY_A],
      ...withStateAndAge({ 1: '5' }, { 1: '3' }, 50)
    }).success);
  });
  test('[runtime] replay attack blocked (nonce goes backwards)', () => {
    const c = contracts.compile('state-channel', { pubKeyA: KEY_A, pubKeyB: KEY_B, timeoutBlocks: '100' });
    assert(!runScript(c.script, {
      signatures: [KEY_A],
      ...withStateAndAge({ 1: '2' }, { 1: '5' }, 200)  // nonce 2 < prev 5
    }).success);
  });
  test('[runtime] third party cannot close channel', () => {
    const c = contracts.compile('state-channel', { pubKeyA: KEY_A, pubKeyB: KEY_B, timeoutBlocks: '100' });
    assert(!runScript(c.script, {
      signatures: [KEY_C],
      ...withStateAndAge({ 1: '5' }, { 1: '3' }, 200)
    }).success);
  });
}

// =====================================================================
console.log('\n\x1b[1m  conditional-payment\x1b[0m');

if (runScript) {
  test('[runtime] passes when payment to recipient', () => {
    const c = contracts.compile('conditional-payment', { recipientAddress: ADDR, amount: '100', ownerPubKey: KEY_A });
    assert(runScript(c.script, { signatures: [],
      transaction: { outputs: [{ address: ADDR, amount: 100, tokenId: '0x00' }] } }).success);
  });
  test('[runtime] fails with wrong amount', () => {
    const c = contracts.compile('conditional-payment', { recipientAddress: ADDR, amount: '100', ownerPubKey: KEY_A });
    assert(!runScript(c.script, { signatures: [],
      transaction: { outputs: [{ address: ADDR, amount: 50, tokenId: '0x00' }] } }).success);
  });
  test('[runtime] owner can reclaim', () => {
    const c = contracts.compile('conditional-payment', { recipientAddress: ADDR, amount: '100', ownerPubKey: KEY_A });
    assert(runScript(c.script, { signatures: [KEY_A], transaction: { outputs: [] } }).success);
  });
}

// =====================================================================
console.log('\n\x1b[90m──────────────────────────────────────────────────\x1b[0m');
if (failed === 0) {
  console.log(`\x1b[32m\x1b[1m\n  ✓ All ${passed} tests passed!\x1b[0m`);
} else {
  console.log(`\x1b[31m\x1b[1m\n  ✗ ${failed} failed, ${passed} passed\x1b[0m`);
  process.exit(1);
}
