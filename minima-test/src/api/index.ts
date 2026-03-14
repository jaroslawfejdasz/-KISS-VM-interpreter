import { KissVMInterpreter } from '../interpreter';
import { Environment } from '../interpreter/environment';
import { MiniValue } from '../interpreter/values';
import { MockTransaction, buildGlobals, defaultTransaction } from '../mock/transaction';

// =============================================
// TEST RUNNER API
// =============================================

export interface TestResult {
  name: string;
  passed: boolean;
  error?: string;
  duration: number;
}

export interface SuiteResult {
  name: string;
  tests: TestResult[];
  passed: number;
  failed: number;
  duration: number;
}

const suites: Array<{ name: string; fn: () => void }> = [];
const currentTests: Array<{ name: string; fn: () => void }> = [];
let currentSuiteName = '';

export function describe(name: string, fn: () => void) {
  suites.push({ name, fn });
}

export function it(name: string, fn: () => void) {
  currentTests.push({ name, fn });
}

export const test = it;

// =============================================
// KISSVM TEST HELPERS
// =============================================

export interface RunScriptOptions {
  transaction?: Partial<MockTransaction>;
  inputIndex?: number;
  signatures?: string[];
  variables?: Record<string, MiniValue>;
  globals?: Record<string, MiniValue>;
  mastScripts?: Record<string, string>; // hash -> script
  // Shorthand convenience overrides
  block?: number;
  coinAge?: number;
  amount?: number;
  tokenId?: string;
  address?: string;
  state?: Record<number, string>;
  prevState?: Record<number, string>;
  outputs?: Array<{ address: string; amount: number; tokenId: string; keepState?: boolean }>;
  inputs?: Array<{ coinId?: string; address: string; amount: number; tokenId: string; blockCreated?: number; stateVars?: Record<number, string> }>;
}

export function runScript(script: string, options: RunScriptOptions = {}): ScriptResult {
  // Build transaction with shorthand overrides
  const txOverrides: Partial<MockTransaction> = { ...options.transaction };

  // block shorthand
  if (options.block !== undefined) txOverrides.blockNumber = options.block;

  // coinAge shorthand: set blockCreated so that blockNumber - blockCreated = coinAge
  // Note: uses blockNumber from txOverrides (already set from options.block above) or default 1100
  if (options.coinAge !== undefined) {
    const bn = txOverrides.blockNumber ?? 1100;
    const inputs = txOverrides.inputs ?? [{
      coinId: '0xabcdef1234567890',
      address: '0x1234567890abcdef',
      amount: 100,
      tokenId: '0x00',
      blockCreated: bn - options.coinAge,
      stateVars: {},
    }];
    txOverrides.inputs = inputs.map((inp, i) =>
      i === 0 ? { ...inp, blockCreated: bn - options.coinAge! } : inp
    );
  }

  // amount shorthand
  if (options.amount !== undefined) {
    const inputs = txOverrides.inputs ?? [{ coinId: '0xabcdef1234567890', address: '0x1234567890abcdef', amount: 100, tokenId: '0x00', blockCreated: 1000, stateVars: {} }];
    txOverrides.inputs = inputs.map((inp, i) =>
      i === 0 ? { ...inp, amount: options.amount! } : inp
    );
  }

  // outputs shorthand
  if (options.outputs !== undefined) txOverrides.outputs = options.outputs;

  // inputs shorthand (full override)
  if (options.inputs !== undefined) {
    txOverrides.inputs = options.inputs.map(inp => ({
      coinId: inp.coinId ?? '0xabcdef1234567890',
      address: inp.address,
      amount: inp.amount,
      tokenId: inp.tokenId,
      blockCreated: inp.blockCreated ?? 1000,
      stateVars: inp.stateVars ?? {},
    }));
  }

  // state / prevState shorthands
  if (options.state !== undefined) txOverrides.stateVars = options.state;
  if (options.prevState !== undefined) txOverrides.prevStateVars = options.prevState;

  const tx = defaultTransaction(txOverrides);
  const env = new Environment();
  
  env.transaction = tx;
  env.inputIndex = options.inputIndex ?? 0;
  env.signatures = options.signatures ?? tx.signatures;

  // Set globals
  const globals = buildGlobals(tx, env.inputIndex);
  for (const [k, v] of Object.entries(globals)) env.setGlobal(k, v);
  if (options.globals) {
    for (const [k, v] of Object.entries(options.globals)) env.setGlobal(k, v);
  }
  
  // Set user variables
  if (options.variables) {
    for (const [k, v] of Object.entries(options.variables)) env.setVariable(k, v);
  }

  // Register MAST scripts
  if (options.mastScripts) {
    for (const [hash, mastScript] of Object.entries(options.mastScripts)) {
      env.setVariable(`__MAST_${hash}`, MiniValue.string(mastScript));
    }
  }

  const interpreter = new KissVMInterpreter(env);
  const start = Date.now();

  try {
    const result = interpreter.run(script);
    return {
      success: result,
      passed: result,
      failed: !result,
      error: null,
      instructions: env.instructionCount,
      duration: Date.now() - start,
      env,
    };
  } catch (e: any) {
    return {
      success: false,
      passed: false,
      failed: true,
      error: e.message,
      instructions: env.instructionCount,
      duration: Date.now() - start,
      env,
    };
  }
}

