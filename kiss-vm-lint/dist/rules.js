"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ALL_RULES = exports.R006_ChecksigNote = exports.R004_InstructionLimit = exports.W070_UseBeforeLet = exports.W060_UnknownGlobal = exports.W050_AssignmentInExpr = exports.W040_DeadCode = exports.W010_UnusedVariable = exports.E082_WrongArgCount = exports.E081_UnclosedParen = exports.E080_FunctionWithoutParens = exports.E060_DivisionByZero = exports.E051_WhileWithoutEndwhile = exports.E050_WhileWithoutDo = exports.E042_IfWithoutEndif = exports.E040_IfWithoutThen = exports.E030_LetInvalid = exports.E021_FunctionAsStatement = exports.E020_InvalidStatement = exports.E011_NoReturn = void 0;
const tokenizer_1 = require("./tokenizer");
// ─────────────────────────────────────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────────────────────────────────────
function err(code, message, pos) {
    return { severity: 'error', code, message, pos };
}
function warn(code, message, pos) {
    return { severity: 'warning', code, message, pos };
}
function info(code, message, pos) {
    return { severity: 'info', code, message, pos };
}
const KNOWN_GLOBALS = new Set([
    '@BLOCK', '@BLOCKDIFF', '@BLOCKMILLI', '@COINAGE',
    '@AMOUNT', '@ADDRESS', '@TOKENID', '@SCRIPT',
    '@TOTIN', '@TOTOUT', '@INCOUNT', '@OUTCOUNT', '@INPUT'
]);
// ─────────────────────────────────────────────────────────────────────────────
// E011: No RETURN statement
// ─────────────────────────────────────────────────────────────────────────────
const E011_NoReturn = (tokens) => {
    const hasReturn = tokens.some(t => t.type === 'KEYWORD' && t.value === 'RETURN');
    if (!hasReturn) {
        return [err('E011', 'Script has no RETURN statement — will always throw at runtime')];
    }
    return [];
};
exports.E011_NoReturn = E011_NoReturn;
// ─────────────────────────────────────────────────────────────────────────────
// E020: Non-keyword (literal/number) used as statement
// ─────────────────────────────────────────────────────────────────────────────
// Statement position: start of script, or after ENDIF/ENDWHILE/THEN/ELSE/DO
const STMT_STARTERS = new Set(['ENDIF', 'ENDWHILE', 'THEN', 'ELSE', 'DO', 'ELSEIF']);
const E020_InvalidStatement = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type !== 'NUMBER' && t.type !== 'HEX' && t.type !== 'STRING')
            continue;
        const prev = tokens[i - 1];
        const isStatementPos = !prev || (prev.type === 'KEYWORD' && STMT_STARTERS.has(prev.value));
        if (isStatementPos) {
            errors.push(err('E020', `Literal '${t.value}' cannot be used as a statement — expected LET, IF, WHILE, RETURN, etc.`, t.pos));
        }
    }
    return errors;
};
exports.E020_InvalidStatement = E020_InvalidStatement;
// ─────────────────────────────────────────────────────────────────────────────
// E021: Function call used as statement (not inside expression)
// ─────────────────────────────────────────────────────────────────────────────
// OK contexts: after RETURN, ASSERT, IF, ELSEIF, LET, =, operators, (, ,
const EXPR_CONTEXT_PREV = new Set([
    'RETURN', 'ASSERT', 'IF', 'ELSEIF', 'LET', 'THEN', 'ELSE', 'DO',
    'AND', 'OR', 'NOT', 'XOR', 'NAND', 'NOR',
    'EQ', 'NEQ', 'LT', 'GT', 'LTE', 'GTE',
]);
const E021_FunctionAsStatement = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type !== 'KEYWORD' || !tokenizer_1.FUNCTION_KEYWORDS.has(t.value))
            continue;
        if (tokens[i + 1]?.type !== 'LPAREN')
            continue; // only flag fn(...) form
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
            || (prev.type === 'KEYWORD' && tokenizer_1.FUNCTION_KEYWORDS.has(prev.value)); // nested fn
        if (!prevOk && prev.type === 'KEYWORD' && (prev.value === 'ENDIF' || prev.value === 'ENDWHILE')) {
            errors.push(err('E021', `Function '${t.value}(...)' used as a statement after ${prev.value} — did you mean RETURN ${t.value}(...)?`, t.pos));
        }
    }
    return errors;
};
exports.E021_FunctionAsStatement = E021_FunctionAsStatement;
// ─────────────────────────────────────────────────────────────────────────────
// E030: LET without valid variable name (got keyword or missing)
// ─────────────────────────────────────────────────────────────────────────────
const E030_LetInvalid = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD' && t.value === 'LET') {
            const next = tokens[i + 1];
            if (!next || next.type === 'EOF') {
                errors.push(err('E030', 'LET statement missing variable name', t.pos));
            }
            else if (next.type === 'OPERATOR' && next.value === '=') {
                errors.push(err('E030', "LET missing variable name — syntax: LET <name> = <value>", t.pos));
            }
            else if (next.type === 'KEYWORD') {
                errors.push(err('E030', `LET cannot use keyword '${next.value}' as variable name`, next.pos));
            }
            else if (next.type !== 'VARIABLE') {
                errors.push(err('E030', `LET expects a variable name, got '${next.value}'`, next.pos));
            }
        }
    }
    return errors;
};
exports.E030_LetInvalid = E030_LetInvalid;
// ─────────────────────────────────────────────────────────────────────────────
// E040: IF without THEN
// ─────────────────────────────────────────────────────────────────────────────
const E040_IfWithoutThen = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD' && (t.value === 'IF' || t.value === 'ELSEIF')) {
            // Scan ahead past the condition to find THEN
            let j = i + 1;
            let depth = 0;
            let hasThen = false;
            while (j < tokens.length && tokens[j].type !== 'EOF') {
                const tk = tokens[j];
                if (tk.type === 'LPAREN')
                    depth++;
                else if (tk.type === 'RPAREN')
                    depth--;
                else if (depth === 0 && tk.type === 'KEYWORD' && tk.value === 'THEN') {
                    hasThen = true;
                    break;
                }
                else if (depth === 0 && tk.type === 'KEYWORD' && (tokenizer_1.STATEMENT_KEYWORDS.has(tk.value) || tk.value === 'ENDIF' || tk.value === 'ELSE'))
                    break;
                j++;
            }
            if (!hasThen) {
                errors.push(err('E040', `'${t.value}' condition must be followed by THEN`, t.pos));
            }
        }
    }
    return errors;
};
exports.E040_IfWithoutThen = E040_IfWithoutThen;
// ─────────────────────────────────────────────────────────────────────────────
// E042: IF without matching ENDIF
// ─────────────────────────────────────────────────────────────────────────────
const E042_IfWithoutEndif = (tokens) => {
    let depth = 0;
    for (const t of tokens) {
        if (t.type === 'KEYWORD' && t.value === 'IF')
            depth++;
        if (t.type === 'KEYWORD' && t.value === 'ENDIF')
            depth--;
    }
    if (depth > 0) {
        return [err('E042', `${depth} unclosed IF block(s) — missing ENDIF`)];
    }
    return [];
};
exports.E042_IfWithoutEndif = E042_IfWithoutEndif;
// ─────────────────────────────────────────────────────────────────────────────
// E050: WHILE without DO
// ─────────────────────────────────────────────────────────────────────────────
const E050_WhileWithoutDo = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD' && t.value === 'WHILE') {
            let j = i + 1;
            let depth = 0;
            let hasDo = false;
            while (j < tokens.length && tokens[j].type !== 'EOF') {
                const tk = tokens[j];
                if (tk.type === 'LPAREN')
                    depth++;
                else if (tk.type === 'RPAREN')
                    depth--;
                else if (depth === 0 && tk.type === 'KEYWORD' && tk.value === 'DO') {
                    hasDo = true;
                    break;
                }
                else if (depth === 0 && tk.type === 'KEYWORD' && (tk.value === 'ENDWHILE' || tk.value === 'RETURN'))
                    break;
                j++;
            }
            if (!hasDo) {
                errors.push(err('E050', 'WHILE condition must be followed by DO', t.pos));
            }
        }
    }
    return errors;
};
exports.E050_WhileWithoutDo = E050_WhileWithoutDo;
// ─────────────────────────────────────────────────────────────────────────────
// E051: WHILE without ENDWHILE
// ─────────────────────────────────────────────────────────────────────────────
const E051_WhileWithoutEndwhile = (tokens) => {
    let depth = 0;
    for (const t of tokens) {
        if (t.type === 'KEYWORD' && t.value === 'WHILE')
            depth++;
        if (t.type === 'KEYWORD' && t.value === 'ENDWHILE')
            depth--;
    }
    if (depth > 0) {
        return [err('E051', `${depth} unclosed WHILE block(s) — missing ENDWHILE`)];
    }
    return [];
};
exports.E051_WhileWithoutEndwhile = E051_WhileWithoutEndwhile;
// ─────────────────────────────────────────────────────────────────────────────
// E060: Division by zero (static)
// ─────────────────────────────────────────────────────────────────────────────
const E060_DivisionByZero = (tokens) => {
    const errors = [];
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
exports.E060_DivisionByZero = E060_DivisionByZero;
// ─────────────────────────────────────────────────────────────────────────────
// E080: Function used without parentheses
// ─────────────────────────────────────────────────────────────────────────────
const E080_FunctionWithoutParens = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD' && tokenizer_1.FUNCTION_KEYWORDS.has(t.value)) {
            const next = tokens[i + 1];
            if (!next || next.type !== 'LPAREN') {
                // Exclude operator-style keywords: AND, OR, NOT, EQ, NEQ, etc.
                const operatorStyle = new Set(['AND', 'OR', 'NOT', 'XOR', 'NAND', 'NOR', 'EQ', 'NEQ', 'LT', 'GT', 'LTE', 'GTE',
                    'SIGNEDBY', 'CHECKSIG', 'MULTISIG', 'TRUE', 'FALSE']);
                if (!operatorStyle.has(t.value)) {
                    errors.push(err('E080', `Function '${t.value}' must be called with parentheses: ${t.value}(...)`, t.pos));
                }
            }
        }
    }
    return errors;
};
exports.E080_FunctionWithoutParens = E080_FunctionWithoutParens;
// ─────────────────────────────────────────────────────────────────────────────
// E081: Unclosed parenthesis in function call
// ─────────────────────────────────────────────────────────────────────────────
const E081_UnclosedParen = (tokens) => {
    let depth = 0;
    for (const t of tokens) {
        if (t.type === 'LPAREN')
            depth++;
        if (t.type === 'RPAREN')
            depth--;
    }
    if (depth > 0)
        return [err('E081', `${depth} unclosed parenthesis — missing ')'`)];
    if (depth < 0)
        return [err('E081', `${Math.abs(depth)} unexpected ')' — missing matching '('`)];
    return [];
};
exports.E081_UnclosedParen = E081_UnclosedParen;
// ─────────────────────────────────────────────────────────────────────────────
// E082/E083: Wrong number of arguments to known functions
// ─────────────────────────────────────────────────────────────────────────────
const E082_WrongArgCount = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type !== 'KEYWORD' || !tokenizer_1.FUNCTION_KEYWORDS.has(t.value))
            continue;
        if (tokens[i + 1]?.type !== 'LPAREN')
            continue;
        const arity = tokenizer_1.FUNCTION_ARITY[t.value];
        if (!arity)
            continue;
        // Count arguments
        let j = i + 2;
        let depth = 1;
        let argCount = 0;
        let hasAnyArg = false;
        while (j < tokens.length && depth > 0) {
            const tk = tokens[j];
            if (tk.type === 'LPAREN')
                depth++;
            else if (tk.type === 'RPAREN') {
                depth--;
                if (depth === 0) {
                    if (hasAnyArg)
                        argCount++;
                    break;
                }
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
        if (hasAnyArg && argCount === 0)
            argCount = 1;
        const [min, max] = arity;
        if (t.value === 'MULTISIG') {
            // MULTISIG(n, key1, key2...) — min 3 args: n + at least 2 keys
            if (argCount < 3) {
                errors.push(err('E082', `MULTISIG requires at least 3 arguments: MULTISIG(n, key1, key2, ...) — got ${argCount}`, t.pos));
            }
            continue;
        }
        if (argCount < min) {
            errors.push(err('E082', `'${t.value}' requires at least ${min} argument(s) — got ${argCount}`, t.pos));
        }
        else if (max !== -1 && argCount > max) {
            errors.push(err('E083', `'${t.value}' takes at most ${max} argument(s) — got ${argCount}`, t.pos));
        }
    }
    return errors;
};
exports.E082_WrongArgCount = E082_WrongArgCount;
// ─────────────────────────────────────────────────────────────────────────────
// W010: Unused variable (assigned with LET but never read)
// ─────────────────────────────────────────────────────────────────────────────
const W010_UnusedVariable = (tokens) => {
    const warnings = [];
    const defined = new Map(); // varName -> position of LET
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
exports.W010_UnusedVariable = W010_UnusedVariable;
// ─────────────────────────────────────────────────────────────────────────────
// W040: Dead code after top-level RETURN
// ─────────────────────────────────────────────────────────────────────────────
const W040_DeadCode = (tokens) => {
    const warnings = [];
    let depth = 0;
    let topLevelReturn = false;
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD') {
            if (t.value === 'IF' || t.value === 'WHILE')
                depth++;
            if (t.value === 'ENDIF' || t.value === 'ENDWHILE')
                depth--;
            if (t.value === 'RETURN' && depth === 0) {
                topLevelReturn = true;
                continue;
            }
        }
        if (topLevelReturn && depth === 0 && t.type !== 'EOF') {
            if (t.type === 'KEYWORD' && tokenizer_1.STATEMENT_KEYWORDS.has(t.value)) {
                warnings.push(warn('W040', `Dead code: '${t.value}' after top-level RETURN will never execute`, t.pos));
                break;
            }
        }
    }
    return warnings;
};
exports.W040_DeadCode = W040_DeadCode;
// ─────────────────────────────────────────────────────────────────────────────
// W050: Using = operator inside expression (suggest EQ)
// ─────────────────────────────────────────────────────────────────────────────
const W050_AssignmentInExpr = (tokens) => {
    const warnings = [];
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
exports.W050_AssignmentInExpr = W050_AssignmentInExpr;
// ─────────────────────────────────────────────────────────────────────────────
// W060: Unknown @GLOBAL variable
// ─────────────────────────────────────────────────────────────────────────────
const W060_UnknownGlobal = (tokens) => {
    const warnings = [];
    for (const t of tokens) {
        if (t.type === 'GLOBAL' && !KNOWN_GLOBALS.has(t.value)) {
            warnings.push(warn('W060', `Unknown global variable '${t.value}' — known globals: ${[...KNOWN_GLOBALS].join(', ')}`, t.pos));
        }
    }
    return warnings;
};
exports.W060_UnknownGlobal = W060_UnknownGlobal;
// ─────────────────────────────────────────────────────────────────────────────
// W070: Variable used before LET
// ─────────────────────────────────────────────────────────────────────────────
const W070_UseBeforeLet = (tokens) => {
    const warnings = [];
    const defined = new Set();
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
            if (prev?.type === 'KEYWORD' && prev.value === 'LET')
                continue;
            if (!defined.has(t.value)) {
                warnings.push(warn('W070', `Variable '${t.value}' used before being defined with LET`, t.pos));
                defined.add(t.value); // warn once
            }
        }
    }
    return warnings;
};
exports.W070_UseBeforeLet = W070_UseBeforeLet;
// ─────────────────────────────────────────────────────────────────────────────
// INFO: R004 — instruction estimate approaching limit
// ─────────────────────────────────────────────────────────────────────────────
const R004_InstructionLimit = (tokens) => {
    const count = tokens.filter(t => t.type === 'KEYWORD' && tokenizer_1.STATEMENT_KEYWORDS.has(t.value)).length;
    if (count > 900)
        return [err('R004', `Estimated ~${count} instructions — at or over the 1024 limit`)];
    if (count > 700)
        return [warn('R004', `Estimated ~${count} instructions — approaching the 1024 limit`)];
    return [];
};
exports.R004_InstructionLimit = R004_InstructionLimit;
// ─────────────────────────────────────────────────────────────────────────────
// INFO: R006 — CHECKSIG informational note
// ─────────────────────────────────────────────────────────────────────────────
const R006_ChecksigNote = (tokens) => {
    if (tokens.some(t => t.type === 'KEYWORD' && t.value === 'CHECKSIG')) {
        return [info('R006', 'CHECKSIG performs raw EC signature verification (secp256k1) — ensure correct data/sig format')];
    }
    return [];
};
exports.R006_ChecksigNote = R006_ChecksigNote;
// ─────────────────────────────────────────────────────────────────────────────
// EXPORT ALL
// ─────────────────────────────────────────────────────────────────────────────

