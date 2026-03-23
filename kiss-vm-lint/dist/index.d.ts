import { LintError } from './tokenizer';
import { Rule } from './rules';
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
export declare function lint(script: string, options?: LintOptions): LintResult;
export declare function lintOrThrow(script: string, options?: LintOptions): LintResult;
export { ALL_RULES } from './rules';
export * as rules from './rules';
export { lint as analyze };
//# sourceMappingURL=index.d.ts.map