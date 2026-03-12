import { MiniValue } from './values';
import { MockTransaction } from '../mock/transaction';
export declare class Environment {
    private variables;
    private globals;
    signatures: string[];
    transaction: MockTransaction | null;
    inputIndex: number;
    returnValue: MiniValue | null;
    returned: boolean;
    instructionCount: number;
    readonly MAX_INSTRUCTIONS = 1024;
    stackDepth: number;
    readonly MAX_STACK_DEPTH = 64;
    setGlobal(name: string, value: MiniValue): void;
    setVariable(name: string, value: MiniValue): void;
    getVariable(name: string): MiniValue;
    hasVariable(name: string): boolean;
    tick(): void;
    pushStack(): void;
    popStack(): void;
}
//# sourceMappingURL=environment.d.ts.map