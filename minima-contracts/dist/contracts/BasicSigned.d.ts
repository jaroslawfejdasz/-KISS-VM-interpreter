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
export declare function BasicSigned(ownerPublicKey: string): string;
/**
 * MultiOwner — Requires M of N signatures
 *
 * @param required - Minimum number of signatures required
 * @param publicKeys - Array of 32-byte public keys
 */
export declare function MultiSig(required: number, publicKeys: string[]): string;
//# sourceMappingURL=BasicSigned.d.ts.map