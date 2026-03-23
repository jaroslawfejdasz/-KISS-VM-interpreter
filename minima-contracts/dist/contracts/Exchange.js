"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Exchange = Exchange;
exports.LimitOrder = LimitOrder;
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
function Exchange(wantTokenId, wantAmount, sellerAddress, ownerPubKey) {
    validateHex(wantTokenId, 'wantTokenId');
    if (wantAmount <= 0)
        throw new Error('Exchange: wantAmount must be > 0');
    validateHex(sellerAddress, 'sellerAddress');
    validatePubKey(ownerPubKey);
    return [
        `/* Exchange: want ${wantAmount} of token ${wantTokenId} to ${sellerAddress} */`,
        `/* Anyone can execute this swap, only owner can cancel */`,
        `IF SIGNEDBY(${ownerPubKey}) THEN`,
        `  /* Owner can always cancel the order */`,
        `  RETURN TRUE`,
        `ENDIF`,
        `/* Verify the correct payment output exists */`,
        `RETURN VERIFYOUT(@OUTCOUNT-1, ${sellerAddress}, ${wantAmount}, ${wantTokenId})`
    ].join('\n');
}
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
function LimitOrder(wantTokenId, rate, sellerAddress, ownerPubKey) {
    validateHex(wantTokenId, 'wantTokenId');
    if (rate <= 0)
        throw new Error('LimitOrder: rate must be > 0');
    validateHex(sellerAddress, 'sellerAddress');
    validatePubKey(ownerPubKey);
    return [
        `/* LimitOrder: rate=${rate} per unit, token=${wantTokenId} */`,
        `IF SIGNEDBY(${ownerPubKey}) THEN`,
        `  RETURN TRUE`,
        `ENDIF`,
        `/* How much of this coin is being taken? */`,
        `LET amountTaken = @AMOUNT - GETOUTAMT(0)`,
        `/* How much must be paid? */`,
        `LET requiredPayment = amountTaken * ${rate}`,
        `/* Check payment output */`,
        `ASSERT VERIFYOUT(@OUTCOUNT-1, ${sellerAddress}, requiredPayment, ${wantTokenId})`,
        `/* Remaining coin must come back to same script (partial fill) */`,
        `RETURN SAMESTATE(0, 4)`
    ].join('\n');
}
function validateHex(val, name) {
    if (!val.startsWith('0x'))
        throw new Error(`${name} must start with 0x`);
    if (!/^0x[0-9a-fA-F]+$/.test(val))
        throw new Error(`${name} contains invalid hex`);
}
function validatePubKey(val) {
    if (!val.startsWith('0x'))
        throw new Error('ownerPubKey must start with 0x');
    if (val.length - 2 !== 64)
        throw new Error('ownerPubKey must be 32 bytes (64 hex chars)');
}
//# sourceMappingURL=Exchange.js.map