// W051 — Potential infinite loop
const W051_InfiniteLoop = (tokens) => {
    const warnings = [];
    for (let i = 0; i < tokens.length - 1; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD' && t.value === 'WHILE') {
            const next = tokens[i + 1];
            if (next && next.type === 'BOOLEAN' && next.value === 'TRUE') {
                warnings.push(warn('W051', 'WHILE TRUE — potential infinite loop. Ensure there is a RETURN inside the body.', t.pos));
            }
        }
    }
    return warnings;
};
exports.W051_InfiniteLoop = W051_InfiniteLoop;
exports.ALL_RULES = [
    exports.E011_NoReturn,
    exports.E020_InvalidStatement,
    exports.E021_FunctionAsStatement,
    exports.E030_LetInvalid,
    exports.E040_IfWithoutThen,
    exports.E042_IfWithoutEndif,
    exports.E050_WhileWithoutDo,
    exports.E051_WhileWithoutEndwhile,
    exports.E060_DivisionByZero,
    exports.E080_FunctionWithoutParens,
    exports.E081_UnclosedParen,
    exports.E082_WrongArgCount,
    exports.W010_UnusedVariable,
    exports.W040_DeadCode,
    exports.W050_AssignmentInExpr,
    exports.W060_UnknownGlobal,
    exports.W070_UseBeforeLet,
    exports.R004_InstructionLimit,
    exports.R006_ChecksigNote,
    W051_InfiniteLoop,
];
//# sourceMappingURL=rules.js.map