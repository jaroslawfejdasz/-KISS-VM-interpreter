"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.TimeLock = TimeLock;
exports.CoinAgeLock = CoinAgeLock;
exports.TimeLockOrRefund = TimeLockOrRefund;
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
function TimeLock(ownerPublicKey, unlockBlock) {
    validatePubKey(ownerPublicKey);
    if (!Number.isInteger(unlockBlock) || unlockBlock < 0) {
        throw new Error('TimeLock: unlockBlock must be a non-negative integer');
    }
    return [
        `/* TimeLock: spendable after block ${unlockBlock} */`,
        `ASSERT @BLOCK GTE ${unlockBlock}`,
        `RETURN SIGNEDBY(${ownerPublicKey})`
    ].join('\n');
}
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
function CoinAgeLock(ownerPublicKey, minAge) {
    validatePubKey(ownerPublicKey);
    if (!Number.isInteger(minAge) || minAge < 1) {
        throw new Error('CoinAgeLock: minAge must be a positive integer (blocks)');
    }
    return [
        `/* CoinAgeLock: must hold for at least ${minAge} blocks */`,
        `ASSERT @COINAGE GTE ${minAge}`,
        `RETURN SIGNEDBY(${ownerPublicKey})`
    ].join('\n');
}
/**
 * TimeLockOrRefund — Coin goes to receiver if claimed in time, else refund to sender
 *
 * @param receiverPublicKey - Who can claim the coin
 * @param senderPublicKey   - Who gets refund if not claimed in time
 * @param deadline          - Last block where receiver can claim
 */
function TimeLockOrRefund(receiverPublicKey, senderPublicKey, deadline) {
    validatePubKey(receiverPublicKey);
    validatePubKey(senderPublicKey);
    if (!Number.isInteger(deadline) || deadline < 0) {
        throw new Error('TimeLockOrRefund: deadline must be a non-negative integer');
    }
    return [
        `/* TimeLockOrRefund: receiver can claim before block ${deadline}, sender can refund after */`,
        `IF @BLOCK LTE ${deadline} THEN`,
        `  RETURN SIGNEDBY(${receiverPublicKey})`,
        `ELSE`,
        `  RETURN SIGNEDBY(${senderPublicKey})`,
        `ENDIF`
    ].join('\n');
}
function validatePubKey(val) {
    if (!val.startsWith('0x'))
        throw new Error('Public key must start with 0x');
    if (val.length - 2 !== 64)
        throw new Error(`Public key must be 32 bytes (64 hex chars), got ${val.length - 2}`);
}
//# sourceMappingURL=TimeLock.js.map