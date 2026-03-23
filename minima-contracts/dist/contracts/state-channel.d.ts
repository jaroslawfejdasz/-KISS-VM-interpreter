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
export declare const stateChannel: ContractTemplate;
//# sourceMappingURL=state-channel.d.ts.map