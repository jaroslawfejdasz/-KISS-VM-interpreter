"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.basicSigned = void 0;
/**
 * Basic Signed Contract
 * The simplest possible contract — only the owner (possessing the private key)
 * can spend the coin. Equivalent to a standard P2PKH in Bitcoin.
 *
 * KISS VM: RETURN SIGNEDBY(ownerPubKey)
 */
exports.basicSigned = {
    name: 'basic-signed',
    description: 'Standard ownership contract. Coin can only be spent by the owner\'s key.',
    params: [
        {
            name: 'ownerPubKey',
            type: 'hex',
            description: 'Public key of the coin owner (64-byte hex)',
        },
    ],
    script({ ownerPubKey }) {
        return `RETURN SIGNEDBY(${ownerPubKey})`;
    },
    example: {
        ownerPubKey: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
    },
};
//# sourceMappingURL=basic-signed.js.map