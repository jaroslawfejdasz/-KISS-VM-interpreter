"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Environment = void 0;
class Environment {
    constructor() {
        this.variables = new Map();
        this.globals = new Map();
        this.signatures = [];
        this.transaction = null;
        this.inputIndex = 0;
        // Execution state
        this.returnValue = null;
        this.returned = false;
        this.instructionCount = 0;
        this.MAX_INSTRUCTIONS = 1024;
        this.stackDepth = 0;
        this.MAX_STACK_DEPTH = 64;
    }
    setGlobal(name, value) {
        this.globals.set(name, value);
    }
    setVariable(name, value) {
        this.variables.set(name, value);
    }
    getVariable(name) {
        if (name.startsWith('@')) {
            const v = this.globals.get(name);
            if (!v)
                throw new Error(`Global variable not found: ${name}`);
            return v;
        }
        const v = this.variables.get(name);
        if (v === undefined)
            throw new Error(`Variable not found: ${name}`);
        return v;
    }
    hasVariable(name) {
        if (name.startsWith('@'))
            return this.globals.has(name);
        return this.variables.has(name);
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
exports.Environment = Environment;
//# sourceMappingURL=environment.js.map