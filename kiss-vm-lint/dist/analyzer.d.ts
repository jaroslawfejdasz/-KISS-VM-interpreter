import { LintError } from './tokenizer';
export interface AnalyzeResult {
    errors: LintError[];
    warnings: LintError[];
    infos: LintError[];
    all: LintError[];
    summary: {
        errors: number;
        warnings: number;
        infos: number;
        instructionEstimate: number;
    };
}
export declare function analyze(script: string): AnalyzeResult;
//# sourceMappingURL=analyzer.d.ts.map