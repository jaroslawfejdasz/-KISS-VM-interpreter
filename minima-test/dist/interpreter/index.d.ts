import { Environment } from './environment';
export declare class KissVMInterpreter {
    private tokens;
    private pos;
    private env;
    constructor(env: Environment);
    private peek;
    private advance;
    private expect;
    private match;
    run(script: string): boolean;
    private executeBlock;
    private executeStatement;
    private executeLet;
    private executeIf;
    private skipElseBranches;
    private skipBlock;
    private executeWhile;
    private executeReturn;
    private executeAssert;
    private executeExec;
    private executeMast;
    private tryGetMastScript;
    private evaluateExpression;
    private parseOr;
    private parseAnd;
    private parseNot;
    private parseComparison;
    private parseAddSub;
    private parseMulDiv;
    private parseUnary;
    private parsePrimary;
}
//# sourceMappingURL=index.d.ts.map