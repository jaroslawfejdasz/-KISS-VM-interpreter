"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.buildGlobals = buildGlobals;
exports.defaultTransaction = defaultTransaction;
const values_1 = require("../interpreter/values");
function buildGlobals(tx, inputIndex) {
    const coin = tx.inputs[inputIndex];
    if (!coin)
        throw new Error(`Input ${inputIndex} not found`);
    return {
        '@BLOCK': values_1.MiniValue.number(tx.blockNumber),
        '@BLOCKMILLI': values_1.MiniValue.number(tx.blockTime),
        '@CREATED': values_1.MiniValue.number(coin.blockCreated),
        '@COINAGE': values_1.MiniValue.number(tx.blockNumber - coin.blockCreated),
        '@INPUT': values_1.MiniValue.number(inputIndex),
        '@COINID': values_1.MiniValue.hex(coin.coinId),
        '@AMOUNT': values_1.MiniValue.number(coin.amount),
        '@ADDRESS': values_1.MiniValue.hex(coin.address),
        '@TOKENID': values_1.MiniValue.hex(coin.tokenId),
        '@SCRIPT': values_1.MiniValue.string(''),
        '@TOTIN': values_1.MiniValue.number(tx.inputs.length),
        '@TOTOUT': values_1.MiniValue.number(tx.outputs.length),
    };
}
function defaultTransaction(overrides = {}) {
    return {
        inputs: [{
                coinId: '0xabcdef1234567890',
                address: '0x1234567890abcdef',
                amount: 100,
                tokenId: '0x00',
                blockCreated: 1000,
                stateVars: {},
            }],
        outputs: [{
                address: '0xdeadbeef12345678',
                amount: 100,
                tokenId: '0x00',
                keepState: false,
            }],
        blockNumber: 1100,
        blockTime: Date.now(),
        signatures: [],
        stateVars: {},
        prevStateVars: {},
        ...overrides,
    };
}
//# sourceMappingURL=transaction.js.map