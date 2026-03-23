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

// STATE/PREVSTATE type coercion (BUG #5 fix)
describe('STATE/PREVSTATE numeric comparison (BUG #5 fix)', () => {
  it('STATE returns numeric value for numeric state', () => {
    expect(runScript('RETURN STATE(1) EQ 42', {
      state: { 1: 42 }
    })).toPass();
  });

  it('PREVSTATE numeric comparison works correctly', () => {
    // Was broken: "5" GT "10" was TRUE (string compare), now correctly FALSE
    expect(runScript('RETURN STATE(1) GT PREVSTATE(1)', {
      state: { 1: 5 },
      transaction: { prevStateVars: { 1: '10' } }
    })).toFail();
  });

  it('nonce increment correctly detected', () => {
    expect(runScript('RETURN STATE(1) GT PREVSTATE(1)', {
      state: { 1: 11 },
      transaction: { prevStateVars: { 1: '10' } }
    })).toPass();
  });

  it('STATE returns HEX value for 0x-prefixed state', () => {
    expect(runScript('RETURN STATE(1) EQ 0xaabb', {
      state: { 1: '0xaabb' }
    })).toPass();
  });

  it('state channel nonce replay prevention', () => {
    const script = [
      'LET bothSigned = MULTISIG(2, 0xaaaa, 0xbbbb)',
      'IF bothSigned THEN RETURN TRUE ENDIF',
      'LET currentNonce = STATE(1)',
      'LET prevNonce = PREVSTATE(1)',
      'LET nonceOk = (currentNonce GT prevNonce)',
      'LET timedOut = (@COINAGE GTE 1440)',
      'LET signedByEither = (SIGNEDBY(0xaaaa) OR SIGNEDBY(0xbbbb))',
      'RETURN nonceOk AND timedOut AND signedByEither'
    ].join('\n');

    // Old state (nonce 5 < prevNonce 10) → replay rejected
    expect(runScript(script, {
      state: { 1: 5 },
      transaction: { prevStateVars: { 1: '10' } },
      signatures: ['0xaaaa'],
      coinAge: 2000
    })).toFail();

    // Fresh state (nonce 11 > prevNonce 10) → passes
    expect(runScript(script, {
      state: { 1: 11 },
      transaction: { prevStateVars: { 1: '10' } },
      signatures: ['0xaaaa'],
      coinAge: 2000
    })).toPass();
  });
});

describe('Globals coercion (BUG #5 fix)', () => {
  it('globals as string numbers are coerced correctly', () => {
    const r = runScript('RETURN @COINAGE GTE 100', { globals: { '@COINAGE': '200' } });
    if (!r.success) throw new Error('Expected TRUE, got: ' + r.error);
  });

  it('globals as JS numbers are coerced correctly', () => {
    const r = runScript('RETURN @BLOCK GTE 1000', { globals: { '@BLOCK': 2000 } });
    if (!r.success) throw new Error('Expected TRUE, got: ' + r.error);
  });

  it('globals as hex strings are coerced correctly', () => {
    const r = runScript('RETURN @TOKENID EQ 0xaabb', { globals: { '@TOKENID': '0xaabb' } });
    if (!r.success) throw new Error('Expected TRUE, got: ' + r.error);
  });

  it('globals string number beats default', () => {
    const r = runScript('RETURN @COINAGE LT 100', { globals: { '@COINAGE': '50' } });
    if (!r.success) throw new Error('Expected TRUE, got: ' + r.error);
  });
});

describe('Security & Spec Fixes (BUG #5 #6 #7)', () => {

  // BUG #5: EXEC instruction limit bypass
  it('EXEC cannot bypass instruction limit', () => {
    // Create an inner script with 600 instructions
    let inner = '';
    for (let i = 0; i < 600; i++) inner += `LET x${i} = ${i} `;
    inner += 'RETURN TRUE';
    // Outer: already has 500 instructions, then EXECs 600-instruction inner
    let outer = '';
    for (let i = 0; i < 500; i++) outer += `LET u${i} = ${i} `;
    outer += `EXEC [${inner}]`;
    const r = runScript(outer);
    if (!r.error || !r.error.includes('MAX_INSTRUCTIONS')) {
      throw new Error('Expected MAX_INSTRUCTIONS error, got: ' + (r.error || 'no error'));
    }
  });

  // BUG #6: Division by zero
  it('division by zero throws', () => {
    expect(runScript('LET x = 10 / 0 RETURN TRUE')).toThrow('zero');
  });

  it('modulo by zero throws', () => {
    expect(runScript('LET x = 10 % 0 RETURN TRUE')).toThrow('zero');
  });

  it('division by non-zero works', () => {
    expect(runScript('RETURN (10 / 2) EQ 5')).toPass();
  });

  // BUG #7: STATE(n) unset returns 0
  it('STATE(n) returns 0 for unset port', () => {
    expect(runScript('RETURN STATE(99) EQ 0')).toPass();
  });

  it('PREVSTATE(n) returns 0 for unset port', () => {
    expect(runScript('RETURN PREVSTATE(99) EQ 0')).toPass();
  });

  it('SAMESTATE works when both ports unset', () => {
    expect(runScript('RETURN SAMESTATE(1, 5)')).toPass();
  });

  it('SAMESTATE fails when current state differs from prev', () => {
    expect(runScript('RETURN SAMESTATE(1, 1)', {
      state: { 1: '42' },
      prevState: { 1: '0' }
    })).toFail();
  });
});

describe('Nested EXEC instruction counting (BUG #5 fix)', () => {
  it('nested EXEC counts instructions in shared counter', () => {
    // inner=500 LETs + outer=500 LETs = ~1005 total — must PASS
    let inner = '';
    for (let i = 0; i < 500; i++) inner += `LET a${i} = ${i} `;
    inner += 'RETURN TRUE';
    let body = `EXEC [${inner}] `;
    for (let i = 0; i < 500; i++) body += `LET b${i} = ${i} `;
    body += 'RETURN TRUE';
    const r = runScript(`EXEC [${body}] RETURN TRUE`);
    if (!r.success) throw new Error('Expected PASS: ' + r.error);
    if (r.instructions < 1000) throw new Error(`Instruction count too low: ${r.instructions} (shared counter not working)`);
  });

  it('nested EXEC blocks when combined total > 1024', () => {
    let inner = '';
    for (let i = 0; i < 510; i++) inner += `LET a${i} = ${i} `;
    inner += 'RETURN TRUE';
    let body = `EXEC [${inner}] `;
    for (let i = 0; i < 510; i++) body += `LET b${i} = ${i} `;
    body += 'RETURN TRUE';
    expect(runScript(`EXEC [${body}] RETURN TRUE`)).toThrow('MAX_INSTRUCTIONS exceeded');
  });

  it('EXEC sub-script continues parent execution after return', () => {
    // EXEC runs sub-script, returns, parent continues
    expect(runScript('EXEC [LET x = 99 RETURN TRUE] RETURN x EQ 99')).toPass();
  });
});
