"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.HTLC = HTLC;
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
function HTLC(receiverPublicKey, senderPublicKey, hashlock, refundBlock, hashFn = 'SHA2') {
    validatePubKey(receiverPublicKey, 'receiverPublicKey');
    validatePubKey(senderPublicKey, 'senderPublicKey');
    validateHash(hashlock);
    if (!Number.isInteger(refundBlock) || refundBlock < 0) {
        throw new Error('HTLC: refundBlock must be a non-negative integer');
    }
    if (hashFn !== 'SHA2' && hashFn !== 'SHA3') {
        throw new Error("HTLC: hashFn must be 'SHA2' or 'SHA3'");
    }
    return [
        `/* HTLC: hashlock=${hashlock} refundBlock=${refundBlock} */`,
        `IF @BLOCK LT ${refundBlock} THEN`,
        `  /* Receiver path: must reveal preimage and sign */`,
        `  LET preimage = STATE(0)`,
        `  ASSERT ${hashFn}(preimage) EQ ${hashlock}`,
        `  RETURN SIGNEDBY(${receiverPublicKey})`,
        `ELSE`,
        `  /* Refund path: only sender can reclaim after timeout */`,
        `  RETURN SIGNEDBY(${senderPublicKey})`,
        `ENDIF`
    ].join('\n');
}
function validatePubKey(val, name) {
    if (!val.startsWith('0x'))
        throw new Error(`${name} must start with 0x`);
    if (val.length - 2 !== 64)
        throw new Error(`${name} must be 32 bytes (64 hex chars)`);
}
function validateHash(val) {
    if (!val.startsWith('0x'))
        throw new Error('hashlock must start with 0x');
    if (val.length - 2 !== 64)
        throw new Error('hashlock must be 32 bytes (64 hex chars) — use SHA2 or SHA3 output');
}
//# sourceMappingURL=HTLC.js.map