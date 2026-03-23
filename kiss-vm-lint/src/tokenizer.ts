// Shared tokenizer types — same as minima-test but standalone
export type TokenType =
  | 'NUMBER' | 'HEX' | 'BOOLEAN' | 'STRING'
  | 'VARIABLE' | 'GLOBAL' | 'KEYWORD' | 'OPERATOR'
  | 'LPAREN' | 'RPAREN' | 'COMMA' | 'EOF';

export interface Token {
  type: TokenType;
  value: string;
  pos: number; // character position for error reporting
}

const KEYWORDS = new Set([
  'LET','IF','THEN','ELSE','ELSEIF','ENDIF',
  'WHILE','DO','ENDWHILE',
  'RETURN','ASSERT','EXEC','MAST',
  'TRUE','FALSE','AND','OR','NOT',
  'XOR','NAND','NOR',
  'EQ','NEQ','LT','GT','LTE','GTE',
  'ASCII','BOOL','HEX','NUMBER','STRING','UTF8',
  'ADDRESS','EXISTS','FUNCTION','GET',
  'BITCOUNT','BITGET','BITSET','CONCAT','LEN','OVERWRITE','REV','SETLEN','SUBSET',
  'ABS','CEIL','DEC','FLOOR','INC','MAX','MIN','POW','SIGDIG','SQRT',
  'PROOF','SHA2','SHA3',
  'CHECKSIG','MULTISIG','SIGNEDBY',
  'PREVSTATE','SAMESTATE','STATE',
  'REPLACE','REPLACEFIRST','SUBSTR',
  'GETINADDR','GETINAMT','GETINID','GETINTOK','SUMINPUTS','VERIFYIN',
  'GETOUTADDR','GETOUTAMT','GETOUTKEEPSTATE','GETOUTTOK','SUMOUTPUTS','VERIFYOUT',
]);

export const STATEMENT_KEYWORDS = new Set([
  'LET','IF','WHILE','RETURN','ASSERT','EXEC','MAST'
]);

export const FUNCTION_KEYWORDS = new Set([
  'ASCII','BOOL','HEX','NUMBER','STRING','UTF8',
  'ADDRESS','EXISTS','FUNCTION','GET',
  'BITCOUNT','BITGET','BITSET','CONCAT','LEN','OVERWRITE','REV','SETLEN','SUBSET',
  'ABS','CEIL','DEC','FLOOR','INC','MAX','MIN','POW','SIGDIG','SQRT',
  'PROOF','SHA2','SHA3',
  'CHECKSIG','MULTISIG','SIGNEDBY',
  'PREVSTATE','SAMESTATE','STATE',
  'REPLACE','REPLACEFIRST','SUBSTR',
  'GETINADDR','GETINAMT','GETINID','GETINTOK','SUMINPUTS','VERIFYIN',
  'GETOUTADDR','GETOUTAMT','GETOUTKEEPSTATE','GETOUTTOK','SUMOUTPUTS','VERIFYOUT',
]);


// FUNCTION_ARITY: [min, max] args, -1 = variadic
export const FUNCTION_ARITY: Record<string, [number, number]> = {
  // Type conversions
  ASCII: [1, 1], BOOL: [1, 1], HEX: [1, 1], NUMBER: [1, 1], STRING: [1, 1], UTF8: [1, 1],
  // Address
  ADDRESS: [1, 1],
  // Variable ops
  EXISTS: [1, 1], GET: [1, -1], FUNCTION: [1, 1],
  // Bit ops
  BITCOUNT: [1, 1], BITGET: [2, 2], BITSET: [3, 3],
  // String / hex ops
  CONCAT: [2, -1], LEN: [1, 1], OVERWRITE: [5, 5], REV: [1, 1],
  SETLEN: [2, 2], SUBSET: [3, 3],
  // Math
  ABS: [1, 1], CEIL: [1, 1], DEC: [1, 1], FLOOR: [1, 1], INC: [1, 1],
  MAX: [2, 2], MIN: [2, 2], POW: [2, 2], SIGDIG: [2, 2], SQRT: [1, 1],
  // Crypto
  PROOF: [3, 3], SHA2: [1, 1], SHA3: [1, 1],
  CHECKSIG: [3, 3], MULTISIG: [3, -1], SIGNEDBY: [1, 1],
  // State
  PREVSTATE: [1, 1], SAMESTATE: [2, 2], STATE: [1, 1],
  // String
  REPLACE: [3, 3], REPLACEFIRST: [3, 3], SUBSTR: [3, 3],
  // Input / output
  GETINADDR: [1, 1], GETINAMT: [1, 1], GETINID: [1, 1], GETINTOK: [1, 1],
  SUMINPUTS: [1, 1], VERIFYIN: [4, 4],
  GETOUTADDR: [1, 1], GETOUTAMT: [1, 1], GETOUTKEEPSTATE: [1, 1], GETOUTTOK: [1, 1],
  SUMOUTPUTS: [1, 1], VERIFYOUT: [4, 4],
};

