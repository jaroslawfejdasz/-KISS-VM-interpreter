/**
 * TimeLock — Coin locked until a specific block
 *
 * Coin cannot be spent until the blockchain reaches `unlockBlock`.
 * After that, only the owner can spend it.
 *
 * Use cases:
 * - Vesting schedules
 * - Delayed payments
 * - Escrow with time limit
 *
 * @param ownerPublicKey - 32-byte owner public key
 * @param unlockBlock    - Block number after which coin is spendable
 */
export declare function TimeLock(ownerPublicKey: string, unlockBlock: number): string;
/**
 * CoingeLock — Coin locked until it reaches a certain age (blocks since creation)
 *
 * Uses @COINAGE — the number of blocks since the coin was created.
 * Great for HODLer contracts where you want to enforce holding periods
 * without knowing the absolute block number at contract creation time.
 *
 * @param ownerPublicKey - 32-byte owner public key
 * @param minAge         - Minimum number of blocks the coin must be held
 */
export declare function CoinAgeLock(ownerPublicKey: string, minAge: number): string;
/**
 * TimeLockOrRefund — Coin goes to receiver if claimed in time, else refund to sender
 *
 * @param receiverPublicKey - Who can claim the coin
 * @param senderPublicKey   - Who gets refund if not claimed in time
 * @param deadline          - Last block where receiver can claim
 */
export declare function TimeLockOrRefund(receiverPublicKey: string, senderPublicKey: string, deadline: number): string;
//# sourceMappingURL=TimeLock.d.ts.map