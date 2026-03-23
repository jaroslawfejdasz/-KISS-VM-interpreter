import { MiniValue } from '../interpreter/values';

export interface MockCoin {
  coinId: string;
  address: string;
  amount: number;
  tokenId: string;
  blockCreated: number;
  stateVars?: Record<number, string>;
}

export interface MockOutput {
  address: string;
  amount: number;
  tokenId: string;
  keepState?: boolean;
}

export interface MockTransaction {
  inputs: MockCoin[];
  outputs: MockOutput[];
  blockNumber: number;
  blockTime: number;
  signatures: string[];
  stateVars?: Record<number, string>;
  prevStateVars?: Record<number, string>;
}

export function buildGlobals(tx: MockTransaction, inputIndex: number): Record<string, MiniValue> {
  const coin = tx.inputs[inputIndex];
  if (!coin) throw new Error(`Input ${inputIndex} not found`);
  return {
    '@BLOCK':      MiniValue.number(tx.blockNumber),
    '@BLOCKMILLI': MiniValue.number(tx.blockTime),
    '@CREATED':    MiniValue.number(coin.blockCreated),
    '@COINAGE':    MiniValue.number(tx.blockNumber - coin.blockCreated),
    '@INPUT':      MiniValue.number(inputIndex),
    '@COINID':     MiniValue.hex(coin.coinId),
    '@AMOUNT':     MiniValue.number(coin.amount),
    '@ADDRESS':    MiniValue.hex(coin.address),
    '@TOKENID':    MiniValue.hex(coin.tokenId),
    '@SCRIPT':     MiniValue.string(''),
    '@TOTIN':      MiniValue.number(tx.inputs.length),
    '@TOTOUT':     MiniValue.number(tx.outputs.length),
  };
}

export function defaultTransaction(overrides: Partial<MockTransaction> = {}): MockTransaction {
  return {
    inputs: [{
      coinId: '0xabcdef1234567890',
      address: '0x1234567890abcdef',
      amount: 100,
      tokenId: '0x00',
      blockCreated: 1000,
      stateVars: {},
    }],
    outputs: [{
      address: '0xdeadbeef12345678',
      amount: 100,
      tokenId: '0x00',
      keepState: false,
    }],
    blockNumber: 1100,
    blockTime: Date.now(),
    signatures: [],
    stateVars: {},
    prevStateVars: {},
    ...overrides,
  };
}
