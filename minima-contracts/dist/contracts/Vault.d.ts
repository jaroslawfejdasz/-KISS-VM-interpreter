/**
 * Vault — Spending rate-limited coin
 *
 * Allows the owner to spend only up to a certain amount per time window.
 * The rest must be returned to the vault script.
 *
 * Use cases:
 * - Cold wallet with daily spending limit
 * - Subscription services
 * - Allowance systems
 *
 * @param ownerPubKey      - Owner's public key
 * @param maxSpendPerBlock - Maximum amount spendable per block window
 * @param windowBlocks     - Size of the time window in blocks
 */
export declare function Vault(ownerPubKey: string, maxSpendPerBlock: number, windowBlocks: number): string;
/**
 * TwoOfThreeRecovery — 2-of-3 with time-based emergency recovery
 *
 * Normal spend: requires owner key.
 * If owner key is lost, after `recoveryDelay` blocks,
 * 2 of 3 recovery keys can reclaim the coin.
 *
 * @param ownerPubKey     - Primary owner key
 * @param recoveryKeys    - Three recovery key addresses (3 exactly)
 * @param recoveryDelay   - Blocks of inactivity before recovery is possible
 */
export declare function TwoOfThreeRecovery(ownerPubKey: string, recoveryKeys: [string, string, string], recoveryDelay: number): string;
//# sourceMappingURL=Vault.d.ts.map