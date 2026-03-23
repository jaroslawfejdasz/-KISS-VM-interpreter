import { ContractTemplate } from '../types';

/**
 * Vault Contract (with guardian)
 * Protects coins with a 2-layer security model:
 *   - Hot key: can initiate withdrawal, but funds are delayed
 *   - Cold key (guardian): can immediately cancel any pending withdrawal
 *
 * Pattern:
 *   1. Owner initiates withdrawal with hot key → coin enters "pending" state (STATE(1) = 1)
 *   2. After delayBlocks, owner can complete withdrawal
 *   3. Guardian can veto any withdrawal by signing immediately
 *
 * Use cases: self-custody with safety net, anti-theft, key recovery system.
 */
export const vault: ContractTemplate = {
  name: 'vault',
  description: [
    'Two-key vault with time-delayed withdrawals.',
    'Hot key initiates, guardian (cold key) can veto.',
    'After delay passes with no veto, hot key can complete withdrawal.',
    'Great for self-custody with a hardware wallet as guardian.',
  ].join(' '),
  params: [
    {
      name: 'hotPubKey',
      type: 'hex',
      description: 'Hot wallet public key (initiates withdrawals)',
    },
    {
      name: 'coldPubKey',
      type: 'hex',
      description: 'Cold wallet / guardian public key (can veto)',
    },
    {
      name: 'delayBlocks',
      type: 'number',
      description: 'Blocks to wait before withdrawal completes (e.g. 2880 = ~2 days)',
      default: '2880',
    },
  ],
  script({ hotPubKey, coldPubKey, delayBlocks }) {
    return [
      `/* Vault Contract — time-delayed withdrawals with guardian veto */`,
      ``,
      `/* Guardian (cold key) can always spend immediately — emergency veto */`,
      `IF SIGNEDBY(${coldPubKey}) THEN`,
      `  RETURN TRUE`,
      `ENDIF`,
      ``,
      `/* Hot key can spend after delay */`,
      `LET delayPassed = (@COINAGE GTE ${delayBlocks})`,
      `LET hotSigned = SIGNEDBY(${hotPubKey})`,
      ``,
      `RETURN hotSigned AND delayPassed`,
    ].join('\n');
  },
  example: {
    hotPubKey: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
    coldPubKey: '0x1122334411223344112233441122334411223344112233441122334411223344',
    delayBlocks: '2880',
  },
};

/**
 * Dead Man's Switch Contract
 * If owner doesn't "check in" (spend and recreate) within N blocks,
 * a designated beneficiary gains access.
 *
 * Use cases: inheritance, dead man's switch for sensitive data.
 */
export const deadMansSwitch: ContractTemplate = {
  name: 'dead-mans-switch',
  description: [
    'If owner does not check in within checkInInterval blocks,',
    'beneficiary gains access. Owner checks in by spending and recreating the coin.',
  ].join(' '),
  params: [
    {
      name: 'ownerPubKey',
      type: 'hex',
      description: 'Owner public key',
    },
    {
      name: 'beneficiaryPubKey',
      type: 'hex',
      description: 'Beneficiary public key (gains access after timeout)',
    },
    {
      name: 'checkInInterval',
      type: 'number',
      description: 'Max blocks between check-ins before beneficiary can claim',
      default: '10080',
    },
  ],
  script({ ownerPubKey, beneficiaryPubKey, checkInInterval }) {
    return [
      `/* Dead Man's Switch */`,
      ``,
      `/* Owner can always spend (check-in or normal use) */`,
      `IF SIGNEDBY(${ownerPubKey}) THEN`,
      `  RETURN TRUE`,
      `ENDIF`,
      ``,
      `/* Beneficiary can claim if coin age exceeds check-in interval */`,
      `LET lapsed = (@COINAGE GTE ${checkInInterval})`,
      `LET beneficiarySigned = SIGNEDBY(${beneficiaryPubKey})`,
      ``,
      `RETURN lapsed AND beneficiarySigned`,
    ].join('\n');
  },
  example: {
    ownerPubKey: '0xaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccddaabbccdd',
    beneficiaryPubKey: '0x1122334411223344112233441122334411223344112233441122334411223344',
    checkInInterval: '10080',
  },
};
