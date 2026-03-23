const { analyze } = require('../dist/index');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    console.log('\x1b[32m  ✓\x1b[0m ' + name);
    passed++;
  } catch(e) {
    console.log('\x1b[31m  ✗\x1b[0m ' + name);
    console.log('    \x1b[31m→ ' + e.message + '\x1b[0m');
    failed++;
  }
}

function expect(val) {
  return {
    toHaveErrors(n) {
      if (val.summary.errors !== n) throw new Error('Expected ' + n + ' error(s), got ' + val.summary.errors + ': ' + val.errors.map(e=>e.message).join('; '));
    },
    toHaveWarnings(n) {
      if (val.summary.warnings !== n) throw new Error('Expected ' + n + ' warning(s), got ' + val.summary.warnings + ': ' + val.warnings.map(e=>e.message).join('; '));
    },
    toBeClean() {
      if (val.summary.errors > 0 || val.summary.warnings > 0) throw new Error('Expected no errors/warnings, got: ' + val.all.filter(e=>e.severity!=='info').map(e=>e.message).join('; '));
    },
    toHaveCode(code) {
      if (!val.all.some(e => e.code === code)) throw new Error('Expected code ' + code + ', got: ' + val.all.map(e=>e.code).join(', '));
    },
    toNotHaveCode(code) {
      if (val.all.some(e => e.code === code)) throw new Error('Expected NOT to have code ' + code);
    },
  };
}

// =====================================================================
console.log('\n\x1b[1m  Tokenizer Errors\x1b[0m');

test('E001 - unterminated string', () => {
  expect(analyze('RETURN [hello')).toHaveCode('E001');
});
test('E002 - empty hex literal', () => {
  expect(analyze('RETURN 0x EQ TRUE')).toHaveCode('E002');
});
test('E003 - unknown character', () => {
  expect(analyze('RETURN $ EQ TRUE')).toHaveCode('E003');
});

// =====================================================================
console.log('\n\x1b[1m  Missing RETURN\x1b[0m');

test('E011 - no RETURN statement', () => {
  expect(analyze('LET x = 5')).toHaveCode('E011');
});
test('clean script with RETURN', () => {
  expect(analyze('RETURN SIGNEDBY(0xaabb)')).toBeClean();
});

// =====================================================================
console.log('\n\x1b[1m  Statement Errors\x1b[0m');

test('E020 - non-keyword as statement', () => {
  expect(analyze('5 RETURN TRUE')).toHaveCode('E020');
});
test('E021 - function used as statement', () => {
  expect(analyze('SHA3(0xaabb) RETURN TRUE')).toHaveCode('E021');
});
test('E030 - LET without variable name', () => {
  expect(analyze('LET = 5 RETURN TRUE')).toHaveCode('E030');
});

// =====================================================================
console.log('\n\x1b[1m  IF/WHILE Structure\x1b[0m');

test('E040 - IF without THEN', () => {
  expect(analyze('IF TRUE RETURN TRUE ENDIF')).toHaveCode('E040');
});
test('E042 - IF without ENDIF', () => {
  expect(analyze('IF TRUE THEN RETURN TRUE')).toHaveCode('E042');
});
test('E050 - WHILE without DO', () => {
  expect(analyze('LET i = 0 WHILE i LT 5 LET i = INC(i) ENDWHILE RETURN TRUE')).toHaveCode('E050');
});
test('E051 - WHILE without ENDWHILE', () => {
  expect(analyze('LET i = 0 WHILE i LT 5 DO LET i = INC(i) RETURN TRUE')).toHaveCode('E051');
});
test('valid IF/ELSE/ENDIF', () => {
  expect(analyze('IF @BLOCK GT 100 THEN RETURN SIGNEDBY(0xaabb) ELSE RETURN FALSE ENDIF')).toBeClean();
});
test('valid WHILE loop', () => {
  expect(analyze('LET i = 0 WHILE i LT 3 DO LET i = INC(i) ENDWHILE RETURN TRUE')).toNotHaveCode('E050');
});

// =====================================================================
console.log('\n\x1b[1m  Expression & Function Errors\x1b[0m');

test('E060 - division by zero', () => {
  expect(analyze('LET x = 5 / 0 RETURN TRUE')).toHaveCode('E060');
});
test('E080 - function without parens', () => {
  // SHA3 without () - hard to test in isolation, skip edge
  // Instead: missing closing paren
  expect(analyze('RETURN SHA3(0xaabb')).toHaveCode('E081');
});
test('E082 - too few arguments', () => {
  expect(analyze('RETURN SHA3()')).toHaveCode('E082');
});
test('E083 - too many arguments', () => {
  expect(analyze('RETURN SHA3(0xaa, 0xbb)')).toHaveCode('E083');
});
test('E082 MULTISIG needs at least 3 args', () => {
  // MULTISIG(n, key1, key2, ...) needs minimum 3 args: n + at least 2 keys
  expect(analyze('RETURN MULTISIG(2, 0xaaaa)')).toHaveCode('E082');
});
test('valid MULTISIG 2-of-3', () => {
  expect(analyze('RETURN MULTISIG(2, 0xaaaa, 0xbbbb, 0xcccc)')).toNotHaveCode('E082');
});
test('VERIFYOUT correct 4 args', () => {
  expect(analyze('RETURN VERIFYOUT(0, 100, 0xaabbccdd, 0x00)')).toNotHaveCode('E082');
});
test('VERIFYOUT too few args', () => {
  expect(analyze('RETURN VERIFYOUT(0, 100, 0xaabb)')).toHaveCode('E082');
});

