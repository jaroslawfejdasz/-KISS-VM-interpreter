/**
 * minima-test — Testing framework for Minima KISS VM smart contracts
 *
 * @example
 * import { describe, it, expectContract, MockTransaction } from 'minima-test';
 *
 * describe('Timelock contract', () => {
 *   it('passes when block >= lockBlock', async () => {
 *     const ctx = MockTransaction.create()
 *       .withInput({ address: '0xABC', amount: 100n })
 *       .atBlock(200n)
 *       .build();
 *
 *     await expectContract('RETURN @BLOCK GTE 100', ctx).toPass();
 *   });
 * });
 */

export { KissVMInterpreter } from './interpreter/index.js';
export { MockTransaction } from './mock-transaction.js';
export {
  describe,
  it,
  test,
  beforeEach,
  afterEach,
  expectContract,
  runContract,
  runAll,
  runTestSuite,
  printSuiteResult,
  Expectation,
} from './test-runner.js';

export type {
  MiniValue,
  StateVar,
  CoinData,
  InputCoin,
  OutputCoin,
  SignerInfo,
  TransactionContext,
  ContractResult,
  ContractExecution,
  TestCase,
  TestSuiteResult,
  TestResult,
} from './types.js';
