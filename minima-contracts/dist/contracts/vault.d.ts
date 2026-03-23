import { ContractTemplate } from '../types';
/**
 * Vault Contract (with guardian)
 * Protects coins with a 2-layer security model:
 *   - Hot key: can initiate withdrawal, but funds are delayed
 *   - Cold key (guardian): can immediately cancel any pending withdrawal
 *
 * Pattern:
 *   1. Owner initiates withdrawal with hot key → coin enters "pending" state (STATE(1) = 1)
 *   2. After delayBlocks, owner can complete withdrawal
 *   3. Guardian can veto any withdrawal by signing immediately
 *
 * Use cases: self-custody with safety net, anti-theft, key recovery system.
 */
export declare const vault: ContractTemplate;
/**
 * Dead Man's Switch Contract
 * If owner doesn't "check in" (spend and recreate) within N blocks,
 * a designated beneficiary gains access.
 *
 * Use cases: inheritance, dead man's switch for sensitive data.
 */
export declare const deadMansSwitch: ContractTemplate;
//# sourceMappingURL=vault.d.ts.map