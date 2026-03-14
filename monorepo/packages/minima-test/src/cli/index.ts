#!/usr/bin/env node
import * as fs from 'fs';
import * as path from 'path';
import { runSuites, SuiteResult } from '../api';

const COLORS = {
  reset: '\x1b[0m',
  green: '\x1b[32m',
  red: '\x1b[31m',
  yellow: '\x1b[33m',
  cyan: '\x1b[36m',
  gray: '\x1b[90m',
  bold: '\x1b[1m',
  dim: '\x1b[2m',
};

function c(color: keyof typeof COLORS, text: string) {
  return `${COLORS[color]}${text}${COLORS.reset}`;
}

function banner() {
  console.log(c('cyan', '\n  ╔═══════════════════════════════╗'));
  console.log(c('cyan', '  ║   minima-test  v0.1.0         ║'));
  console.log(c('cyan', '  ║   KISS VM Testing Framework   ║'));
  console.log(c('cyan', '  ╚═══════════════════════════════╝\n'));
}

async function findTestFiles(dir: string): Promise<string[]> {
  const files: string[] = [];
  
  if (!fs.existsSync(dir)) return files;
  
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      files.push(...await findTestFiles(full));
    } else if (entry.name.match(/\.(test|spec)\.(js|ts|mjs)$/) || entry.name.match(/test.*\.(js|ts)$/)) {
      files.push(full);
    }
  }
  return files;
}

function printResults(results: SuiteResult[]) {
  let totalPassed = 0;
  let totalFailed = 0;
  let totalDuration = 0;

  for (const suite of results) {
    console.log(c('bold', `\n  ${suite.name}`));
    
    for (const test of suite.tests) {
      const icon = test.passed ? c('green', '  ✓') : c('red', '  ✗');
      const name = test.passed ? c('gray', test.name) : c('red', test.name);
      const dur = c('dim', `  ${test.duration}ms`);
      console.log(`${icon} ${name}${dur}`);
      
      if (!test.passed && test.error) {
        console.log(c('red', `      → ${test.error}`));
      }
    }

    const passedStr = c('green', `${suite.passed} passed`);
    const failedStr = suite.failed > 0 ? c('red', `${suite.failed} failed`) : '';
    const parts = [passedStr, failedStr].filter(Boolean).join(', ');
    console.log(c('dim', `\n    ${parts}  (${suite.duration}ms)`));

    totalPassed += suite.passed;
    totalFailed += suite.failed;
    totalDuration += suite.duration;
  }

  console.log('\n' + '─'.repeat(50));
  
  if (totalFailed === 0) {
    console.log(c('green', c('bold', `\n  ✓ All ${totalPassed} tests passed!`)) + c('dim', `  (${totalDuration}ms)\n`));
  } else {
    console.log(c('red', c('bold', `\n  ✗ ${totalFailed} failed, ${totalPassed} passed`)) + c('dim', `  (${totalDuration}ms)\n`));
  }

  return totalFailed;
}

async function main() {
  const args = process.argv.slice(2);
  const command = args[0];

  banner();

  if (command === '--version' || command === '-v' || command === 'version') {
    const pkg = require('../../package.json');
    console.log(`  v${pkg.version}`);
    process.exit(0);
  }

  if (command === 'run' || !command) {
    const targetDir = args[1] || './tests';
    const files = await findTestFiles(path.resolve(targetDir));

    if (files.length === 0) {
      console.log(c('yellow', `  No test files found in ${targetDir}`));
      console.log(c('gray', '  Create files matching *.test.js or *.spec.js\n'));
      process.exit(0);
    }

    console.log(c('gray', `  Found ${files.length} test file(s):\n`));
    for (const f of files) console.log(c('dim', `    ${f}`));
    console.log();

    // Require each test file (they register suites via describe/it)
    for (const file of files) {
      try {
        require(file);
      } catch (e: any) {
        console.log(c('red', `  Failed to load ${file}: ${e.message}`));
        process.exit(1);
      }
    }

    const results = await runSuites();
    const failed = printResults(results);
    process.exit(failed > 0 ? 1 : 0);
  }

  if (command === 'eval') {
    // Quick script evaluation: minima-test eval "RETURN TRUE" [--signed 0xkey1,0xkey2] [--block N]
    const script = args[1];
    if (!script) { console.log('Usage: minima-test eval "<script>" [--signed 0xkey1,0xkey2] [--block N]'); process.exit(1); }

    const { runScript } = require('../api');

    // Parse optional flags
    const signedIdx = args.indexOf('--signed');
    const blockIdx  = args.indexOf('--block');
    const signatures = signedIdx !== -1 ? args[signedIdx + 1].split(',') : [];
    const block      = blockIdx  !== -1 ? parseInt(args[blockIdx + 1], 10) : undefined;

    const tx: any = {};
    if (signatures.length) tx.signatures = signatures;
    if (block !== undefined) tx.blockNumber = block;

    const result = runScript(script, Object.keys(tx).length ? tx : undefined);
    console.log(c('bold', `  Script: `) + script);
    if (signatures.length) console.log(c('dim', `  Signed by: ${signatures.join(', ')}`));
    if (block !== undefined) console.log(c('dim', `  @BLOCK: ${block}`));
    console.log(c('bold', `  Result: `) + (result.success ? c('green', 'TRUE ✓') : c('red', 'FALSE ✗')));
    if (result.error) console.log(c('red', `  Error: `) + result.error);
    console.log(c('dim', `  Instructions: ${result.instructions}`));
    console.log();
    process.exit(result.success ? 0 : 1);
  }

  if (command === 'help' || command === '--help' || command === '-h') {
    console.log('  Usage:');
    console.log(c('cyan', '    minima-test run [dir]      ') + 'Run all tests in directory (default: ./tests)');
    console.log(c('cyan', '    minima-test eval "<script>"') + 'Quickly evaluate a KISS VM script');
    console.log(c('cyan', '    minima-test help           ') + 'Show this help\n');
    process.exit(0);
  }

  console.log(c('red', `  Unknown command: ${command}`));
  process.exit(1);
}

main().catch(e => { console.error(e); process.exit(1); });
