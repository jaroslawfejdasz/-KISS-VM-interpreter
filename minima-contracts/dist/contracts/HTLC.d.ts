/**
 * HTLC — Hash Time Lock Contract
 *
 * Enables trustless atomic swaps across chains or between parties.
 *
 * Flow:
 * 1. Alice locks coin with HTLC using SHA2(secret) as hashlock
 * 2. Bob can claim by revealing the secret (preimage of the hash)
 * 3. If Bob doesn't claim before `refundBlock`, Alice gets refund
 *
 * @param receiverPublicKey - Bob's public key (who can claim with secret)
 * @param senderPublicKey   - Alice's public key (who gets refund after timeout)
 * @param hashlock          - SHA2 or SHA3 hash of the secret (0x prefixed, 32 bytes)
 * @param refundBlock       - Block after which sender can reclaim
 * @param hashFn            - Which hash function was used: 'SHA2' or 'SHA3'
 */
export declare function HTLC(receiverPublicKey: string, senderPublicKey: string, hashlock: string, refundBlock: number, hashFn?: 'SHA2' | 'SHA3'): string;
//# sourceMappingURL=HTLC.d.ts.map