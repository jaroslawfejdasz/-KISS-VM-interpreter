import { ContractTemplate } from '../types';
/**
 * HTLC — Hash Time Locked Contract
 * Classic atomic swap primitive.
 *
 * Two spending paths:
 *   1. CLAIM: recipient provides preimage R where SHA3(R) == secretHash, signed by recipient
 *   2. REFUND: after timeoutBlock passes, sender can reclaim funds (signed by sender)
 *
 * Use cases: cross-chain atomic swaps, payment channels, conditional payments.
 */
export declare const htlc: ContractTemplate;
//# sourceMappingURL=htlc.d.ts.map