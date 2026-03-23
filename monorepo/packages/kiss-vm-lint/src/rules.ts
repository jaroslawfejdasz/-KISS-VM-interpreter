import { Token, LintError, FUNCTION_KEYWORDS, STATEMENT_KEYWORDS, FUNCTION_ARITY } from './tokenizer';

export type Rule = (tokens: Token[], script: string) => LintError[];

// ─────────────────────────────────────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────────────────────────────────────
function err(code: string, message: string, pos?: number): LintError {
  return { severity: 'error', code, message, pos };
}
function warn(code: string, message: string, pos?: number): LintError {
  return { severity: 'warning', code, message, pos };
}
function info(code: string, message: string, pos?: number): LintError {
  return { severity: 'info', code, message, pos };
}

const KNOWN_GLOBALS = new Set([
  '@BLOCK','@BLOCKDIFF','@BLOCKMILLI','@COINAGE',
  '@AMOUNT','@ADDRESS','@TOKENID','@SCRIPT',
  '@TOTIN','@TOTOUT','@INCOUNT','@OUTCOUNT','@INPUT'
]);

// ─────────────────────────────────────────────────────────────────────────────
// E011: No RETURN statement
// ─────────────────────────────────────────────────────────────────────────────
export const E011_NoReturn: Rule = (tokens) => {
  const hasReturn = tokens.some(t => t.type === 'KEYWORD' && t.value === 'RETURN');
  if (!hasReturn) {
    return [err('E011', 'Script has no RETURN statement — will always throw at runtime')];
  }
  return [];
};

// ─────────────────────────────────────────────────────────────────────────────
// E020: Non-keyword (literal/number) used as statement
// ─────────────────────────────────────────────────────────────────────────────
// Statement position: start of script, or after ENDIF/ENDWHILE/THEN/ELSE/DO
const STMT_STARTERS = new Set(['ENDIF', 'ENDWHILE', 'THEN', 'ELSE', 'DO', 'ELSEIF']);
export const E020_InvalidStatement: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type !== 'NUMBER' && t.type !== 'HEX' && t.type !== 'STRING') continue;
    const prev = tokens[i - 1];
    const isStatementPos = !prev || (prev.type === 'KEYWORD' && STMT_STARTERS.has(prev.value));
    if (isStatementPos) {
      errors.push(err('E020', `Literal '${t.value}' cannot be used as a statement — expected LET, IF, WHILE, RETURN, etc.`, t.pos));
    }
  }
  return errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// E021: Function call used as statement (not inside expression)
// ─────────────────────────────────────────────────────────────────────────────
// OK contexts: after RETURN, ASSERT, IF, ELSEIF, LET, =, operators, (, ,
const EXPR_CONTEXT_PREV = new Set([
  'RETURN', 'ASSERT', 'IF', 'ELSEIF', 'LET', 'THEN', 'ELSE', 'DO',
  'AND', 'OR', 'NOT', 'XOR', 'NAND', 'NOR',
  'EQ', 'NEQ', 'LT', 'GT', 'LTE', 'GTE',
]);
export const E021_FunctionAsStatement: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type !== 'KEYWORD' || !FUNCTION_KEYWORDS.has(t.value)) continue;
    if (tokens[i + 1]?.type !== 'LPAREN') continue; // only flag fn(...) form
    const prev = tokens[i - 1];
    // Safe contexts: inside expression (after keyword/operator/paren/comma)
    if (!prev) {
      errors.push(err('E021', `Function '${t.value}(...)' used as a statement — did you mean RETURN ${t.value}(...)?`, t.pos));
      continue;
    }
    const prevOk = prev.type === 'LPAREN'
      || prev.type === 'COMMA'
      || (prev.type === 'OPERATOR')
      || (prev.type === 'KEYWORD' && EXPR_CONTEXT_PREV.has(prev.value))
      || (prev.type === 'KEYWORD' && FUNCTION_KEYWORDS.has(prev.value)); // nested fn
    if (!prevOk && prev.type === 'KEYWORD' && (prev.value === 'ENDIF' || prev.value === 'ENDWHILE')) {
      errors.push(err('E021', `Function '${t.value}(...)' used as a statement after ${prev.value} — did you mean RETURN ${t.value}(...)?`, t.pos));
    }
  }
  return errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// E030: LET without valid variable name (got keyword or missing)
