"use strict";
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
Object.defineProperty(exports, "__esModule", { value: true });
exports.deadMansSwitch = exports.vault = exports.stateChannel = exports.conditionalPayment = exports.exchange = exports.htlc = exports.multisigMofN = exports.multisig2of3 = exports.multisig2of2 = exports.coinAgeLock = exports.timeLock = exports.basicSigned = exports.MinimaContractLibrary = exports.contracts = void 0;
var library_1 = require("./library");
Object.defineProperty(exports, "contracts", { enumerable: true, get: function () { return library_1.contracts; } });
Object.defineProperty(exports, "MinimaContractLibrary", { enumerable: true, get: function () { return library_1.MinimaContractLibrary; } });
// Named contract templates (for direct import)
var basic_signed_1 = require("./contracts/basic-signed");
Object.defineProperty(exports, "basicSigned", { enumerable: true, get: function () { return basic_signed_1.basicSigned; } });
var time_lock_1 = require("./contracts/time-lock");
Object.defineProperty(exports, "timeLock", { enumerable: true, get: function () { return time_lock_1.timeLock; } });
Object.defineProperty(exports, "coinAgeLock", { enumerable: true, get: function () { return time_lock_1.coinAgeLock; } });
var multisig_1 = require("./contracts/multisig");
Object.defineProperty(exports, "multisig2of2", { enumerable: true, get: function () { return multisig_1.multisig2of2; } });
Object.defineProperty(exports, "multisig2of3", { enumerable: true, get: function () { return multisig_1.multisig2of3; } });
Object.defineProperty(exports, "multisigMofN", { enumerable: true, get: function () { return multisig_1.multisigMofN; } });
var htlc_1 = require("./contracts/htlc");
Object.defineProperty(exports, "htlc", { enumerable: true, get: function () { return htlc_1.htlc; } });
var exchange_1 = require("./contracts/exchange");
Object.defineProperty(exports, "exchange", { enumerable: true, get: function () { return exchange_1.exchange; } });
Object.defineProperty(exports, "conditionalPayment", { enumerable: true, get: function () { return exchange_1.conditionalPayment; } });
var state_channel_1 = require("./contracts/state-channel");
Object.defineProperty(exports, "stateChannel", { enumerable: true, get: function () { return state_channel_1.stateChannel; } });
var vault_1 = require("./contracts/vault");
Object.defineProperty(exports, "vault", { enumerable: true, get: function () { return vault_1.vault; } });
Object.defineProperty(exports, "deadMansSwitch", { enumerable: true, get: function () { return vault_1.deadMansSwitch; } });
//# sourceMappingURL=index.js.map