export interface ScriptResult {
  success: boolean;
  passed: boolean;
  failed: boolean;
  error: string | null;
  instructions: number;
  duration: number;
  env: Environment;
}

// =============================================
// ASSERTIONS
// =============================================

export class AssertionError extends Error {
  constructor(msg: string) {
    super(msg);
    this.name = 'AssertionError';
  }
}

class Expect {
  constructor(private val: ScriptResult | any) {}

  toPass() {
    if (this.val?.success !== undefined) {
      if (!this.val.success) {
        throw new AssertionError(
          `Expected script to PASS, but it returned FALSE${this.val.error ? ': ' + this.val.error : ''}`
        );
      }
    } else {
      if (!this.val) throw new AssertionError(`Expected truthy, got ${this.val}`);
    }
    return this;
  }

  toFail() {
    if (this.val?.success !== undefined) {
      if (this.val.success) {
        throw new AssertionError('Expected script to FAIL, but it returned TRUE');
      }
    } else {
      if (this.val) throw new AssertionError(`Expected falsy, got ${this.val}`);
    }
    return this;
  }

  toThrow(message?: string) {
    if (!this.val.error) throw new AssertionError('Expected script to throw an error, but it succeeded');
    if (message && !this.val.error.includes(message)) {
      throw new AssertionError(`Expected error containing '${message}', got: '${this.val.error}'`);
    }
    return this;
  }

  toBe(expected: any) {
    const actual = this.val?.success !== undefined ? this.val.success : this.val;
    if (actual !== expected) {
      throw new AssertionError(`Expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    }
    return this;
  }

  toEqual(expected: any) {
    const actual = this.val?.success !== undefined ? this.val.success : this.val;
    if (JSON.stringify(actual) !== JSON.stringify(expected)) {
      throw new AssertionError(`Expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    }
    return this;
  }

  toBeWithinInstructions(max: number) {
    if (!this.val?.instructions !== undefined) {
      if (this.val.instructions > max) {
        throw new AssertionError(
          `Expected ≤${max} instructions, got ${this.val.instructions}`
        );
      }
    }
    return this;
  }
}

export function expect(val: ScriptResult | any): Expect {
  return new Expect(val);
}

// =============================================
// TEST RUNNER
// =============================================

export async function runSuites(): Promise<SuiteResult[]> {
  const results: SuiteResult[] = [];

  for (const suite of suites) {
    currentTests.length = 0;
    currentSuiteName = suite.name;
    suite.fn();

    const suiteStart = Date.now();
    const testResults: TestResult[] = [];

    for (const testCase of [...currentTests]) {
      const start = Date.now();
      try {
        testCase.fn();
        testResults.push({ name: testCase.name, passed: true, duration: Date.now() - start });
      } catch (e: any) {
        testResults.push({ name: testCase.name, passed: false, error: e.message, duration: Date.now() - start });
      }
    }

    results.push({
      name: suite.name,
      tests: testResults,
      passed: testResults.filter(t => t.passed).length,
      failed: testResults.filter(t => !t.passed).length,
      duration: Date.now() - suiteStart,
    });
  }

  return results;
}

// Re-export
export { MiniValue } from '../interpreter/values';
export { defaultTransaction } from '../mock/transaction';
export type { MockTransaction, MockCoin, MockOutput } from '../mock/transaction';
