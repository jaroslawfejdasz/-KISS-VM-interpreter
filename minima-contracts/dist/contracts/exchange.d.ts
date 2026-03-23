import { ContractTemplate } from '../types';
/**
 * Exchange Contract (Atomic Token Swap)
 * Locks Minima coins that can only be claimed if a specific token output
 * is sent to the seller's address in the same transaction.
 *
 * This is Minima's native DEX primitive — no orderbook, no liquidity pool,
 * just a coin with conditions on the outputs of the spending transaction.
 *
 * Use cases: P2P token trading, on-chain DEX, OTC swaps.
 */
export declare const exchange: ContractTemplate;
/**
 * Simple Payment Contract
 * Coin can be spent only if output[0] goes to recipientAddress with exact amount.
 * Alternative: owner can reclaim.
 *
 * Use cases: conditional payment, escrow release.
 */
export declare const conditionalPayment: ContractTemplate;
//# sourceMappingURL=exchange.d.ts.map