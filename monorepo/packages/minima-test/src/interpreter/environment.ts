import { MiniValue } from './values';
import { MockTransaction } from '../mock/transaction';

export class Environment {
  private variables: Map<string, MiniValue> = new Map();
  private globals: Map<string, MiniValue> = new Map();
  signatures: string[] = [];
  transaction: MockTransaction | null = null;
  inputIndex: number = 0;

  // Execution state
  returnValue: MiniValue | null = null;
  returned: boolean = false;
  instructionCount: number = 0;
  readonly MAX_INSTRUCTIONS = 1024;
  stackDepth: number = 0;
  readonly MAX_STACK_DEPTH = 64;

  setGlobal(name: string, value: MiniValue): void {
    this.globals.set(name, value);
  }

  setVariable(name: string, value: MiniValue): void {
    this.variables.set(name, value);
  }

  getVariable(name: string): MiniValue {
    if (name.startsWith('@')) {
      const v = this.globals.get(name);
      if (!v) throw new Error(`Global variable not found: ${name}`);
      return v;
    }
    const v = this.variables.get(name);
    if (v === undefined) throw new Error(`Variable not found: ${name}`);
    return v;
  }

  hasVariable(name: string): boolean {
    if (name.startsWith('@')) return this.globals.has(name);
    return this.variables.has(name);
  }

  tick(): void {
    this.instructionCount++;
    if (this.instructionCount > this.MAX_INSTRUCTIONS) {
      throw new Error(`MAX_INSTRUCTIONS exceeded (${this.MAX_INSTRUCTIONS})`);
    }
  }

  pushStack(): void {
    this.stackDepth++;
    if (this.stackDepth > this.MAX_STACK_DEPTH) {
      throw new Error(`MAX_STACK_DEPTH exceeded (${this.MAX_STACK_DEPTH})`);
    }
  }

  popStack(): void {
    this.stackDepth--;
  }
}
