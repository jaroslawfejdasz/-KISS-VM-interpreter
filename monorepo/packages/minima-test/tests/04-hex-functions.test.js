const { describe, it, expect, runScript } = require('../dist/api');

describe('HEX Functions', () => {

  it('LEN of HEX returns byte count', () => {
    expect(runScript('RETURN LEN(0xdeadbeef) EQ 4')).toPass();
  });

  it('CONCAT joins two hex values', () => {
    expect(runScript('LET h = CONCAT(0xdead, 0xbeef) RETURN LEN(h) EQ 4')).toPass();
  });

  it('SUBSET extracts bytes', () => {
    // SUBSET(hex, start, length)
    expect(runScript('LET h = SUBSET(0xdeadbeef, 0, 2) RETURN LEN(h) EQ 2')).toPass();
  });

  it('SHA2 produces 32-byte hash', () => {
    expect(runScript('LET h = SHA2(0xdeadbeef) RETURN LEN(h) EQ 32')).toPass();
  });

  it('SHA3 produces 32-byte hash', () => {
    expect(runScript('LET h = SHA3(0xdeadbeef) RETURN LEN(h) EQ 32')).toPass();
  });

  it('SHA3 is deterministic', () => {
    expect(runScript('RETURN SHA3(0xdeadbeef) EQ SHA3(0xdeadbeef)')).toPass();
  });
});
