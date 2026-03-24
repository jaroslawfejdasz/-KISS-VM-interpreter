/**
 * kiss-vm-lint tests
 */
const assert = require('node:assert/strict');
const { describe, it, run } = require('node:test');

const { lint } = require('../dist/index.js');

// ── Helpers ───────────────────────────────────────────────────────────────────

function hasRule(result, ruleId) {
  return result.issues.some(i => i.rule === ruleId);
}

function errors(result)   { return result.issues.filter(i => i.severity === 'error'); }
function warnings(result) { return result.issues.filter(i => i.severity === 'warning'); }

// ── Tests ─────────────────────────────────────────────────────────────────────

describe('instruction-limit', () => {
  it('clean for short script', () => {
    const r = lint('RETURN TRUE');
    assert.equal(hasRule(r, 'instruction-limit'), false);
  });

  it('warning for 850 tokens', () => {
    const script = Array(850).fill('NOP').join(' ');
    // We need RETURN too — but just test the rule triggers
    const r = lint(script);
    assert.ok(hasRule(r, 'instruction-limit'));
    assert.equal(r.issues.find(i => i.rule === 'instruction-limit').severity, 'warning');
  });

  it('error for 1100 tokens', () => {
    const script = Array(1100).fill('NOP').join(' ');
    const r = lint(script);
    assert.ok(hasRule(r, 'instruction-limit'));
    assert.equal(r.issues.find(i => i.rule === 'instruction-limit').severity, 'error');
  });
});

describe('return-present', () => {
  it('no warning when RETURN is present', () => {
    const r = lint('RETURN TRUE');
    assert.equal(hasRule(r, 'return-present'), false);
  });

  it('warning when RETURN is missing', () => {
    const r = lint('LET x = 5');
    assert.ok(hasRule(r, 'return-present'));
  });

  it('no warning for RETURN FALSE', () => {
    const r = lint('RETURN FALSE');
    assert.equal(hasRule(r, 'return-present'), false);
  });
});

describe('balanced-if', () => {
  it('clean for balanced IF/ENDIF', () => {
    const r = lint('IF TRUE THEN RETURN TRUE ENDIF RETURN FALSE');
    assert.equal(hasRule(r, 'balanced-if'), false);
  });

  it('error for unclosed IF', () => {
    const r = lint('IF TRUE THEN LET x = 1 RETURN TRUE');
    assert.ok(hasRule(r, 'balanced-if'));
    assert.equal(errors(r).find(i => i.rule === 'balanced-if').severity, 'error');
  });

  it('error for ENDIF without IF', () => {
    const r = lint('RETURN TRUE ENDIF');
    assert.ok(hasRule(r, 'balanced-if'));
  });

  it('clean for nested IF/ENDIF', () => {
    const script = 'IF TRUE THEN IF FALSE THEN RETURN FALSE ENDIF RETURN TRUE ENDIF RETURN FALSE';
    const r = lint(script);
    assert.equal(hasRule(r, 'balanced-if'), false);
  });
});

describe('balanced-while', () => {
  it('clean for balanced WHILE/ENDWHILE', () => {
    const r = lint('WHILE TRUE DO LET x = 1 ENDWHILE RETURN TRUE');
    assert.equal(hasRule(r, 'balanced-while'), false);
  });

  it('error for unclosed WHILE', () => {
    const r = lint('WHILE TRUE DO LET x = 1 RETURN TRUE');
    assert.ok(hasRule(r, 'balanced-while'));
  });

  it('error for ENDWHILE without WHILE', () => {
    const r = lint('RETURN TRUE ENDWHILE');
    assert.ok(hasRule(r, 'balanced-while'));
  });
});

describe('address-check', () => {
  it('no warning for SIGNEDBY contract', () => {
    const r = lint('LET sig = SIGNEDBY(0xABCD) RETURN sig');
    assert.equal(hasRule(r, 'address-check'), false);
  });

  it('no warning for CHECKSIG contract', () => {
    const r = lint('LET ok = CHECKSIG(0xPUBKEY 0xSIG) RETURN ok');
    assert.equal(hasRule(r, 'address-check'), false);
  });

  it('no warning for MULTISIG', () => {
    const r = lint('RETURN MULTISIG(2 0xA 0xB 0xC)');
    assert.equal(hasRule(r, 'address-check'), false);
  });

  it('no warning for VERIFYOUT', () => {
    const r = lint('LET ok = VERIFYOUT(0 0xADDR 100 0x00 TRUE) RETURN ok');
    assert.equal(hasRule(r, 'address-check'), false);
  });

  it('warning when no access control at all (non-trivial script)', () => {
    const r = lint('LET x = 5 LET y = 10 RETURN x LT y');
    assert.ok(hasRule(r, 'address-check'));
  });

  it('no warning for RETURN TRUE (trivial, < 3 tokens)', () => {
    const r = lint('RETURN TRUE');
    assert.equal(hasRule(r, 'address-check'), false);
  });

  it('no warning for timelock contract', () => {
    const r = lint('RETURN @BLOCK GTE 1000');
    assert.equal(hasRule(r, 'address-check'), false);
  });
});

