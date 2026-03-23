/**
 * Exchange — Trustless atomic token swap
 *
 * Coin is only spendable if the transaction simultaneously sends
 * a specific amount of a specific token to a specific address.
 *
 * This enforces the "exchange" atomically — no trusted intermediary needed.
 *
 * Use cases:
 * - DEX (decentralized exchange) order
 * - OTC trade
 * - Token sale
 *
 * @param wantTokenId   - Token ID the seller wants to receive (0x00 = native Minima)
 * @param wantAmount    - Amount of wantToken required in exchange
 * @param sellerAddress - Address where the payment must be sent
 * @param ownerPubKey   - Seller's public key (for cancellation)
 */
export declare function Exchange(wantTokenId: string, wantAmount: number, sellerAddress: string, ownerPubKey: string): string;
/**
 * LimitOrder — Partial-fill capable exchange order
 *
 * Unlike basic Exchange, this allows partial fills.
 * The rate is enforced: if you take X of my coin, you must pay X * rate.
 *
 * @param wantTokenId   - Token to receive
 * @param rate          - Exchange rate: how much wantToken per 1 unit of this coin
 * @param sellerAddress - Where payment goes
 * @param ownerPubKey   - For cancellation
 */
export declare function LimitOrder(wantTokenId: string, rate: number, sellerAddress: string, ownerPubKey: string): string;
//# sourceMappingURL=Exchange.d.ts.map