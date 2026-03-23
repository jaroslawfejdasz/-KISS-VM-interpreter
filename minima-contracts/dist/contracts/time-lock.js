"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.coinAgeLock = exports.timeLock = void 0;
/**
 * Time Lock Contract
 * Coin is locked until a specific block height.
 * Only spendable after @BLOCK >= unlockBlock AND signed by owner.
 *
 * Use cases: vesting schedules, delayed payments, savings accounts.
 */
exports.timeLock = {
    name: 'time-lock',
    description: 'Coin locked until a specific block height. Perfect for vesting or scheduled payments.',
    params: [
        {
            name: 'ownerPubKey',
            type: 'hex',
            description: 'Public key of the coin owner',
        },
        {
            name: 'unlockBlock',
            type: 'number',
            description: 'Block height at which the coin becomes spendable',
        },
    ],
    script({ ownerPubKey, unlockBlock }) {
        return [
            `LET blockOk = (@BLOCK GTE ${unlockBlock})`,
            `LET signedOk = SIGNEDBY(${ownerPubKey})`,
            `RETURN blockOk AND signedOk`,
        ].join('\n');
    },
    example: {
        ownerPubKey: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
        unlockBlock: '1000000',
    },
};
/**
 * Coin Age Lock Contract
 * Coin is locked until it has been unspent for a minimum number of blocks.
 * Uses @COINAGE — the number of blocks since the coin was created.
 *
 * Use cases: HODLer rewards, anti-spam, staking-like patterns.
 */
exports.coinAgeLock = {
    name: 'coin-age-lock',
    description: 'Coin must be held for a minimum number of blocks before spending. Uses @COINAGE.',
    params: [
        {
            name: 'ownerPubKey',
            type: 'hex',
            description: 'Public key of the coin owner',
        },
        {
            name: 'minAge',
            type: 'number',
            description: 'Minimum number of blocks the coin must have been unspent',
        },
    ],
    script({ ownerPubKey, minAge }) {
        return [
            `LET ageOk = (@COINAGE GTE ${minAge})`,
            `LET signedOk = SIGNEDBY(${ownerPubKey})`,
            `RETURN ageOk AND signedOk`,
        ].join('\n');
    },
    example: {
        ownerPubKey: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
        minAge: '100',
    },
};
//# sourceMappingURL=time-lock.js.map