export type TokenType =
  | 'NUMBER' | 'HEX' | 'BOOLEAN' | 'STRING'
  | 'VARIABLE' | 'GLOBAL' | 'KEYWORD' | 'OPERATOR'
  | 'LPAREN' | 'RPAREN' | 'COMMA' | 'EOF';

export interface Token {
  type: TokenType;
  value: string;
}

const KEYWORDS = new Set([
  'LET','IF','THEN','ELSE','ELSEIF','ENDIF',
  'WHILE','DO','ENDWHILE',
  'RETURN','ASSERT','EXEC','MAST',
  'TRUE','FALSE','AND','OR','NOT',
  'XOR','NAND','NOR',
  'EQ','NEQ','LT','GT','LTE','GTE',
  // Functions
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

export function tokenize(script: string): Token[] {
  const tokens: Token[] = [];
  let i = 0;

  while (i < script.length) {
    // Skip whitespace
    if (/\s/.test(script[i])) { i++; continue; }

    // Comments /* ... */
    if (script[i] === '/' && script[i+1] === '*') {
      i += 2;
      while (i < script.length && !(script[i] === '*' && script[i+1] === '/')) i++;
      i += 2;
      continue;
    }

    // String literals [ ... ]
    if (script[i] === '[') {
      let val = '';
      i++; // skip [
      let depth = 1;
      while (i < script.length && depth > 0) {
        if (script[i] === '[') depth++;
        else if (script[i] === ']') { depth--; if (depth === 0) { i++; break; } }
        val += script[i++];
      }
      tokens.push({ type: 'STRING', value: val });
      continue;
    }

    // HEX 0x...
    if (script[i] === '0' && script[i+1] === 'x') {
      let val = '0x';
      i += 2;
      while (i < script.length && /[0-9a-fA-F]/.test(script[i])) val += script[i++];
      if (val === '0x') throw new Error(`Invalid hex literal at position ${i - 2}: '0x' must be followed by hex digits`);
      tokens.push({ type: 'HEX', value: val });
      continue;
    }

    // Numbers (including negatives and decimals)
    if (/[0-9]/.test(script[i]) || (script[i] === '-' && /[0-9]/.test(script[i+1] || ''))) {
      let val = '';
      if (script[i] === '-') val += script[i++];
      while (i < script.length && /[0-9.]/.test(script[i])) val += script[i++];
      tokens.push({ type: 'NUMBER', value: val });
      continue;
    }

    // Operators
    if (script[i] === '(' ) { tokens.push({ type: 'LPAREN', value: '(' }); i++; continue; }
    if (script[i] === ')' ) { tokens.push({ type: 'RPAREN', value: ')' }); i++; continue; }
    if (script[i] === ',' ) { tokens.push({ type: 'COMMA', value: ',' }); i++; continue; }

    // Multi-char operators
    const twoChar = script.slice(i, i+2);
    if (twoChar === '!=' || twoChar === '<=' || twoChar === '>=') {
      tokens.push({ type: 'OPERATOR', value: twoChar }); i+=2; continue;
    }

    // Operators: = + - * / % & | ^
    if (/[+\-*\/%&|^~=]/.test(script[i])) {
      tokens.push({ type: 'OPERATOR', value: script[i++] }); continue;
    }

    // Words (keywords, variables, globals)
    if (/[a-zA-Z@_]/.test(script[i])) {
      let val = '';
      while (i < script.length && /[a-zA-Z0-9@_]/.test(script[i])) val += script[i++];
      const upper = val.toUpperCase();
      if (val.startsWith('@')) {
        tokens.push({ type: 'GLOBAL', value: upper });
      } else if (KEYWORDS.has(upper)) {
        if (upper === 'TRUE' || upper === 'FALSE') {
          tokens.push({ type: 'BOOLEAN', value: upper });
        } else {
          tokens.push({ type: 'KEYWORD', value: upper });
        }
      } else {
        tokens.push({ type: 'VARIABLE', value: val });
      }
      continue;
    }

    throw new Error(`Unknown character at position ${i}: '${script[i]}'`);
  }

  tokens.push({ type: 'EOF', value: '' });
  return tokens;
}
