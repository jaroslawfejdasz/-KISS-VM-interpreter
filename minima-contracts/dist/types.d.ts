/**
 * Core types for minima-contracts
 */
export interface ContractTemplate {
    /** Contract name, e.g. "basic-signed" */
    name: string;
    /** Human-readable description */
    description: string;
    /** Parameters required to instantiate this contract */
    params: ContractParam[];
    /** Generate the KISS VM script from params */
    script(params: Record<string, string>): string;
    /** Example usage */
    example: Record<string, string>;
}
export interface ContractParam {
    name: string;
    type: 'hex' | 'number' | 'string' | 'address';
    description: string;
    /** Optional default value */
    default?: string;
}
export interface CompiledContract {
    name: string;
    script: string;
    params: Record<string, string>;
    /** SHA3 hash of the script — used as contract address in Minima */
    scriptHash: string;
}
export interface ContractLibrary {
    get(name: string): ContractTemplate | undefined;
    list(): ContractTemplate[];
    compile(name: string, params: Record<string, string>): CompiledContract;
}
//# sourceMappingURL=types.d.ts.map