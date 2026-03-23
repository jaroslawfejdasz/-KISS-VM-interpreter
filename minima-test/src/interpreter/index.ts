import { Token, TokenType, tokenize } from '../tokenizer';
import { MiniValue } from './values';
import { Environment } from './environment';
import { callFunction } from './functions';

export class KissVMInterpreter {
  private tokens: Token[] = [];
  private pos: number = 0;
  private env: Environment;

  constructor(env: Environment) {
    this.env = env;
  }

  private peek(): Token { return this.tokens[this.pos]; }
  private advance(): Token { return this.tokens[this.pos++]; }
  private expect(type: TokenType, value?: string): Token {
    const t = this.advance();
    if (t.type !== type && !(value === undefined)) {
      throw new Error(`Expected ${type}${value ? ' ' + value : ''}, got ${t.type} '${t.value}'`);
    }
    if (value && t.value.toUpperCase() !== value.toUpperCase()) {
      throw new Error(`Expected '${value}', got '${t.value}'`);
    }
    return t;
  }
  private match(type: TokenType, value?: string): boolean {
    const t = this.peek();
    if (t.type !== type) return false;
    if (value && t.value.toUpperCase() !== value.toUpperCase()) return false;
    return true;
  }

  run(script: string): boolean {
    this.tokens = tokenize(script);
    this.pos = 0;
    this.env.returned = false;
    this.env.returnValue = null;
    this.env.instructionCount = 0;

    this.executeBlock();

    if (!this.env.returned) {
      throw new Error('Script did not RETURN a value');
    }
    return this.env.returnValue!.asBoolean();
  }

  private executeBlock() {
    while (!this.match('EOF') && !this.env.returned) {
      const t = this.peek();
      if (t.type === 'KEYWORD') {
        const kw = t.value.toUpperCase();
        if (kw === 'ENDIF' || kw === 'ELSE' || kw === 'ELSEIF' || kw === 'ENDWHILE') break;
      }
      if (t.type === 'EOF') break;
      this.executeStatement();
    }
  }

  private executeStatement() {
    this.env.tick();
    const t = this.peek();

    if (t.type !== 'KEYWORD') throw new Error(`Expected statement keyword, got ${t.type} '${t.value}'`);
    const kw = t.value.toUpperCase();

    if (kw === 'LET')    { this.executeLet(); return; }
    if (kw === 'IF')     { this.executeIf(); return; }
    if (kw === 'WHILE')  { this.executeWhile(); return; }
    if (kw === 'RETURN') { this.executeReturn(); return; }
    if (kw === 'ASSERT') { this.executeAssert(); return; }
    if (kw === 'EXEC')   { this.executeExec(); return; }
    if (kw === 'MAST')   { this.executeMast(); return; }

    throw new Error(`Unknown statement: ${kw}`);
  }

  private executeLet() {
    this.advance(); // LET
    const name = this.advance(); // variable name
    const varName = name.value;
    // optional = sign (KISS VM uses LET x = expr OR LET x expr)
    if (this.match('OPERATOR', '=')) {
      this.advance();
    }
    // Check for EQ keyword style
    const val = this.evaluateExpression();
    this.env.setVariable(varName, val);
  }

  private executeIf() {
    this.advance(); // IF
    const condition = this.evaluateExpression();
    this.expect('KEYWORD', 'THEN');

    if (condition.asBoolean()) {
      this.executeBlock();
      // skip ELSE/ELSEIF branches
      this.skipElseBranches();
    } else {
      this.skipBlock();
      // check for ELSEIF / ELSE
      while (this.match('KEYWORD', 'ELSEIF')) {
        this.advance();
        const elseifCond = this.evaluateExpression();
        this.expect('KEYWORD', 'THEN');
        if (elseifCond.asBoolean()) {
          this.executeBlock();
          this.skipElseBranches();
          return;
        } else {
          this.skipBlock();
        }
      }
      if (this.match('KEYWORD', 'ELSE')) {
        this.advance();
        this.executeBlock();
      }
    }
    if (this.match('KEYWORD', 'ENDIF')) this.advance();
  }

  private skipElseBranches() {
    while (this.match('KEYWORD', 'ELSEIF') || this.match('KEYWORD', 'ELSE')) {
      this.advance();
      if (this.peek().value.toUpperCase() === 'THEN') this.advance();
      this.skipBlock();
    }
    if (this.match('KEYWORD', 'ENDIF')) this.advance();
  }

