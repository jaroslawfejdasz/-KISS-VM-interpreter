
// ─── NEW RULES: W071, W072 ───────────────────────────────────────────────────

describe('W071: Unknown function call', () => {
  it('warns on unknown function call', () => {
    const r = lint('RETURN FAKEFUNC(0xaabb)');
    const w = r.warnings.find(w => w.code === 'W071');
    if (!w) throw new Error('Expected W071 warning');
    if (!w.message.includes('FAKEFUNC')) throw new Error('Message should mention function name');
  });

  it('does not warn on known functions', () => {
    const r = lint('RETURN SHA3(0xaabb)');
    if (r.warnings.find(w => w.code === 'W071')) throw new Error('SHA3 should not trigger W071');
  });

  it('does not warn on valid variable reference', () => {
    const r = lint('LET fn = 1 RETURN fn EQ 1');
    if (r.warnings.find(w => w.code === 'W071')) throw new Error('Variable ref should not trigger W071');
  });
});

describe('W072: Variable redefined', () => {
  it('warns when variable is redefined', () => {
    const r = lint('LET x = 1 LET x = 2 RETURN x EQ 2');
    const w = r.warnings.find(w => w.code === 'W072');
    if (!w) throw new Error('Expected W072 warning');
    if (!w.message.includes("'x'")) throw new Error('Message should include var name');
  });

  it('does not warn for different variables', () => {
    const r = lint('LET x = 1 LET y = 2 RETURN x EQ y');
    if (r.warnings.find(w => w.code === 'W072')) throw new Error('Different vars should not warn');
  });
});
