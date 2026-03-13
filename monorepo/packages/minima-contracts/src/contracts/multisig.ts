import { ContractTemplate } from '../types';

/**
 * 2-of-2 MultiSig Contract
 * Requires signatures from BOTH parties to spend.
 * Use cases: joint accounts, escrow requiring both parties.
 */
export const multisig2of2: ContractTemplate = {
  name: 'multisig-2of2',
  description: 'Requires signatures from both parties. Neither can spend alone.',
  params: [
    {
      name: 'pubKey1',
      type: 'hex',
      description: 'Public key of first party',
    },
    {
      name: 'pubKey2',
      type: 'hex',
      description: 'Public key of second party',
    },
  ],
  script({ pubKey1, pubKey2 }) {
    return `RETURN MULTISIG(2, ${pubKey1}, ${pubKey2})`;
  },
  example: {
    pubKey1: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
    pubKey2: '0x1122334411223344112233441122334411223344112233441122334411223344',
  },
};

/**
 * 2-of-3 MultiSig Contract
 * Requires any 2 of 3 signatures. Classic threshold signature.
 * Use cases: treasury, DAO governance, 3-party escrow.
 */
export const multisig2of3: ContractTemplate = {
  name: 'multisig-2of3',
  description: 'Requires 2 of 3 signatures. Classic threshold — good for treasury or DAO.',
  params: [
    {
      name: 'pubKey1',
      type: 'hex',
      description: 'Public key of first party',
    },
    {
      name: 'pubKey2',
      type: 'hex',
      description: 'Public key of second party',
    },
    {
      name: 'pubKey3',
      type: 'hex',
      description: 'Public key of third party',
    },
  ],
  script({ pubKey1, pubKey2, pubKey3 }) {
    return `RETURN MULTISIG(2, ${pubKey1}, ${pubKey2}, ${pubKey3})`;
  },
  example: {
    pubKey1: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
    pubKey2: '0x1122334411223344112233441122334411223344112233441122334411223344',
    pubKey3: '0x5566778855667788556677885566778855667788556677885566778855667788',
  },
};

/**
 * M-of-N MultiSig Contract (generic)
 * Flexible threshold: requires M of N signatures.
 */
export const multisigMofN: ContractTemplate = {
  name: 'multisig-m-of-n',
  description: 'Generic M-of-N multisig. Requires at least M of the N provided keys to sign.',
  params: [
    {
      name: 'required',
      type: 'number',
      description: 'Minimum number of signatures required (M)',
    },
    {
      name: 'keys',
      type: 'string',
      description: 'Space-separated list of public keys (hex)',
    },
  ],
  script({ required, keys }) {
    const keyList = keys.trim().split(/\s+/).join(', ');
    return `RETURN MULTISIG(${required}, ${keyList})`;
  },
  example: {
    required: '2',
    keys: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd 0x1122334411223344112233441122334411223344112233441122334411223344',
  },
};
