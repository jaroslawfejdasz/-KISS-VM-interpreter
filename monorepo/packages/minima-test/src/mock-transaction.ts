/**
 * MockTransaction — fluent builder for test transaction contexts
 */

import { TransactionContext, InputCoin, OutputCoin, SignerInfo, StateVar } from './types.js';

export class MockTransaction {
  private ctx: TransactionContext = {
    inputs: [],
    outputs: [],
    signers: [],
    block: 0n,
    blocktime: 0n,
  };

  static create(): MockTransaction {
    return new MockTransaction();
  }

  // ── Inputs ────────────────────────────────────────────────────────────────

  addInput(coin: InputCoin): this {
    this.ctx.inputs.push(coin);
    return this;
  }

  withInput(opts: Partial<InputCoin> & { address: string; amount: bigint }): this {
    return this.addInput({
      coinid: opts.coinid ?? '0x' + '00'.repeat(32),
      address: opts.address,
      amount: opts.amount,
      tokenid: opts.tokenid ?? '0x00',
      stateVars: opts.stateVars,
      script: opts.script,
    });
  }

  // ── Outputs ───────────────────────────────────────────────────────────────

  addOutput(coin: OutputCoin): this {
    this.ctx.outputs.push(coin);
    return this;
  }

  withOutput(opts: Partial<OutputCoin> & { address: string; amount: bigint }): this {
    return this.addOutput({
      coinid: opts.coinid,
      address: opts.address,
      amount: opts.amount,
      tokenid: opts.tokenid ?? '0x00',
      stateVars: opts.stateVars,
    });
  }

  // ── Signers ───────────────────────────────────────────────────────────────

  signedBy(address: string, publicKey?: string): this {
    this.ctx.signers.push({ address, publicKey });
    return this;
  }

  // ── Block / time ──────────────────────────────────────────────────────────

  atBlock(block: bigint): this {
    this.ctx.block = block;
    return this;
  }

  atTime(blocktime: bigint): this {
    this.ctx.blocktime = blocktime;
    return this;
  }

  // ── State variables ───────────────────────────────────────────────────────

  withState(stateVars: StateVar): this {
    this.ctx.stateVars = { ...this.ctx.stateVars, ...stateVars };
    return this;
  }

  withPrevState(prevStateVars: StateVar): this {
    this.ctx.prevStateVars = { ...this.ctx.prevStateVars, ...prevStateVars };
    return this;
  }

  // ── MMR ───────────────────────────────────────────────────────────────────

  withMMR(root: string, peaks?: string): this {
    this.ctx.mmrroot  = root;
    this.ctx.mmrpeaks = peaks ?? root;
    return this;
  }

  // ── Build ─────────────────────────────────────────────────────────────────

  build(): TransactionContext {
    return { ...this.ctx };
  }
}