// ─────────────────────────────────────────────────────────────────────────────
export const E030_LetInvalid: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && t.value === 'LET') {
      const next = tokens[i + 1];
      if (!next || next.type === 'EOF') {
        errors.push(err('E030', 'LET statement missing variable name', t.pos));
      } else if (next.type === 'OPERATOR' && next.value === '=') {
        errors.push(err('E030', "LET missing variable name — syntax: LET <name> = <value>", t.pos));
      } else if (next.type === 'KEYWORD') {
        errors.push(err('E030', `LET cannot use keyword '${next.value}' as variable name`, next.pos));
      } else if (next.type !== 'VARIABLE') {
        errors.push(err('E030', `LET expects a variable name, got '${next.value}'`, next.pos));
      }
    }
  }
  return errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// E040: IF without THEN
// ─────────────────────────────────────────────────────────────────────────────
export const E040_IfWithoutThen: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && (t.value === 'IF' || t.value === 'ELSEIF')) {
      // Scan ahead past the condition to find THEN
      let j = i + 1;
      let depth = 0;
      let hasThen = false;
      while (j < tokens.length && tokens[j].type !== 'EOF') {
        const tk = tokens[j];
        if (tk.type === 'LPAREN') depth++;
        else if (tk.type === 'RPAREN') depth--;
        else if (depth === 0 && tk.type === 'KEYWORD' && tk.value === 'THEN') { hasThen = true; break; }
        else if (depth === 0 && tk.type === 'KEYWORD' && (STATEMENT_KEYWORDS.has(tk.value) || tk.value === 'ENDIF' || tk.value === 'ELSE')) break;
        j++;
      }
      if (!hasThen) {
        errors.push(err('E040', `'${t.value}' condition must be followed by THEN`, t.pos));
      }
    }
  }
  return errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// E042: IF without matching ENDIF
// ─────────────────────────────────────────────────────────────────────────────
export const E042_IfWithoutEndif: Rule = (tokens) => {
  let depth = 0;
  for (const t of tokens) {
    if (t.type === 'KEYWORD' && t.value === 'IF') depth++;
    if (t.type === 'KEYWORD' && t.value === 'ENDIF') depth--;
  }
  if (depth > 0) {
    return [err('E042', `${depth} unclosed IF block(s) — missing ENDIF`)];
  }
  return [];
};

// ─────────────────────────────────────────────────────────────────────────────
// E050: WHILE without DO
// ─────────────────────────────────────────────────────────────────────────────
export const E050_WhileWithoutDo: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && t.value === 'WHILE') {
      let j = i + 1;
      let depth = 0;
      let hasDo = false;
      while (j < tokens.length && tokens[j].type !== 'EOF') {
        const tk = tokens[j];
        if (tk.type === 'LPAREN') depth++;
        else if (tk.type === 'RPAREN') depth--;
        else if (depth === 0 && tk.type === 'KEYWORD' && tk.value === 'DO') { hasDo = true; break; }
        else if (depth === 0 && tk.type === 'KEYWORD' && (tk.value === 'ENDWHILE' || tk.value === 'RETURN')) break;
        j++;
      }
      if (!hasDo) {
        errors.push(err('E050', 'WHILE condition must be followed by DO', t.pos));
      }
    }
  }
  return errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// E051: WHILE without ENDWHILE
// ─────────────────────────────────────────────────────────────────────────────
export const E051_WhileWithoutEndwhile: Rule = (tokens) => {
  let depth = 0;
  for (const t of tokens) {
    if (t.type === 'KEYWORD' && t.value === 'WHILE') depth++;
    if (t.type === 'KEYWORD' && t.value === 'ENDWHILE') depth--;
  }
  if (depth > 0) {
    return [err('E051', `${depth} unclosed WHILE block(s) — missing ENDWHILE`)];
  }
  return [];
};

// ─────────────────────────────────────────────────────────────────────────────
// E060: Division by zero (static)
// ─────────────────────────────────────────────────────────────────────────────
export const E060_DivisionByZero: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'OPERATOR' && t.value === '/') {
      const next = tokens[i + 1];
      if (next && next.type === 'NUMBER' && parseFloat(next.value) === 0) {
        errors.push(err('E060', 'Division by zero detected', t.pos));
      }
    }
  }
  return errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// E080: Function used without parentheses
