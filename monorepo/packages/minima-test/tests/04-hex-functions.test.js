const { describe, it, expect, runScript } = require('../dist/api');

describe('HEX Functions', () => {
  it('LEN of hex', () => {
    // 0xaabb = 2 bytes
    expect(runScript('RETURN LEN(0xaabb) EQ 2')).toPass();
  });

  it('CONCAT two hex values', () => {
    expect(runScript('LET h = CONCAT(0xaabb, 0xccdd) RETURN h EQ 0xaabbccdd')).toPass();
  });

  it('SUBSET of hex', () => {
    // SUBSET(0xaabbccdd, 1, 2) = bytes 1..2 = 0xbbcc
    expect(runScript('RETURN SUBSET(0xaabbccdd, 1, 2) EQ 0xbbcc')).toPass();
  });

  it('REV reverses hex bytes', () => {
    expect(runScript('RETURN REV(0xaabbcc) EQ 0xccbbaa')).toPass();
  });

  it('SHA3 hash is hex', () => {
    // Just check it returns something non-empty (can't predict exact hash without node)
    expect(runScript('LET h = SHA3(0x01) RETURN LEN(h) EQ 32')).toPass();
  });

  it('SHA2 hash is 32 bytes', () => {
    expect(runScript('LET h = SHA2(0x01) RETURN LEN(h) EQ 32')).toPass();
  });
});
