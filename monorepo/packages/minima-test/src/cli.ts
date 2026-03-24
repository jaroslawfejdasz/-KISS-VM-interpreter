#!/usr/bin/env node
/**
 * minima-test CLI — run KISS VM test files
 *
 * Usage:
 *   minima-test [pattern]              # run tests matching glob
 *   minima-test --watch [pattern]      # watch mode
 *   minima-test --version              # print version
 */

import { existsSync, readdirSync, statSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { runAll } from './test-runner.js';

const args = process.argv.slice(2);

if (args.includes('--version') || args.includes('-v')) {
  const pkg = JSON.parse(
    require('fs').readFileSync(join(__dirname, '..', 'package.json'), 'utf8')
  );
  console.log(pkg.version);
  process.exit(0);
}

const pattern = args.find(a => !a.startsWith('--')) ?? '**/*.test.{ts,js}';
const cwd = process.cwd();

function findTestFiles(dir: string, pat: string): string[] {
  const files: string[] = [];
  const ext = ['.test.ts', '.test.js', '.spec.ts', '.spec.js'];
  
  function walk(d: string) {
    if (!existsSync(d)) return;
    for (const f of readdirSync(d)) {
      const fp = join(d, f);
      const st = statSync(fp);
      if (st.isDirectory() && f !== 'node_modules' && f !== 'dist') {
        walk(fp);
      } else if (ext.some(e => f.endsWith(e))) {
        files.push(fp);
      }
    }
  }
  walk(dir);
  return files;
}

async function main() {
  const files = findTestFiles(cwd, pattern);
  
  if (files.length === 0) {
    console.error(`No test files found matching: ${pattern}`);
    process.exit(1);
  }

  console.log(`\n  minima-test — found ${files.length} test file(s)\n`);

  for (const file of files) {
    const url = pathToFileURL(file).href;
    await import(url);
  }

  const ok = await runAll();
  process.exit(ok ? 0 : 1);
}

main().catch(err => {
  console.error(err);
  process.exit(1);
});
