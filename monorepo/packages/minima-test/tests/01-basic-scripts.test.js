const { describe, it, expect, runScript, defaultTransaction } = require('../dist/api');

describe('Basic KISS VM Scripts', () => {

  it('RETURN TRUE passes', () => {
    expect(runScript('RETURN TRUE')).toPass();
  });

  it('RETURN FALSE fails', () => {
    expect(runScript('RETURN FALSE')).toFail();
  });

  it('simple boolean AND', () => {
    expect(runScript('RETURN TRUE AND TRUE')).toPass();
  });

  it('boolean AND with FALSE fails', () => {
    expect(runScript('RETURN TRUE AND FALSE')).toFail();
  });

  it('boolean OR', () => {
    expect(runScript('RETURN FALSE OR TRUE')).toPass();
  });

  it('NOT TRUE is FALSE', () => {
    expect(runScript('RETURN NOT TRUE')).toFail();
  });

  it('NOT FALSE is TRUE', () => {
    expect(runScript('RETURN NOT FALSE')).toPass();
  });

  it('LET variable assignment', () => {
    expect(runScript('LET x = 5 RETURN x EQ 5')).toPass();
  });

  it('LET without = (original Minima style)', () => {
    expect(runScript('LET x 5 RETURN x EQ 5')).toPass();
  });

  it('numeric comparison EQ', () => {
    expect(runScript('RETURN 3 EQ 3')).toPass();
  });

  it('numeric comparison NEQ', () => {
    expect(runScript('RETURN 3 NEQ 4')).toPass();
  });

  it('numeric comparison LT', () => {
    expect(runScript('RETURN 3 LT 5')).toPass();
  });

  it('numeric comparison GT', () => {
    expect(runScript('RETURN 10 GT 7')).toPass();
  });

  it('IF THEN ELSE - true branch', () => {
    expect(runScript('IF TRUE THEN RETURN TRUE ELSE RETURN FALSE ENDIF')).toPass();
  });

  it('IF THEN ELSE - false branch', () => {
    expect(runScript('IF FALSE THEN RETURN FALSE ELSE RETURN TRUE ENDIF')).toPass();
  });

  it('WHILE loop', () => {
    expect(runScript(
      'LET i = 0 WHILE i LT 3 DO LET i = i + 1 ENDWHILE RETURN i EQ 3'
    )).toPass();
  });

  it('nested IF inside WHILE', () => {
    expect(runScript(
      'LET x = 0 WHILE x LT 5 DO IF x EQ 3 THEN RETURN TRUE ENDIF LET x = x + 1 ENDWHILE RETURN FALSE'
    )).toPass();
  });
});