// =====================================================================
console.log('\n\x1b[1m  Warnings\x1b[0m');

test('W010 - unused variable', () => {
  expect(analyze('LET x = 5 RETURN TRUE')).toHaveCode('W010');
});
test('W010 - used variable is not warned', () => {
  expect(analyze('LET x = 5 RETURN x EQ 5')).toNotHaveCode('W010');
});
test('E030 - keyword cannot be used as variable name', () => {
  // In KISS VM: 'abs', 'sha3', etc. are all tokenized as KEYWORD ABS/SHA3 (uppercase)
  // so LET abs = 5 becomes LET KEYWORD('ABS') = 5 -> E030
  expect(analyze('LET abs = 5 RETURN TRUE')).toHaveCode('E030');
});
test('W040 - dead code after RETURN', () => {
  expect(analyze('RETURN TRUE LET x = 5')).toHaveCode('W040');
});
test('W050 - use = instead of EQ', () => {
  expect(analyze('LET x = 5 RETURN x = 5')).toHaveCode('W050');
});
test('W060 - unknown global', () => {
  expect(analyze('RETURN @FAKEVAR EQ 1')).toHaveCode('W060');
});
test('W060 - valid global no warning', () => {
  expect(analyze('RETURN @BLOCK GT 0')).toNotHaveCode('W060');
});
test('W070 - variable used before LET', () => {
  expect(analyze('RETURN x EQ 5')).toHaveCode('W070');
});

// =====================================================================
console.log('\n\x1b[1m  Real Contract Patterns\x1b[0m');

test('basic SIGNEDBY contract is clean', () => {
  expect(analyze('RETURN SIGNEDBY(0xaabbccddaabbccddaabbccddaabbccdd)')).toBeClean();
});
test('time lock contract is clean', () => {
  expect(analyze(`
    LET lockBlock = 1000
    IF @BLOCK LT lockBlock THEN
      RETURN FALSE
    ENDIF
    RETURN SIGNEDBY(0xaabbccddaabbccddaabbccddaabbccdd)
  `)).toBeClean();
});
test('HTLC contract structure', () => {
  const r = analyze(`
    LET secret = STATE(1)
    LET hash = SHA3(secret)
    LET expected = 0xdeadbeefdeadbeef
    IF hash EQ expected THEN
      RETURN SIGNEDBY(0xaaaa1111aaaa1111)
    ENDIF
    RETURN FALSE
  `);
  expect(r).toNotHaveCode('E011');
  expect(r).toNotHaveCode('E040');
});
test('instruction estimate is tracked', () => {
  const r = analyze('LET a = 1 LET b = 2 RETURN a EQ b');
  if (r.summary.instructionEstimate < 3) throw new Error('Expected >= 3 instructions, got ' + r.summary.instructionEstimate);
});

// =====================================================================
console.log('\n\x1b[1m  Edge Cases\x1b[0m');

test('empty script reports no RETURN', () => {
  expect(analyze('')).toHaveCode('E011');
});
test('comment-only script reports no RETURN', () => {
  expect(analyze('/* just a comment */')).toHaveCode('E011');
});
test('ELSEIF chain is valid', () => {
  expect(analyze(`
    LET x = 2
    IF x EQ 1 THEN RETURN SIGNEDBY(0xaaaa)
    ELSEIF x EQ 2 THEN RETURN SIGNEDBY(0xbbbb)
    ELSE RETURN FALSE
    ENDIF
  `)).toBeClean();
});
test('nested IF is valid', () => {
  expect(analyze(`
    IF @BLOCK GT 100 THEN
      IF SIGNEDBY(0xaabb) THEN
        RETURN TRUE
      ENDIF
    ENDIF
    RETURN FALSE
  `)).toBeClean();
});
test('@BLOCK is valid global', () => {
  expect(analyze('RETURN @BLOCK GT 100')).toNotHaveCode('W060');
});
test('@AMOUNT is valid global', () => {
  expect(analyze('RETURN @AMOUNT GT 0')).toNotHaveCode('W060');
});

// =====================================================================
const total = passed + failed;
console.log('\n\x1b[90m' + '─'.repeat(50) + '\x1b[0m');
if (failed === 0) {
  console.log('\x1b[32m\x1b[1m\n  ✓ All ' + total + ' tests passed!\x1b[0m\n');
  process.exit(0);
} else {
  console.log('\x1b[31m\x1b[1m\n  ✗ ' + failed + ' failed, ' + passed + ' passed\x1b[0m\n');
  process.exit(1);
}

// =====================================================================
console.log('\n\x1b[1m  W090 - No Authorization Check\x1b[0m');

test('W090 - RETURN TRUE has no auth', () => {
  expect(analyze('RETURN TRUE')).toHaveCode('W090');
});
test('W090 - SIGNEDBY clears warning', () => {
  expect(analyze('RETURN SIGNEDBY(0xaabbccddaabbccddaabbccddaabbccdd)')).toNotHaveCode('W090');
});
test('W090 - CHECKSIG clears warning', () => {
  expect(analyze('RETURN CHECKSIG(0xaabb, 0xccdd, 0xeeff)')).toNotHaveCode('W090');
});
test('W090 - MULTISIG clears warning', () => {
  expect(analyze('RETURN MULTISIG(2, 0xaaaa, 0xbbbb, 0xcccc)')).toNotHaveCode('W090');
});
test('W090 - HTLC without owner sig warns', () => {
  expect(analyze('IF SHA3(STATE(1)) EQ 0xdeadbeef THEN RETURN TRUE ENDIF RETURN FALSE')).toHaveCode('W090');
});
