export type ValueType = 'BOOLEAN' | 'NUMBER' | 'HEX' | 'STRING';

export class MiniValue {
  constructor(public type: ValueType, public raw: string) {}
  
  toString() { return this.raw; }

  static boolean(v: boolean): MiniValue { return new MiniValue('BOOLEAN', v ? 'TRUE' : 'FALSE'); }
  static number(v: number | string): MiniValue { return new MiniValue('NUMBER', String(v)); }
  static hex(v: string): MiniValue {
    const clean = v.startsWith('0x') ? v : '0x' + v;
    return new MiniValue('HEX', clean.toLowerCase());
  }
  static string(v: string): MiniValue { return new MiniValue('STRING', v); }

  asBoolean(): boolean {
    if (this.type === 'BOOLEAN') return this.raw === 'TRUE';
    if (this.type === 'NUMBER') return parseFloat(this.raw) !== 0;
    throw new Error(`Cannot convert ${this.type} to BOOLEAN`);
  }

  asNumber(): number {
    if (this.type === 'NUMBER') return parseFloat(this.raw);
    if (this.type === 'BOOLEAN') return this.raw === 'TRUE' ? 1 : 0;
    if (this.type === 'HEX') return parseInt(this.raw, 16);
    throw new Error(`Cannot convert ${this.type} to NUMBER`);
  }

  asHex(): string {
    if (this.type === 'HEX') return this.raw;
    if (this.type === 'NUMBER') {
      const n = Math.floor(parseFloat(this.raw));
      return '0x' + n.toString(16).padStart(2, '0');
    }
    throw new Error(`Cannot convert ${this.type} to HEX`);
  }

  asString(): string {
    return this.raw;
  }
}
