"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.htlc = void 0;
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
exports.htlc = {
    name: 'htlc',
    description: [
        'Hash Time Locked Contract (HTLC). Two paths:',
        '(1) Claim: recipient reveals preimage of secretHash before timeout.',
        '(2) Refund: sender reclaims after timeout block.',
        'Foundation of atomic swaps and payment channels.',
    ].join(' '),
    params: [
        {
            name: 'recipientPubKey',
            type: 'hex',
            description: 'Public key of the recipient (claim path)',
        },
        {
            name: 'senderPubKey',
            type: 'hex',
            description: 'Public key of the sender (refund path)',
        },
        {
            name: 'secretHash',
            type: 'hex',
            description: 'SHA3 hash of the secret preimage (32 bytes hex)',
        },
        {
            name: 'timeoutBlock',
            type: 'number',
            description: 'Block height after which sender can refund',
        },
    ],
    script({ recipientPubKey, senderPubKey, secretHash, timeoutBlock }) {
        return [
            `/* HTLC — Hash Time Locked Contract */`,
            `/* Claim path: recipient reveals preimage */`,
            `LET preimage = STATE(1)`,
            `LET hashOk = (SHA3(preimage) EQ ${secretHash})`,
            `LET recipientSig = SIGNEDBY(${recipientPubKey})`,
            ``,
            `IF hashOk AND recipientSig THEN`,
            `  RETURN TRUE`,
            `ENDIF`,
            ``,
            `/* Refund path: sender reclaims after timeout */`,
            `LET timedOut = (@BLOCK GTE ${timeoutBlock})`,
            `LET senderSig = SIGNEDBY(${senderPubKey})`,
            ``,
            `RETURN timedOut AND senderSig`,
        ].join('\n');
    },
    example: {
        recipientPubKey: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
        senderPubKey: '0x1122334411223344112233441122334411223344112233441122334411223344',
        secretHash: '0x' + 'a1b2c3d4'.repeat(8),
        timeoutBlock: '2000000',
    },
};
//# sourceMappingURL=htlc.js.map