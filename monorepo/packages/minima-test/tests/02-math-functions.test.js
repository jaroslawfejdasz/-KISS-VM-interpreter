const { describe, it, expect, runScript } = require('../dist/api');

describe('Math Functions', () => {
  it('ABS - absolute value', () => {
    expect(runScript('RETURN ABS(-5) EQ 5')).toPass();
    expect(runScript('RETURN ABS(5) EQ 5')).toPass();
  });

  it('CEIL - ceiling', () => {
    expect(runScript('RETURN CEIL(4.1) EQ 5')).toPass();
    expect(runScript('RETURN CEIL(4.0) EQ 4')).toPass();
  });

  it('FLOOR - floor', () => {
    expect(runScript('RETURN FLOOR(4.9) EQ 4')).toPass();
  });

  it('INC / DEC', () => {
    expect(runScript('LET x = 5 RETURN INC(x) EQ 6')).toPass();
    expect(runScript('LET x = 5 RETURN DEC(x) EQ 4')).toPass();
  });

  it('MAX / MIN', () => {
    expect(runScript('RETURN MAX(3,7) EQ 7')).toPass();
    expect(runScript('RETURN MIN(3,7) EQ 3')).toPass();
  });

  it('POW', () => {
    expect(runScript('RETURN POW(2,10) EQ 1024')).toPass();
  });

  it('SQRT', () => {
    expect(runScript('RETURN SQRT(144) EQ 12')).toPass();
  });

  it('SIGDIG - digits in base 10', () => {
    // SIGDIG(num, base) returns the number of digits of num in given base
    expect(runScript('RETURN SIGDIG(12345, 10) EQ 5')).toPass();
  });
});
