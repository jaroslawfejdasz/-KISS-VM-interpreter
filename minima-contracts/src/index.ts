/**
 * minima-contracts
 * Ready-to-use KISS VM smart contract patterns for Minima blockchain
 *
 * Usage:
 *   import { contracts } from 'minima-contracts';
 *
 *   const compiled = contracts.compile('htlc', {
 *     recipientPubKey: '0x...',
 *     senderPubKey: '0x...',
 *     secretHash: '0x...',
 *     timeoutBlock: '2000000',
 *   });
 *
 *   console.log(compiled.script);    // KISS VM script
 *   console.log(compiled.scriptHash); // SHA3 hash — use as contract address
 */

export { contracts, MinimaContractLibrary } from './library';
export type { ContractTemplate, ContractParam, CompiledContract, ContractLibrary } from './types';

// Named contract templates (for direct import)
export { basicSigned } from './contracts/basic-signed';
export { timeLock, coinAgeLock } from './contracts/time-lock';
export { multisig2of2, multisig2of3, multisigMofN } from './contracts/multisig';
export { htlc } from './contracts/htlc';
export { exchange, conditionalPayment } from './contracts/exchange';
export { stateChannel } from './contracts/state-channel';
export { vault, deadMansSwitch } from './contracts/vault';
