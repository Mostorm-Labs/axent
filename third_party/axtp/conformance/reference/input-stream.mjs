// Protocol reference only. Native preparation/submission/cleanup are substitute owners.
// This is not a system injector or evidence that a target application received input.
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
const check = (condition, message) => { if (!condition)
    throw new Error(message); };
const integer = (v, min, max) => Number.isSafeInteger(v) && v >= min && v <= max;
const canonical = v => JSON.stringify(v, (_k, x) => x && typeof x === "object" && !Array.isArray(x) ? Object.fromEntries(Object.entries(x).sort(([a], [b]) => a.localeCompare(b))) : x);
const same = (a, b) => canonical(a) === canonical(b);
export function crc16(bytes) { let c = 0xffff; for (const b of bytes) {
    c ^= b << 8;
    for (let i = 0; i < 8; i++)
        c = ((c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1) & 0xffff;
} return c; }
function fieldsRead(bytes, fields) { const r = {}; for (const f of fields)
    r[f.name] = f.type.startsWith("int") ? bytes.readIntBE(f.offset, f.bytes) : bytes.readUIntBE(f.offset, f.bytes); return r; }
function fieldsWrite(value, fields, size) { const bytes = Buffer.alloc(size); for (const f of fields) {
    const signed = f.type.startsWith("int"), max = 2 ** (f.bytes * 8 - (signed ? 1 : 0)) - 1, min = signed ? -(2 ** (f.bytes * 8 - 1)) : 0;
    check(integer(value[f.name], min, max), `input field range: ${f.name}`);
    signed ? bytes.writeIntBE(value[f.name], f.offset, f.bytes) : bytes.writeUIntBE(value[f.name], f.offset, f.bytes);
} return bytes; }
export function decodeData(raw, w) {
    const data = Buffer.from(raw);
    check(data.length > 1 && data.length <= w.maxDataBytes, "input data length/budget");
    if (data[0] === w.inputTypes.touch) {
        check((data.length - w.typeBytes) % w.touch.recordBytes === 0, "touch length");
        const n = (data.length - w.typeBytes) / w.touch.recordBytes;
        check(n >= 1 && n <= w.touch.maxContacts, "touch count limit");
        const records = Array.from({ length: n }, (_, i) => fieldsRead(data.subarray(w.typeBytes + i * w.touch.recordBytes), w.touch.fields));
        check(records.every(r => Object.values(w.touch.phases).includes(r.phase)), "touch phase");
        return { inputType: "touch", records };
    }
    check(data[0] === w.inputTypes.mouse, "unknown input type");
    const shape = w.mouse.records.find(r => r.kind === data[1]);
    check(shape, "mouse kind");
    check(data.length === w.typeBytes + shape.bytes, "mouse exact length");
    const r = fieldsRead(data.subarray(w.typeBytes), shape.fields);
    if (r.buttons !== undefined)
        check((r.buttons & ~31) === 0, "button bits");
    if (r.modifiers !== undefined)
        check((r.modifiers & ~15) === 0, "modifier bits");
    if (r.eventKind === 2)
        check(Object.values(w.mouse.wheelUnits).includes(r.wheelUnit) && Object.values(w.mouse.wheelPhases).includes(r.wheelPhase), "wheel unit/phase");
    return { inputType: "mouse", ...r };
}
export function encodeData(value, w) {
    check(Object.hasOwn(w.inputTypes, value.inputType), "unknown input type");
    let data;
    if (value.inputType === "touch") {
        check(Array.isArray(value.records) && value.records.length >= 1 && value.records.length <= w.touch.maxContacts, "touch count limit");
        data = Buffer.concat([Buffer.from([w.inputTypes.touch]), ...value.records.map(r => fieldsWrite(r, w.touch.fields, w.touch.recordBytes))]);
    }
    else {
        const shape = w.mouse.records.find(r => r.kind === value.eventKind);
        check(shape, "mouse kind");
        data = Buffer.concat([Buffer.from([w.inputTypes.mouse]), fieldsWrite(value, shape.fields, shape.bytes)]);
    }
    decodeData(data, w);
    return data;
}
function payloadOf(e, w) { check(integer(e.streamId, 1, 0xffffffff) && integer(e.seqId, 1, 0xffffffff), "stream identity/sequence range"); const p = Buffer.alloc(16); p.writeUInt32BE(e.streamId, 0); p.writeUInt32BE(e.seqId, 4); const cursor = BigInt(e.cursor); check(cursor >= 0n && cursor <= 0xffffffffffffffffn, "cursor range"); p.writeBigUInt64BE(cursor, 8); return Buffer.concat([p, encodeData(e, w)]); }
export function encodeRawFrame(payload, { sourceId = 1, destinationId = 16, messageId = 1, index = 0, count = 1 } = {}) {
    check(payload.length <= 65535 && integer(sourceId, 0, 255) && integer(destinationId, 0, 255) && integer(messageId, 0, 65535) && integer(count, 1, 255) && integer(index, 0, count - 1), "frame fields");
    const h = Buffer.alloc(12);
    h.write("AX");
    h[2] = 1;
    h[3] = 3;
    h.writeUInt16BE(payload.length, 4);
    h[6] = sourceId;
    h[7] = destinationId;
    h.writeUInt16BE(messageId, 8);
    h[10] = index;
    h[11] = count;
    const body = Buffer.concat([h, payload]);
    const c = Buffer.alloc(2);
    c.writeUInt16BE(crc16(body));
    return Buffer.concat([body, c]);
}
export function encodeFrame(e, w) { return encodeRawFrame(payloadOf(e, w), { messageId: e.seqId & 65535 }); }
export function encodeFragments(e, w, maxFrameSize) { check(integer(maxFrameSize, 15, 65549), "frame budget"); const p = payloadOf(e, w), budget = maxFrameSize - 14, count = Math.ceil(p.length / budget); check(count <= 255, "fragment count limit"); return Array.from({ length: count }, (_, index) => encodeRawFrame(p.subarray(index * budget, (index + 1) * budget), { messageId: e.seqId & 65535, index, count })); }
function parseFrame(raw) { const b = Buffer.from(raw); check(b.length >= 14, "frame truncated"); check(b[0] === 65 && b[1] === 88 && b[2] === 1 && b[3] === 3, "frame header"); check(b.length === b.readUInt16BE(4) + 14, "frame length"); check(crc16(b.subarray(0, -2)) === b.readUInt16BE(b.length - 2), "frame CRC"); check(b[11] >= 1 && b[10] < b[11], "fragment header"); return { payload: b.subarray(12, -2), source: b[6], destination: b[7], messageId: b.readUInt16BE(8), index: b[10], count: b[11], bytes: b.length }; }
function decodePayload(p, w, frameBytes) { check(p.length >= 18 && p.length <= 16 + w.maxDataBytes, "input payload length/budget"); const streamId = p.readUInt32BE(0), seqId = p.readUInt32BE(4), stamp = p.readBigUInt64BE(8); check(streamId > 0 && seqId > 0, "stream identity/sequence zero"); return { streamId, seqId, cursor: stamp <= BigInt(Number.MAX_SAFE_INTEGER) ? Number(stamp) : String(stamp), dataBytes: p.length - 16, frameBytes, ...decodeData(p.subarray(16), w) }; }
export function decodeFrame(raw, w) { const f = parseFrame(raw); check(f.count === 1 && f.index === 0, "fragment reassembly required"); return decodePayload(f.payload, w, f.bytes); }
export function decodeFrames(raws, w) { check(raws.length >= 1 && raws.length <= 255, "fragment count"); const frames = raws.map(parseFrame), first = frames[0]; check(first.count === frames.length, "fragment incomplete"); const seen = new Set(); for (const f of frames) {
    check(f.count === first.count && f.source === first.source && f.destination === first.destination && f.messageId === first.messageId && !seen.has(f.index), "fragment identity/duplicate");
    seen.add(f.index);
} const size = frames.reduce((n, f) => n + f.payload.length, 0); check(size <= 16 + w.maxDataBytes, "fragment budget"); return decodePayload(Buffer.concat(frames.sort((a, b) => a.index - b.index).map(f => f.payload)), w, frames.reduce((n, f) => n + f.bytes, 0)); }
export class InputContext {
    constructor(wire, facts) { this.w = wire; this.facts = structuredClone(facts); this.nextId = facts.initialStreamId ?? 1; this.lifecycleState = "closed"; this.contacts = new Set(); this.buttons = 0; this.modifiers = 0; this.scroll = null; this.seq = 0; this.cursor = 0n; this.submitted = 0; this.cleanupSucceeds = true; this.commands = new Map(); this.closedIds = new Set(); this.generation = 0; }
    beginOpen(r) { check(this.lifecycleState === "closed", "BUSY prepare"); const token = ++this.generation; this.pending = { token, request: structuredClone(r) }; this.lifecycleState = "opening"; return token; }
    completeOpen(token) { check(this.lifecycleState === "opening" && this.pending?.token === token && this.generation === token, "stale prepare completion"); const r = this.pending.request; this.pending = null; this.lifecycleState = "closed"; return this.open(r); }
    open(r) {
        check(r.controlVersion === 1 && r.format === this.w.format, "NOT_SUPPORTED format/control version");
        check(r.sessionId === this.facts.sessionId && r.runtimeInstanceId === this.facts.runtimeInstanceId, "PERMISSION_DENIED session");
        check(same(r.target, this.facts.target) && r.sourceBindingId === this.facts.sourceBindingId, "INVALID_STATE source/target binding");
        check(this.facts.nativeReady && this.facts.geometryReady, "NOT_SUPPORTED native/geometry readiness");
        check(typeof r.commandId === "string" && r.commandId.length > 0 && r.commandId.length <= 128, "INVALID_ARGUMENT commandId");
        const prior = this.commands.get(r.commandId);
        if (prior) {
            check(prior.key === canonical(r), "INVALID_ARGUMENT idempotency content");
            check(this.lifecycleState === "streaming" && prior.result.streamId === this.streamId, "INVALID_STATE retired open");
            return structuredClone(prior.result);
        }
        check(this.lifecycleState === "closed", "BUSY input owner/cleanup");
        check(Array.isArray(r.inputTypes) && r.inputTypes.length > 0 && new Set(r.inputTypes).size === r.inputTypes.length && r.inputTypes.every(t => t === "touch" || t === "mouse"), "INVALID_ARGUMENT input types");
        const accepted = r.inputTypes.filter(t => this.facts.inputTypes.includes(t));
        check(accepted.length > 0, "NOT_SUPPORTED input types");
        check(integer(r.maxDataBytes, 8, this.w.maxDataBytes), "INVALID_ARGUMENT data budget");
        const maxDataBytes = Math.min(r.maxDataBytes, this.facts.maxDataBytes), result = { format: this.w.format, acceptedInputTypes: accepted, maxDataBytes, revision: String(this.generation + 1), accepted: true };
        check(Boolean(r.touch) === r.inputTypes.includes("touch") && Boolean(r.mouse) === r.inputTypes.includes("mouse"), "INVALID_ARGUMENT per-type constraints");
        if (accepted.includes("touch")) {
            check(integer(r.touch.maxContacts, 1, this.w.touch.maxContacts), "INVALID_ARGUMENT touch limit");
            result.touch = { maxContacts: Math.min(r.touch.maxContacts, this.facts.touch.maxContacts, Math.floor((maxDataBytes - 1) / 7)) };
        }
        if (accepted.includes("mouse")) {
            const m = r.mouse, f = this.facts.mouse;
            check(integer(m.buttonsMask, 0, 31) && integer(m.modifierMask, 0, 15) && typeof m.allowChords === "boolean" && typeof m.continuousScroll === "boolean", "INVALID_ARGUMENT mouse limits");
            check(Array.isArray(m.wheelUnits) && new Set(m.wheelUnits).size === m.wheelUnits.length && m.wheelUnits.every(u => Object.hasOwn(this.w.mouse.wheelUnits, u)), "INVALID_ARGUMENT wheel units");
            const units = m.wheelUnits.filter(u => f.wheelUnits.includes(u));
            check(maxDataBytes >= (units.length ? 22 : 12), "NOT_SUPPORTED mouse data budget");
            result.mouse = { buttonsMask: m.buttonsMask & f.buttonsMask, modifierMask: m.modifierMask & f.modifierMask, allowChords: m.allowChords && f.allowChords, wheelUnits: units, continuousScroll: m.continuousScroll && f.continuousScroll && units.includes("regionQ16_16") };
        }
        check(this.nextId <= 0xffffffff, "UNAVAILABLE stream ID exhausted");
        this.streamId = this.nextId++;
        result.streamId = this.streamId;
        this.contract = result;
        this.seq = 0;
        this.cursor = 0n;
        this.lifecycleState = "streaming";
        this.generation++;
        if (this.commands.size === 64)
            this.commands.delete(this.commands.keys().next().value);
        this.commands.set(r.commandId, { key: canonical(r), result: structuredClone(result) });
        return structuredClone(result);
    }
    accept(raw) {
        let e;
        try {
            e = decodeFrame(raw, this.w);
        }
        catch (error) {
            if (this.lifecycleState === "streaming")
                this.fault();
            throw error;
        }
        check(this.lifecycleState === "streaming" && e.streamId === this.streamId, "unknown or retired stream");
        try {
            check(this.facts.nativeReady && this.facts.geometryReady, "input readiness lost");
            check(this.contract.acceptedInputTypes.includes(e.inputType), "unaccepted input type");
            check(e.dataBytes <= this.contract.maxDataBytes, "data budget limit");
            check(e.seqId === this.seq + 1, "sequence gap/duplicate");
            check(BigInt(e.cursor) >= this.cursor, "cursor backwards");
            let contacts = new Set(this.contacts), buttons = this.buttons, modifiers = this.modifiers, scroll = this.scroll;
            if (e.inputType === "touch") {
                check(e.records.length <= this.contract.touch.maxContacts, "touch count limit");
                const ids = e.records.map(r => r.contactId);
                check(new Set(ids).size === ids.length, "duplicate touch ID");
                check([...this.contacts].every(id => ids.includes(id)), "missing active touch ID");
                for (const r of e.records) {
                    if (r.phase === 0) {
                        check(!this.contacts.has(r.contactId), "touch Down active");
                        contacts.add(r.contactId);
                    }
                    else {
                        check(this.contacts.has(r.contactId), "orphan touch contact");
                        if (r.phase === 2 || r.phase === 3)
                            contacts.delete(r.contactId);
                    }
                }
            }
            else {
                const m = this.contract.mouse, k = e.eventKind;
                if (k === 3) {
                    buttons = 0;
                    modifiers = 0;
                    scroll = null;
                }
                else {
                    check((e.modifiers & ~m.modifierMask) === 0, "unsupported modifier");
                    modifiers = e.modifiers;
                    if (k !== 4) {
                        check((e.buttons & ~m.buttonsMask) === 0, "unsupported button");
                        check(m.allowChords || (e.buttons & (e.buttons - 1)) === 0, "unsupported chord");
                        if (k === 1)
                            check(e.buttons !== buttons, "button event without transition");
                        else
                            check(e.buttons === buttons, "implicit button change");
                        buttons = e.buttons;
                        if (k === 2) {
                            const unit = e.wheelUnit === 0 ? "detent120" : "regionQ16_16";
                            check(m.wheelUnits.includes(unit), "unsupported wheel unit");
                            if (e.wheelUnit === 0) {
                                check(e.wheelPhase === 0 && (e.wheelX !== 0 || e.wheelY !== 0) && !scroll, "invalid detent/overlapping wheel");
                            }
                            else {
                                check(m.continuousScroll && e.wheelPhase >= 1, "unsupported continuous phase");
                                const anchor = [e.x, e.y];
                                if (e.wheelPhase === 1) {
                                    check(!scroll, "overlapping scroll");
                                    scroll = anchor;
                                }
                                else {
                                    check(scroll && same(scroll, anchor), "scroll without Began or changed anchor");
                                    if (e.wheelPhase === 3 || e.wheelPhase === 4) {
                                        check(e.wheelX === 0 && e.wheelY === 0, "scroll terminal delta");
                                        scroll = null;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            this.contacts = contacts;
            this.buttons = buttons;
            this.modifiers = modifiers;
            this.scroll = scroll;
            this.seq = e.seqId;
            this.cursor = BigInt(e.cursor);
            this.submitted++;
            return this.snapshot();
        }
        catch (error) {
            this.fault();
            throw error;
        }
    }
    fault() { this.generation++; this.lifecycleState = "closing"; this.cleanup(); }
    cleanup() { if (!this.cleanupSucceeds)
        return false; this.contacts.clear(); this.buttons = 0; this.modifiers = 0; this.scroll = null; this.lifecycleState = "closed"; if (this.streamId)
        this.closedIds.add(this.streamId); return true; }
    close(r) { check(r.sessionId === this.facts.sessionId && same(r.target, this.facts.target), "PERMISSION_DENIED close identity"); if (this.closedIds.has(r.streamId))
        return { streamId: r.streamId, closed: true, alreadyClosed: true }; check(r.streamId === this.streamId, "unknown stream close"); this.generation++; this.lifecycleState = "closing"; check(this.cleanup(), "UNAVAILABLE cleanupPending"); return { streamId: r.streamId, closed: true, alreadyClosed: false }; }
    invalidate() { this.fault(); return this.snapshot(); }
    snapshot() { return { lifecycleState: this.lifecycleState, streamId: this.streamId ?? 0, activeContacts: this.contacts.size, buttons: this.buttons, modifiers: this.modifiers, scrollActive: Boolean(this.scroll), submitted: this.submitted, generation: this.generation }; }
}
export function assertSubset(actual, expected) { for (const [key, value] of Object.entries(expected)) {
    if (value && typeof value === "object" && !Array.isArray(value))
        assertSubset(actual[key], value);
    else
        assert.deepEqual(actual[key], value, key);
} }
export function runInputCase(value, fixture, wire, root) {
    const model = new InputContext(wire, fixture.facts), base = fixture.request;
    let receiverOptIn = true, controllerRequested = true, queue = [], prepareToken, observations = 0;
    for (const step of value.steps ?? []) {
        const op = step.input_model;
        if (!op)
            throw new Error("input case has a non-executable step");
        const input = op.input ?? {};
        let output = {};
        try {
            switch (op.action) {
                case "vectors": {
                    check(input.file === "contract/test-vectors/input_stream.json", "unexpected vector input");
                    const corpus = JSON.parse(fs.readFileSync(path.join(root, input.file), "utf8"));
                    assert.deepEqual(corpus.wire, wire);
                    let positiveFrames = 0, negativeCases = 0;
                    for (const c of corpus.cases) {
                        const owner = new InputContext(wire, { ...fixture.facts, initialStreamId: 7 });
                        owner.open(base);
                        let error;
                        for (const row of c.frames) {
                            const raw = Buffer.from(row.hex, "hex");
                            try {
                                if (row.expectDecode)
                                    assert.deepEqual(decodeFrame(raw, wire), row.expectDecode);
                                owner.accept(raw);
                                if (c.valid)
                                    positiveFrames++;
                            }
                            catch (e) {
                                error = e;
                                break;
                            }
                        }
                        if (c.valid) {
                            check(!error, `${c.name}: ${error?.message}`);
                            assertSubset(owner.snapshot(), { activeContacts: 0, buttons: 0, modifiers: 0, scrollActive: false });
                        }
                        else {
                            check(error && error.message.toLowerCase().includes(c.expectError.toLowerCase()), `${c.name} expected ${c.expectError}, got ${error?.message}`);
                            negativeCases++;
                        }
                    }
                    output = { positiveFrames, negativeCases };
                    observations += positiveFrames + negativeCases;
                    break;
                }
                case "gate":
                    receiverOptIn = input.receiverOptIn ?? receiverOptIn;
                    controllerRequested = input.controllerRequested ?? controllerRequested;
                    if (!receiverOptIn || !controllerRequested) {
                        queue = [];
                        model.invalidate();
                    }
                    break;
                case "open":
                    check(receiverOptIn && controllerRequested, "PERMISSION_DENIED gate");
                    output = model.open({ ...structuredClone(base), ...input });
                    break;
                case "prepare":
                    check(receiverOptIn && controllerRequested, "PERMISSION_DENIED gate");
                    prepareToken = model.beginOpen({ ...structuredClone(base), ...input });
                    break;
                case "complete_prepare":
                    output = model.completeOpen(prepareToken);
                    break;
                case "data":
                    model.accept(encodeFrame(input, wire));
                    break;
                case "enqueue":
                    check(model.lifecycleState === "streaming", "inactive input queue");
                    if (queue.length === (fixture.queueCapacity ?? 2)) {
                        queue = [];
                        model.invalidate();
                        throw new Error("UNAVAILABLE queue overflow");
                    }
                    queue.push({ raw: encodeFrame(input, wire), generation: model.generation });
                    break;
                case "pump":
                    for (const job of queue)
                        if (job.generation === model.generation)
                            model.accept(job.raw);
                    queue = [];
                    break;
                case "cleanup_result":
                    model.cleanupSucceeds = input.success;
                    break;
                case "close":
                    queue = [];
                    output = model.close({ sessionId: fixture.facts.sessionId, target: fixture.facts.target, streamId: model.streamId, ...input });
                    break;
                case "invalidate":
                    queue = [];
                    model.invalidate();
                    break;
                case "readiness":
                    Object.assign(model.facts, input);
                    if (!model.facts.nativeReady || !model.facts.geometryReady) {
                        queue = [];
                        model.invalidate();
                    }
                    break;
                case "snapshot": break;
                default: throw new Error(`unknown input oracle action: ${op.action}`);
            }
            if (op.expect.errorContains)
                throw new Error("expected operation rejection");
        }
        catch (error) {
            check(op.expect.errorContains && error.message.toLowerCase().includes(op.expect.errorContains.toLowerCase()), `${value.id}/${op.action}: ${error.message}`);
            output = { ...output, errorContains: op.expect.errorContains };
        }
        const actual = { ...model.snapshot(), ...output, queueLength: queue.length };
        assertSubset(actual, op.expect);
        observations++;
    }
    return observations;
}
