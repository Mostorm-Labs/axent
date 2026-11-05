import assert from "node:assert/strict";
import test from "node:test";
import fs from "node:fs";
import { createRequire } from "node:module";
import { decodeFrame, encodeFrame, decodeFrames, encodeFragments, InputContext } from "./input-stream.mjs";
const require = createRequire(new URL("../../tooling/generators/package.json", import.meta.url));
const YAML = require("yaml");
const source = YAML.parse(fs.readFileSync(new URL("../../contract/registry/domains/input/domain.yaml", import.meta.url), "utf8"));
const wire = source.input_stream;
const review = JSON.parse(fs.readFileSync(new URL("../../workspace/protocol/input/input.sample.review-examples.json", import.meta.url), "utf8"));
const defaults = { sessionId: "auth-session", sourceBindingId: "binding-1", target: { sessionId: "source-1", sessionGeneration: "1", slotId: "slot-1", bindingGeneration: "1", lifecycleGeneration: "1" }, runtimeInstanceId: "rx-1", format: wire.format, inputTypes: ["touch", "mouse"], maxDataBytes: 4096, touch: { maxContacts: 10 }, mouse: { buttonsMask: 31, modifierMask: 15, allowChords: true, wheelUnits: ["detent120", "regionQ16_16"], continuousScroll: true }, nativeReady: true, geometryReady: true };
const ctx = () => new InputContext(wire, defaults);
const request = (extra = {}) => ({ ...structuredClone(defaults), controlVersion: 1, commandId: "open-1", ...extra });
const event = (seq, records) => ({ streamId: 1, seqId: seq, cursor: seq * 1000, inputType: "touch", records });
const record = (id, phase = 0) => ({ contactId: id, phase, x: 10000, y: 20000 });
test("known independent double-touch bytes decode without an input header", () => {
    const row = review.cases.find(c => c.id === "touch-two-same-sample-terminal").frames[0];
    let decoded;
    assert.doesNotThrow(() => decoded = decodeFrame(Buffer.from(row.hex.replaceAll(" ", ""), "hex"), wire));
    assert.deepEqual(decoded, row.expected);
    assert.equal(encodeFrame(decoded, wire).toString("hex"), row.hex.replaceAll(" ", ""));
});
test("25 independently produced samples preserve exact fields and lengths", () => {
    for (const c of review.cases.filter(c => c.valid))
        for (const row of c.frames) {
            const raw = Buffer.from(row.hex.replaceAll(" ", ""), "hex");
            const decoded = decodeFrame(raw, wire);
            assert.deepEqual(decoded, row.expected, c.id);
            assert.deepEqual(encodeFrame(decoded, wire), raw, c.id);
        }
});
test("fragmentation completes before any input parse, and invalid fragments do not inject", () => {
    const sample = event(1, Array.from({ length: 5 }, (_, i) => record(i)));
    const parts = encodeFragments(sample, wire, 64);
    assert.equal(parts.length, 2);
    assert(parts.every(p => p.length <= 64));
    assert.deepEqual(decodeFrames(parts, wire).records, sample.records);
    assert.throws(() => decodeFrame(parts[0], wire), /reassembly/i);
    assert.throws(() => decodeFrames(parts.slice(1), wire), /fragment/i);
    assert.throws(() => decodeFrames([parts[0], parts[0]], wire), /fragment/i);
});
test("complete samples reject a missing active finger without submitting any prefix", () => {
    const m = ctx();
    assert.doesNotThrow(() => m.open(request()));
    m.accept(encodeFrame(event(1, [record(7), record(8)]), wire));
    assert.equal(m.snapshot().submitted, 1);
    assert.throws(() => m.accept(encodeFrame(event(2, [record(7, 1)]), wire)), /missing.*active/i);
    assert.equal(m.snapshot().submitted, 1);
    assert.equal(m.snapshot().lifecycleState, "closed");
});
test("all reviewed positive and negative wire/lifecycle traces run in the context oracle", () => {
    for (const c of review.cases) {
        const m = new InputContext(wire, { ...defaults, initialStreamId: 7 });
        m.open(request());
        let result;
        for (const row of c.frames) {
            const raw = Buffer.from(row.hex.replaceAll(" ", ""), "hex");
            try {
                m.accept(raw);
                result = null;
            }
            catch (e) {
                result = e;
                break;
            }
        }
        if (c.valid) {
            assert.equal(result, null, c.id);
            assert.equal(m.snapshot().activeContacts, 0, c.id);
        }
        else
            assert(result instanceof Error, c.id);
    }
});
test("input types interleave and only close cancels all ownership", () => {
    const m = ctx();
    m.open(request());
    m.accept(encodeFrame(event(1, [record(7), record(8)]), wire));
    m.accept(encodeFrame({ streamId: 1, seqId: 2, cursor: 2000, inputType: "mouse", eventKind: 1, modifiers: 0, buttons: 1, x: 20000, y: 30000 }, wire));
    assert.equal(m.snapshot().activeContacts, 2);
    assert.equal(m.snapshot().buttons, 1);
    m.accept(encodeFrame({ streamId: 1, seqId: 3, cursor: 3000, inputType: "mouse", eventKind: 3 }, wire));
    assert.equal(m.snapshot().activeContacts, 2);
    assert.equal(m.snapshot().buttons, 0);
    m.close({ streamId: 1, sessionId: "auth-session", target: defaults.target });
    assert.equal(m.snapshot().activeContacts, 0);
    assert.equal(m.open(request({ commandId: "open-2" })).streamId, 2);
    assert.throws(() => m.accept(encodeFrame(event(4, [record(7, 2)]), wire)), /retired|stream/i);
    assert.equal(m.snapshot().lifecycleState, "streaming");
});
test("cleanup failure blocks a new owner and a repeated close retries cleanup", () => {
    const m = ctx();
    m.open(request());
    m.accept(encodeFrame(event(1, [record(7)]), wire));
    m.cleanupSucceeds = false;
    assert.throws(() => m.close({ streamId: 1, sessionId: "auth-session", target: defaults.target }), /cleanupPending/);
    assert.equal(m.snapshot().lifecycleState, "closing");
    assert.equal(m.snapshot().activeContacts, 1);
    assert.throws(() => m.open(request({ commandId: "open-2" })), /BUSY/);
    m.cleanupSucceeds = true;
    m.close({ streamId: 1, sessionId: "auth-session", target: defaults.target });
    assert.equal(m.snapshot().lifecycleState, "closed");
    assert.equal(m.open(request({ commandId: "open-2" })).streamId, 2);
});
test("unsupported readiness, stale identity, allowed type and data budgets are fenced", () => {
    for (const extra of [{ sourceBindingId: "stale" }, { sessionId: "foreign" }, { format: "touch-batch-v1" }, { inputTypes: [] }])
        assert.throws(() => ctx().open(request(extra)));
    for (const facts of [{ nativeReady: false }, { geometryReady: false }])
        assert.throws(() => new InputContext(wire, { ...defaults, ...facts }).open(request()));
    const m = ctx();
    m.open(request({ inputTypes: ["touch"], mouse: undefined }));
    assert.throws(() => m.accept(encodeFrame({ streamId: 1, seqId: 1, cursor: 1000, inputType: "mouse", eventKind: 3 }, wire)), /type/i);
    const limited = ctx();
    limited.open(request({ inputTypes: ["touch"], mouse: undefined, touch: { maxContacts: 1 }, maxDataBytes: 8 }));
    assert.throws(() => limited.accept(encodeFrame(event(1, [record(1), record(2)]), wire)), /limit|budget/i);
});
test("late prepare completion cannot activate a fenced context", () => {
    const m = ctx();
    const token = m.beginOpen(request());
    assert.equal(m.snapshot().lifecycleState, "opening");
    m.invalidate();
    assert.throws(() => m.completeOpen(token), /stale.*prepare/i);
    assert.equal(m.snapshot().lifecycleState, "closed");
    assert.equal(m.open(request({ commandId: "fresh" })).streamId, 1);
});
