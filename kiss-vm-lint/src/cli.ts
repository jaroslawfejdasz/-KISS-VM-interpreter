#!/usr/bin/env node
import * as fs from 'fs';
import * as path from 'path';
import { lint } from './index';

const RESET  = '\x1b[0m';
const RED    = '\x1b[31m';
const YELLOW = '\x1b[33m';
const CYAN   = '\x1b[36m';
const GRAY   = '\x1b[90m';
const GREEN  = '\x1b[32m';
const BOLD   = '\x1b[1m';

function colorSeverity(sev: string): string {
  if (sev === 'error')   return RED + 'error' + RESET;
  if (sev === 'warning') return YELLOW + 'warning' + RESET;
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
    if (idx === -1) return [];
    return (args[idx + 1] || '').split(',').filter(Boolean);
  })();

  const strict = args.includes('--strict');

  let script = '';

  if (args[0] === '--eval') {
    script = args.slice(1).filter(a => !a.startsWith('--')).join(' ');
    if (!script) { console.error('--eval requires a script string'); process.exit(1); }
  } else {
    const filePath = path.resolve(args[0]);
    if (!fs.existsSync(filePath)) {
      console.error(`File not found: ${filePath}`);
      process.exit(1);
    }
    script = fs.readFileSync(filePath, 'utf8');
  }

  const result = lint(script, { ignoreRules });

  if (result.all.length === 0) {
    console.log(`${GREEN}✓ No issues found${RESET}`);
    process.exit(0);
  }

  let hasBlock = false;
  for (const issue of result.all) {
    const sev = colorSeverity(issue.severity);
    const code = GRAY + issue.code + RESET;
    console.log(`  ${sev}  ${code}  ${issue.message}`);
    if (issue.severity === 'error') hasBlock = true;
  }

  const errCount = result.errors.length;
  const warnCount = result.warnings.length;
  console.log(`\n${errCount > 0 ? RED : YELLOW}${errCount} error(s), ${warnCount} warning(s)${RESET}`);

  if (hasBlock || (strict && warnCount > 0)) process.exit(1);
}

run();
