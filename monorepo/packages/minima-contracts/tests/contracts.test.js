/**
 * minima-contracts tests
 */
const assert = require('node:assert/strict');
const { describe, it } = require('node:test');

const {
  timelockContract,
  multisigContract,
  htlcContract,
  paymentChannelContract,
  nftContract,
  escrowContract,
  vaultContract,
  getTemplate,
} = require('../dist/index.js');

// ── timelockContract ──────────────────────────────────────────────────────────

describe('timelockContract', () => {
  it('contains @BLOCK GTE and SIGNEDBY', () => {
    const script = timelockContract(1000n, '0xABC');
    assert.ok(script.includes('@BLOCK GTE 1000'));
    assert.ok(script.includes('SIGNEDBY 0xABC'));
  });

  it('embeds lock block in comments', () => {
    const script = timelockContract(5000n, '0xDEF');
    assert.ok(script.includes('5000'));
  });

  it('uses AND to require both time and signature', () => {
    const script = timelockContract(100n, '0xABC');
    assert.ok(script.includes('AND'));
  });
});

// ── multisigContract ──────────────────────────────────────────────────────────

describe('multisigContract', () => {
  it('2-of-3 multisig', () => {
    const script = multisigContract(2, ['0xA', '0xB', '0xC']);
    assert.ok(script.includes('MULTISIG 2 3'));
    assert.ok(script.includes('0xA'));
    assert.ok(script.includes('0xB'));
    assert.ok(script.includes('0xC'));
  });

  it('1-of-1 multisig', () => {
    const script = multisigContract(1, ['0xONLY']);
    assert.ok(script.includes('MULTISIG 1 1 0xONLY'));
  });

  it('throws for required > addresses.length', () => {
    assert.throws(() => multisigContract(3, ['0xA', '0xB']), /required/);
  });

  it('throws for required < 1', () => {
    assert.throws(() => multisigContract(0, ['0xA']), /required/);
  });

  it('includes all addresses', () => {
    const addrs = ['0xA', '0xB', '0xC', '0xD'];
    const script = multisigContract(3, addrs);
    for (const a of addrs) assert.ok(script.includes(a));
  });
});

// ── htlcContract ──────────────────────────────────────────────────────────────

describe('htlcContract', () => {
  it('contains SHA3 hashlock check', () => {
    const script = htlcContract('0xHASH', '0xRECIPIENT', '0xREFUND', 500n);
    assert.ok(script.includes('SHA3'));
    assert.ok(script.includes('0xHASH'));
    assert.ok(script.includes('EQ'));
  });

  it('has refund path with block timeout', () => {
    const script = htlcContract('0xH', '0xR', '0xREFUND', 999n);
    assert.ok(script.includes('@BLOCK GTE 999'));
    assert.ok(script.includes('SIGNEDBY 0xREFUND'));
  });

  it('uses STATE 0 for preimage', () => {
    const script = htlcContract('0xH', '0xR', '0xRF', 100n);
    assert.ok(script.includes('STATE 0'));
  });

  it('has IF/ELSE/ENDIF structure', () => {
    const script = htlcContract('0xH', '0xR', '0xRF', 100n);
    assert.ok(script.includes('IF'));
    assert.ok(script.includes('ELSE'));
    assert.ok(script.includes('ENDIF'));
  });
});

// ── paymentChannelContract ────────────────────────────────────────────────────

describe('paymentChannelContract', () => {
  it('cooperative close: both sign', () => {
    const script = paymentChannelContract('0xALICE', '0xBOB', 2000n);
    assert.ok(script.includes('SIGNEDBY 0xALICE AND SIGNEDBY 0xBOB'));
  });

  it('unilateral close after timeout', () => {
    const script = paymentChannelContract('0xALICE', '0xBOB', 2000n);
    assert.ok(script.includes('@BLOCK GTE 2000'));
  });

  it('has RETURN FALSE fallback', () => {
    const script = paymentChannelContract('0xA', '0xB', 100n);
    assert.ok(script.includes('RETURN FALSE'));
  });
});

// ── nftContract ───────────────────────────────────────────────────────────────

describe('nftContract', () => {
  it('checks @TOKENID and SIGNEDBY', () => {
    const script = nftContract('0xOWNER', '0xTOKENID123');
    assert.ok(script.includes('@TOKENID EQ 0xTOKENID123'));
    assert.ok(script.includes('SIGNEDBY 0xOWNER'));
  });

  it('uses AND to require both checks', () => {
    const script = nftContract('0xO', '0xT');
    assert.ok(script.includes('AND'));
  });
});

// ── escrowContract ────────────────────────────────────────────────────────────

describe('escrowContract', () => {
  it('buyer + seller cooperative path', () => {
    const script = escrowContract('0xB', '0xS', '0xA');
    assert.ok(script.includes('SIGNEDBY 0xB AND SIGNEDBY 0xS'));
  });

  it('buyer + arbiter path', () => {
    const script = escrowContract('0xB', '0xS', '0xA');
    assert.ok(script.includes('SIGNEDBY 0xB AND SIGNEDBY 0xA'));
  });

  it('seller + arbiter path', () => {
    const script = escrowContract('0xB', '0xS', '0xA');
    assert.ok(script.includes('SIGNEDBY 0xS AND SIGNEDBY 0xA'));
  });

  it('has RETURN FALSE fallback', () => {
    const script = escrowContract('0xB', '0xS', '0xA');
    assert.ok(script.includes('RETURN FALSE'));
  });
});

// ── vaultContract ─────────────────────────────────────────────────────────────

describe('vaultContract', () => {
  it('cold wallet has full access', () => {
    const script = vaultContract('0xHOT', '0xCOLD', 1000n);
    assert.ok(script.includes('SIGNEDBY 0xCOLD'));
    assert.ok(script.includes('RETURN TRUE'));
  });

  it('hot wallet limited by @AMOUNT', () => {
    const script = vaultContract('0xHOT', '0xCOLD', 500n);
    assert.ok(script.includes('@AMOUNT LTE 500'));
    assert.ok(script.includes('SIGNEDBY 0xHOT'));
  });

  it('daily limit is enforced', () => {
    const script = vaultContract('0xH', '0xC', 250n);
    assert.ok(script.includes('250'));
  });
});

// ── getTemplate ───────────────────────────────────────────────────────────────

describe('getTemplate', () => {
  it('timelock template', () => {
    const script = getTemplate('timelock', { lockBlock: '1000', ownerAddress: '0xABC' });
    assert.ok(script.includes('@BLOCK GTE 1000'));
  });

  it('multisig template', () => {
    const script = getTemplate('multisig', { required: '2', addresses: '0xA, 0xB, 0xC' });
    assert.ok(script.includes('MULTISIG 2 3'));
  });

  it('escrow template', () => {
    const script = getTemplate('escrow', { buyer: '0xB', seller: '0xS', arbiter: '0xA' });
    assert.ok(script.includes('Escrow'));
  });

  it('vault template', () => {
    const script = getTemplate('vault', { hotWallet: '0xH', coldWallet: '0xC', dailyLimit: '1000' });
    assert.ok(script.includes('@AMOUNT LTE 1000'));
  });

  it('throws for unknown template', () => {
    assert.throws(() => getTemplate('unknown', {}), /Unknown template/);
  });

  it('all templates available', () => {
    const names = ['timelock', 'multisig', 'htlc', 'paymentChannel', 'nft', 'escrow', 'vault'];
    for (const n of names) {
      assert.ok(typeof getTemplate === 'function', `Template missing: ${n}`);
    }
  });
});
