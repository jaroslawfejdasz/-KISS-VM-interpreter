export type TokenType = 'NUMBER' | 'HEX' | 'BOOLEAN' | 'STRING' | 'VARIABLE' | 'GLOBAL' | 'KEYWORD' | 'OPERATOR' | 'LPAREN' | 'RPAREN' | 'COMMA' | 'EOF';
export interface Token {
    type: TokenType;
    value: string;
    pos: number;
}
export declare const STATEMENT_KEYWORDS: Set<string>;
export declare const FUNCTION_KEYWORDS: Set<string>;
export declare const FUNCTION_ARITY: Record<string, [number, number]>;
export interface TokenizeResult {
    tokens: Token[];
    errors: LintError[];
}
export interface LintError {
    severity: 'error' | 'warning' | 'info';
    code: string;
    message: string;
    pos?: number;
}
export declare function tokenize(script: string): TokenizeResult;
//# sourceMappingURL=tokenizer.d.ts.map