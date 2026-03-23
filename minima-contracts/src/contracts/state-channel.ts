import { ContractTemplate } from '../types';

/**
 * State Channel Contract
 * Foundation for payment channel / state channel patterns.
 *
 * Coins in the channel can be updated by mutual agreement (both sign a new state).
 * Either party can close the channel by submitting the latest signed state.
 * If one party disappears, the other can force-close after a timeout.
 *
 * State variable layout (used via STATE(n)):
 *   STATE(1) = channel nonce (incremented on each update)
 *   STATE(2) = balance party A
 *   STATE(3) = balance party B
 *
 * Use cases: Off-chain micropayments, game state, streaming payments.
 * Note: Full implementation requires Omnia (L2) for off-chain messaging.
 */
export const stateChannel: ContractTemplate = {
  name: 'state-channel',
  description: [
    'Payment channel between two parties. Both can update state off-chain,',
    'either can close with latest state. Force-close after timeout if one disappears.',
    'Foundation for Omnia L2 payment channels.',
  ].join(' '),
  params: [
    {
      name: 'pubKeyA',
      type: 'hex',
      description: 'Public key of party A',
    },
    {
      name: 'pubKeyB',
      type: 'hex',
      description: 'Public key of party B',
    },
    {
      name: 'timeoutBlocks',
      type: 'number',
      description: 'Blocks to wait for force-close (e.g. 1440 = ~1 day)',
      default: '1440',
    },
  ],
  script({ pubKeyA, pubKeyB, timeoutBlocks }) {
    return [
      `/* State Channel Contract */`,
      `/* Both parties must agree to update or close */`,
      ``,
      `/* Cooperative close: both sign the final state */`,
      `LET bothSigned = MULTISIG(2, ${pubKeyA}, ${pubKeyB})`,
      `IF bothSigned THEN`,
      `  RETURN TRUE`,
      `ENDIF`,
      ``,
      `/* Force-close: one party after timeout, with latest nonce */`,
      `/* Higher nonce = newer state (prevents replay of old states) */`,
      `LET currentNonce = STATE(1)`,
      `LET prevNonce = PREVSTATE(1)`,
      `LET nonceOk = (currentNonce GTE prevNonce)`,
      `LET timedOut = (@COINAGE GTE ${timeoutBlocks})`,
      `LET signedByEither = (SIGNEDBY(${pubKeyA}) OR SIGNEDBY(${pubKeyB}))`,
      ``,
      `RETURN nonceOk AND timedOut AND signedByEither`,
    ].join('\n');
  },
  example: {
    pubKeyA: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
    pubKeyB: '0x1122334411223344112233441122334411223344112233441122334411223344',
    timeoutBlocks: '1440',
  },
};
