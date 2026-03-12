export type ValueType = 'BOOLEAN' | 'NUMBER' | 'HEX' | 'STRING';
export declare class MiniValue {
    type: ValueType;
    raw: string;
    constructor(type: ValueType, raw: string);
    toString(): string;
    static boolean(v: boolean): MiniValue;
    static number(v: number | string): MiniValue;
    static hex(v: string): MiniValue;
    static string(v: string): MiniValue;
    asBoolean(): boolean;
    asNumber(): number;
    asHex(): string;
    asString(): string;
}
//# sourceMappingURL=values.d.ts.map