/**
 * KISS VM Linter — static analysis engine
 */

import { LintError } from './tokenizer.js';
import { Rule, ALL_RULES } from './rules.js';
import { tokenize } from './tokenizer.js';

export type { LintError };
export type { Rule };

export interface LintResult {
  script: string;
  issues: LintError[];
  errors: number;
  warnings: number;
  infos: number;
  clean: boolean;
}

export class KissVMLinter {
  private rules: Rule[];

  constructor(rules: Rule[] = ALL_RULES) {
    this.rules = rules;
  }

  lint(script: string): LintResult {
    const { tokens } = tokenize(script);
    const issues: LintError[] = [];

    for (const rule of this.rules) {
      issues.push(...rule(tokens, script));
    }

    const errors   = issues.filter(i => i.severity === 'error').length;
    const warnings = issues.filter(i => i.severity === 'warning').length;
    const infos    = issues.filter(i => i.severity === 'info').length;

    return {
      script,
      issues,
      errors,
      warnings,
      infos,
      clean: errors === 0 && warnings === 0,
    };
  }
}

export function lint(script: string, rules?: Rule[]): LintResult {
  return new KissVMLinter(rules).lint(script);
}

// ── Formatter ─────────────────────────────────────────────────────────────────

type Severity = 'error' | 'warning' | 'info';

const SEVERITY_COLOR: Record<Severity, string> = {
  error:   '\x1b[31m',
  warning: '\x1b[33m',
  info:    '\x1b[36m',
};
const RESET = '\x1b[0m';

export function formatResult(result: LintResult, filename = '<script>'): string {
  if (result.clean) {
    return `\x1b[32m✓ ${filename} — no issues\x1b[0m`;
  }

  const lines: string[] = [`\x1b[1m${filename}\x1b[0m`];
  for (const issue of result.issues) {
    const c = SEVERITY_COLOR[issue.severity];
    lines.push(`  ${c}${issue.severity.toUpperCase()}${RESET} [${issue.code}] ${issue.message}`);
  }
  const summary: string[] = [];
  if (result.errors   > 0) summary.push(`\x1b[31m${result.errors} error(s)\x1b[0m`);
  if (result.warnings > 0) summary.push(`\x1b[33m${result.warnings} warning(s)\x1b[0m`);
  if (result.infos    > 0) summary.push(`\x1b[36m${result.infos} info(s)\x1b[0m`);
  lines.push(`\n  ${summary.join(', ')}`);
  return lines.join('\n');
}
