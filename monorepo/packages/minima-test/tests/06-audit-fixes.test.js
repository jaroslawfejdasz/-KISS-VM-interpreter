const { describe, it, expect, runScript, defaultTransaction } = require('../dist/api');

describe('Audit Fixes', () => {

  // BUG #1: String whitespace preservation
  describe('String whitespace (BUG #1 fix)', () => {
    it('preserves internal spaces in strings', () => {
      expect(runScript('RETURN [hello world] EQ [hello world]')).toPass();
    });

    it('preserves leading space in string', () => {
      expect(runScript('LET a = [hi] LET b = [ there] RETURN LEN(b) EQ 6')).toPass();
    });

    it('string comparison is exact', () => {
      expect(runScript('RETURN [abc] NEQ [ abc]')).toPass();
    });
  });

  // BUG #2: MULTISIG implementation
  describe('MULTISIG (BUG #2 fix)', () => {
    const k1 = '0xaabbccdd11223344';
    const k2 = '0xdeadbeef12345678';
    const k3 = '0x1234567890abcdef';

    it('1-of-2 passes with one sig', () => {
      expect(runScript(
        `RETURN MULTISIG(1, ${k1}, ${k2})`,
        { signatures: [k1] }
      )).toPass();
    });

    it('2-of-2 passes with both sigs', () => {
      expect(runScript(
        `RETURN MULTISIG(2, ${k1}, ${k2})`,
        { signatures: [k1, k2] }
      )).toPass();
    });

    it('2-of-2 fails with one sig', () => {
      expect(runScript(
        `RETURN MULTISIG(2, ${k1}, ${k2})`,
        { signatures: [k1] }
      )).toFail();
    });

    it('2-of-3 passes with two correct sigs', () => {
      expect(runScript(
        `RETURN MULTISIG(2, ${k1}, ${k2}, ${k3})`,
        { signatures: [k1, k3] }
      )).toPass();
    });

    it('2-of-3 fails with zero sigs', () => {
      expect(runScript(
        `RETURN MULTISIG(2, ${k1}, ${k2}, ${k3})`,
        { signatures: [] }
      )).toFail();
    });

    it('2-of-3 fails with wrong keys', () => {
      expect(runScript(
        `RETURN MULTISIG(2, ${k1}, ${k2}, ${k3})`,
        { signatures: ['0xdeaddeaddeaddead'] }
      )).toFail();
    });

    it('MULTISIG is case-insensitive for signatures', () => {
      // hex prefix must be lowercase 0x (Minima spec), but hex digits can be any case
      const k1lower = '0xaabbccdd11223344';
      const k1upper = '0xAABBCCDD11223344';
      expect(runScript(
        `RETURN MULTISIG(1, ${k1upper})`,
        { signatures: [k1lower] }
      )).toPass();
    });
  });

  // BUG #3: Invalid HEX literal
  describe('HEX tokenization (BUG #3 fix)', () => {
    it('rejects 0x without hex digits', () => {
      expect(runScript('RETURN 0x EQ 0x00')).toThrow();
    });

    it('accepts valid 0x00', () => {
      expect(runScript('RETURN 0x00 EQ 0x00')).toPass();
    });

    it('accepts 0x with uppercase digits', () => {
      expect(runScript('RETURN 0xAABB EQ 0xaabb')).toPass();
    });
  });

  // Additional edge cases found during audit
  describe('Edge Cases', () => {
    it('LET without = (original Minima style)', () => {
      expect(runScript('LET x 5 RETURN x EQ 5')).toPass();
    });

    it('comments are ignored', () => {
      expect(runScript('/* ignore this */ RETURN TRUE /* and this */')).toPass();
    });

    it('negative number literal', () => {
      expect(runScript('LET x = -5 RETURN x EQ -5')).toPass();
    });

    it('subtraction vs negative unary', () => {
      expect(runScript('LET x = 10 - 3 RETURN x EQ 7')).toPass();
    });

    it('EXEC shares env variables', () => {
      expect(runScript('EXEC [LET x = 42 RETURN TRUE] RETURN x EQ 42')).toPass();
    });

    it('nested function calls', () => {
      expect(runScript('RETURN ABS(MIN(-3, -7)) EQ 7')).toPass();
    });

    it('instruction limit: exactly 1024 ok', () => {
      let script = '';
      for (let i = 0; i < 1023; i++) script += `LET v${i} = ${i} `;
      script += 'RETURN TRUE';
      const r = runScript(script);
      if (!r.success) throw new Error(`Expected pass, got: ${r.error}`);
      if (r.instructions !== 1024) throw new Error(`Expected 1024 instructions, got ${r.instructions}`);
    });

    it('instruction limit: 1025 throws', () => {
      let script = '';
      for (let i = 0; i < 1024; i++) script += `LET v${i} = ${i} `;
      script += 'RETURN TRUE';
      expect(runScript(script)).toThrow('MAX_INSTRUCTIONS exceeded');
    });

    it('WHILE never executes if condition false from start', () => {
      expect(runScript(
        'LET i = 0 WHILE i GT 10 DO LET i = i + 1 ENDWHILE RETURN i EQ 0'
      )).toPass();
    });

    it('HEX EQ comparison (case-insensitive)', () => {
      expect(runScript('RETURN 0xAABB EQ 0xaabb')).toPass();
    });
  });
});
