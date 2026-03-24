import { MiniValue } from './values';
import { MockTransaction } from '../mock/transaction';

export class Environment {
  private variables: Map<string, MiniValue> = new Map();
  private globals: Map<string, MiniValue> = new Map();
  public signatures: string[] = [];
  public transaction: MockTransaction | null = null;
  public inputIndex: number = 0;
  
  // Execution state
  public returnValue: MiniValue | null = null;
  public returned: boolean = false;
  public instructionCount: number = 0;
  public readonly MAX_INSTRUCTIONS = 1024;
  public stackDepth: number = 0;
  public readonly MAX_STACK_DEPTH = 64;

  setGlobal(name: string, value: MiniValue) {
    this.globals.set(name, value);
  }

  setVariable(name: string, value: MiniValue) {
    this.variables.set(name.toUpperCase(), value);
  }

  getVariable(name: string): MiniValue {
    if (name.startsWith('@')) {
      const v = this.globals.get(name);
      if (!v) throw new Error(`Global variable not found: ${name}`);
      return v;
    }
    const v = this.variables.get(name.toUpperCase());
    if (v === undefined) throw new Error(`Variable not found: ${name}`);
    return v;
  }

  hasVariable(name: string): boolean {
    if (name.startsWith('@')) return this.globals.has(name);
    return this.variables.has(name.toUpperCase());
  }

  tick() {
    this.instructionCount++;
    if (this.instructionCount > this.MAX_INSTRUCTIONS) {
      throw new Error(`MAX_INSTRUCTIONS exceeded (${this.MAX_INSTRUCTIONS})`);
    }
  }

  pushStack() {
    this.stackDepth++;
    if (this.stackDepth > this.MAX_STACK_DEPTH) {
      throw new Error(`MAX_STACK_DEPTH exceeded (${this.MAX_STACK_DEPTH})`);
    }
  }

  popStack() {
    this.stackDepth--;
  }
}
