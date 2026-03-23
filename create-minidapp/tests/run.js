/**
 * create-minidapp test suite
 */
const fs   = require('fs');
const path = require('path');
const os   = require('os');
const { scaffold } = require('../dist/index');
const { TEMPLATES, listTemplates } = require('../dist/templates');

let passed = 0;
let failed = 0;

function it(name, fn) {
  try {
    fn();
    console.log(`  ✓ ${name}`);
    passed++;
  } catch (err) {
    console.log(`  ✗ ${name}`);
    console.log(`      → ${err.message}`);
    failed++;
  }
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg || 'Assertion failed');
}
function assertEq(a, b, msg) {
  if (a !== b) throw new Error((msg || 'Expected equal') + ` — got: ${JSON.stringify(a)} vs ${JSON.stringify(b)}`);
}
function assertContains(str, sub, msg) {
  if (!str.includes(sub)) throw new Error((msg || 'Expected to contain') + `: "${sub}" in "${str.slice(0,100)}"`);
}

const tmpBase = fs.mkdtempSync(path.join(os.tmpdir(), 'minima-test-'));

function tmpDir(name) {
  const d = path.join(tmpBase, name);
  fs.mkdirSync(d, { recursive: true });
  return d;
}

// ─── Templates ─────────────────────────────────────────────────────────────────
console.log('\n  Template Registry');

it('has 3 templates', () => {
  assert(Object.keys(TEMPLATES).length >= 3);
});

it('basic template has required files', () => {
  const files = TEMPLATES.basic.files.map(f => f.path);
  assert(files.includes('dapp.conf'));
  assert(files.includes('index.html'));
  assert(files.includes('mds.js'));
  assert(files.includes('package.json'));
});

it('counter template includes contract.kiss', () => {
  const files = TEMPLATES.counter.files.map(f => f.path);
  assert(files.includes('contract.kiss'));
});

it('exchange template includes contract.kiss', () => {
  const files = TEMPLATES.exchange.files.map(f => f.path);
  assert(files.includes('contract.kiss'));
});

it('all templates have dapp.conf', () => {
  for (const [name, tpl] of Object.entries(TEMPLATES)) {
    const files = tpl.files.map(f => f.path);
    assert(files.includes('dapp.conf'), `${name} missing dapp.conf`);
  }
});

it('all templates have index.html', () => {
  for (const [name, tpl] of Object.entries(TEMPLATES)) {
    const files = tpl.files.map(f => f.path);
    assert(files.includes('index.html'), `${name} missing index.html`);
  }
});

// ─── Scaffold: basic ───────────────────────────────────────────────────────────
console.log('\n  Scaffold — basic template');

it('creates project directory', () => {
  const out = tmpDir('basic-1');
  scaffold({ projectName: 'test-app', template: 'basic', outputDir: out });
  assert(fs.existsSync(path.join(out, 'test-app')));
});

it('creates all required files', () => {
  const out = tmpDir('basic-2');
  scaffold({ projectName: 'wallet', template: 'basic', outputDir: out });
  const base = path.join(out, 'wallet');
  assert(fs.existsSync(path.join(base, 'dapp.conf')));
  assert(fs.existsSync(path.join(base, 'index.html')));
  assert(fs.existsSync(path.join(base, 'mds.js')));
  assert(fs.existsSync(path.join(base, 'package.json')));
  assert(fs.existsSync(path.join(base, 'README.md')));
});

it('replaces {{NAME}} placeholder in dapp.conf', () => {
  const out = tmpDir('basic-3');
  scaffold({ projectName: 'MyApp', template: 'basic', outputDir: out });
  const conf = fs.readFileSync(path.join(out, 'MyApp', 'dapp.conf'), 'utf8');
  assertContains(conf, '"name": "MyApp"');
  assert(!conf.includes('{{NAME}}'), 'placeholder not replaced');
});

it('replaces {{NAME_LOWER}} in package.json', () => {
  const out = tmpDir('basic-4');
  scaffold({ projectName: 'MyApp', template: 'basic', outputDir: out });
  const pkg = fs.readFileSync(path.join(out, 'MyApp', 'package.json'), 'utf8');
  assertContains(pkg, '"name": "myapp"');
  assert(!pkg.includes('{{NAME_LOWER}}'), 'placeholder not replaced');
});

it('replaces {{NAME}} in index.html title', () => {
  const out = tmpDir('basic-5');
  scaffold({ projectName: 'WalletApp', template: 'basic', outputDir: out });
  const html = fs.readFileSync(path.join(out, 'WalletApp', 'index.html'), 'utf8');
  assertContains(html, '<title>WalletApp</title>');
});

it('replaces {{NAME}} in README.md', () => {
  const out = tmpDir('basic-6');
  scaffold({ projectName: 'ReadmeTest', template: 'basic', outputDir: out });
  const readme = fs.readFileSync(path.join(out, 'ReadmeTest', 'README.md'), 'utf8');
  assertContains(readme, '# ReadmeTest');
});