// ─────────────────────────────────────────────────────────────────────────────
export const E080_FunctionWithoutParens: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && FUNCTION_KEYWORDS.has(t.value)) {
      const next = tokens[i + 1];
      if (!next || next.type !== 'LPAREN') {
        // Exclude operator-style keywords: AND, OR, NOT, EQ, NEQ, etc.
        const operatorStyle = new Set(['AND','OR','NOT','XOR','NAND','NOR','EQ','NEQ','LT','GT','LTE','GTE',
          'SIGNEDBY','CHECKSIG','MULTISIG','TRUE','FALSE']);
        if (!operatorStyle.has(t.value)) {
          errors.push(err('E080', `Function '${t.value}' must be called with parentheses: ${t.value}(...)`, t.pos));
        }
      }
    }
  }
  return errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// E081: Unclosed parenthesis in function call
// ─────────────────────────────────────────────────────────────────────────────
export const E081_UnclosedParen: Rule = (tokens) => {
  let depth = 0;
  for (const t of tokens) {
    if (t.type === 'LPAREN') depth++;
    if (t.type === 'RPAREN') depth--;
  }
  if (depth > 0) return [err('E081', `${depth} unclosed parenthesis — missing ')'`)];
  if (depth < 0) return [err('E081', `${Math.abs(depth)} unexpected ')' — missing matching '('`)];
  return [];
};

// ─────────────────────────────────────────────────────────────────────────────
// E082/E083: Wrong number of arguments to known functions
// ─────────────────────────────────────────────────────────────────────────────
export const E082_WrongArgCount: Rule = (tokens) => {
  const errors: LintError[] = [];

  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type !== 'KEYWORD' || !FUNCTION_KEYWORDS.has(t.value)) continue;
    if (tokens[i + 1]?.type !== 'LPAREN') continue;

    const arity = FUNCTION_ARITY[t.value];
    if (!arity) continue;

    // Count arguments
    let j = i + 2;
    let depth = 1;
    let argCount = 0;
    let hasAnyArg = false;

    while (j < tokens.length && depth > 0) {
      const tk = tokens[j];
      if (tk.type === 'LPAREN') depth++;
      else if (tk.type === 'RPAREN') {
        depth--;
        if (depth === 0) { if (hasAnyArg) argCount++; break; }
      }
      else if (tk.type === 'COMMA' && depth === 1) {
        argCount++;
        hasAnyArg = true;
      }
      else if (tk.type !== 'EOF') {
        hasAnyArg = true;
      }
      j++;
    }
    if (hasAnyArg && argCount === 0) argCount = 1;

    const [min, max] = arity;
    if (t.value === 'MULTISIG') {
      // MULTISIG(n, key1, key2, ...) — min 3 args: n + at least 2 keys (1-of-1 multisig is pointless, use SIGNEDBY)
      if (argCount < 3) {
        errors.push(err('E082', `MULTISIG requires at least 3 arguments: MULTISIG(n, key1, key2, ...) — got ${argCount}`, t.pos));
      }
      continue;
    }
    if (argCount < min) {
      errors.push(err('E082', `'${t.value}' requires at least ${min} argument(s) — got ${argCount}`, t.pos));
    } else if (max !== -1 && argCount > max) {
      errors.push(err('E083', `'${t.value}' takes at most ${max} argument(s) — got ${argCount}`, t.pos));
    }
  }
  return errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// W010: Unused variable (assigned with LET but never read)
// ─────────────────────────────────────────────────────────────────────────────
export const W010_UnusedVariable: Rule = (tokens) => {
  const warnings: LintError[] = [];
  const defined = new Map<string, number>(); // varName -> position of LET

  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && t.value === 'LET') {
      const next = tokens[i + 1];
      if (next && next.type === 'VARIABLE') {
        defined.set(next.value, next.pos ?? i);
      }
    }
  }

  for (const [varName, defPos] of defined) {
    // Check if it's used (appears as VARIABLE not immediately after LET)
    let used = false;
    for (let i = 0; i < tokens.length; i++) {
      const prev = tokens[i - 1];
      if (tokens[i].type === 'VARIABLE' && tokens[i].value === varName) {
        if (!prev || !(prev.type === 'KEYWORD' && prev.value === 'LET')) {
          used = true;
          break;
        }
      }
    }
    if (!used) {
      warnings.push(warn('W010', `Variable '${varName}' is assigned but never used`, defPos));
    }
  }
  return warnings;
};

