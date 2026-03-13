# kiss-vm-lint

Static analyzer and linter for [Minima](https://minima.global) **KISS VM** smart contract scripts.

Catches errors before you deploy — without running the contract.

---

## Install

```bash
npm install -g kiss-vm-lint
```

## Usage

```bash
# Lint a file
kiss-vm-lint my-contract.kvm

# Lint multiple files
kiss-vm-lint contracts/*.kvm

# Pipe from stdin
echo "RETURN SIGNEDBY(0xaabb)" | kiss-vm-lint

# JSON output (for editors/CI)
kiss-vm-lint --json contract.kvm

# Strict mode: fail on warnings too
kiss-vm-lint --strict contract.kvm

# Hide info messages
kiss-vm-lint --no-info contract.kvm
```

## What it catches

### Errors (block deployment)
| Code | Description |
|------|-------------|
| E001 | Unterminated string literal |
| E002 | Invalid hex literal (`0x` without digits) |
| E003 | Unknown character |
| E010 | Instruction limit exceeded (> 1024) |
| E011 | Script has no `RETURN` statement |
| E020 | Expected statement keyword |
| E021 | Function used as statement |
| E030 | `LET` requires a variable name |
| E040-E042 | `IF`/`THEN`/`ENDIF` structure errors |
| E050-E051 | `WHILE`/`DO`/`ENDWHILE` structure errors |
| E060 | Division by zero |
| E070-E083 | Expression / function call errors (arity, missing parens) |

### Warnings (potential issues)
| Code | Description |
|------|-------------|
| W010 | Variable assigned but never used |
| W030 | Deeply nested `WHILE` loop |
| W040 | Dead code after `RETURN` |
| W050 | Use `EQ` instead of `=` for equality |
| W060 | Unknown global variable |
| W070 | Variable used before `LET` |

### Info (security hints)
| Code | Description |
|------|-------------|
| I001 | `EXEC` with dynamic code — static analysis limited |
| I002 | `MAST` by hash — ensure hash is audited |
| I010 | `STATE()` — port number consistency |
| I011 | `PREVSTATE()` — pair with `SAMESTATE()` if needed |
| I012 | `VERIFYOUT()` — validate both amount AND address |

## Programmatic API

```js
const { analyze } = require('kiss-vm-lint');

const result = analyze(`
  LET lockBlock = 1000
  IF @BLOCK LT lockBlock THEN
    RETURN FALSE
  ENDIF
  RETURN SIGNEDBY(0xYOUR_PUBLIC_KEY)
`);

console.log(result.summary);
// { errors: 0, warnings: 0, infos: 0, instructionEstimate: 4 }

if (result.errors.length > 0) {
  result.errors.forEach(e => console.log(`[${e.code}] ${e.message}`));
}
```

## KISS VM Quick Reference

KISS VM is Minima's scripting language for UTxO smart contracts. Every coin has a script that must return `TRUE` to be spent.

```
/* Time lock + signature */
LET lockBlock = 1000
IF @BLOCK LT lockBlock THEN
  RETURN FALSE
ENDIF
RETURN SIGNEDBY(0xYOUR_KEY)

/* 2-of-3 multisig */
RETURN MULTISIG(2, 0xKEY1, 0xKEY2, 0xKEY3)

/* Atomic swap */
RETURN VERIFYOUT(0, 100, 0xRECIPIENT_ADDR, 0x00)
```

## Known limitations

- `CHECKSIG` is not validated (requires live node)
- `PROOF` (MMR) is not validated (requires live node)
- `EXEC` with dynamic scripts cannot be statically analyzed

## License

MIT