describe('no-rsa', () => {
  it('clean for non-RSA script', () => {
    const r = lint('RETURN CHECKSIG(0xKEY 0xSIG)');
    assert.equal(hasRule(r, 'no-rsa'), false);
  });

  it('warning when RSA is used', () => {
    const r = lint('LET ok = RSA(0xKEY 0xSIG 0xDATA) RETURN ok');
    assert.ok(hasRule(r, 'no-rsa'));
  });
});

describe('no-dead-code', () => {
  it('no warning for single RETURN TRUE', () => {
    const r = lint('RETURN TRUE');
    assert.equal(hasRule(r, 'no-dead-code'), false);
  });

  it('no warning for RETURN <expression>', () => {
    const r = lint('LET x = @BLOCK GTE 100 RETURN x');
    assert.equal(hasRule(r, 'no-dead-code'), false);
  });

  it('warning for two top-level RETURNs', () => {
    const r = lint('RETURN TRUE RETURN FALSE');
    assert.ok(hasRule(r, 'no-dead-code'));
  });

  it('no warning for RETURN inside IF + one top-level RETURN', () => {
    const r = lint('IF TRUE THEN RETURN TRUE ENDIF RETURN FALSE');
    assert.equal(hasRule(r, 'no-dead-code'), false);
  });

  it('no warning for complex expression: RETURN MULTISIG(2 0xA 0xB 0xC)', () => {
    const r = lint('RETURN MULTISIG(2 0xA 0xB 0xC)');
    assert.equal(hasRule(r, 'no-dead-code'), false);
  });
});

describe('no-shadow-globals', () => {
  it('no warning for normal variable', () => {
    const r = lint('LET x = 5 RETURN x GT 3');
    assert.equal(hasRule(r, 'no-shadow-globals'), false);
  });

  it('warning for LET @BLOCK = ...', () => {
    const r = lint('LET @BLOCK = 999 RETURN @BLOCK GT 100');
    assert.ok(hasRule(r, 'no-shadow-globals'));
  });
});

describe('assert-usage', () => {
  it('no info for < 3 ASSERTs', () => {
    const r = lint('ASSERT @BLOCK GT 100 ASSERT SIGNEDBY(0xA) RETURN TRUE');
    assert.equal(hasRule(r, 'assert-usage'), false);
  });

  it('info for > 3 ASSERTs', () => {
    const r = lint('ASSERT TRUE ASSERT TRUE ASSERT TRUE ASSERT TRUE RETURN TRUE');
    assert.ok(hasRule(r, 'assert-usage'));
  });
});

describe('clean scripts', () => {
  it('timelock: RETURN @BLOCK GTE 1000', () => {
    const r = lint('RETURN @BLOCK GTE 1000');
    assert.ok(r.clean, `Expected clean but got: ${JSON.stringify(r.issues)}`);
  });

  it('signedby: LET sig = SIGNEDBY(0xABC) RETURN sig', () => {
    const r = lint('LET sig = SIGNEDBY(0xABC) RETURN sig');
    assert.ok(r.clean, `Expected clean but got: ${JSON.stringify(r.issues)}`);
  });

  it('multisig: RETURN MULTISIG(2 0xA 0xB 0xC)', () => {
    const r = lint('RETURN MULTISIG(2 0xA 0xB 0xC)');
    assert.ok(r.clean, `Expected clean but got: ${JSON.stringify(r.issues)}`);
  });

  it('checksig: RETURN CHECKSIG(0xPUBKEY 0xSIG)', () => {
    const r = lint('RETURN CHECKSIG(0xPUBKEY 0xSIG)');
    assert.ok(r.clean, `Expected clean but got: ${JSON.stringify(r.issues)}`);
  });

  it('coinage: RETURN @COINAGE GT 100', () => {
    const r = lint('RETURN @COINAGE GT 100');
    assert.ok(r.clean, `Expected clean but got: ${JSON.stringify(r.issues)}`);
  });
});

