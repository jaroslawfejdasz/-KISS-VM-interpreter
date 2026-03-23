import crypto from 'node:crypto';
import { ContractTemplate, CompiledContract, ContractLibrary } from './types';

import { basicSigned } from './contracts/basic-signed';
import { timeLock, coinAgeLock } from './contracts/time-lock';
import { multisig2of2, multisig2of3, multisigMofN } from './contracts/multisig';
import { htlc } from './contracts/htlc';
import { exchange, conditionalPayment } from './contracts/exchange';
import { stateChannel } from './contracts/state-channel';
import { vault, deadMansSwitch } from './contracts/vault';

const ALL_CONTRACTS: ContractTemplate[] = [
  basicSigned,
  timeLock,
  coinAgeLock,
  multisig2of2,
  multisig2of3,
  multisigMofN,
  htlc,
  exchange,
  conditionalPayment,
  stateChannel,
  vault,
  deadMansSwitch,
];

function sha3(data: string): string {
  return '0x' + crypto.createHash('sha3-256').update(data, 'utf8').digest('hex');
}

function validateParams(template: ContractTemplate, params: Record<string, string>): void {
  for (const p of template.params) {
    if (!(p.name in params)) {
      if (p.default !== undefined) {
        params[p.name] = p.default;
      } else {
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

export class MinimaContractLibrary implements ContractLibrary {
  private registry: Map<string, ContractTemplate>;

  constructor(extraContracts: ContractTemplate[] = []) {
    this.registry = new Map();
    for (const c of [...ALL_CONTRACTS, ...extraContracts]) {
      this.registry.set(c.name, c);
    }
  }

  get(name: string): ContractTemplate | undefined {
    return this.registry.get(name);
  }

  list(): ContractTemplate[] {
    return Array.from(this.registry.values());
  }

  compile(name: string, params: Record<string, string>): CompiledContract {
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
  register(template: ContractTemplate): void {
    if (this.registry.has(template.name)) {
      throw new Error(`Contract '${template.name}' already registered. Use a unique name.`);
    }
    this.registry.set(template.name, template);
  }
}

/** Default singleton library instance */
export const contracts = new MinimaContractLibrary();
