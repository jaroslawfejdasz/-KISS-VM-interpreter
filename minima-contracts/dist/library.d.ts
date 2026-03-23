import { ContractTemplate, CompiledContract, ContractLibrary } from './types';
export declare class MinimaContractLibrary implements ContractLibrary {
    private registry;
    constructor(extraContracts?: ContractTemplate[]);
    get(name: string): ContractTemplate | undefined;
    list(): ContractTemplate[];
    compile(name: string, params: Record<string, string>): CompiledContract;
    /** Register a custom contract template */
    register(template: ContractTemplate): void;
}
/** Default singleton library instance */
export declare const contracts: MinimaContractLibrary;
//# sourceMappingURL=library.d.ts.map