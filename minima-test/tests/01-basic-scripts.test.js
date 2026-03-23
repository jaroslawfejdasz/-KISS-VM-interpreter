const { describe, it, expect, runScript } = require('../dist/api');

describe('Basic KISS VM Scripts', () => {

  it('RETURN TRUE always passes', () => {
    expect(runScript('RETURN TRUE')).toPass();
  });

  it('RETURN FALSE always fails', () => {
    expect(runScript('RETURN FALSE')).toFail();
  });

  it('LET variable and compare', () => {
    expect(runScript('LET x = 5 RETURN x EQ 5')).toPass();
  });

  it('LET with expression', () => {
    expect(runScript('LET x = 10 + 5 RETURN x EQ 15')).toPass();
  });

  it('LET string variable', () => {
    expect(runScript('LET s = [hello] RETURN s EQ [hello]')).toPass();
  });

  it('Arithmetic operators', () => {
    expect(runScript('LET x = 3 * 4 RETURN x EQ 12')).toPass();
  });

  it('Nested arithmetic', () => {
    expect(runScript('LET x = (2 + 3) * 4 RETURN x EQ 20')).toPass();
  });

  it('IF THEN ENDIF - true branch', () => {
    expect(runScript('LET x = 1 IF x EQ 1 THEN RETURN TRUE ENDIF RETURN FALSE')).toPass();
  });

  it('IF THEN ELSE ENDIF - false branch', () => {
    expect(runScript('IF FALSE THEN RETURN FALSE ELSE RETURN TRUE ENDIF')).toPass();
  });

  it('ELSEIF branch', () => {
    expect(runScript(
      'LET x = 2 IF x EQ 1 THEN RETURN FALSE ELSEIF x EQ 2 THEN RETURN TRUE ENDIF RETURN FALSE'
    )).toPass();
  });

  it('AND operator', () => {
    expect(runScript('RETURN TRUE AND TRUE')).toPass();
    expect(runScript('RETURN TRUE AND FALSE')).toFail();
  });

  it('OR operator', () => {
    expect(runScript('RETURN FALSE OR TRUE')).toPass();
    expect(runScript('RETURN FALSE OR FALSE')).toFail();
  });

  it('NOT operator', () => {
    expect(runScript('RETURN NOT FALSE')).toPass();
    expect(runScript('RETURN NOT TRUE')).toFail();
  });

  it('WHILE loop', () => {
    expect(runScript(
      'LET i = 0 WHILE i LT 5 DO LET i = i + 1 ENDWHILE RETURN i EQ 5'
    )).toPass();
  });

  it('ASSERT passes on TRUE', () => {
    expect(runScript('ASSERT TRUE RETURN TRUE')).toPass();
  });

  it('ASSERT throws on FALSE', () => {
    expect(runScript('ASSERT FALSE RETURN TRUE')).toThrow('ASSERT failed');
  });

  it('Comparison operators', () => {
    expect(runScript('RETURN 5 GT 3')).toPass();
    expect(runScript('RETURN 5 LT 3')).toFail();
    expect(runScript('RETURN 5 GTE 5')).toPass();
    expect(runScript('RETURN 5 LTE 4')).toFail();
    expect(runScript('RETURN 5 NEQ 3')).toPass();
  });
});