export interface TokenizeResult {
  tokens: Token[];
  errors: LintError[];
}

export interface LintError {
  severity: 'error' | 'warning' | 'info';
  code: string;
  message: string;
  pos?: number;
}

export function tokenize(script: string): TokenizeResult {
  const tokens: Token[] = [];
  const errors: LintError[] = [];
  let i = 0;

  while (i < script.length) {
    const startPos = i;

    if (/\s/.test(script[i])) { i++; continue; }

    // Comments
    if (script[i] === '/' && script[i+1] === '*') {
      i += 2;
      while (i < script.length && !(script[i] === '*' && script[i+1] === '/')) i++;
      i += 2;
      continue;
    }

    // Strings
    if (script[i] === '[') {
      let val = '';
      i++;
      let depth = 1;
      const strStart = startPos;
      while (i < script.length && depth > 0) {
        if (script[i] === '[') depth++;
        else if (script[i] === ']') { depth--; if (depth === 0) { i++; break; } }
        val += script[i++];
      }
      if (depth > 0) {
        errors.push({ severity: 'error', code: 'E001', message: 'Unterminated string literal (missing ])', pos: strStart });
      }
      tokens.push({ type: 'STRING', value: val, pos: startPos });
      continue;
    }

    // HEX
    if (script[i] === '0' && script[i+1] === 'x') {
      let val = '0x';
      i += 2;
      while (i < script.length && /[0-9a-fA-F]/.test(script[i])) val += script[i++];
      if (val === '0x') {
        errors.push({ severity: 'error', code: 'E002', message: "Invalid hex literal: '0x' must be followed by hex digits", pos: startPos });
        tokens.push({ type: 'HEX', value: '0x00', pos: startPos }); // recovery
      } else {
        if (val.length > 66) { // 0x + 64 hex chars = 32 bytes
          errors.push({ severity: 'warning', code: 'W001', message: `Very long hex literal (${(val.length-2)/2} bytes) — may exceed stack limits`, pos: startPos });
        }
        tokens.push({ type: 'HEX', value: val, pos: startPos });
      }
      continue;
    }

    // Numbers
    if (/[0-9]/.test(script[i]) || (script[i] === '-' && /[0-9]/.test(script[i+1] || ''))) {
      let val = '';
      if (script[i] === '-') val += script[i++];
      while (i < script.length && /[0-9.]/.test(script[i])) val += script[i++];
      tokens.push({ type: 'NUMBER', value: val, pos: startPos });
      continue;
    }

    if (script[i] === '(') { tokens.push({ type: 'LPAREN', value: '(', pos: startPos }); i++; continue; }
    if (script[i] === ')') { tokens.push({ type: 'RPAREN', value: ')', pos: startPos }); i++; continue; }
    if (script[i] === ',') { tokens.push({ type: 'COMMA', value: ',', pos: startPos }); i++; continue; }

    const twoChar = script.slice(i, i+2);
    if (twoChar === '!=' || twoChar === '<=' || twoChar === '>=') {
      tokens.push({ type: 'OPERATOR', value: twoChar, pos: startPos }); i+=2; continue;
    }

    if (/[+\-*\/%&|^~=<>]/.test(script[i])) {
      tokens.push({ type: 'OPERATOR', value: script[i++], pos: startPos }); continue;
    }

    if (/[a-zA-Z@_]/.test(script[i])) {
      let val = '';
      while (i < script.length && /[a-zA-Z0-9@_]/.test(script[i])) val += script[i++];
      const upper = val.toUpperCase();
      if (val.startsWith('@')) {
        tokens.push({ type: 'GLOBAL', value: upper, pos: startPos });
      } else if (KEYWORDS.has(upper)) {
        if (upper === 'TRUE' || upper === 'FALSE') {
          tokens.push({ type: 'BOOLEAN', value: upper, pos: startPos });
        } else {
          tokens.push({ type: 'KEYWORD', value: upper, pos: startPos });
        }
      } else {
        tokens.push({ type: 'VARIABLE', value: val, pos: startPos });
      }
      continue;
    }

    errors.push({ severity: 'error', code: 'E003', message: `Unknown character '${script[i]}'`, pos: startPos });
    i++;
  }

  tokens.push({ type: 'EOF', value: '', pos: i });
  return { tokens, errors };
}
