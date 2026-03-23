"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.contracts = exports.MinimaContractLibrary = void 0;
const node_crypto_1 = __importDefault(require("node:crypto"));
const basic_signed_1 = require("./contracts/basic-signed");
const time_lock_1 = require("./contracts/time-lock");
const multisig_1 = require("./contracts/multisig");
const htlc_1 = require("./contracts/htlc");
const exchange_1 = require("./contracts/exchange");
const state_channel_1 = require("./contracts/state-channel");
const vault_1 = require("./contracts/vault");
const ALL_CONTRACTS = [
    basic_signed_1.basicSigned,
    time_lock_1.timeLock,
    time_lock_1.coinAgeLock,
    multisig_1.multisig2of2,
    multisig_1.multisig2of3,
    multisig_1.multisigMofN,
    htlc_1.htlc,
    exchange_1.exchange,
    exchange_1.conditionalPayment,
    state_channel_1.stateChannel,
    vault_1.vault,
    vault_1.deadMansSwitch,
];
function sha3(data) {
    return '0x' + node_crypto_1.default.createHash('sha3-256').update(data, 'utf8').digest('hex');
}
function validateParams(template, params) {
    for (const p of template.params) {
        if (!(p.name in params)) {
            if (p.default !== undefined) {
                params[p.name] = p.default;
            }
            else {
                throw new Error(`Missing required param '${p.name}' for contract '${template.name}'`);
            }
        }
        // Type validation
        const val = params[p.name];
        if (p.type === 'hex' && !val.startsWith('0x')) {
            throw new Error(`Param '${p.name}' must be a hex value (start with 0x), got: '${val}'`);
        }
        if (p.type === 'address' && !val.startsWith('0x')) {
            throw new Error(`Param '${p.name}' must be an address (hex starting with 0x), got: '${val}'`);
        }
        if (p.type === 'number' && isNaN(Number(val))) {
            throw new Error(`Param '${p.name}' must be a number, got: '${val}'`);
        }
    }
}
class MinimaContractLibrary {
    constructor(extraContracts = []) {
        this.registry = new Map();
        for (const c of [...ALL_CONTRACTS, ...extraContracts]) {
            this.registry.set(c.name, c);
        }
    }
    get(name) {
        return this.registry.get(name);
    }
    list() {
        return Array.from(this.registry.values());
    }
    compile(name, params) {
        const template = this.registry.get(name);
        if (!template) {
            throw new Error(`Contract '${name}' not found. Available: ${Array.from(this.registry.keys()).join(', ')}`);
        }
        // Fill defaults and validate
        const resolvedParams = { ...params };
        validateParams(template, resolvedParams);
        const script = template.script(resolvedParams);
        const scriptHash = sha3(script);
        return {
            name,
            script,
            params: resolvedParams,
            scriptHash,
        };
    }
    /** Register a custom contract template */
    register(template) {
        if (this.registry.has(template.name)) {
            throw new Error(`Contract '${template.name}' already registered. Use a unique name.`);
        }
        this.registry.set(template.name, template);
    }
}
exports.MinimaContractLibrary = MinimaContractLibrary;
/** Default singleton library instance */
exports.contracts = new MinimaContractLibrary();
//# sourceMappingURL=library.js.map