it('dapp.conf is valid JSON', () => {
  const out = tmpDir('basic-7');
  scaffold({ projectName: 'json-test', template: 'basic', outputDir: out });
  const conf = fs.readFileSync(path.join(out, 'json-test', 'dapp.conf'), 'utf8');
  const parsed = JSON.parse(conf); // throws if invalid
  assertEq(parsed.name, 'json-test');
  assertEq(parsed.browser, 'index.html');
  assertEq(parsed.icon, 'icon.png');
});

it('package.json is valid JSON', () => {
  const out = tmpDir('basic-8');
  scaffold({ projectName: 'pkg-test', template: 'basic', outputDir: out });
  const pkg = JSON.parse(fs.readFileSync(path.join(out, 'pkg-test', 'package.json'), 'utf8'));
  assert(pkg.scripts && pkg.scripts.package, 'missing package script');
});

// ─── Scaffold: counter ─────────────────────────────────────────────────────────
console.log('\n  Scaffold — counter template');

it('creates counter project', () => {
  const out = tmpDir('counter-1');
  scaffold({ projectName: 'my-counter', template: 'counter', outputDir: out });
  assert(fs.existsSync(path.join(out, 'my-counter', 'contract.kiss')));
});

it('counter contract.kiss contains KISS VM code', () => {
  const out = tmpDir('counter-2');
  scaffold({ projectName: 'cnt', template: 'counter', outputDir: out });
  const kiss = fs.readFileSync(path.join(out, 'cnt', 'contract.kiss'), 'utf8');
  assertContains(kiss, 'STATE');
  assertContains(kiss, 'PREVSTATE');
  assertContains(kiss, 'SIGNEDBY');
  assertContains(kiss, 'RETURN');
});

it('counter index.html has KISS VM contract comment', () => {
  const out = tmpDir('counter-3');
  scaffold({ projectName: 'cnt2', template: 'counter', outputDir: out });
  const html = fs.readFileSync(path.join(out, 'cnt2', 'index.html'), 'utf8');
  assertContains(html, 'CONTRACT');
  assertContains(html, 'STATE(1)');
});

// ─── Scaffold: exchange ────────────────────────────────────────────────────────
console.log('\n  Scaffold — exchange template');

it('creates exchange project', () => {
  const out = tmpDir('exchange-1');
  scaffold({ projectName: 'my-swap', template: 'exchange', outputDir: out });
  assert(fs.existsSync(path.join(out, 'my-swap', 'contract.kiss')));
});

it('exchange contract.kiss contains VERIFYOUT', () => {
  const out = tmpDir('exchange-2');
  scaffold({ projectName: 'swap2', template: 'exchange', outputDir: out });
  const kiss = fs.readFileSync(path.join(out, 'swap2', 'contract.kiss'), 'utf8');
  assertContains(kiss, 'VERIFYOUT');
  assertContains(kiss, 'RETURN');
});

it('exchange index.html has swap UI', () => {
  const out = tmpDir('exchange-3');
  scaffold({ projectName: 'swap3', template: 'exchange', outputDir: out });
  const html = fs.readFileSync(path.join(out, 'swap3', 'index.html'), 'utf8');
  assertContains(html, 'createSwapOffer');
  assertContains(html, 'VERIFYOUT');
});

// ─── Error handling ────────────────────────────────────────────────────────────
console.log('\n  Error handling');

it('throws on unknown template', () => {
  const out = tmpDir('err-1');
  let threw = false;
  try {
    scaffold({ projectName: 'test', template: 'nonexistent', outputDir: out });
  } catch { threw = true; }
  assert(threw, 'Should have thrown');
});

it('throws if directory already exists', () => {
  const out = tmpDir('err-2');
  scaffold({ projectName: 'exists', template: 'basic', outputDir: out });
  let threw = false;
  try {
    scaffold({ projectName: 'exists', template: 'basic', outputDir: out });
  } catch { threw = true; }
  assert(threw, 'Should throw on duplicate');
});

it('no {{placeholder}} left in any generated file', () => {
  const templates = ['basic', 'counter', 'exchange'];
  for (const tpl of templates) {
    const out = tmpDir(`noplach-${tpl}`);
    scaffold({ projectName: 'TestProject', template: tpl, outputDir: out });
    const projectDir = path.join(out, 'TestProject');
    const files = fs.readdirSync(projectDir);
    for (const f of files) {
      const content = fs.readFileSync(path.join(projectDir, f), 'utf8');
      assert(!content.includes('{{'), `${tpl}/${f} still has {{ placeholder }}`);
    }
  }
});

// ─── Results ───────────────────────────────────────────────────────────────────
console.log('\n' + '─'.repeat(50));
if (failed === 0) {
  console.log(`\n  ✓ All ${passed} tests passed!\n`);
  process.exit(0);
} else {
  console.log(`\n  ✗ ${failed} failed, ${passed} passed\n`);
  process.exit(1);
}
