import { tokenize, LintError } from './tokenizer';
import { ALL_RULES, Rule } from './rules';

export { LintError } from './tokenizer';

export interface LintSummary {
  errors: number;
  warnings: number;
  infos: number;
  total: number;
  instructionEstimate: number;
}

export interface LintResult {
  errors: LintError[];
  warnings: LintError[];
  infos: LintError[];
  all: LintError[];
  hasErrors: boolean;
  script: string;
  summary: LintSummary;
}

export interface LintOptions {
  rules?: Rule[];
  ignoreRules?: string[];
}

/**
 * Rough instruction count estimate — counts statement keywords.
 * Used for early detection of scripts approaching the 1024 limit.
 */
function estimateInstructions(script: string): number {
  const stmtKeywords = /\b(LET|IF|WHILE|RETURN|ASSERT|EXEC|MAST)\b/gi;
  return (script.match(stmtKeywords) ?? []).length;
}

export function lint(script: string, options: LintOptions = {}): LintResult {
  const rules = options.rules ?? ALL_RULES;
  const ignore = new Set(options.ignoreRules ?? []);

  const { tokens, errors: tokenErrors } = tokenize(script);

  const all: LintError[] = [...tokenErrors];

  for (const rule of rules) {
    const ruleErrors = rule(tokens, script);
    all.push(...ruleErrors);
  }

  const filtered = all.filter(e => !ignore.has(e.code));
  const errors   = filtered.filter(e => e.severity === 'error');
  const warnings = filtered.filter(e => e.severity === 'warning');
  const infos    = filtered.filter(e => e.severity === 'info');

  const summary: LintSummary = {
    errors:   errors.length,
    warnings: warnings.length,
    infos:    infos.length,
    total:    filtered.length,
    instructionEstimate: estimateInstructions(script),
  };

  return {
    errors,
    warnings,
    infos,
    all: filtered,
    hasErrors: errors.length > 0,
    script,
    summary,
  };
}

export function lintOrThrow(script: string, options?: LintOptions): LintResult {
  const result = lint(script, options);
  if (result.hasErrors) {
    const msgs = result.errors.map(e => `[${e.code}] ${e.message}`).join('\n');
    throw new Error(`KISS VM lint errors:\n${msgs}`);
  }
  return result;
}

export { ALL_RULES } from './rules';
export * as rules from './rules';

// Backward-compat alias
export { lint as analyze };
