import { MiniValue } from '../interpreter/values';
export interface MockCoin {
    coinId: string;
    address: string;
    amount: number;
    tokenId: string;
    blockCreated: number;
    stateVars?: Record<number, string>;
}
export interface MockOutput {
    address: string;
    amount: number;
    tokenId: string;
    keepState?: boolean;
}
export interface MockTransaction {
    inputs: MockCoin[];
    outputs: MockOutput[];
    blockNumber: number;
    blockTime: number;
    signatures: string[];
    stateVars?: Record<number, string>;
    prevStateVars?: Record<number, string>;
}
export declare function buildGlobals(tx: MockTransaction, inputIndex: number): Record<string, MiniValue>;
export declare function defaultTransaction(overrides?: Partial<MockTransaction>): MockTransaction;
//# sourceMappingURL=transaction.d.ts.map