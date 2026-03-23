"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BasicSigned = BasicSigned;
exports.MultiSig = MultiSig;
/**
 * BasicSigned — Standard single-owner contract
 *
 * The most fundamental pattern: coin can only be spent by the owner
 * who holds the private key corresponding to the given public key.
 *
 * Equivalent to Minima's default coin script.
 *
 * @param ownerPublicKey - 32-byte (64 hex char) Minima public key (0x prefixed)
 */
function BasicSigned(ownerPublicKey) {
    validateHex32(ownerPublicKey, 'ownerPublicKey');
    return `RETURN SIGNEDBY(${ownerPublicKey})`;
}
/**
 * MultiOwner — Requires M of N signatures
 *
 * @param required - Minimum number of signatures required
 * @param publicKeys - Array of 32-byte public keys
 */
function MultiSig(required, publicKeys) {
    if (publicKeys.length === 0)
        throw new Error('MultiSig: at least one public key required');
    if (required < 1)
        throw new Error('MultiSig: required must be >= 1');
    if (required > publicKeys.length)
        throw new Error(`MultiSig: required (${required}) cannot exceed key count (${publicKeys.length})`);
    publicKeys.forEach((k, i) => validateHex32(k, `publicKeys[${i}]`));
    const keys = publicKeys.join(', ');
    return `RETURN MULTISIG(${required}, ${keys})`;
}
function validateHex32(val, name) {
    if (!val.startsWith('0x'))
        throw new Error(`${name} must start with 0x`);
    const hexLen = val.length - 2;
    if (hexLen !== 64)
        throw new Error(`${name} must be 32 bytes (64 hex chars), got ${hexLen}`);
    if (!/^0x[0-9a-fA-F]+$/.test(val))
        throw new Error(`${name} contains invalid hex characters`);
}
//# sourceMappingURL=BasicSigned.js.map