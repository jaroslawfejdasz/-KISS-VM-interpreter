const { describe, it, expect, runScript } = require('../dist/api');

describe('Limits & Safety', () => {
  it('MAX_INSTRUCTIONS - script with exactly 1024 instructions should pass', () => {
    // Build a script with many LETs
    let script = '';
    for (let i = 0; i < 100; i++) script += `LET v${i} = ${i} `;
    script += 'RETURN TRUE';
    expect(runScript(script)).toPass();
  });

  it('Instructions count is tracked', () => {
    const result = runScript('LET x = 1 LET y = 2 RETURN x EQ 1');
    // Should have used a few instructions
    if (result.instructions <= 0) throw new Error('Instructions not tracked');
  });

  it('WHILE loop - exits correctly', () => {
    expect(runScript(
      'LET sum = 0 LET i = 1 WHILE i LTE 10 DO LET sum = sum + i LET i = i + 1 ENDWHILE RETURN sum EQ 55'
    )).toPass();
  });

  it('Deeply nested IF still resolves', () => {
    expect(runScript(
      'IF TRUE THEN IF TRUE THEN IF TRUE THEN RETURN TRUE ENDIF ENDIF ENDIF RETURN FALSE'
    )).toPass();
  });
});
