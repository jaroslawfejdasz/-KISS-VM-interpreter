#!/usr/bin/env node
/**
 * Simple test runner for minima-test .test.js files
 * Usage: node run-tests.js [pattern]
 */
const path = require('path');
const fs = require('fs');

// Patch api module to auto-run on process exit
const api = require('./dist/api');

// Collect test files
const testsDir = path.join(__dirname, 'tests');
const files = fs.readdirSync(testsDir)
  .filter(f => f.endsWith('.test.js'))
  .sort();

const pattern = process.argv[2] || '';
const filtered = pattern ? files.filter(f => f.includes(pattern)) : files;

// Load all test files (registers describe/it calls)
for (const file of filtered) {
  require(path.join(testsDir, file));
}

// Run all suites
api.runSuites().then(results => {
  let totalPassed = 0;
  let totalFailed = 0;

  for (const suite of results) {
    const failed = suite.tests.filter(t => !t.passed);
    const passed = suite.tests.filter(t => t.passed);
    const icon = failed.length === 0 ? '✅' : '❌';
    console.log(`\n${icon} ${suite.name} (${passed.length}/${suite.tests.length})`);
    for (const t of suite.tests) {
      if (t.passed) {
        console.log(`   ✓ ${t.name}`);
      } else {
        console.log(`   ✗ ${t.name}`);
        console.log(`     ${t.error}`);
      }
    }
    totalPassed += passed.length;
    totalFailed += failed.length;
  }

  const total = totalPassed + totalFailed;
  console.log(`\n${'─'.repeat(50)}`);
  if (totalFailed === 0) {
    console.log(`✅ ${totalPassed}/${total} tests passed`);
  } else {
    console.log(`❌ ${totalPassed}/${total} passed, ${totalFailed} failed`);
  }

  process.exit(totalFailed > 0 ? 1 : 0);
}).catch(err => {
  console.error('Runner error:', err);
  process.exit(1);
});
