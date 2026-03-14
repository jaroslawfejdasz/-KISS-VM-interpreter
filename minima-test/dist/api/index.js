"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.defaultTransaction = exports.MiniValue = exports.AssertionError = exports.test = void 0;
exports.describe = describe;
exports.it = it;
exports.runScript = runScript;
exports.expect = expect;
exports.runSuites = runSuites;
const interpreter_1 = require("../interpreter");
const environment_1 = require("../interpreter/environment");
const values_1 = require("../interpreter/values");
const transaction_1 = require("../mock/transaction");
const suites = [];
const currentTests = [];
let currentSuiteName = '';
function describe(name, fn) {
    suites.push({ name, fn });
}
function it(name, fn) {
    currentTests.push({ name, fn });
}
exports.test = it;
function runScript(script, options = {}) {
    // Build transaction with shorthand overrides
    const txOverrides = { ...options.transaction };
    // block shorthand
    if (options.block !== undefined)
        txOverrides.blockNumber = options.block;
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
        txOverrides.inputs = inputs.map((inp, i) => i === 0 ? { ...inp, blockCreated: bn - options.coinAge } : inp);
    }
    // amount shorthand
    if (options.amount !== undefined) {
        const inputs = txOverrides.inputs ?? [{ coinId: '0xabcdef1234567890', address: '0x1234567890abcdef', amount: 100, tokenId: '0x00', blockCreated: 1000, stateVars: {} }];
        txOverrides.inputs = inputs.map((inp, i) => i === 0 ? { ...inp, amount: options.amount } : inp);
    }
    // outputs shorthand
    if (options.outputs !== undefined)
        txOverrides.outputs = options.outputs;
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
    if (options.state !== undefined)
        txOverrides.stateVars = options.state;
    if (options.prevState !== undefined)
        txOverrides.prevStateVars = options.prevState;
    const tx = (0, transaction_1.defaultTransaction)(txOverrides);
    const env = new environment_1.Environment();
    env.transaction = tx;
    env.inputIndex = options.inputIndex ?? 0;
    env.signatures = options.signatures ?? tx.signatures;
    // Set globals
    const globals = (0, transaction_1.buildGlobals)(tx, env.inputIndex);
    for (const [k, v] of Object.entries(globals))
        env.setGlobal(k, v);
    if (options.globals) {
        for (const [k, v] of Object.entries(options.globals))
            env.setGlobal(k, v);
    }
    // Set user variables
    if (options.variables) {
        for (const [k, v] of Object.entries(options.variables))
            env.setVariable(k, v);
    }
    // Register MAST scripts
    if (options.mastScripts) {
        for (const [hash, mastScript] of Object.entries(options.mastScripts)) {
            env.setVariable(`__MAST_${hash}`, values_1.MiniValue.string(mastScript));
        }
    }
    const interpreter = new interpreter_1.KissVMInterpreter(env);
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
    }
    catch (e) {
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
// =============================================
// ASSERTIONS
// =============================================
class AssertionError extends Error {
    constructor(msg) {
        super(msg);
        this.name = 'AssertionError';
    }
}
exports.AssertionError = AssertionError;
class Expect {
    constructor(val) {
        this.val = val;
    }
    toPass() {
        if (this.val?.success !== undefined) {
            if (!this.val.success) {
                throw new AssertionError(`Expected script to PASS, but it returned FALSE${this.val.error ? ': ' + this.val.error : ''}`);
            }
        }
        else {
            if (!this.val)
                throw new AssertionError(`Expected truthy, got ${this.val}`);
        }
        return this;
    }
    toFail() {
        if (this.val?.success !== undefined) {
            if (this.val.success) {
                throw new AssertionError('Expected script to FAIL, but it returned TRUE');
            }
        }
        else {
            if (this.val)
                throw new AssertionError(`Expected falsy, got ${this.val}`);
        }
        return this;
    }
    toThrow(message) {
        if (!this.val.error)
            throw new AssertionError('Expected script to throw an error, but it succeeded');
        if (message && !this.val.error.includes(message)) {
            throw new AssertionError(`Expected error containing '${message}', got: '${this.val.error}'`);
        }
        return this;
    }
    toBe(expected) {
        const actual = this.val?.success !== undefined ? this.val.success : this.val;
        if (actual !== expected) {
            throw new AssertionError(`Expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
        }
        return this;
    }
    toEqual(expected) {
        const actual = this.val?.success !== undefined ? this.val.success : this.val;
        if (JSON.stringify(actual) !== JSON.stringify(expected)) {
            throw new AssertionError(`Expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
        }
        return this;
    }
    toBeWithinInstructions(max) {
        if (!this.val?.instructions !== undefined) {
            if (this.val.instructions > max) {
                throw new AssertionError(`Expected ≤${max} instructions, got ${this.val.instructions}`);
            }
        }
        return this;
    }
}
function expect(val) {
    return new Expect(val);
}
// =============================================
// TEST RUNNER
// =============================================
async function runSuites() {
    const results = [];
    for (const suite of suites) {
        currentTests.length = 0;
        currentSuiteName = suite.name;
        suite.fn();
        const suiteStart = Date.now();
        const testResults = [];
        for (const testCase of [...currentTests]) {
            const start = Date.now();
            try {
                testCase.fn();
                testResults.push({ name: testCase.name, passed: true, duration: Date.now() - start });
            }
            catch (e) {
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
var values_2 = require("../interpreter/values");
Object.defineProperty(exports, "MiniValue", { enumerable: true, get: function () { return values_2.MiniValue; } });
var transaction_2 = require("../mock/transaction");
Object.defineProperty(exports, "defaultTransaction", { enumerable: true, get: function () { return transaction_2.defaultTransaction; } });
//# sourceMappingURL=index.js.map