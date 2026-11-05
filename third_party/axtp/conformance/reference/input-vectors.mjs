import { decodeFrame, encodeFrame, encodeRawFrame } from "./input-stream.mjs";
// Values are deterministic fixtures; layouts and enum values come from adopted YAML.
export function createInputVectors(w) {
    const t = (...records) => ({ inputType: "touch", records });
    const r = (contactId, phase, x = 10000, y = 20000) => ({ contactId, phase, x, y });
    const m = (eventKind, extra = {}) => ({ inputType: "mouse", eventKind, modifiers: 0, buttons: 0, x: 32768, y: 32768, ...extra });
    const d = w.touch.phases.down, mv = w.touch.phases.move, u = w.touch.phases.up, c = w.touch.phases.cancel;
    const five = Array.from({ length: 5 }, (_, i) => r(i + 1, d, 5000 * (i + 1), 4000 * (i + 1)));
    const traces = [
        ["touch_single", [t(r(7, d)), t(r(7, mv, 11000, 21000)), t(r(7, u, 11000, 21000))]],
        ["touch_two", [t(r(7, d), r(8, d, 30000, 40000)), t(r(7, mv, 11000, 21000), r(8, mv, 31000, 41000)), t(r(7, u, 11000, 21000), r(8, mv, 31000, 41000)), t(r(8, u, 31000, 41000))]],
        ["touch_five", [t(...five), t(...five.map(x => ({ ...x, phase: mv }))), t(...five.map(x => ({ ...x, phase: c })))]],
        ["mixed", [t(r(7, d), r(8, d, 30000, 40000)), m(0), t(r(7, u, 11000, 21000), r(8, mv, 31000, 41000)), m(1, { buttons: 1 }), t(r(8, u, 31000, 41000)), m(1), m(4, { modifiers: 2 }), m(4), m(3)]],
        ["mouse_wheel", [m(0), m(2, { wheelUnit: 0, wheelPhase: 0, wheelX: 0, wheelY: 120 }), m(2, { wheelUnit: 1, wheelPhase: 1, wheelX: 0, wheelY: 0 }), m(2, { wheelUnit: 1, wheelPhase: 2, wheelX: -4096, wheelY: 8192 }), m(2, { wheelUnit: 1, wheelPhase: 3, wheelX: 0, wheelY: 0 }), m(3)]]
    ];
    const entry = (sample, seq) => { const bytes = encodeFrame({ ...sample, streamId: 7, seqId: seq, cursor: seq * 1000 }, w); return { hex: bytes.toString("hex"), expectDecode: decodeFrame(bytes, w) }; };
    const cases = traces.map(([name, samples]) => ({ name, valid: true, frames: samples.map((sample, i) => entry(sample, i + 1)) }));
    const rawData = (data, seq = 1) => { const header = Buffer.alloc(16); header.writeUInt32BE(7, 0); header.writeUInt32BE(seq, 4); header.writeBigUInt64BE(BigInt(seq * 1000), 8); return encodeRawFrame(Buffer.concat([header, data]), { messageId: seq }).toString("hex"); };
    const down = entry(t(r(7, d)), 1), double = entry(t(r(7, d), r(8, d)), 1);
    const crc = Buffer.from(down.hex, "hex");
    crc[crc.length - 1] ^= 1;
    const bad = (name, frames, error) => cases.push({ name, valid: false, frames: frames.map(hex => ({ hex })), expectError: error });
    bad("frame_crc", [crc.toString("hex")], "CRC");
    bad("frame_truncated", [down.hex.slice(0, -2)], "length");
    bad("unknown_type", [rawData(Buffer.from([255, 0]))], "type");
    bad("touch_extra_byte", [rawData(Buffer.from("0100070027104e2000", "hex"))], "length");
    bad("duplicate_contact", [entry(t(r(7, d), r(7, d)), 1).hex], "duplicate");
    bad("orphan_move", [entry(t(r(7, mv)), 1).hex], "orphan");
    bad("missing_active", [double.hex, entry(t(r(7, mv)), 2).hex], "missing active");
    bad("sequence_gap", [down.hex, entry(t(r(7, mv)), 3).hex], "sequence");
    bad("implicit_buttons", [entry(m(0, { buttons: 1 }), 1).hex], "implicit");
    bad("unknown_button", [rawData(Buffer.from("020100000000002080008000", "hex"))], "button");
    bad("unknown_modifier", [rawData(Buffer.from("02040010", "hex"))], "modifier");
    bad("wheel_without_begin", [entry(m(2, { wheelUnit: 1, wheelPhase: 2, wheelX: 0, wheelY: 120 }), 1).hex], "Began");
    bad("zero_detent", [entry(m(2, { wheelUnit: 0, wheelPhase: 0, wheelX: 0, wheelY: 0 }), 1).hex], "detent");
    return { format: w.format, wire: w, limits: { maxDataBytes: 4096, touch: { maxContacts: 10 }, mouse: { buttonsMask: 31, modifierMask: 15, allowChords: true, wheelUnits: ["detent120", "regionQ16_16"], continuousScroll: true } }, cases };
}
