/**
 * Core types for minima-test framework
 */

export type MiniValue = boolean | bigint | string;

export interface StateVar {
  [key: number]: MiniValue;
}

export interface CoinData {
  coinid?: string;
  address: string;
  amount: bigint;
  tokenid?: string;
  stateVars?: StateVar;
  floating?: boolean;
}

export interface InputCoin extends CoinData {
  script?: string;
}

export interface OutputCoin extends CoinData {}

export interface SignerInfo {
  address: string;
  publicKey?: string;
}

export interface TransactionContext {
  inputs: InputCoin[];
  outputs: OutputCoin[];
  signers: SignerInfo[];
  block?: bigint;
  blocktime?: bigint;
  mmrroot?: string;
  mmrpeaks?: string;
  stateVars?: StateVar;
  prevStateVars?: StateVar;
  tokenScript?: string;
  tokenAmount?: bigint;
}

export type ContractResult = 'TRUE' | 'FALSE' | 'EXCEPTION';

export interface ContractExecution {
  result: ContractResult;
  trace: string[];
  error?: string;
  gasUsed?: number;
}

export interface TestCase {
  name: string;
  script: string;
  context: TransactionContext;
  expected: boolean;
  expectedError?: string;
}

export interface TestSuiteResult {
  name: string;
  passed: number;
  failed: number;
  total: number;
  results: TestResult[];
  durationMs: number;
}

export interface TestResult {
  name: string;
  passed: boolean;
  expected: boolean;
  got: ContractResult;
  error?: string;
  durationMs: number;
}
