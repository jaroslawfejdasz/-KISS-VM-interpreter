const { describe, it, expect, runScript, MiniValue, defaultTransaction } = require('../dist/api');

// === SMART CONTRACT PATTERNS ===

describe('Basic Signed (SIGNEDBY)', () => {
  const ownerPubKey = '0xaabbccdd11223344';

  it('passes when signed by owner', () => {
    expect(runScript(
      `RETURN SIGNEDBY(${ownerPubKey})`,
      { signatures: [ownerPubKey] }
    )).toPass();
  });

  it('fails when not signed', () => {
    expect(runScript(
      `RETURN SIGNEDBY(${ownerPubKey})`,
      { signatures: [] }
    )).toFail();
  });

  it('fails when signed by wrong key', () => {
    expect(runScript(
      `RETURN SIGNEDBY(${ownerPubKey})`,
      { signatures: ['0xdeadbeefdeadbeef'] }
    )).toFail();
  });
});

describe('Time Lock Contract', () => {
  const LOCK_BLOCK = 1000;
  const script = `RETURN @BLOCK GTE ${LOCK_BLOCK}`;

  it('fails before lock expires', () => {
    expect(runScript(script, { block: 999 })).toFail();
  });

  it('passes exactly at lock block', () => {
    expect(runScript(script, { block: 1000 })).toPass();
  });

  it('passes after lock block', () => {
    expect(runScript(script, { block: 1500 })).toPass();
  });
});

describe('Coin Age Lock (HODLer)', () => {
  const MIN_AGE = 10000;
  const script = `RETURN @COINAGE GTE ${MIN_AGE}`;

  it('fails when coin is too young', () => {
    expect(runScript(script, { coinAge: 9999 })).toFail();
  });

  it('passes when coin is old enough', () => {
    expect(runScript(script, { coinAge: 10000 })).toPass();
  });
});

describe('Exchange Contract (atomic swap)', () => {
  const wantToken  = '0xtoken123';
  const wantAmount = 100;
  const myAddress  = '0xmyaddress';
  const script     = `RETURN VERIFYOUT(0, ${wantAmount}, ${myAddress}, ${wantToken})`;

  it('passes when correct token output exists', () => {
    expect(runScript(script, {
      outputs: [{ amount: 100, address: '0xmyaddress', tokenId: '0xtoken123', keepState: false }]
    })).toPass();
  });

  it('fails when output has wrong amount', () => {
    expect(runScript(script, {
      outputs: [{ amount: 99, address: '0xmyaddress', tokenId: '0xtoken123', keepState: false }]
    })).toFail();
  });

  it('fails when output goes to wrong address', () => {
    expect(runScript(script, {
      outputs: [{ amount: 100, address: '0xwrongaddress', tokenId: '0xtoken123', keepState: false }]
    })).toFail();
  });
});

describe('State Variables', () => {
  it('reads state variable', () => {
    expect(runScript('LET s = STATE(1) RETURN s EQ 42', {
      state: { 1: '42' }
    })).toPass();
  });

  it('SAMESTATE - passes when state unchanged', () => {
    expect(runScript('RETURN SAMESTATE(1, 3)', {
      state:     { 1: 'a', 2: 'b', 3: 'c' },
      prevState: { 1: 'a', 2: 'b', 3: 'c' }
    })).toPass();
  });

  it('SAMESTATE - fails when state changed', () => {
    expect(runScript('RETURN SAMESTATE(1, 3)', {
      state:     { 1: 'a', 2: 'b', 3: 'X' },
      prevState: { 1: 'a', 2: 'b', 3: 'c' }
    })).toFail();
  });
});

describe('Global Variables', () => {
  it('@BLOCK is accessible', () => {
    expect(runScript('RETURN @BLOCK GT 0', { block: 1000 })).toPass();
  });

  it('@AMOUNT reflects coin amount', () => {
    expect(runScript('RETURN @AMOUNT GTE 100', { amount: 150 })).toPass();
  });

  it('@TOTIN and @TOTOUT', () => {
    expect(runScript('RETURN @TOTIN GTE @TOTOUT', {
      inputs:  [{ amount: 100, address: '0xabc', tokenId: '0x00', id: '0xid1' }],
      outputs: [{ amount: 90,  address: '0xdef', tokenId: '0x00', keepState: false }]
    })).toPass();
  });
});
