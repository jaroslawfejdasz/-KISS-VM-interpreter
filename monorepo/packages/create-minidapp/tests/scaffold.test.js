/**
 * create-minidapp scaffold tests
 */
const assert = require('node:assert/strict');
const { describe, it } = require('node:test');
const { existsSync, readFileSync } = require('node:fs');
const { join } = require('node:path');
const { tmpdir } = require('node:os');

const { scaffold } = require('../dist/scaffold.js');

function tmpTarget(suffix = '') {
  return join(tmpdir(), `cmd-test-${Date.now()}-${suffix}`);
}

// ── Basic scaffold ────────────────────────────────────────────────────────────

describe('scaffold — basic', () => {
  it('creates all required files', () => {
    const dir = tmpTarget('basic');
    const files = scaffold(dir, { name: 'TestApp' });
    const required = ['dapp.conf', 'icon.png', 'index.html', 'app.js', 'mds.js', '.gitignore', 'README.md'];
    for (const f of required) {
      assert.ok(existsSync(join(dir, f)), `Missing: ${f}`);
    }
  });

  it('returns list of created files', () => {
    const dir = tmpTarget('list');
    const files = scaffold(dir, { name: 'MyApp' });
    assert.ok(Array.isArray(files));
    assert.ok(files.length >= 7);
  });

  it('throws if directory already exists', () => {
    const dir = tmpTarget('existing');
    scaffold(dir, { name: 'First' });
    assert.throws(() => scaffold(dir, { name: 'Second' }), /already exists/);
  });
});

// ── dapp.conf ─────────────────────────────────────────────────────────────────

describe('scaffold — dapp.conf', () => {
  it('contains correct name and defaults', () => {
    const dir = tmpTarget('conf');
    scaffold(dir, { name: 'MyWallet' });
    const conf = JSON.parse(readFileSync(join(dir, 'dapp.conf'), 'utf8'));
    assert.equal(conf.name, 'MyWallet');
    assert.equal(conf.category, 'Utility');
    assert.equal(conf.permission, 'read');
    assert.equal(conf.browser, 'internal');
    assert.equal(conf.icon, 'icon.png');
  });

  it('sets permission to write with --with-maxima', () => {
    const dir = tmpTarget('maxima-perm');
    scaffold(dir, { name: 'MsgApp', withMaxima: true });
    const conf = JSON.parse(readFileSync(join(dir, 'dapp.conf'), 'utf8'));
    assert.equal(conf.permission, 'write');
  });

  it('uses provided description, author, category', () => {
    const dir = tmpTarget('meta');
    scaffold(dir, {
      name: 'DeFiApp',
      description: 'My DeFi app',
      author: 'Jarek',
      category: 'DeFi',
    });
    const conf = JSON.parse(readFileSync(join(dir, 'dapp.conf'), 'utf8'));
    assert.equal(conf.description, 'My DeFi app');
    assert.equal(conf.author, 'Jarek');
    assert.equal(conf.category, 'DeFi');
  });
});

// ── Optional files ────────────────────────────────────────────────────────────

describe('scaffold — optional files', () => {
  it('does NOT create service.js without --with-maxima', () => {
    const dir = tmpTarget('no-svc');
    scaffold(dir, { name: 'Basic' });
    assert.ok(!existsSync(join(dir, 'service.js')), 'service.js should not exist');
  });

  it('creates service.js with --with-maxima', () => {
    const dir = tmpTarget('svc');
    scaffold(dir, { name: 'MsgApp', withMaxima: true });
    assert.ok(existsSync(join(dir, 'service.js')));
    const content = readFileSync(join(dir, 'service.js'), 'utf8');
    assert.ok(content.includes('MDS_MAXIMA_MESSAGE'));
  });

  it('does NOT create contracts/ without --with-contract', () => {
    const dir = tmpTarget('no-contract');
    scaffold(dir, { name: 'Basic' });
    assert.ok(!existsSync(join(dir, 'contracts')), 'contracts/ should not exist');
  });

  it('creates contracts/main.kiss with --with-contract', () => {
    const dir = tmpTarget('contract');
    scaffold(dir, { name: 'LockedApp', withContract: true });
    assert.ok(existsSync(join(dir, 'contracts/main.kiss')));
    const content = readFileSync(join(dir, 'contracts/main.kiss'), 'utf8');
    assert.ok(content.includes('SIGNEDBY'));
  });

  it('creates all optional files with both flags', () => {
    const dir = tmpTarget('both');
    const files = scaffold(dir, { name: 'FullApp', withMaxima: true, withContract: true });
    assert.ok(existsSync(join(dir, 'service.js')));
    assert.ok(existsSync(join(dir, 'contracts/main.kiss')));
  });
});

// ── File content checks ───────────────────────────────────────────────────────

describe('scaffold — file content', () => {
  it('index.html references app name', () => {
    const dir = tmpTarget('html');
    scaffold(dir, { name: 'CoolApp' });
    const html = readFileSync(join(dir, 'index.html'), 'utf8');
    assert.ok(html.includes('CoolApp'));
    assert.ok(html.includes('<script src="mds.js">'));
    assert.ok(html.includes('<script src="app.js">'));
  });

  it('app.js references app name and MDS', () => {
    const dir = tmpTarget('appjs');
    scaffold(dir, { name: 'MyApp' });
    const js = readFileSync(join(dir, 'app.js'), 'utf8');
    assert.ok(js.includes('MDS.init'));
    assert.ok(js.includes('MDS.cmd'));
    assert.ok(js.includes('MyApp'));
  });

  it('mds.js has dev stub', () => {
    const dir = tmpTarget('mdsjs');
    scaffold(dir, { name: 'App' });
    const mds = readFileSync(join(dir, 'mds.js'), 'utf8');
    assert.ok(mds.includes('dev mode'));
    assert.ok(mds.includes('balance'));
    assert.ok(mds.includes('getaddress'));
  });

  it('README.md contains name and deployment instructions', () => {
    const dir = tmpTarget('readme');
    scaffold(dir, { name: 'Wallet' });
    const readme = readFileSync(join(dir, 'README.md'), 'utf8');
    assert.ok(readme.includes('Wallet'));
    assert.ok(readme.includes('zip'));
    assert.ok(readme.includes('minima.global'));
  });

  it('icon.png is a valid PNG (starts with PNG magic bytes)', () => {
    const dir = tmpTarget('icon');
    scaffold(dir, { name: 'App' });
    const buf = require('node:fs').readFileSync(join(dir, 'icon.png'));
    // PNG magic: 89 50 4E 47 0D 0A 1A 0A
    assert.equal(buf[0], 0x89);
    assert.equal(buf[1], 0x50); // 'P'
    assert.equal(buf[2], 0x4E); // 'N'
    assert.equal(buf[3], 0x47); // 'G'
  });
});

// ── ZIP readiness ─────────────────────────────────────────────────────────────

describe('scaffold — zip readiness', () => {
  it('all mandatory MiniDapp files present', () => {
    const dir = tmpTarget('zip');
    scaffold(dir, { name: 'ZipApp' });
    // Required by Minima MiniDapp spec
    const required = ['dapp.conf', 'icon.png', 'index.html'];
    for (const f of required) {
      assert.ok(existsSync(join(dir, f)), `MiniDapp spec requires: ${f}`);
    }
  });
});
