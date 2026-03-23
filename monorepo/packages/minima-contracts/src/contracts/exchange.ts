import { ContractTemplate } from '../types';

/**
 * Exchange Contract (Atomic Token Swap)
 * Locks Minima coins that can only be claimed if a specific token output
 * is sent to the seller's address in the same transaction.
 *
 * This is Minima's native DEX primitive — no orderbook, no liquidity pool,
 * just a coin with conditions on the outputs of the spending transaction.
 *
 * Use cases: P2P token trading, on-chain DEX, OTC swaps.
 */
export const exchange: ContractTemplate = {
  name: 'exchange',
  description: [
    'Atomic token swap. Minima coins are locked until buyer sends exact tokenAmount',
    'of tokenId to sellerAddress in the same transaction.',
    'Native DEX primitive — no intermediary needed.',
  ].join(' '),
  params: [
    {
      name: 'sellerAddress',
      type: 'address',
      description: 'Address where the token payment must go',
    },
    {
      name: 'tokenId',
      type: 'hex',
      description: 'Token ID the seller wants to receive (0x00 for native Minima)',
    },
    {
      name: 'tokenAmount',
      type: 'number',
      description: 'Exact amount of tokens required',
    },
    {
      name: 'sellerPubKey',
      type: 'hex',
      description: 'Seller public key — only seller can cancel the order',
    },
  ],
  script({ sellerAddress, tokenId, tokenAmount, sellerPubKey }) {
    return [
      `/* Exchange Contract — Atomic Token Swap */`,
      `/* Anyone can trigger the swap if they pay the price */`,
      `LET paymentOk = VERIFYOUT(0, ${tokenAmount}, ${sellerAddress}, ${tokenId})`,
      ``,
      `IF paymentOk THEN`,
      `  RETURN TRUE`,
      `ENDIF`,
      ``,
      `/* Seller can cancel (reclaim coins) by signing */`,
      `RETURN SIGNEDBY(${sellerPubKey})`,
    ].join('\n');
  },
  example: {
    sellerAddress: '0xc0ffee00c0ffee00c0ffee00c0ffee00c0ffee00c0ffee00c0ffee00c0ffee00',
    tokenId: '0x' + '1234abcd'.repeat(8),
    tokenAmount: '1000',
    sellerPubKey: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
  },
};

/**
 * Simple Payment Contract
 * Coin can be spent only if output[0] goes to recipientAddress with exact amount.
 * Alternative: owner can reclaim.
 *
 * Use cases: conditional payment, escrow release.
 */
export const conditionalPayment: ContractTemplate = {
  name: 'conditional-payment',
  description: 'Coin released only when output[0] pays recipientAddress the exact amount. Owner can reclaim.',
  params: [
    {
      name: 'recipientAddress',
      type: 'address',
      description: 'Address that must receive the payment',
    },
    {
      name: 'amount',
      type: 'number',
      description: 'Exact amount that must be sent to recipient',
    },
    {
      name: 'ownerPubKey',
      type: 'hex',
      description: 'Owner public key (can reclaim)',
    },
  ],
  script({ recipientAddress, amount, ownerPubKey }) {
    return [
      `LET paymentOk = VERIFYOUT(0, ${amount}, ${recipientAddress}, 0x00)`,
      `RETURN paymentOk OR SIGNEDBY(${ownerPubKey})`,
    ].join('\n');
  },
  example: {
    recipientAddress: '0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef',
    amount: '500',
    ownerPubKey: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
  },
};