// ─────────────────────────────────────────────────────────────────────────────
// W040: Dead code after top-level RETURN
// ─────────────────────────────────────────────────────────────────────────────
export const W040_DeadCode: Rule = (tokens) => {
  const warnings: LintError[] = [];
  let depth = 0;
  let topLevelReturn = false;

  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD') {
      if (t.value === 'IF' || t.value === 'WHILE') depth++;
      if (t.value === 'ENDIF' || t.value === 'ENDWHILE') depth--;
      if (t.value === 'RETURN' && depth === 0) { topLevelReturn = true; continue; }
    }
    if (topLevelReturn && depth === 0 && t.type !== 'EOF') {
      if (t.type === 'KEYWORD' && STATEMENT_KEYWORDS.has(t.value)) {
        warnings.push(warn('W040', `Dead code: '${t.value}' after top-level RETURN will never execute`, t.pos));
        break;
      }
    }
  }
  return warnings;
};

// ─────────────────────────────────────────────────────────────────────────────
// W050: Using = operator inside expression (suggest EQ)
// ─────────────────────────────────────────────────────────────────────────────
export const W050_AssignmentInExpr: Rule = (tokens) => {
  const warnings: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'OPERATOR' && t.value === '=') {
      // Is it assignment context (LET <var> =) or comparison context?
      const prev1 = tokens[i - 1];
      const prev2 = tokens[i - 2];
      const isLetAssign = prev1?.type === 'VARIABLE' && prev2?.type === 'KEYWORD' && prev2?.value === 'LET';
      if (!isLetAssign) {
        warnings.push(warn('W050', "Use EQ for comparison, not '=' — e.g. RETURN x EQ 5 instead of RETURN x = 5", t.pos));
      }
    }
  }
  return warnings;
};

// ─────────────────────────────────────────────────────────────────────────────
// W060: Unknown @GLOBAL variable
// ─────────────────────────────────────────────────────────────────────────────
export const W060_UnknownGlobal: Rule = (tokens) => {
  const warnings: LintError[] = [];
  for (const t of tokens) {
    if (t.type === 'GLOBAL' && !KNOWN_GLOBALS.has(t.value)) {
      warnings.push(warn('W060', `Unknown global variable '${t.value}' — known globals: ${[...KNOWN_GLOBALS].join(', ')}`, t.pos));
    }
  }
  return warnings;
};

// ─────────────────────────────────────────────────────────────────────────────
// W070: Variable used before LET
// ─────────────────────────────────────────────────────────────────────────────
export const W070_UseBeforeLet: Rule = (tokens) => {
  const warnings: LintError[] = [];
  const defined = new Set<string>();

  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && t.value === 'LET') {
      const next = tokens[i + 1];
      if (next?.type === 'VARIABLE') {
        defined.add(next.value);
        i++;
        continue;
      }
    }
    if (t.type === 'VARIABLE') {
      const prev = tokens[i - 1];
      if (prev?.type === 'KEYWORD' && prev.value === 'LET') continue;
      if (!defined.has(t.value)) {
        warnings.push(warn('W070', `Variable '${t.value}' used before being defined with LET`, t.pos));
        defined.add(t.value); // warn once
      }
    }
  }
  return warnings;
};

// ─────────────────────────────────────────────────────────────────────────────
// INFO: R004 — instruction estimate approaching limit
// ─────────────────────────────────────────────────────────────────────────────
export const R004_InstructionLimit: Rule = (tokens) => {
  const count = tokens.filter(t => t.type === 'KEYWORD' && STATEMENT_KEYWORDS.has(t.value)).length;
  if (count > 900) return [err('R004', `Estimated ~${count} instructions — at or over the 1024 limit`)];
  if (count > 700) return [warn('R004', `Estimated ~${count} instructions — approaching the 1024 limit`)];
  return [];
};

