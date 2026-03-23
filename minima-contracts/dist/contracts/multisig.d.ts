import { ContractTemplate } from '../types';
/**
 * 2-of-2 MultiSig Contract
 * Requires signatures from BOTH parties to spend.
 * Use cases: joint accounts, escrow requiring both parties.
 */
export declare const multisig2of2: ContractTemplate;
/**
 * 2-of-3 MultiSig Contract
 * Requires any 2 of 3 signatures. Classic threshold signature.
 * Use cases: treasury, DAO governance, 3-party escrow.
 */
export declare const multisig2of3: ContractTemplate;
/**
 * M-of-N MultiSig Contract (generic)
 * Flexible threshold: requires M of N signatures.
 */
export declare const multisigMofN: ContractTemplate;
//# sourceMappingURL=multisig.d.ts.map