const { describe, it, expect, runScript, defaultTransaction } = require('../dist/api');

describe('Advanced Functions (07)', () => {

  // === EXISTS ===
  describe('EXISTS function', () => {
    it('returns TRUE when variable is defined', () => {
      expect(runScript('LET x = 42 RETURN EXISTS(x)')).toPass();
    });

    it('returns FALSE when variable is not defined', () => {
      expect(runScript('RETURN NOT(EXISTS(undefinedVar))')).toPass();
    });

    it('works after LET string assignment', () => {
      expect(runScript('LET s = [hello] RETURN EXISTS(s)')).toPass();
    });

    it('second variable not shadowed', () => {
      expect(runScript('LET a = 1 LET b = 2 RETURN EXISTS(a) AND EXISTS(b)')).toPass();
    });
  });

  // === STRING conversions ===
  describe('STRING / HEX / NUMBER casts', () => {
    it('STRING converts hex bytes to UTF-8 string', () => {
      // 0x68656c6c6f = "hello"
      expect(runScript('LET s = STRING(0x68656c6c6f) RETURN s EQ [hello]')).toPass();
    });

    it('STRING of number gives decimal string', () => {
      expect(runScript('LET s = STRING(42) RETURN s EQ [42]')).toPass();
    });

    it('NUMBER converts string to number', () => {
      expect(runScript('LET n = NUMBER([100]) RETURN n EQ 100')).toPass();
    });

    it('HEX converts number to hex', () => {
      expect(runScript('LET h = HEX(255) RETURN LEN(h) GT 0')).toPass();
    });

    it('ASCII converts hex to ascii string', () => {
      // 0x41 = 'A'
      expect(runScript('LET c = ASCII(0x41) RETURN c EQ [A]')).toPass();
    });
  });

  // === SETLEN / OVERWRITE / REV ===
  describe('HEX manipulation', () => {
    it('SETLEN truncates hex to N bytes', () => {
      expect(runScript('LET h = SETLEN(0xdeadbeef, 2) RETURN LEN(h) EQ 2')).toPass();
    });

    it('SETLEN pads hex to N bytes', () => {
      expect(runScript('LET h = SETLEN(0xaa, 4) RETURN LEN(h) EQ 4')).toPass();
    });

    it('REV reverses hex', () => {
      expect(runScript('LET h = REV(0x0102) RETURN h EQ 0x0201')).toPass();
    });

    it('OVERWRITE replaces bytes', () => {
      // OVERWRITE(dest, dstart, src, sstart, len)
      expect(runScript('LET r = OVERWRITE(0x00000000, 0, 0xFFFF, 0, 2) RETURN r EQ 0xffff0000')).toPass();
    });

    it('BITCOUNT counts set bits', () => {
      // 0xff = 8 bits set
      expect(runScript('RETURN BITCOUNT(0xff) EQ 8')).toPass();
    });

    it('BITGET returns specific bit', () => {
      // bit 0 of 0x01 = 1
      expect(runScript('RETURN BITGET(0x01, 0) EQ 1')).toPass();
    });

    it('BITSET sets specific bit', () => {
      expect(runScript('LET h = BITSET(0x00, 0, 1) RETURN BITGET(h, 0) EQ 1')).toPass();
    });
  });

  // === GETINADDR / GETINAMT / GETINTOK ===
  describe('Input accessors', () => {
    it('GETINADDR returns input address', () => {
      expect(runScript('RETURN GETINADDR(0) EQ 0xaabbccdd', {
        transaction: {
          inputs: [{ coinId: '0x01', address: '0xaabbccdd', amount: 100, tokenId: '0x00', blockCreated: 100 }],
          outputs: [], blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });

    it('GETINAMT returns input amount', () => {
      expect(runScript('RETURN GETINAMT(0) EQ 750', {
        transaction: {
          inputs: [{ coinId: '0x01', address: '0x01', amount: 750, tokenId: '0x00', blockCreated: 100 }],
          outputs: [], blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });

    it('GETINTOK returns token ID', () => {
      expect(runScript('RETURN GETINTOK(0) EQ 0x00', {
        transaction: {
          inputs: [{ coinId: '0x01', address: '0x01', amount: 100, tokenId: '0x00', blockCreated: 100 }],
          outputs: [], blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });

    it('SUMINPUTS sums native Minima inputs', () => {
      expect(runScript('RETURN SUMINPUTS(0x00) EQ 150', {
        transaction: {
          inputs: [
            { coinId: '0x01', address: '0x01', amount: 100, tokenId: '0x00', blockCreated: 100 },
            { coinId: '0x02', address: '0x02', amount: 50,  tokenId: '0x00', blockCreated: 100 },
          ],
          outputs: [], blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });
  });

  // === GETOUTADDR / GETOUTAMT / SUMOUTPUTS ===
  describe('Output accessors', () => {
    it('GETOUTADDR returns output address', () => {
      expect(runScript('RETURN GETOUTADDR(0) EQ 0xdeadbeef', {
        transaction: {
          inputs:  [{ coinId: '0x01', address: '0x01', amount: 100, tokenId: '0x00', blockCreated: 100 }],
          outputs: [{ address: '0xdeadbeef', amount: 80, tokenId: '0x00' }],
          blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });

    it('GETOUTAMT returns output amount', () => {
      expect(runScript('RETURN GETOUTAMT(0) EQ 80', {
        transaction: {
          inputs:  [{ coinId: '0x01', address: '0x01', amount: 100, tokenId: '0x00', blockCreated: 100 }],
          outputs: [{ address: '0xdeadbeef', amount: 80, tokenId: '0x00' }],
          blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });

    it('SUMOUTPUTS sums native token outputs', () => {
      expect(runScript('RETURN SUMOUTPUTS(0x00) EQ 130', {
        transaction: {
          inputs:  [{ coinId: '0x01', address: '0x01', amount: 200, tokenId: '0x00', blockCreated: 100 }],
          outputs: [
            { address: '0x01', amount: 80,  tokenId: '0x00' },
            { address: '0x02', amount: 50,  tokenId: '0x00' },
          ],
          blockNumber: 200, blockTime: Date.now(), signatures: []
        }
      })).toPass();
    });
  });

  // === PROOF ===
  describe('PROOF function', () => {
    it('PROOF returns FALSE for non-matching proof', () => {
      expect(runScript('RETURN NOT(PROOF(0xabcdef, 0x112233, 0xffeedd))')).toPass();
    });

    it('PROOF result can be used in IF', () => {
      expect(runScript(
        'IF PROOF(0xabcdef, 0x112233, 0xffeedd) THEN RETURN FALSE ELSE RETURN TRUE ENDIF'
      )).toPass();
    });
  });

  // === SIGDIG ===
  describe('SIGDIG precision', () => {
    it('SIGDIG(12345, 10) gives 5 significant digits', () => {
      // 5 significant decimal digits
      expect(runScript('RETURN SIGDIG(12345, 10) EQ 5')).toPass();
    });

    it('SIGDIG(0, 10) gives 0', () => {
      expect(runScript('RETURN SIGDIG(0, 10) EQ 0')).toPass();
    });
  });
});
