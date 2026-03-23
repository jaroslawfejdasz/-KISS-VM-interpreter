"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Vault = Vault;
exports.TwoOfThreeRecovery = TwoOfThreeRecovery;
/**
 * Vault — Spending rate-limited coin
 *
 * Allows the owner to spend only up to a certain amount per time window.
 * The rest must be returned to the vault script.
 *
 * Use cases:
 * - Cold wallet with daily spending limit
 * - Subscription services
 * - Allowance systems
 *
 * @param ownerPubKey      - Owner's public key
 * @param maxSpendPerBlock - Maximum amount spendable per block window
 * @param windowBlocks     - Size of the time window in blocks
 */
function Vault(ownerPubKey, maxSpendPerBlock, windowBlocks) {
    validatePubKey(ownerPubKey);
    if (maxSpendPerBlock <= 0)
        throw new Error('Vault: maxSpendPerBlock must be > 0');
    if (!Number.isInteger(windowBlocks) || windowBlocks < 1)
        throw new Error('Vault: windowBlocks must be >= 1');
    const maxPerWindow = maxSpendPerBlock * windowBlocks;
    return [
        `/* Vault: max spend ${maxPerWindow} per ${windowBlocks} blocks */`,
        `/* Must be signed by owner */`,
        `ASSERT SIGNEDBY(${ownerPubKey})`,
        `/* How much is being spent? */`,
        `LET spent = @AMOUNT - GETOUTAMT(0)`,
        `/* Enforce spending limit */`,
        `ASSERT spent LTE ${maxPerWindow}`,
        `/* Remaining must return to this exact script */`,
        `RETURN SAMESTATE(0, 4)`
    ].join('\n');
}
/**
 * TwoOfThreeRecovery — 2-of-3 with time-based emergency recovery
 *
 * Normal spend: requires owner key.
 * If owner key is lost, after `recoveryDelay` blocks,
 * 2 of 3 recovery keys can reclaim the coin.
 *
 * @param ownerPubKey     - Primary owner key
 * @param recoveryKeys    - Three recovery key addresses (3 exactly)
 * @param recoveryDelay   - Blocks of inactivity before recovery is possible
 */
function TwoOfThreeRecovery(ownerPubKey, recoveryKeys, recoveryDelay) {
    validatePubKey(ownerPubKey);
    recoveryKeys.forEach((k, i) => validatePubKey(k, `recoveryKeys[${i}]`));
    if (!Number.isInteger(recoveryDelay) || recoveryDelay < 1) {
        throw new Error('TwoOfThreeRecovery: recoveryDelay must be >= 1');
    }
    const [k1, k2, k3] = recoveryKeys;
    return [
        `/* TwoOfThreeRecovery: owner OR 2-of-3 recovery after ${recoveryDelay} blocks */`,
        `IF SIGNEDBY(${ownerPubKey}) THEN`,
        `  RETURN TRUE`,
        `ENDIF`,
        `/* Recovery path — requires time lock */`,
        `ASSERT @COINAGE GTE ${recoveryDelay}`,
        `RETURN MULTISIG(2, ${k1}, ${k2}, ${k3})`
    ].join('\n');
}
function validatePubKey(val, name = 'pubKey') {
    if (!val.startsWith('0x'))
        throw new Error(`${name} must start with 0x`);
    if (val.length - 2 !== 64)
        throw new Error(`${name} must be 32 bytes (64 hex chars)`);
}
//# sourceMappingURL=Vault.js.map