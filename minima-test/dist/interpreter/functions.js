"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.callFunction = callFunction;
exports.functionExists = functionExists;
const values_1 = require("./values");
const crypto = __importStar(require("crypto"));
const FUNCTIONS = {
    // === CAST ===
    BOOL: ([a]) => values_1.MiniValue.boolean(a.asBoolean()),
    NUMBER: ([a]) => values_1.MiniValue.number(a.asNumber()),
    HEX: ([a]) => {
        if (a.type === 'HEX')
            return a;
        if (a.type === 'NUMBER')
            return values_1.MiniValue.hex(Math.floor(a.asNumber()).toString(16));
        if (a.type === 'STRING')
            return values_1.MiniValue.hex(Buffer.from(a.raw, 'utf8').toString('hex'));
        return values_1.MiniValue.hex(a.raw);
    },
    STRING: ([a]) => values_1.MiniValue.string(a.asString()),
    ASCII: ([a]) => {
        if (a.type === 'HEX') {
            const hex = a.raw.replace('0x', '');
            return values_1.MiniValue.string(Buffer.from(hex, 'hex').toString('ascii'));
        }
        return values_1.MiniValue.string(String.fromCharCode(a.asNumber()));
    },
    UTF8: ([a]) => {
        if (a.type === 'HEX') {
            const hex = a.raw.replace('0x', '');
            return values_1.MiniValue.string(Buffer.from(hex, 'hex').toString('utf8'));
        }
        return values_1.MiniValue.string(a.asString());
    },
    // === GENERAL ===
    ADDRESS: ([a]) => {
        if (a.type === 'HEX')
            return a;
        const hash = crypto.createHash('sha3-256').update(a.raw).digest('hex');
        return values_1.MiniValue.hex(hash);
    },
    EXISTS: ([a], env) => {
        if (a.type === 'VARIABLE')
            return values_1.MiniValue.boolean(env.hasVariable(a.raw));
        return values_1.MiniValue.boolean(true);
    },
    GET: (args) => {
        // GET(array, index1, index2...) - returns element from comma-separated string
        const arr = args[0].raw.split(',');
        const idx = args[1].asNumber();
        const val = arr[idx] ?? '';
        return values_1.MiniValue.string(val.trim());
    },
    FUNCTION: ([a], env) => {
        // FUNCTION(funcName, ...args) - check if function exists
        return values_1.MiniValue.boolean(FUNCTIONS[a.raw.toUpperCase()] !== undefined);
    },
    // === NUMBER MATH ===
    ABS: ([a]) => values_1.MiniValue.number(Math.abs(a.asNumber())),
    CEIL: ([a]) => values_1.MiniValue.number(Math.ceil(a.asNumber())),
    FLOOR: ([a]) => values_1.MiniValue.number(Math.floor(a.asNumber())),
    INC: ([a]) => values_1.MiniValue.number(a.asNumber() + 1),
    DEC: ([a]) => values_1.MiniValue.number(a.asNumber() - 1),
    MAX: ([a, b]) => values_1.MiniValue.number(Math.max(a.asNumber(), b.asNumber())),
    MIN: ([a, b]) => values_1.MiniValue.number(Math.min(a.asNumber(), b.asNumber())),
    POW: ([a, b]) => values_1.MiniValue.number(Math.pow(a.asNumber(), b.asNumber())),
    SQRT: ([a]) => values_1.MiniValue.number(Math.sqrt(a.asNumber())),
    SIGDIG: ([a, b]) => {
        const n = a.asNumber();
        const digits = b.asNumber();
        return values_1.MiniValue.number(parseFloat(n.toPrecision(digits)));
    },
    // === HEX ===
    LEN: ([a]) => {
        if (a.type === 'HEX')
            return values_1.MiniValue.number((a.raw.length - 2) / 2); // bytes
        return values_1.MiniValue.number(a.raw.length);
    },
    CONCAT: (args) => {
        const hexes = args.map(a => a.raw.replace('0x', ''));
        return values_1.MiniValue.hex(hexes.join(''));
    },
    SUBSET: ([a, start, len]) => {
        const hex = a.raw.replace('0x', '');
        const s = start.asNumber() * 2;
        const l = len.asNumber() * 2;
        return values_1.MiniValue.hex(hex.slice(s, s + l));
    },
    REV: ([a]) => {
        const hex = a.raw.replace('0x', '');
        const reversed = hex.match(/.{2}/g)?.reverse().join('') ?? '';
        return values_1.MiniValue.hex(reversed);
    },
    SETLEN: ([a, len]) => {
        const hex = a.raw.replace('0x', '');
        const target = len.asNumber() * 2;
        if (hex.length >= target)
            return values_1.MiniValue.hex(hex.slice(0, target));
        return values_1.MiniValue.hex(hex.padEnd(target, '0'));
    },
    OVERWRITE: ([dest, dstart, src, sstart, len]) => {
        let dhex = dest.raw.replace('0x', '').split('');
        const shex = src.raw.replace('0x', '');
        const ds = dstart.asNumber() * 2;
        const ss = sstart.asNumber() * 2;
        const l = len.asNumber() * 2;
        for (let i = 0; i < l; i++) {
            dhex[ds + i] = shex[ss + i] ?? '0';
        }
        return values_1.MiniValue.hex(dhex.join(''));
    },
    BITCOUNT: ([a]) => {
        const hex = a.raw.replace('0x', '');
        let count = 0;
        for (const c of hex)
            count += parseInt(c, 16).toString(2).split('1').length - 1;
        return values_1.MiniValue.number(count);
    },
    BITGET: ([a, pos]) => {
        const hex = a.raw.replace('0x', '');
        const num = BigInt('0x' + hex);
        const bit = (num >> BigInt(pos.asNumber())) & 1n;
        return values_1.MiniValue.boolean(bit === 1n);
    },
    BITSET: ([a, pos, val]) => {
        const hex = a.raw.replace('0x', '');
        let num = BigInt('0x' + hex);
        const p = BigInt(pos.asNumber());
        if (val.asBoolean())
            num |= (1n << p);
        else
            num &= ~(1n << p);
        return values_1.MiniValue.hex(num.toString(16));
    },
    // === STRING ===
    SUBSTR: ([a, start, len]) => {
        return values_1.MiniValue.string(a.raw.slice(start.asNumber(), start.asNumber() + len.asNumber()));
    },
    REPLACE: ([a, from, to]) => {
        return values_1.MiniValue.string(a.raw.split(from.raw).join(to.raw));
    },
    REPLACEFIRST: ([a, from, to]) => {
        return values_1.MiniValue.string(a.raw.replace(from.raw, to.raw));
    },
    // === SHA ===
    SHA2: ([a]) => {
        const data = a.type === 'HEX' ? Buffer.from(a.raw.replace('0x', ''), 'hex') : Buffer.from(a.raw, 'utf8');
        return values_1.MiniValue.hex(crypto.createHash('sha256').update(data).digest('hex'));
    },
    SHA3: ([a]) => {
        const data = a.type === 'HEX' ? Buffer.from(a.raw.replace('0x', ''), 'hex') : Buffer.from(a.raw, 'utf8');
        return values_1.MiniValue.hex(crypto.createHash('sha3-256').update(data).digest('hex'));
    },
    PROOF: ([_data, _proof, _root]) => {
        // MMR proof verification - simplified mock
        return values_1.MiniValue.boolean(false);
    },
    // === SIGNATURES ===
    SIGNEDBY: ([pubkey], env) => {
        const pk = pubkey.raw.toLowerCase();
        return values_1.MiniValue.boolean(env.signatures.some(s => s.toLowerCase() === pk));
    },
    CHECKSIG: ([pubkey, data, sig]) => {
        // In real impl verifies EC signature. Mock: always false unless explicitly mocked
        return values_1.MiniValue.boolean(false);
    },
    MULTISIG: (args, env) => {
        // MULTISIG(n, key1, key2, ...) - at least n of the given keys must have signed
        // Usage: MULTISIG(2, 0xpub1, 0xpub2, 0xpub3) = 2-of-3
        if (args.length < 2)
            throw new Error('MULTISIG requires at least 2 arguments');
        const required = args[0].asNumber();
        const keys = args.slice(1).map(a => a.raw.toLowerCase());
        let count = 0;
        for (const key of keys) {
            if (env.signatures.some(s => s.toLowerCase() === key))
                count++;
        }
        return values_1.MiniValue.boolean(count >= required);
    },
    // === STATE ===
    STATE: ([port], env) => {
        const p = port.asNumber();
        const sv = env.transaction?.stateVars?.[p];
        if (sv === undefined)
            throw new Error(`State variable ${p} not found`);
        return values_1.MiniValue.string(sv);
    },
    PREVSTATE: ([port], env) => {
        const p = port.asNumber();
        const sv = env.transaction?.prevStateVars?.[p];
        if (sv === undefined)
            throw new Error(`Previous state variable ${p} not found`);
        return values_1.MiniValue.string(sv);
    },
    SAMESTATE: ([from, to], env) => {
        const f = from.asNumber();
        const t = to.asNumber();
        for (let i = f; i <= t; i++) {
            const curr = env.transaction?.stateVars?.[i];
            const prev = env.transaction?.prevStateVars?.[i];
            if (curr !== prev)
                return values_1.MiniValue.boolean(false);
        }
        return values_1.MiniValue.boolean(true);
    },
    // === TXN INPUTS ===
    GETINADDR: ([idx], env) => {
        const coin = env.transaction?.inputs[idx.asNumber()];
        if (!coin)
            throw new Error(`Input ${idx.asNumber()} not found`);
        return values_1.MiniValue.hex(coin.address);
    },
    GETINAMT: ([idx], env) => {
        const coin = env.transaction?.inputs[idx.asNumber()];
        if (!coin)
            throw new Error(`Input ${idx.asNumber()} not found`);
        return values_1.MiniValue.number(coin.amount);
    },
    GETINID: ([idx], env) => {
        const coin = env.transaction?.inputs[idx.asNumber()];
        if (!coin)
            throw new Error(`Input ${idx.asNumber()} not found`);
        return values_1.MiniValue.hex(coin.coinId);
    },
    GETINTOK: ([idx], env) => {
        const coin = env.transaction?.inputs[idx.asNumber()];
        if (!coin)
            throw new Error(`Input ${idx.asNumber()} not found`);
        return values_1.MiniValue.hex(coin.tokenId);
    },
    SUMINPUTS: ([tokenId], env) => {
        const tid = tokenId.raw.toLowerCase();
        const sum = (env.transaction?.inputs ?? [])
            .filter(i => i.tokenId.toLowerCase() === tid)
            .reduce((acc, i) => acc + i.amount, 0);
        return values_1.MiniValue.number(sum);
    },
    VERIFYIN: ([idx, amount, address, tokenId], env) => {
        const coin = env.transaction?.inputs[idx.asNumber()];
        if (!coin)
            return values_1.MiniValue.boolean(false);
        return values_1.MiniValue.boolean(coin.amount >= amount.asNumber() &&
            coin.address.toLowerCase() === address.raw.toLowerCase() &&
            coin.tokenId.toLowerCase() === tokenId.raw.toLowerCase());
    },
    // === TXN OUTPUTS ===
    GETOUTADDR: ([idx], env) => {
        const out = env.transaction?.outputs[idx.asNumber()];
        if (!out)
            throw new Error(`Output ${idx.asNumber()} not found`);
        return values_1.MiniValue.hex(out.address);
    },
    GETOUTAMT: ([idx], env) => {
        const out = env.transaction?.outputs[idx.asNumber()];
        if (!out)
            throw new Error(`Output ${idx.asNumber()} not found`);
        return values_1.MiniValue.number(out.amount);
    },
    GETOUTTOK: ([idx], env) => {
        const out = env.transaction?.outputs[idx.asNumber()];
        if (!out)
            throw new Error(`Output ${idx.asNumber()} not found`);
        return values_1.MiniValue.hex(out.tokenId);
    },
    GETOUTKEEPSTATE: ([idx], env) => {
        const out = env.transaction?.outputs[idx.asNumber()];
        if (!out)
            throw new Error(`Output ${idx.asNumber()} not found`);
        return values_1.MiniValue.boolean(out.keepState ?? false);
    },
    SUMOUTPUTS: ([tokenId], env) => {
        const tid = tokenId.raw.toLowerCase();
        const sum = (env.transaction?.outputs ?? [])
            .filter(o => o.tokenId.toLowerCase() === tid)
            .reduce((acc, o) => acc + o.amount, 0);
        return values_1.MiniValue.number(sum);
    },
    VERIFYOUT: ([idx, amount, address, tokenId], env) => {
        const out = env.transaction?.outputs[idx.asNumber()];
        if (!out)
            return values_1.MiniValue.boolean(false);
        return values_1.MiniValue.boolean(out.amount >= amount.asNumber() &&
            out.address.toLowerCase() === address.raw.toLowerCase() &&
            out.tokenId.toLowerCase() === tokenId.raw.toLowerCase());
    },
};
function callFunction(name, args, env) {
    const fn = FUNCTIONS[name.toUpperCase()];
    if (!fn)
        throw new Error(`Unknown function: ${name}`);
    return fn(args, env);
}
function functionExists(name) {
    return FUNCTIONS[name.toUpperCase()] !== undefined;
}
//# sourceMappingURL=functions.js.map