import { ContractTemplate } from '../types';
/**
 * Basic Signed Contract
 * The simplest possible contract — only the owner (possessing the private key)
 * can spend the coin. Equivalent to a standard P2PKH in Bitcoin.
 *
 * KISS VM: RETURN SIGNEDBY(ownerPubKey)
 */
export declare const basicSigned: ContractTemplate;
//# sourceMappingURL=basic-signed.d.ts.map