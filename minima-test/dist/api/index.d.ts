import { Environment } from '../interpreter/environment';
import { MiniValue } from '../interpreter/values';
import { MockTransaction } from '../mock/transaction';
export interface TestResult {
    name: string;
    passed: boolean;
    error?: string;
    duration: number;
}
export interface SuiteResult {
    name: string;
    tests: TestResult[];
    passed: number;
    failed: number;
    duration: number;
}
export declare function describe(name: string, fn: () => void): void;
export declare function it(name: string, fn: () => void): void;
export declare const test: typeof it;
export interface RunScriptOptions {
    transaction?: Partial<MockTransaction>;
    inputIndex?: number;
    signatures?: string[];
    variables?: Record<string, MiniValue>;
    globals?: Record<string, MiniValue>;
    mastScripts?: Record<string, string>;
}
export declare function runScript(script: string, options?: RunScriptOptions): ScriptResult;
export interface ScriptResult {
    success: boolean;
    passed: boolean;
    failed: boolean;
    error: string | null;
    instructions: number;
    duration: number;
    env: Environment;
}
export declare class AssertionError extends Error {
    constructor(msg: string);
}
declare class Expect {
    private val;
    constructor(val: ScriptResult | any);
    toPass(): this;
    toFail(): this;
    toThrow(message?: string): this;
    toBe(expected: any): this;
    toEqual(expected: any): this;
    toBeWithinInstructions(max: number): this;
}
export declare function expect(val: ScriptResult | any): Expect;
export declare function runSuites(): Promise<SuiteResult[]>;
export { MiniValue } from '../interpreter/values';
export { defaultTransaction } from '../mock/transaction';
export type { MockTransaction, MockCoin, MockOutput } from '../mock/transaction';
//# sourceMappingURL=index.d.ts.map