// ─────────────────────────────────────────────────────────────────────────────
// INFO: R006 — CHECKSIG informational note
// ─────────────────────────────────────────────────────────────────────────────
export const R006_ChecksigNote: Rule = (tokens) => {
  if (tokens.some(t => t.type === 'KEYWORD' && t.value === 'CHECKSIG')) {
    return [info('R006', 'CHECKSIG performs raw EC signature verification (secp256k1) — ensure correct data/sig format')];
  }
  return [];
};

// ─────────────────────────────────────────────────────────────────────────────
// EXPORT ALL
// ─────────────────────────────────────────────────────────────────────────────
// W080: Potential infinite loop — WHILE with literal TRUE or non-zero constant
export const W080_InfiniteLoop: Rule = (tokens) => {
  const issues: LintError[] = [];
  for (let i = 0; i < tokens.length - 2; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && t.value === 'WHILE') {
      const next = tokens[i + 1];
      if (
        (next.type === 'BOOLEAN' && next.value === 'TRUE') ||
        (next.type === 'NUMBER' && parseFloat(next.value) !== 0)
      ) {
        issues.push(warn('W080', `Potential infinite loop: WHILE ${next.value} — condition may never be false`, t.pos));
      }
    }
  }
  return issues;
};



// ─────────────────────────────────────────────────────────────────────────────
// W091: MULTISIG impossible — n > number of provided keys
// ─────────────────────────────────────────────────────────────────────────────
export const W091_MultisigImpossible: Rule = (tokens) => {
  const issues: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type !== 'KEYWORD' || t.value !== 'MULTISIG') continue;
    if (tokens[i + 1]?.type !== 'LPAREN') continue;
    // Count args
    let j = i + 2;
    let depth = 1;
    let argCount = 1;
    while (j < tokens.length && depth > 0) {
      const tk = tokens[j];
      if (tk.type === 'LPAREN') depth++;
      else if (tk.type === 'RPAREN') { depth--; if (depth === 0) break; }
      else if (tk.type === 'COMMA' && depth === 1) argCount++;
      j++;
    }
    if (argCount < 3) continue; // already caught by E082 (min 3 args required)
    // First arg must be a number literal for static analysis
    const firstArg = tokens[i + 2];
    if (firstArg?.type !== 'NUMBER') continue;
    const required = parseInt(firstArg.value, 10);
    const keyCount = argCount - 1; // args minus the 'n' param
    if (required > keyCount) {
      issues.push(warn('W091',
        `MULTISIG(${required}, ...) requires ${required} signatures but only ${keyCount} key(s) provided — can never pass`,
        t.pos));
    }
  }
  return issues;
};


// W090: No authorization check — anyone can spend this coin
// Scripts that return TRUE without any SIGNEDBY/CHECKSIG/MULTISIG are dangerous
export const W090_NoAuthCheck: Rule = (tokens) => {
  const issues: LintError[] = [];
  const authFns = new Set(['SIGNEDBY','CHECKSIG','MULTISIG']);
  const hasAuth = tokens.some(t => t.type === 'KEYWORD' && authFns.has(t.value));
  if (!hasAuth) {
    issues.push(warn('W090',
      'No authorization check found (SIGNEDBY/CHECKSIG/MULTISIG) — this coin can be spent by anyone',
      0));
  }
  return issues;
};

export const ALL_RULES: Rule[] = [
  E011_NoReturn,
  E020_InvalidStatement,
  E021_FunctionAsStatement,
  E030_LetInvalid,
  E040_IfWithoutThen,
  E042_IfWithoutEndif,
  E050_WhileWithoutDo,
  E051_WhileWithoutEndwhile,
  E060_DivisionByZero,
  E080_FunctionWithoutParens,
  E081_UnclosedParen,
  E082_WrongArgCount,
  W090_NoAuthCheck,
  W010_UnusedVariable,
  W040_DeadCode,
  W050_AssignmentInExpr,
  W060_UnknownGlobal,
  W070_UseBeforeLet,
  R004_InstructionLimit,
  R006_ChecksigNote,
  W080_InfiniteLoop,
  W091_MultisigImpossible,
];
