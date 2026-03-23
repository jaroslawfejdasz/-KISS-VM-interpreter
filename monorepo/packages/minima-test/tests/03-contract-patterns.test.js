const { describe, it, expect, runScript, defaultTransaction } = require('../dist/api');

describe('Smart Contract Patterns', () => {

  // ---- BASIC SIGNED ----
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

  // ---- TIME LOCK ----
  describe('Time Lock Contract', () => {
    const lockUntilBlock = 2000;
    const timeLockScript = `RETURN @BLOCK GTE ${lockUntilBlock}`;

    it('fails before lock expires', () => {
      expect(runScript(timeLockScript, {
        transaction: { blockNumber: 1500 }
      })).toFail();
    });

    it('passes exactly at lock block', () => {
      expect(runScript(timeLockScript, {
        transaction: { blockNumber: 2000 }
      })).toPass();
    });

    it('passes after lock block', () => {
      expect(runScript(timeLockScript, {
        transaction: { blockNumber: 2500 }
      })).toPass();
    });
  });

  // ---- COINAGE LOCK ----
  describe('Coin Age Lock (HODLer)', () => {
    const minAge = 500;
    const script = `RETURN @COINAGE GTE ${minAge}`;

    it('fails when coin is too young', () => {
      expect(runScript(script, {
        transaction: {
          blockNumber: 1100,
          inputs: [{ coinId: '0x01', address: '0x01', amount: 100, tokenId: '0x00', blockCreated: 1000 }]
        }
      })).toFail();
    });

    it('passes when coin is old enough', () => {
      expect(runScript(script, {
        transaction: {
          blockNumber: 2000,
          inputs: [{ coinId: '0x01', address: '0x01', amount: 100, tokenId: '0x00', blockCreated: 1000 }]
        }
      })).toPass();
    });
  });

  // ---- EXCHANGE CONTRACT ----
  describe('Exchange Contract (atomic swap)', () => {
    const aliceAddr = '0x1234567890abcdef';
    const tokenId   = '0xdeadbeef00000000';
    const amount    = 100;

    const exchangeScript = `
      LET validOutput = VERIFYOUT(0, ${amount}, ${aliceAddr}, ${tokenId})
      RETURN validOutput
    `;

    it('passes when correct token output exists', () => {
      expect(runScript(exchangeScript, {
        transaction: {
          inputs: [{ coinId: '0xabc', address: aliceAddr, amount: 100, tokenId: '0x00', blockCreated: 1000 }],
          outputs: [{ address: aliceAddr, amount: 100, tokenId: tokenId }],
          blockNumber: 1100, blockTime: Date.now(), signatures: [],
        }
      })).toPass();
    });

    it('fails when output has wrong amount', () => {
      expect(runScript(exchangeScript, {
        transaction: {
          inputs: [{ coinId: '0xabc', address: aliceAddr, amount: 100, tokenId: '0x00', blockCreated: 1000 }],
          outputs: [{ address: aliceAddr, amount: 50, tokenId: tokenId }],
          blockNumber: 1100, blockTime: Date.now(), signatures: [],
        }
      })).toFail();
    });

    it('fails when output goes to wrong address', () => {
      expect(runScript(exchangeScript, {
        transaction: {
          inputs: [{ coinId: '0xabc', address: aliceAddr, amount: 100, tokenId: '0x00', blockCreated: 1000 }],
          outputs: [{ address: '0xeviladdress000000', amount: 100, tokenId: tokenId }],
          blockNumber: 1100, blockTime: Date.now(), signatures: [],
        }
      })).toFail();
    });
  });

  // ---- STATE VARIABLE CONTRACT ----
  describe('State Variables', () => {
    it('reads state variable', () => {
      expect(runScript('LET s = STATE(0) RETURN s EQ [hello]', {
        transaction: { ...defaultTransaction(), stateVars: { 0: 'hello' } }
      })).toPass();
    });

    it('SAMESTATE - passes when state unchanged', () => {
      expect(runScript('RETURN SAMESTATE(0, 2)', {
        transaction: {
          ...defaultTransaction(),
          stateVars:     { 0: 'a', 1: 'b', 2: 'c' },
          prevStateVars: { 0: 'a', 1: 'b', 2: 'c' }
        }
      })).toPass();
    });

    it('SAMESTATE - fails when state changed', () => {
      expect(runScript('RETURN SAMESTATE(0, 2)', {
        transaction: {
          ...defaultTransaction(),
          stateVars:     { 0: 'a', 1: 'CHANGED', 2: 'c' },
          prevStateVars: { 0: 'a', 1: 'b', 2: 'c' }
        }
      })).toFail();
    });
  });

  // ---- GLOBALS ----
  describe('Global Variables', () => {
    it('@BLOCK is accessible', () => {
      expect(runScript('RETURN @BLOCK EQ 1100', {
        transaction: { ...defaultTransaction(), blockNumber: 1100 }
      })).toPass();
    });

    it('@AMOUNT reflects coin amount', () => {
      expect(runScript('RETURN @AMOUNT EQ 250', {
        transaction: {
          inputs: [{ coinId: '0x01', address: '0x01', amount: 250, tokenId: '0x00', blockCreated: 100 }],
          outputs: [], blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });

    it('@TOTIN and @TOTOUT', () => {
      expect(runScript('RETURN @TOTIN EQ 1 AND @TOTOUT EQ 2', {
        transaction: {
          inputs: [{ coinId: '0x01', address: '0x01', amount: 100, tokenId: '0x00', blockCreated: 100 }],
          outputs: [
            { address: '0x02', amount: 60, tokenId: '0x00' },
            { address: '0x03', amount: 40, tokenId: '0x00' },
          ],
          blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });
  });
});
