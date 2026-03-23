const { describe, it, expect, runScript } = require('../dist/api');

describe('Limits & Safety', () => {

  it('instruction limit: 1024 ok', () => {
    let script = '';
    for (let i = 0; i < 1023; i++) script += `LET v${i} = ${i} `;
    script += 'RETURN TRUE';
    const r = runScript(script);
    if (!r.success) throw new Error('Expected pass, got: ' + r.error);
    if (r.instructions !== 1024) throw new Error('Expected 1024 instructions, got ' + r.instructions);
  });

  it('instruction limit: 1025 throws', () => {
    let script = '';
    for (let i = 0; i < 1024; i++) script += `LET v${i} = ${i} `;
    script += 'RETURN TRUE';
    expect(runScript(script)).toThrow('MAX_INSTRUCTIONS exceeded');
  });

  it('division by zero throws', () => {
    expect(runScript('LET x = 10 / 0 RETURN TRUE')).toThrow('zero');
  });

  it('WHILE never executes if condition false from start', () => {
    expect(runScript(
      'LET i = 0 WHILE i GT 10 DO LET i = i + 1 ENDWHILE RETURN i EQ 0'
    )).toPass();
  });
});
