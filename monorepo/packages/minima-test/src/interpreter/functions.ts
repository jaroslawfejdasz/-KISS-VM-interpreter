import { MiniValue } from './values';
import { Environment } from './environment';
import * as crypto from 'crypto';

type FnImpl = (args: MiniValue[], env: Environment) => MiniValue;

const FUNCTIONS: Record<string, FnImpl> = {
  // === CAST ===
  BOOL: ([a]) => MiniValue.boolean(a.asBoolean()),
  NUMBER: ([a]) => MiniValue.number(a.asNumber()),
  HEX: ([a]) => {
    if (a.type === 'HEX') return a;
    if (a.type === 'NUMBER') return MiniValue.hex(Math.floor(a.asNumber()).toString(16));
    if (a.type === 'STRING') return MiniValue.hex(Buffer.from(a.raw, 'utf8').toString('hex'));
    return MiniValue.hex(a.raw);
  },
  STRING: ([a]) => MiniValue.string(a.asString()),
  ASCII: ([a]) => {
    if (a.type === 'HEX') {
      const hex = a.raw.replace('0x', '');
      return MiniValue.string(Buffer.from(hex, 'hex').toString('ascii'));
    }
    return MiniValue.string(String.fromCharCode(a.asNumber()));
  },
  UTF8: ([a]) => {
    if (a.type === 'HEX') {
      const hex = a.raw.replace('0x', '');
      return MiniValue.string(Buffer.from(hex, 'hex').toString('utf8'));
    }
    return MiniValue.string(a.asString());
  },

  // === GENERAL ===
  ADDRESS: ([a]) => {
    if (a.type === 'HEX') return a;
    const hash = crypto.createHash('sha3-256').update(a.raw).digest('hex');
    return MiniValue.hex(hash);
  },
  EXISTS: ([a], env) => {
    if (a.type === 'VARIABLE' as any) return MiniValue.boolean(env.hasVariable(a.raw));
    return MiniValue.boolean(true);
  },
  GET: (args) => {
    // GET(array, index1, index2...) - returns element from comma-separated string
    const arr = args[0].raw.split(',');
    const idx = args[1].asNumber();
    const val = arr[idx] ?? '';
    return MiniValue.string(val.trim());
  },
  FUNCTION: ([a], env) => {
    // FUNCTION(funcName, ...args) - check if function exists
    return MiniValue.boolean(FUNCTIONS[a.raw.toUpperCase()] !== undefined);
  },

  // === NUMBER MATH ===
  ABS: ([a]) => MiniValue.number(Math.abs(a.asNumber())),
  CEIL: ([a]) => MiniValue.number(Math.ceil(a.asNumber())),
  FLOOR: ([a]) => MiniValue.number(Math.floor(a.asNumber())),
  INC: ([a]) => MiniValue.number(a.asNumber() + 1),
  DEC: ([a]) => MiniValue.number(a.asNumber() - 1),
  MAX: ([a, b]) => MiniValue.number(Math.max(a.asNumber(), b.asNumber())),
  MIN: ([a, b]) => MiniValue.number(Math.min(a.asNumber(), b.asNumber())),
  POW: ([a, b]) => MiniValue.number(Math.pow(a.asNumber(), b.asNumber())),
  SQRT: ([a]) => MiniValue.number(Math.sqrt(a.asNumber())),
  SIGDIG: ([a, b]) => {
    const n = a.asNumber();
    const digits = b.asNumber();
    return MiniValue.number(parseFloat(n.toPrecision(digits)));
  },

  // === HEX ===
  LEN: ([a]) => {
    if (a.type === 'HEX') return MiniValue.number((a.raw.length - 2) / 2); // bytes
    return MiniValue.number(a.raw.length);
  },
  CONCAT: (args) => {
    const hexes = args.map(a => a.raw.replace('0x', ''));
    return MiniValue.hex(hexes.join(''));
  },
  SUBSET: ([a, start, len]) => {
    const hex = a.raw.replace('0x', '');
    const s = start.asNumber() * 2;
    const l = len.asNumber() * 2;
    return MiniValue.hex(hex.slice(s, s + l));
  },
  REV: ([a]) => {
    const hex = a.raw.replace('0x', '');
    const reversed = hex.match(/.{2}/g)?.reverse().join('') ?? '';
    return MiniValue.hex(reversed);
  },
  SETLEN: ([a, len]) => {
    const hex = a.raw.replace('0x', '');
    const target = len.asNumber() * 2;
    if (hex.length >= target) return MiniValue.hex(hex.slice(0, target));
    return MiniValue.hex(hex.padEnd(target, '0'));
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
    return MiniValue.hex(dhex.join(''));
  },
  BITCOUNT: ([a]) => {
    const hex = a.raw.replace('0x', '');
    let count = 0;
    for (const c of hex) count += parseInt(c, 16).toString(2).split('1').length - 1;
    return MiniValue.number(count);
  },
  BITGET: ([a, pos]) => {
    const hex = a.raw.replace('0x', '');
    const num = BigInt('0x' + hex);
    const bit = (num >> BigInt(pos.asNumber())) & 1n;
    return MiniValue.boolean(bit === 1n);
  },
  BITSET: ([a, pos, val]) => {
    const hex = a.raw.replace('0x', '');
    let num = BigInt('0x' + hex);
    const p = BigInt(pos.asNumber());
    if (val.asBoolean()) num |= (1n << p);
    else num &= ~(1n << p);
    return MiniValue.hex(num.toString(16));
  },

  // === STRING ===
  SUBSTR: ([a, start, len]) => {
    return MiniValue.string(a.raw.slice(start.asNumber(), start.asNumber() + len.asNumber()));
  },
  REPLACE: ([a, from, to]) => {
    return MiniValue.string(a.raw.split(from.raw).join(to.raw));
  },
  REPLACEFIRST: ([a, from, to]) => {
    return MiniValue.string(a.raw.replace(from.raw, to.raw));
  },

  // === SHA ===
  SHA2: ([a]) => {
    const data = a.type === 'HEX' ? Buffer.from(a.raw.replace('0x',''), 'hex') : Buffer.from(a.raw, 'utf8');
    return MiniValue.hex(crypto.createHash('sha256').update(data).digest('hex'));
  },
  SHA3: ([a]) => {
    const data = a.type === 'HEX' ? Buffer.from(a.raw.replace('0x',''), 'hex') : Buffer.from(a.raw, 'utf8');
    return MiniValue.hex(crypto.createHash('sha3-256').update(data).digest('hex'));
  },
  PROOF: ([_data, _proof, _root]) => {
    // MMR proof verification - simplified mock
    return MiniValue.boolean(false);
  },

  // === SIGNATURES ===
  SIGNEDBY: ([pubkey], env) => {
    if (!pubkey || pubkey.raw == null) throw new Error('SIGNEDBY: invalid public key argument');
    const pk = pubkey.raw.toLowerCase();
    return MiniValue.boolean(env.signatures.some(s => s.toLowerCase() === pk));
  },
  CHECKSIG: ([pubkey, data, sig]) => {
    // In real impl verifies EC signature. Mock: always false unless explicitly mocked
    return MiniValue.boolean(false);
  },
  MULTISIG: (args, env) => {
    // MULTISIG(n, key1, key2, ...) - at least n of the given keys must have signed
    // Usage: MULTISIG(2, 0xpub1, 0xpub2, 0xpub3) = 2-of-3
    if (args.length < 2) throw new Error('MULTISIG requires at least 2 arguments');
    const required = args[0].asNumber();
    const keys = args.slice(1).map(a => a.raw.toLowerCase());
    let count = 0;
    for (const key of keys) {
      if (env.signatures.some(s => s.toLowerCase() === key)) count++;
    }
    return MiniValue.boolean(count >= required);
  },

  // === STATE ===
  STATE: ([port], env) => {
    const p = port.asNumber();
    const sv = env.transaction?.stateVars?.[p];
    if (sv === undefined) return MiniValue.number(0); // Minima spec: unset state = 0
    // Parse as number if numeric, hex if 0x-prefixed, otherwise string
    if (/^-?[0-9]+(\.[0-9]+)?$/.test(sv)) return MiniValue.number(Number(sv));
    if (sv.startsWith('0x') || sv.startsWith('0X')) return MiniValue.hex(sv);
    return MiniValue.string(sv);
  },
  PREVSTATE: ([port], env) => {
    const p = port.asNumber();
    const sv = env.transaction?.prevStateVars?.[p];
    if (sv === undefined) return MiniValue.number(0); // Minima spec: unset state = 0
    // Parse as number if numeric, hex if 0x-prefixed, otherwise string
    if (/^-?[0-9]+(\.[0-9]+)?$/.test(sv)) return MiniValue.number(Number(sv));
    if (sv.startsWith('0x') || sv.startsWith('0X')) return MiniValue.hex(sv);
    return MiniValue.string(sv);
  },
  SAMESTATE: ([from, to], env) => {
    const f = from.asNumber();
    const t = to.asNumber();
    for (let i = f; i <= t; i++) {
      const curr = env.transaction?.stateVars?.[i];
      const prev = env.transaction?.prevStateVars?.[i];
      if (curr !== prev) return MiniValue.boolean(false);
    }
    return MiniValue.boolean(true);
  },

  // === TXN INPUTS ===
  GETINADDR: ([idx], env) => {
    const coin = env.transaction?.inputs[idx.asNumber()];
    if (!coin) throw new Error(`Input ${idx.asNumber()} not found`);
    return MiniValue.hex(coin.address);
  },
  GETINAMT: ([idx], env) => {
    const coin = env.transaction?.inputs[idx.asNumber()];
    if (!coin) throw new Error(`Input ${idx.asNumber()} not found`);
    return MiniValue.number(coin.amount);
  },
  GETINID: ([idx], env) => {
    const coin = env.transaction?.inputs[idx.asNumber()];
    if (!coin) throw new Error(`Input ${idx.asNumber()} not found`);
    return MiniValue.hex(coin.coinId);
  },
  GETINTOK: ([idx], env) => {
    const coin = env.transaction?.inputs[idx.asNumber()];
    if (!coin) throw new Error(`Input ${idx.asNumber()} not found`);
    return MiniValue.hex(coin.tokenId);
  },
  SUMINPUTS: ([tokenId], env) => {
    const tid = tokenId.raw.toLowerCase();
    const sum = (env.transaction?.inputs ?? [])
      .filter(i => i.tokenId.toLowerCase() === tid)
      .reduce((acc, i) => acc + i.amount, 0);
    return MiniValue.number(sum);
  },
  VERIFYIN: ([idx, amount, address, tokenId], env) => {
    const coin = env.transaction?.inputs[idx.asNumber()];
    if (!coin) return MiniValue.boolean(false);
    return MiniValue.boolean(
      coin.amount >= amount.asNumber() &&
      coin.address.toLowerCase() === address.raw.toLowerCase() &&
      coin.tokenId.toLowerCase() === tokenId.raw.toLowerCase()
    );
  },

  // === TXN OUTPUTS ===
  GETOUTADDR: ([idx], env) => {
    const out = env.transaction?.outputs[idx.asNumber()];
    if (!out) throw new Error(`Output ${idx.asNumber()} not found`);
    return MiniValue.hex(out.address);
  },
  GETOUTAMT: ([idx], env) => {
    const out = env.transaction?.outputs[idx.asNumber()];
    if (!out) throw new Error(`Output ${idx.asNumber()} not found`);
    return MiniValue.number(out.amount);
  },
  GETOUTTOK: ([idx], env) => {
    const out = env.transaction?.outputs[idx.asNumber()];
    if (!out) throw new Error(`Output ${idx.asNumber()} not found`);
    return MiniValue.hex(out.tokenId);
  },
  GETOUTKEEPSTATE: ([idx], env) => {
    const out = env.transaction?.outputs[idx.asNumber()];
    if (!out) throw new Error(`Output ${idx.asNumber()} not found`);
    return MiniValue.boolean(out.keepState ?? false);
  },
  SUMOUTPUTS: ([tokenId], env) => {
    const tid = tokenId.raw.toLowerCase();
    const sum = (env.transaction?.outputs ?? [])
      .filter(o => o.tokenId.toLowerCase() === tid)
      .reduce((acc, o) => acc + o.amount, 0);
    return MiniValue.number(sum);
  },
  VERIFYOUT: ([idx, amount, address, tokenId], env) => {
    const out = env.transaction?.outputs[idx.asNumber()];
    if (!out) return MiniValue.boolean(false);
    return MiniValue.boolean(
      out.amount >= amount.asNumber() &&
      out.address.toLowerCase() === address.raw.toLowerCase() &&
      out.tokenId.toLowerCase() === tokenId.raw.toLowerCase()
    );
  },
};

export function callFunction(name: string, args: MiniValue[], env: Environment): MiniValue {
  const fn = FUNCTIONS[name.toUpperCase()];
  if (!fn) throw new Error(`Unknown function: ${name}`);
  return fn(args, env);
}

export function functionExists(name: string): boolean {
  return FUNCTIONS[name.toUpperCase()] !== undefined;
}