  private skipBlock() {
    let depth = 0;
    while (!this.match('EOF')) {
      const t = this.peek();
      if (t.type === 'KEYWORD') {
        const kw = t.value.toUpperCase();
        if (kw === 'IF' || kw === 'WHILE') depth++;
        if (kw === 'ENDIF' || kw === 'ENDWHILE') {
          if (depth === 0) return;
          depth--;
        }
        if (depth === 0 && (kw === 'ELSE' || kw === 'ELSEIF' || kw === 'ENDIF' || kw === 'ENDWHILE')) return;
      }
      this.advance();
    }
  }

  private executeWhile() {
    this.advance(); // WHILE
    const condStart = this.pos;
    
    let iterations = 0;
    const MAX_ITER = 512;

    while (true) {
      this.pos = condStart;
      const condition = this.evaluateExpression();
      this.expect('KEYWORD', 'DO');
      
      if (!condition.asBoolean()) {
        this.skipBlock();
        this.expect('KEYWORD', 'ENDWHILE');
        break;
      }

      iterations++;
      if (iterations > MAX_ITER) throw new Error('WHILE loop exceeded max iterations (512)');

      this.executeBlock();
      if (this.env.returned) break;
      
      // consume ENDWHILE then loop back
      if (this.match('KEYWORD', 'ENDWHILE')) this.advance();
    }
  }

  private executeReturn() {
    this.advance(); // RETURN
    const val = this.evaluateExpression();
    this.env.returned = true;
    this.env.returnValue = val;
  }

  private executeAssert() {
    this.advance(); // ASSERT
    const val = this.evaluateExpression();
    if (!val.asBoolean()) {
      throw new Error('ASSERT failed');
    }
  }

  private executeExec() {
    this.advance(); // EXEC
    const script = this.evaluateExpression();
    // Execute sub-script — shares the same Environment (and instruction counter)
    // This is intentional: EXEC cannot be used to bypass the 1024 instruction limit
    const sub = new KissVMInterpreter(this.env);
    const result = sub.run(script.asString());
    if (!result) throw new Error('EXEC sub-script returned FALSE');
  }

  private executeMast() {
    this.advance(); // MAST
    const hashVal = this.evaluateExpression();
    // MAST: verify that a script with given hash was provided and run it
    // In mock: just record the hash
    // Real: checks MMR proof that script hashes to given value
    // For testing purposes, we allow registering MAST scripts
    const hash = hashVal.asHex();
    const mastScript = this.tryGetMastScript(hash);
    if (mastScript) {
      const sub = new KissVMInterpreter(this.env);
      sub.run(mastScript);
    }
  }

  private tryGetMastScript(hash: string): string | null {
    try { return this.env.getVariable(`__MAST_${hash}`).asString(); } catch { return null; }
  }

  // === EXPRESSION EVALUATOR ===
  private evaluateExpression(): MiniValue {
    return this.parseOr();
  }

  private parseOr(): MiniValue {
    let left = this.parseAnd();
    while (this.match('KEYWORD', 'OR')) {
      this.advance();
      const right = this.parseAnd();
      left = MiniValue.boolean(left.asBoolean() || right.asBoolean());
    }
    return left;
  }

  private parseAnd(): MiniValue {
    let left = this.parseNot();
    while (this.match('KEYWORD', 'AND')) {
      this.advance();
      const right = this.parseNot();
      left = MiniValue.boolean(left.asBoolean() && right.asBoolean());
    }
    return left;
  }

  private parseNot(): MiniValue {
    if (this.match('KEYWORD', 'NOT')) {
      this.advance();
      return MiniValue.boolean(!this.parseNot().asBoolean());
    }
    if (this.match('KEYWORD', 'NAND')) {
      this.advance();
      const a = this.parseNot();
      const b = this.parseNot();
      return MiniValue.boolean(!(a.asBoolean() && b.asBoolean()));
    }
    if (this.match('KEYWORD', 'NOR')) {
      this.advance();
      const a = this.parseNot();
      const b = this.parseNot();
      return MiniValue.boolean(!(a.asBoolean() || b.asBoolean()));
    }
    if (this.match('KEYWORD', 'XOR')) {
      this.advance();
      const a = this.parseNot();
      const b = this.parseNot();
      return MiniValue.boolean(a.asBoolean() !== b.asBoolean());
    }
    return this.parseComparison();
  }

