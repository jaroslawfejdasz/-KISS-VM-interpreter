export type TokenType = 'NUMBER' | 'HEX' | 'BOOLEAN' | 'STRING' | 'VARIABLE' | 'GLOBAL' | 'KEYWORD' | 'OPERATOR' | 'LPAREN' | 'RPAREN' | 'COMMA' | 'EOF';
export interface Token {
    type: TokenType;
    value: string;
}
export declare function tokenize(script: string): Token[];
//# sourceMappingURL=index.d.ts.map