import { ContractTemplate } from '../types';
/**
 * Time Lock Contract
 * Coin is locked until a specific block height.
 * Only spendable after @BLOCK >= unlockBlock AND signed by owner.
 *
 * Use cases: vesting schedules, delayed payments, savings accounts.
 */
export declare const timeLock: ContractTemplate;
/**
 * Coin Age Lock Contract
 * Coin is locked until it has been unspent for a minimum number of blocks.
 * Uses @COINAGE — the number of blocks since the coin was created.
 *
 * Use cases: HODLer rewards, anti-spam, staking-like patterns.
 */
export declare const coinAgeLock: ContractTemplate;
//# sourceMappingURL=time-lock.d.ts.map