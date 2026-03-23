#!/usr/bin/env node
"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
const index_1 = require("./index");
const RESET = '\x1b[0m';
const RED = '\x1b[31m';
const YELLOW = '\x1b[33m';
const CYAN = '\x1b[36m';
const GRAY = '\x1b[90m';
const GREEN = '\x1b[32m';
const BOLD = '\x1b[1m';
function colorSeverity(sev) {
    if (sev === 'error')
        return RED + 'error' + RESET;
    if (sev === 'warning')
        return YELLOW + 'warning' + RESET;
    return CYAN + 'info' + RESET;
}
function run() {
    const args = process.argv.slice(2);
    if (args.length === 0 || args[0] === '--help') {
        console.log(`
${BOLD}kiss-vm-lint${RESET} — Static analyzer for Minima KISS VM scripts

Usage:
  kiss-vm-lint <file.kissvm>
  kiss-vm-lint --eval "RETURN SIGNEDBY(0x...)"
  kiss-vm-lint --list-rules

Options:
  --ignore R001,R002   Ignore specific rule codes
  --strict             Treat warnings as errors
    `);
        process.exit(0);
    }
    if (args[0] === '--list-rules') {
        const { ALL_RULES } = require('./rules');
        console.log(`${BOLD}Available rules:${RESET}`);
        // Rules don't have metadata in current form, just list them
        console.log(`  ${ALL_RULES.length} rules loaded`);
        process.exit(0);
    }
    const ignoreRules = (() => {
        const idx = args.indexOf('--ignore');
        if (idx === -1)
            return [];
        return (args[idx + 1] || '').split(',').filter(Boolean);
    })();
    const strict = args.includes('--strict');
    let script = '';
    if (args[0] === '--eval') {
        script = args.slice(1).filter(a => !a.startsWith('--')).join(' ');
        if (!script) {
            console.error('--eval requires a script string');
            process.exit(1);
        }
    }
    else {
        const filePath = path.resolve(args[0]);
        if (!fs.existsSync(filePath)) {
            console.error(`File not found: ${filePath}`);
            process.exit(1);
        }
        script = fs.readFileSync(filePath, 'utf8');
    }
    const result = (0, index_1.lint)(script, { ignoreRules });
    if (result.all.length === 0) {
        console.log(`${GREEN}✓ No issues found${RESET}`);
        process.exit(0);
    }
    let hasBlock = false;
    for (const issue of result.all) {
        const sev = colorSeverity(issue.severity);
        const code = GRAY + issue.code + RESET;
        console.log(`  ${sev}  ${code}  ${issue.message}`);
        if (issue.severity === 'error')
            hasBlock = true;
    }
    const errCount = result.errors.length;
    const warnCount = result.warnings.length;
    console.log(`\n${errCount > 0 ? RED : YELLOW}${errCount} error(s), ${warnCount} warning(s)${RESET}`);
    if (hasBlock || (strict && warnCount > 0))
        process.exit(1);
}
run();
//# sourceMappingURL=cli.js.map