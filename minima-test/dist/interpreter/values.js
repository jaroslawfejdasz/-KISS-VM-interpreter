"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MiniValue = void 0;
class MiniValue {
    constructor(type, raw) {
        this.type = type;
        this.raw = raw;
    }
    toString() { return this.raw; }
    static boolean(v) { return new MiniValue('BOOLEAN', v ? 'TRUE' : 'FALSE'); }
    static number(v) { return new MiniValue('NUMBER', String(v)); }
    static hex(v) {
        const clean = v.startsWith('0x') ? v : '0x' + v;
        return new MiniValue('HEX', clean.toLowerCase());
    }
    static string(v) { return new MiniValue('STRING', v); }
    asBoolean() {
        if (this.type === 'BOOLEAN')
            return this.raw === 'TRUE';
        if (this.type === 'NUMBER')
            return parseFloat(this.raw) !== 0;
        throw new Error(`Cannot convert ${this.type} to BOOLEAN`);
    }
    asNumber() {
        if (this.type === 'NUMBER')
            return parseFloat(this.raw);
        if (this.type === 'BOOLEAN')
            return this.raw === 'TRUE' ? 1 : 0;
        if (this.type === 'HEX')
            return parseInt(this.raw, 16);
        throw new Error(`Cannot convert ${this.type} to NUMBER`);
    }
    asHex() {
        if (this.type === 'HEX')
            return this.raw;
        if (this.type === 'NUMBER') {
            const n = Math.floor(parseFloat(this.raw));
            return '0x' + n.toString(16).padStart(2, '0');
        }
        throw new Error(`Cannot convert ${this.type} to HEX`);
    }
    asString() {
        return this.raw;
    }
}
exports.MiniValue = MiniValue;
//# sourceMappingURL=values.js.map