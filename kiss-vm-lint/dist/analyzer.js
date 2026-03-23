"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.analyze = analyze;
const tokenizer_1 = require("./tokenizer");
function pk(s) { return s.tokens[s.pos] ?? { type: 'EOF', value: '', pos: -1 }; }
function adv(s) { return s.tokens[s.pos++] ?? { type: 'EOF', value: '', pos: -1 }; }
function mtch(s, type, value) {
    const t = pk(s);
    if (t.type !== type)
        return false;
    if (value && t.value.toUpperCase() !== value.toUpperCase())
        return false;
    return true;
}
function err(s, severity, code, message, pos) {
    s.errors.push({ severity, code, message, pos });
}
function tick(s) {
    s.instructionCount++;
    if (s.instructionCount > s.maxInstructions) {
        err(s, 'error', 'E010', 'Script exceeds 1024 instruction limit (estimated ' + s.instructionCount + '+)');
        s.maxInstructions = Infinity;
    }
}
function analyze(script) {
    const tokenResult = (0, tokenizer_1.tokenize)(script);
    const errors = [...tokenResult.errors];
    const state = {
        tokens: tokenResult.tokens,
        pos: 0,
        errors,
        variables: new Map(),
        ifDepth: 0,
        whileDepth: 0,
        hasReturn: false,
        instructionCount: 0,
        maxInstructions: 1024,
    };
    const fatalErrors = tokenResult.errors.filter(e => e.severity === 'error');
    if (fatalErrors.length === 0) {
        analyzeBlock(state);
        if (!state.hasReturn) {
            err(state, 'error', 'E011', 'Script has no RETURN statement');
        }
        for (const [name, info] of state.variables) {
            if (!info.used) {
                err(state, 'warning', 'W010', "Variable '" + name + "' is assigned but never used", info.pos);
            }
        }
    }
    const all = state.errors;
    return {
        errors: all.filter(e => e.severity === 'error'),
        warnings: all.filter(e => e.severity === 'warning'),
        infos: all.filter(e => e.severity === 'info'),
        all,
        summary: {
            errors: all.filter(e => e.severity === 'error').length,
            warnings: all.filter(e => e.severity === 'warning').length,
            infos: all.filter(e => e.severity === 'info').length,
            instructionEstimate: state.instructionCount,
        },
    };
}
function analyzeBlock(s) {
    while (!mtch(s, 'EOF')) {
        const t = pk(s);
        if (t.type === 'KEYWORD') {
            const kw = t.value.toUpperCase();
            if (kw === 'ENDIF' || kw === 'ELSE' || kw === 'ELSEIF' || kw === 'ENDWHILE')
                break;
        }
        analyzeStatement(s);
    }
}
function analyzeStatement(s) {
    tick(s);
    const t = pk(s);
    if (t.type !== 'KEYWORD') {
        err(s, 'error', 'E020', "Expected statement keyword, got " + t.type + " '" + t.value + "'", t.pos);
        adv(s);
        return;
    }
    const kw = t.value.toUpperCase();
    if (kw === 'LET') {
        analyzeLet(s);
        return;
    }
    if (kw === 'IF') {
        analyzeIf(s);
        return;
    }
    if (kw === 'WHILE') {
        analyzeWhile(s);
        return;
    }
    if (kw === 'RETURN') {
        analyzeReturn(s);
        return;
    }
    if (kw === 'ASSERT') {
        analyzeAssert(s);
        return;
    }
    if (kw === 'EXEC') {
        analyzeExec(s);
        return;
    }
    if (kw === 'MAST') {
        analyzeMast(s);
        return;
    }
    if (tokenizer_1.FUNCTION_KEYWORDS.has(kw)) {
        err(s, 'error', 'E021', "'" + kw + "' is a function, not a statement. Did you mean: LET x = " + kw + "(...)?", t.pos);
        adv(s);
        return;
    }
    err(s, 'error', 'E022', "Unknown statement: '" + kw + "'", t.pos);
    adv(s);
}
function analyzeLet(s) {
    const letPos = pk(s).pos;
    adv(s); // LET
    const nameToken = pk(s);
    if (nameToken.type !== 'VARIABLE') {
        err(s, 'error', 'E030', "LET requires a variable name, got " + nameToken.type + " '" + nameToken.value + "'", nameToken.pos);
        skipToNextStatement(s);
        return;
    }
    // Note: W020 (variable shadows keyword) is unreachable — the tokenizer already
    // converts all keyword-like identifiers to KEYWORD tokens, so they never reach here as VARIABLE.
    const varName = nameToken.value;
    adv(s); // variable name
    if (mtch(s, 'OPERATOR', '='))
        adv(s);
    analyzeExpr(s, varName);
    s.variables.set(varName, { pos: letPos, used: false });
}
function analyzeIf(s) {
    adv(s); // IF
    s.ifDepth++;
    analyzeExpr(s);
    if (!mtch(s, 'KEYWORD', 'THEN'))
        err(s, 'error', 'E040', 'IF condition must be followed by THEN', pk(s).pos);
    else
        adv(s);
    analyzeBlock(s);
    while (mtch(s, 'KEYWORD', 'ELSEIF')) {
        adv(s);
        analyzeExpr(s);
        if (!mtch(s, 'KEYWORD', 'THEN'))
            err(s, 'error', 'E041', 'ELSEIF condition must be followed by THEN', pk(s).pos);
        else
            adv(s);
        analyzeBlock(s);
    }
    if (mtch(s, 'KEYWORD', 'ELSE')) {
        adv(s);
        analyzeBlock(s);
    }
    if (!mtch(s, 'KEYWORD', 'ENDIF'))
        err(s, 'error', 'E042', 'IF block not closed with ENDIF', pk(s).pos);
    else
        adv(s);
    s.ifDepth--;
}
function analyzeWhile(s) {
    const whilePos = pk(s).pos;
    adv(s); // WHILE
    s.whileDepth++;
    if (s.whileDepth > 3)
        err(s, 'warning', 'W030', 'Deeply nested WHILE loop (depth ' + s.whileDepth + ')', whilePos);
    analyzeExpr(s);
    if (!mtch(s, 'KEYWORD', 'DO'))
        err(s, 'error', 'E050', 'WHILE condition must be followed by DO', pk(s).pos);
    else
        adv(s);
    analyzeBlock(s);
    if (!mtch(s, 'KEYWORD', 'ENDWHILE'))
        err(s, 'error', 'E051', 'WHILE block not closed with ENDWHILE', pk(s).pos);
    else
        adv(s);
    s.whileDepth--;
}
function analyzeReturn(s) {
    adv(s); // RETURN
    analyzeExpr(s);
    s.hasReturn = true;
    const next = pk(s);
    if (next.type !== 'EOF') {
        const kw = next.value?.toUpperCase();
        if (kw !== 'ENDIF' && kw !== 'ELSE' && kw !== 'ELSEIF' && kw !== 'ENDWHILE') {
            err(s, 'warning', 'W040', 'Dead code after RETURN statement', next.pos);
        }
    }
}
function analyzeAssert(s) { adv(s); analyzeExpr(s); }
function analyzeExec(s) {
    adv(s);
    const argPos = pk(s).pos;
    analyzeExpr(s);
    err(s, 'info', 'I001', 'EXEC runs a script at runtime — static analysis cannot inspect dynamically built scripts', argPos);
}
function analyzeMast(s) {
    adv(s);
    const argPos = pk(s).pos;
    analyzeExpr(s);
    err(s, 'info', 'I002', 'MAST executes script by hash — ensure the hash corresponds to an audited script', argPos);
}
function analyzeExpr(s, beingAssigned) { analyzeOr(s, beingAssigned); }
function analyzeOr(s, ba) {
    analyzeAnd(s, ba);
    while (mtch(s, 'KEYWORD', 'OR')) {
        adv(s);
        analyzeAnd(s);
    }
}
function analyzeAnd(s, ba) {
    analyzeNot(s, ba);
    while (mtch(s, 'KEYWORD', 'AND')) {
        adv(s);
        analyzeNot(s);
    }
}
function analyzeNot(s, ba) {
    const kw = pk(s).value?.toUpperCase();
    if (kw === 'NOT') {
        adv(s);
    }
    else if (kw === 'NAND' || kw === 'NOR' || kw === 'XOR') {
        adv(s);
        analyzeAtom(s);
    }
    analyzeComparison(s, ba);
}
function analyzeComparison(s, ba) {
    analyzeAddSub(s, ba);
    const kwOps = new Set(['EQ', 'NEQ', 'LT', 'GT', 'LTE', 'GTE']);
    while ((pk(s).type === 'KEYWORD' && kwOps.has(pk(s).value.toUpperCase())) ||
        (pk(s).type === 'OPERATOR' && ['!=', '<', '>', '<=', '>=', '='].includes(pk(s).value))) {
        const op = pk(s);
        if (op.type === 'OPERATOR' && op.value === '=') {
            err(s, 'warning', 'W050', "Use EQ instead of '=' for equality in KISS VM", op.pos);
        }
        adv(s);
        analyzeAddSub(s);
    }
}
function analyzeAddSub(s, ba) {
    analyzeMulDiv(s, ba);
    while (pk(s).type === 'OPERATOR' && (pk(s).value === '+' || pk(s).value === '-')) {
        adv(s);
        analyzeMulDiv(s);
    }
}
function analyzeMulDiv(s, ba) {
    analyzeAtom(s, ba);
    while (pk(s).type === 'OPERATOR' && ['*', '/', '%'].includes(pk(s).value)) {
        const op = adv(s);
        if (op.value === '/') {
            const next = pk(s);
            if (next.type === 'NUMBER' && next.value === '0')
                err(s, 'error', 'E060', 'Division by zero', next.pos);
        }
        analyzeAtom(s);
    }
}
function analyzeAtom(s, ba) {
    const t = pk(s);
    if (t.type === 'NUMBER' || t.type === 'HEX' || t.type === 'STRING' || t.type === 'BOOLEAN') {
        adv(s);
        return;
    }
    if (t.type === 'GLOBAL') {
        adv(s);
        const validGlobals = new Set(['@BLOCK', '@BLOCKMILLI', '@CREATED', '@COINAGE', '@SCRIPT', '@ADDRESS', '@AMOUNT', '@TOKENID', '@COINID', '@TOTIN', '@TOTOUT', '@INBLKDIFF']);
        if (!validGlobals.has(t.value.toUpperCase()))
            err(s, 'warning', 'W060', "Unknown global '" + t.value + "'", t.pos);
        return;
    }
    if (t.type === 'VARIABLE') {
        adv(s);
        const info = s.variables.get(t.value);
        if (info && t.value !== ba)
            info.used = true;
        else if (!info)
            err(s, 'warning', 'W070', "Variable '" + t.value + "' used before LET", t.pos);
        return;
    }
    if (t.type === 'KEYWORD' && tokenizer_1.FUNCTION_KEYWORDS.has(t.value.toUpperCase())) {
        analyzeFn(s);
        return;
    }
    if (t.type === 'LPAREN') {
        adv(s);
        analyzeExpr(s);
        if (!mtch(s, 'RPAREN'))
            err(s, 'error', 'E070', "Expected closing ')'", pk(s).pos);
        else
            adv(s);
        return;
    }
    if (t.type === 'EOF')
        return;
    err(s, 'error', 'E071', "Unexpected token: " + t.type + " '" + t.value + "'", t.pos);
    adv(s);
}
const ARITY = {
    BOOL: [1, 1], NUMBER: [1, 1], STRING: [1, 1], HEX: [1, 1], ASCII: [1, 1], UTF8: [1, 1],
    ADDRESS: [1, 1], EXISTS: [1, 1], FUNCTION: [1, 1], GET: [2, 2],
    ABS: [1, 1], CEIL: [1, 1], FLOOR: [1, 1], INC: [1, 1], DEC: [1, 1],
    MAX: [2, 2], MIN: [2, 2], POW: [2, 2], SQRT: [1, 1], SIGDIG: [2, 2],
    LEN: [1, 1], REV: [1, 1], SETLEN: [2, 2], CONCAT: [2, 32],
    SUBSET: [3, 3], OVERWRITE: [5, 5], BITCOUNT: [1, 1], BITGET: [2, 2], BITSET: [3, 3],
    SUBSTR: [3, 3], REPLACE: [3, 3], REPLACEFIRST: [3, 3],
    SHA2: [1, 1], SHA3: [1, 1], PROOF: [3, 3],
    CHECKSIG: [3, 3], SIGNEDBY: [1, 1], MULTISIG: [3, 64],
    STATE: [1, 1], PREVSTATE: [1, 1], SAMESTATE: [2, 2],
    GETINADDR: [1, 1], GETINAMT: [1, 1], GETINID: [1, 1], GETINTOK: [1, 1], SUMINPUTS: [1, 1], VERIFYIN: [4, 4],
    GETOUTADDR: [1, 1], GETOUTAMT: [1, 1], GETOUTTOK: [1, 1], GETOUTKEEPSTATE: [1, 1], SUMOUTPUTS: [1, 1], VERIFYOUT: [4, 4],
};
function analyzeFn(s) {
    const fnToken = adv(s);
    const fnName = fnToken.value.toUpperCase();
    if (!mtch(s, 'LPAREN')) {
        err(s, 'error', 'E080', "Function '" + fnName + "' must be called with parentheses", fnToken.pos);
        return;
    }
    adv(s); // (
    let argc = 0;
    while (!mtch(s, 'RPAREN') && !mtch(s, 'EOF')) {
        analyzeExpr(s);
        argc++;
        if (mtch(s, 'COMMA'))
            adv(s);
    }
    if (!mtch(s, 'RPAREN'))
        err(s, 'error', 'E081', "Function '" + fnName + "' missing closing ')'", fnToken.pos);
    else
        adv(s);
    // arity check
    const spec = ARITY[fnName];
    if (spec) {
        const [min, max] = spec;
        if (argc < min)
            err(s, 'error', 'E082', "'" + fnName + "' needs at least " + min + " arg(s), got " + argc, fnToken.pos);
        else if (max !== -1 && argc > max)
            err(s, 'error', 'E083', "'" + fnName + "' accepts at most " + max + " arg(s), got " + argc, fnToken.pos);
    }
    // security hints
    if (fnName === 'STATE')
        err(s, 'info', 'I010', 'STATE() — ensure port numbers are consistent across contract versions', fnToken.pos);
    if (fnName === 'PREVSTATE')
        err(s, 'info', 'I011', 'PREVSTATE() — pair with SAMESTATE() if state must not change', fnToken.pos);
    if (fnName === 'VERIFYOUT')
        err(s, 'info', 'I012', 'VERIFYOUT() — validate BOTH amount AND address to prevent partial-spend attacks', fnToken.pos);
}
function skipToNextStatement(s) {
    while (!mtch(s, 'EOF')) {
        const t = pk(s);
        if (t.type === 'KEYWORD' && tokenizer_1.STATEMENT_KEYWORDS.has(t.value.toUpperCase()))
            break;
        adv(s);
    }
}
//# sourceMappingURL=analyzer.js.map