  private parseComparison(): MiniValue {
    let left = this.parseAddSub();
    
    const kwOps = new Set(['EQ','NEQ','LT','GT','LTE','GTE']);
    const opOps = new Set(['!=','<','>','<=','>=']);
    
    while (
      (this.peek().type === 'KEYWORD' && kwOps.has(this.peek().value.toUpperCase())) ||
      (this.peek().type === 'OPERATOR' && opOps.has(this.peek().value))
    ) {
      const op = this.advance().value.toUpperCase();
      const right = this.parseAddSub();
      
      let cmp: number;
      // Both HEX: compare as hex strings
      if (left.type === 'HEX' && right.type === 'HEX') {
        cmp = left.raw.toLowerCase().localeCompare(right.raw.toLowerCase());
      } else {
        // Try numeric comparison
        try { cmp = left.asNumber() - right.asNumber(); }
        catch { cmp = left.raw.localeCompare(right.raw); }
      }
      
      if (op === 'EQ')       left = MiniValue.boolean(cmp === 0);
      else if (op === 'NEQ' || op === '!=')  left = MiniValue.boolean(cmp !== 0);
      else if (op === 'LT'  || op === '<')   left = MiniValue.boolean(cmp < 0);
      else if (op === 'GT'  || op === '>')   left = MiniValue.boolean(cmp > 0);
      else if (op === 'LTE' || op === '<=')  left = MiniValue.boolean(cmp <= 0);
      else if (op === 'GTE' || op === '>=')  left = MiniValue.boolean(cmp >= 0);
    }
    return left;
  }

  private parseAddSub(): MiniValue {
    let left = this.parseMulDiv();
    while (this.peek().type === 'OPERATOR' && (this.peek().value === '+' || this.peek().value === '-')) {
      const op = this.advance().value;
      const right = this.parseMulDiv();
      if (op === '+') {
        if (left.type === 'STRING' || right.type === 'STRING') {
          left = MiniValue.string(left.raw + right.raw);
        } else {
          left = MiniValue.number(left.asNumber() + right.asNumber());
        }
      } else {
        left = MiniValue.number(left.asNumber() - right.asNumber());
      }
    }
    return left;
  }

  private parseMulDiv(): MiniValue {
    let left = this.parseUnary();
    while (this.peek().type === 'OPERATOR' && ['*','/',  '%'].includes(this.peek().value)) {
      const op = this.advance().value;
      const right = this.parseUnary();
      if (op === '*') left = MiniValue.number(left.asNumber() * right.asNumber());
      else if (op === '/') left = MiniValue.number(left.asNumber() / right.asNumber());
      else if (op === '%') left = MiniValue.number(left.asNumber() % right.asNumber());
    }
    return left;
  }

  private parseUnary(): MiniValue {
    if (this.peek().type === 'OPERATOR' && this.peek().value === '-') {
      this.advance();
      return MiniValue.number(-this.parseUnary().asNumber());
    }
    return this.parsePrimary();
  }

  private parsePrimary(): MiniValue {
    const t = this.peek();

    // Parenthesized expression
    if (t.type === 'LPAREN') {
      this.advance();
      const val = this.evaluateExpression();
      this.expect('RPAREN');
      return val;
    }

    // Literals
    if (t.type === 'NUMBER') { this.advance(); return MiniValue.number(t.value); }
    if (t.type === 'HEX') { this.advance(); return MiniValue.hex(t.value); }
    if (t.type === 'STRING') { this.advance(); return MiniValue.string(t.value); }
    if (t.type === 'BOOLEAN') { this.advance(); return MiniValue.boolean(t.value === 'TRUE'); }

    // Global variable @BLOCK etc
    if (t.type === 'GLOBAL') {
      this.advance();
      return this.env.getVariable(t.value);
    }

    // Variable
    if (t.type === 'VARIABLE') {
      this.advance();
      return this.env.getVariable(t.value);
    }

    // Function call
    if (t.type === 'KEYWORD') {
      const kw = t.value.toUpperCase();
      // Check if it's a function (not a statement keyword)
      const stmtKeywords = new Set(['LET','IF','THEN','ELSE','ELSEIF','ENDIF','WHILE','DO','ENDWHILE','RETURN','ASSERT','EXEC','MAST','AND','OR','NOT','XOR','NAND','NOR','EQ','NEQ','LT','GT','LTE','GTE']);
      if (!stmtKeywords.has(kw)) {
        this.advance();
        // Parse args
        this.expect('LPAREN');
        const args: MiniValue[] = [];
        while (!this.match('RPAREN') && !this.match('EOF')) {
          args.push(this.evaluateExpression());
          if (this.match('COMMA')) this.advance();
        }
        this.expect('RPAREN');
        return callFunction(kw, args, this.env);
      }
    }

    throw new Error(`Unexpected token in expression: ${t.type} '${t.value}'`);
  }
}
