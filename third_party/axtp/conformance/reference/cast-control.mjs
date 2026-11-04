// Executable protocol oracle with substitute mutation/cleanup owners.
// This models observable guarantees only; it is not a media runtime.
import assert from "node:assert/strict";

const clone = (value) => structuredClone(value);
const MAX64 = 18446744073709551615n;
const uuid = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
const decimals = new Set(["generation", "bindingGeneration", "lifecycleGeneration", "revision", "eventSequence", "batchId", "appliedRevision", "settingsRevision", "expectedSettingsRevision", "settingsEventSequence", "storeRevision", "appliedControlRevision"]);
const sections = ["controlSync", "receivers", "sessions", "presentation", "audioPolicy", "window"];
const features = ["cast.presentation", "cast.audioPolicy", "cast.controlOperations"];
const methods = ["cast.getControlCapabilities", "cast.stopPresentation", "cast.setSessionAudioEnabled", "cast.getControlOperation", "cast.getStatus"];
const eventSections = {
  "cast.receiversChanged": "receivers", "cast.sessionsChanged": "sessions",
  "cast.presentationChanged": "presentation", "cast.audioPolicyChanged": "audioPolicy",
  "cast.controlOperationChanged": "operation", "cast.windowChanged": "window"
};
const eventSchemas = {
  "cast.receiversChanged": "CastReceiversChangedEvent", "cast.sessionsChanged": "CastSessionsChangedEvent",
  "cast.presentationChanged": "CastPresentationChangedEvent", "cast.audioPolicyChanged": "CastAudioPolicyChangedEvent",
  "cast.controlOperationChanged": "CastControlOperationChangedEvent", "cast.windowChanged": "CastWindowChangedEvent"
};
const canonical = (value) => JSON.stringify(value, (_key, item) => item && !Array.isArray(item) && typeof item === "object"
  ? Object.fromEntries(Object.entries(item).sort(([a], [b]) => a.localeCompare(b))) : item);
const same = (a, b) => canonical(a) === canonical(b);
const refKey = (ref) => `${ref.id}/${ref.generation}`;
const check = (condition, message) => { if (!condition) throw new Error(message); };
const tick = (value) => { check(BigInt(value) < MAX64, "counter exhausted"); return String(BigInt(value) + 1n); };
function knownValue(types, name, value) {
  return Object.fromEntries(types[name].fields.filter((f) => value[f.name] !== undefined).map((f) =>
    [f.name, types[f.type] ? knownValue(types, f.type, value[f.name]) : value[f.name]]));
}

export function decimal(value) {
  check(typeof value === "string" && /^(0|[1-9][0-9]{0,19})$/.test(value), "decimal syntax");
  check(BigInt(value) <= MAX64, "decimal overflow");
  return BigInt(value);
}

export function validateValue(types, name, value) {
  const schema = types[name];
  check(schema && value && typeof value === "object" && !Array.isArray(value), `object ${name}`);
  for (const field of schema.fields) {
    const item = value[field.name];
    if (item === undefined) { check(!field.required, `missing ${name}.${field.name}`); continue; }
    check(item !== null, `null ${field.name}`);
    if (types[field.type]) validateValue(types, field.type, item);
    else if (field.type === "array") {
      check(Array.isArray(item), `array ${field.name}`);
      for (const child of item) {
        if (types[field.array.item_type]) validateValue(types, field.array.item_type, child);
        else if (field.array.item_type === "string") check(typeof child === "string", "string array item");
        else check(Number.isInteger(child) && child >= 0 && child <= 0xffffffff, "uint32 array item");
      }
    } else if (field.type === "string" || field.type === "enum") {
      check(typeof item === "string", `string ${field.name}`);
      if (field.max_length !== undefined) check(Buffer.byteLength(item) <= field.max_length, `length ${field.name}`);
      if (Array.isArray(field.enum)) check(field.enum.includes(item), `enum ${field.name}`);
      if (decimals.has(field.name)) decimal(item);
      if (/InstanceId$/.test(field.name) || field.name === "commandId") check(uuid.test(item), `UUID ${field.name}`);
      if (["id", "receiverId", "operationId"].includes(field.name)) check(item.length > 0, `empty ${field.name}`);
      if (/At$/.test(field.name)) check(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?Z$/.test(item) && Number.isFinite(Date.parse(item)), `UTC ${field.name}`);
    } else if (field.type === "bool") check(typeof item === "boolean", `bool ${field.name}`);
    else if (field.type === "uint32") check(Number.isInteger(item) && item >= 0 && item <= 0xffffffff, `uint32 ${field.name}`);
    else throw new Error(`unmodeled field type ${field.type}`);
    if (field.min !== undefined) check(item >= field.min, `min ${field.name}`);
    if (field.max !== undefined) check(item <= field.max, `max ${field.name}`);
  }
  return value;
}

export function validateProjection(types, value) {
  validateValue(types, "CastStatus", value);
  check(sections.every((name) => value[name] !== undefined), "incomplete unifiedControl");
  check(value.controlSync.controlVersion === 1, "control version");
  const receivers = new Map(value.receivers.map((r) => [r.receiverId, r]));
  check(receivers.size === value.receivers.length, "duplicate receiver");
  const sessions = new Set(value.sessions.map((s) => refKey(s.session)));
  check(sessions.size === value.sessions.length, "duplicate session");
  for (const session of value.sessions) {
    check(receivers.has(session.receiverId) && receivers.get(session.receiverId).protocol === session.protocol, "receiver reference");
  }
  const p = value.presentation;
  check(p.validationAvailable === (p.validatedCapacity !== undefined), "capacity availability");
  const slots = new Set();
  for (const slot of p.slots) {
    check(!slots.has(slot.slotId), "duplicate slot"); slots.add(slot.slotId);
    check(slot.occupied === (slot.target !== undefined), "slot occupancy");
    if (slot.occupied) {
      check(slot.target.slotId === slot.slotId && slot.target.bindingGeneration === slot.bindingGeneration && slot.target.lifecycleGeneration === p.lifecycleGeneration, "slot fence");
      check(sessions.has(refKey(slot.target.session)), "ghost slot");
    }
  }
  for (const key of ["enabledSessions", "effectiveAudioSessions"]) {
    const refs = value.audioPolicy[key].map(refKey);
    check(new Set(refs).size === refs.length && refs.every((ref) => sessions.has(ref)), "audio reference");
  }
  return value;
}

export function decodeSessionRef(types, hex) {
  return decodeScalarObject(types, "CastSessionRef", hex);
}

export function decodeScalarObject(types, schemaName, hex) {
  check(typeof hex === "string" && /^(?:[0-9a-fA-F]{2})*$/.test(hex), "TLV hex");
  const bytes = Buffer.from(hex, "hex"), decoded = {};
  let offset = 0;
  while (offset < bytes.length) {
    check(offset + 2 <= bytes.length, "TLV header");
    const id = bytes[offset++], size = bytes[offset++]; check(offset + size <= bytes.length, "TLV length");
    const field = types[schemaName].fields.find((f) => f.id === id);
    if (field) {
      check(decoded[field.name] === undefined, "duplicate TLV field");
      if (field.type === "uint32") { check(size === 4, "uint32 TLV width"); decoded[field.name] = bytes.readUInt32BE(offset); }
      else { check(field.type === "string", "scalar string or uint32"); decoded[field.name] = new TextDecoder("utf-8", { fatal: true }).decode(bytes.subarray(offset, offset + size)); }
    }
    offset += size;
  }
  return decoded;
}

export function validateOperation(types, op, codes) {
  validateValue(types, "CastControlOperation", op);
  const settings = op.kind === "setPresentationSettings";
  if (settings) {
    check(!Object.hasOwn(op, "target") && !Object.hasOwn(op, "ownerCleanup"), "global settings target");
    check(op.settingsResult, "settings result required");
    check(["layoutMode", "audioControlMode", "audioMixMode", "capacity", "enabledProtocols", "profileHint"].includes(op.settingsResult.field), "settings result field");
    if (op.state !== "queued") check(op.settingsResult.persistenceStatus !== "pending", "terminal persistence pending");
    if (op.state === "applied") check(op.settingsResult.configurationStatus !== "notCommitted" && !["notAttempted", "failed"].includes(op.settingsResult.applicationStatus), "applied settings stages");
  } else check(op.target, "operation target");
  if (op.state !== "queued") check(op.completedAt && op.statusCode !== undefined && op.outcomeReason, "terminal fields");
  if (op.state === "applied") check((settings || op.appliedRevision !== undefined) && op.statusCode === codes.SUCCESS, "applied fields");
  if (["rejected", "failed"].includes(op.state)) check(Object.values(codes).includes(op.statusCode) && op.statusCode !== codes.SUCCESS, "failure status");
  if (op.state === "rejected") check(op.appliedRevision === undefined, "rejected commit");
}

function discover(types, descriptor, declaredFeatures) {
  validateValue(types, "CastControlCapabilities", descriptor);
  check(features.every((name) => declaredFeatures.includes(name)), "missing feature");
  check(descriptor.supportedControlVersions.includes(1), "version absent");
  check(methods.every((name) => descriptor.methods.includes(name)), "missing method");
  check(Object.keys(eventSections).every((name) => descriptor.events.includes(name)), "missing event");
  check(sections.every((name) => descriptor.statusSections.includes(name)), "missing section");
  check(descriptor.presentation?.supportsAtomicSnapshot && descriptor.presentation.supportsStrictTargets, "presentation guarantee");
  check(descriptor.audioPolicy?.supportsPerSessionEnable && descriptor.audioPolicy.reportsEffectiveSet, "audio guarantee");
  check(descriptor.operations?.supportsQuery && descriptor.operations.supportsDedup, "operation guarantee");
  const p = descriptor.presentation;
  check(p.protocols.length > 0 && p.protocols.every((name) => ["hid", "airplay", "webcast"].includes(name)), "protocol declaration");
  check((p.capacityChoices ?? []).every((n) => n > 0 && n <= p.maxPresentationCapacity), "capacity choices");
  check(p.layoutModes.length > 0, "layout modes");
  return descriptor;
}

export class CastControlModel {
  constructor(fixture, types, codes) {
    this.fixture = clone(fixture); this.types = types; this.codes = codes;
    this.state = clone(fixture.snapshot); this.capabilities = clone(fixture.controlCapabilities);
    this.targets = fixture.snapshot.presentation.slots.map((s) => clone(s.target));
    this.records = new Map(); this.clock = 0; this.nextOperation = 1;
    this.clientOperations = new Map();
    this.cleanups = []; this.client = undefined; this.pendingBatch = []; this.seen = new Map();
    this.eventBytes = 0; this.beforeSnapshot = []; this.closed = false; this.needsSnapshot = false;
  }
  success(result) { return { status: "SUCCESS", ...result }; }
  key(p) { return [p.runtimeInstanceId, p.clientInstanceId, p.commandId].join("/"); }
  params(input, kind) {
    return { controlVersion: 1, runtimeInstanceId: this.fixture.snapshot.controlSync.runtimeInstanceId,
      clientInstanceId: this.fixture.clientInstanceId, commandId: this.fixture.commandId,
      ...(kind ? { target: clone(this.targets[input.targetIndex ?? 0]) } : {}), ...(input.params ?? {}) };
  }
  expire() {
    for (const [key, item] of this.records) if (item.operation.state !== "queued" && this.clock >= item.expires) this.records.delete(key);
  }
  admit(input) {
    const kind = input.method === "cast.setSessionAudioEnabled" ? "setSessionAudioEnabled" : "stopPresentation";
    check(["cast.stopPresentation", "cast.setSessionAudioEnabled"].includes(input.method), "unknown write method");
    const p = this.params(input, kind);
    validateValue(this.types, kind === "stopPresentation" ? "CastStopPresentationParams" : "CastSetSessionAudioEnabledParams", p);
    if (p.controlVersion !== 1 || input.supported === false) return { status: "NOT_SUPPORTED" };
    if (p.runtimeInstanceId !== this.state.controlSync.runtimeInstanceId || this.closed) return { status: "INVALID_STATE" };
    if (decimal(this.state.controlSync.revision) === MAX64) return { status: "UNAVAILABLE" };
    this.expire();
    const key = this.key(p);
    // Only known schema fields are part of intent equality; key ordering is irrelevant.
    const target = knownValue(this.types, "CastControlTarget", p.target);
    const intent = { kind, target, ...(kind === "stopPresentation" ? (p.reason === undefined ? {} : { reason: p.reason }) : { enabled: p.enabled }) };
    const old = this.records.get(key);
    if (old) return same(old.intent, intent) ? this.success({ operation: clone(old.operation) }) : { status: "INVALID_ARGUMENT", outcomeReason: "COMMAND_ID_CONFLICT" };
    const limits = { ...this.capabilities.operations.limits, ...(input.limits ?? {}) };
    const queued = [...this.records.values()].filter((item) => item.operation.state === "queued");
    if (queued.length >= limits.maxPendingOperations || queued.filter((item) => item.operation.clientInstanceId === p.clientInstanceId).length >= limits.maxPendingPerClient || this.records.size >= limits.maxRetainedResults || Buffer.byteLength(JSON.stringify(p)) > limits.maxRequestBytes) return { status: "BUSY" };
    const operation = { operationId: `operation-${this.nextOperation++}`, runtimeInstanceId: p.runtimeInstanceId,
      clientInstanceId: p.clientInstanceId, commandId: p.commandId, kind, state: "queued",
      submittedAt: this.fixture.snapshot.sampledAt, target: clone(target) };
    this.records.set(key, { operation, intent, expires: this.clock + limits.replayWindowMs });
    return this.success({ operation: clone(operation) });
  }
  execute(input) {
    const item = this.records.get(this.key(this.params(input)));
    if (!item) return { status: "NOT_FOUND" };
    const op = item.operation;
    if (op.state !== "queued") return this.success({ operation: clone(op) });
    const target = op.target, slot = this.state.presentation.slots.find((s) => s.slotId === target.slotId);
    const terminal = (state, status, reason, committed = false, cleanup) => {
      Object.assign(op, { state, completedAt: this.state.sampledAt, statusCode: this.codes[status], outcomeReason: reason });
      if (committed) op.appliedRevision = this.state.controlSync.revision = tick(this.state.controlSync.revision);
      if (cleanup) op.ownerCleanup = cleanup;
      return this.success({ operation: clone(op) });
    };
    if (this.closed || op.runtimeInstanceId !== this.state.controlSync.runtimeInstanceId || !slot || !slot.occupied || !same(slot.target, target) || target.lifecycleGeneration !== this.state.presentation.lifecycleGeneration || !this.state.sessions.some((s) => same(s.session, target.session))) return terminal("rejected", "INVALID_STATE", "STALE_TARGET");
    if (input.ownerUnavailable) return terminal("failed", "UNAVAILABLE", "OWNER_UNAVAILABLE");
    if (op.kind === "stopPresentation") {
      slot.occupied = false; delete slot.target;
      const session = this.state.sessions.find((s) => same(s.session, target.session));
      session.presentationEligibility = "notPresented";
      for (const key of ["enabledSessions", "effectiveAudioSessions"]) this.state.audioPolicy[key] = this.state.audioPolicy[key].filter((ref) => !same(ref, target.session));
      this.cleanups.push({ receiverId: session.receiverId, session: clone(target.session) });
      return input.partialFailure ? terminal("failed", "INTERNAL_ERROR", "PARTIAL_EXECUTION", true, "failed") : terminal("applied", "SUCCESS", "PRESENTATION_WITHDRAWN", true, "pending");
    }
    const audio = this.state.audioPolicy;
    audio.enabledSessions = audio.enabledSessions.filter((ref) => !same(ref, target.session));
    if (item.intent.enabled) audio.enabledSessions.push(clone(target.session));
    if (input.makeEffective) audio.effectiveAudioSessions = audio.mixMode === "exclusive" ? (item.intent.enabled ? [clone(target.session)] : []) : clone(audio.enabledSessions);
    return terminal("applied", "SUCCESS", "INTENT_APPLIED", true, "notApplicable");
  }
  install(snapshot) {
    validateProjection(this.types, snapshot);
    if (this.client?.controlSync.runtimeInstanceId !== snapshot.controlSync.runtimeInstanceId) this.clientOperations.clear();
    const before = this.beforeSnapshot; this.beforeSnapshot = [];
    this.client = clone(snapshot); this.pendingBatch = []; this.seen.clear(); this.eventBytes = 0; this.needsSnapshot = false;
    for (const event of before) {
      const context = event.data.context ?? event.data.controlContext;
      if (context.runtimeInstanceId === snapshot.controlSync.runtimeInstanceId && decimal(context.eventSequence) > decimal(snapshot.controlSync.eventSequence)) this.receive(event);
    }
    return { installed: true, eventSequence: snapshot.controlSync.eventSequence };
  }
  receive(event, limits = {}) {
    if (event.name === "cast.presentationSettingsChanged") return { ignored: true };
    const section = eventSections[event.name];
    const data = event.data;
    if (event.name === "cast.windowChanged" && !data.controlContext) return { ignored: true };
    check(section && data, "participating event");
    if (this.needsSnapshot) return { resync: true };
    validateValue(this.types, eventSchemas[event.name], data);
    check(event.name !== "cast.controlOperationChanged" || data.operation.kind !== "setPresentationSettings", "P1 operation in P0 stream");
    const context = event.name === "cast.windowChanged" ? data.controlContext : data.context;
    validateValue(this.types, "CastControlEventContext", context);
    check(context.controlVersion === 1, "required version changed");
    if (!this.client) {
      const bound = { ...this.capabilities.operations.limits, ...limits };
      const size = Buffer.byteLength(JSON.stringify(event));
      check(this.beforeSnapshot.length + 1 <= bound.maxEventBufferEvents && this.eventBytes + size <= bound.maxEventBufferBytes, "initial buffer overflow");
      this.beforeSnapshot.push(clone(event)); this.eventBytes += size;
      return { applied: false, buffered: this.beforeSnapshot.length };
    }
    check(context.runtimeInstanceId === this.client.controlSync.runtimeInstanceId, "epoch changed");
    const seq = decimal(context.eventSequence), barrier = decimal(this.client.controlSync.eventSequence);
    if (this.seen.has(context.eventSequence)) { check(same(this.seen.get(context.eventSequence), event), "conflicting duplicate"); return { ignored: true }; }
    if (seq <= barrier) return { ignored: true };
    check(context.partIndex < context.partCount && decimal(context.batchId) + BigInt(context.partIndex) === seq, "batch arithmetic");
    check(seq === barrier + BigInt(this.pendingBatch.length) + 1n, "sequence gap or reorder");
    const first = this.pendingBatch[0]?.data;
    const firstContext = first?.context ?? first?.controlContext;
    if (firstContext) check(["revision", "batchId", "partCount"].every((key) => context[key] === firstContext[key]) && context.partIndex === this.pendingBatch.length, "mixed batch");
    else check(context.partIndex === 0 && decimal(context.revision) >= decimal(this.client.controlSync.revision), "batch start/revision");
    check(!this.pendingBatch.some((part) => eventSections[part.name] === section), "duplicate section");
    const size = Buffer.byteLength(JSON.stringify(event));
    const bound = { ...this.capabilities.operations.limits, ...limits };
    check(this.pendingBatch.length + 1 <= bound.maxEventBufferEvents && this.eventBytes + size <= bound.maxEventBufferBytes, "buffer overflow");
    this.pendingBatch.push(clone(event)); this.eventBytes += size; this.seen.set(context.eventSequence, clone(event));
    if (this.pendingBatch.length < context.partCount) return { applied: false, buffered: this.pendingBatch.length };
    const next = clone(this.client);
    for (const part of this.pendingBatch) {
      const key = eventSections[part.name];
      if (key === "operation") validateOperation(this.types, part.data.operation, this.codes);
      else next[key] = clone(key === "window" ? part.data.state : part.data[key]);
    }
    next.controlSync = Object.fromEntries(["controlVersion", "runtimeInstanceId", "revision", "eventSequence"].map((key) => [key, context[key]]));
    validateProjection(this.types, next);
    for (const part of this.pendingBatch) if (part.name === "cast.controlOperationChanged") {
      const op = part.data.operation;
      this.clientOperations.set(this.key(op), clone(op));
      while (this.clientOperations.size > bound.maxRetainedResults) this.clientOperations.delete(this.clientOperations.keys().next().value);
    }
    this.client = next; this.pendingBatch = []; this.eventBytes = 0;
    // Duplicate detection memory is bounded to the most recently installed batch.
    const min = decimal(context.batchId);
    for (const key of this.seen.keys()) if (decimal(key) < min) this.seen.delete(key);
    return { applied: true, eventSequence: context.eventSequence };
  }
  action(action, input) {
    try {
      switch (action) {
        case "discover": {
          const d = { ...clone(this.capabilities), ...(input.descriptor ?? {}) };
          for (const name of input.omit ?? []) delete d[name];
          try { discover(this.types, d, input.features ?? features); return { supported: true }; }
          catch { return { supported: false }; }
        }
        case "capture": {
          if (input.controlVersion !== undefined && input.controlVersion !== 1) return { status: "NOT_SUPPORTED" };
          if (input.unavailable || Buffer.byteLength(JSON.stringify(this.state)) > (input.maxSnapshotBytes ?? this.capabilities.operations.limits.maxSnapshotBytes)) return { status: "UNAVAILABLE" };
          return this.success({ result: clone(this.state) });
        }
        case "install_snapshot": {
          const snapshot = { ...clone(this.state), ...(input.patch ?? {}) };
          for (const name of input.omit ?? []) delete snapshot[name];
          return this.install(snapshot);
        }
        case "receive_event": return this.receive(input.event, input.limits);
        case "flush_events": check(this.pendingBatch.length === 0 && this.beforeSnapshot.length === 0, "incomplete group"); return { complete: true };
        case "admit": return this.admit(input);
        case "execute": return this.execute(input);
        case "query": {
          const p = this.params(input); validateValue(this.types, "CastGetControlOperationParams", p);
          if (p.controlVersion !== 1) return { status: "NOT_SUPPORTED" };
          if (p.runtimeInstanceId !== this.state.controlSync.runtimeInstanceId) return { status: "INVALID_STATE" };
          this.expire(); const item = this.records.get(this.key(p));
          return item ? this.success({ operation: clone(item.operation) }) : { status: "NOT_FOUND", outcome: "unknown" };
        }
        case "advance_clock": this.clock += input.ms; this.expire(); return { retained: this.records.size };
        case "rebind": {
          const slot = this.state.presentation.slots[input.slotIndex ?? 0];
          slot.bindingGeneration = tick(slot.bindingGeneration); slot.target.bindingGeneration = slot.bindingGeneration;
          return { bindingGeneration: slot.bindingGeneration };
        }
        case "shutdown": this.closed = true; return { accepting: false };
        case "restart": this.state.controlSync.runtimeInstanceId = input.runtimeInstanceId; this.records.clear(); this.clientOperations.clear(); this.client = undefined; this.pendingBatch = []; this.beforeSnapshot = []; this.seen.clear(); this.eventBytes = 0; this.closed = false; return { retained: 0, synchronized: false };
        case "validate_value": {
          validateValue(this.types, input.schema, input.value);
          if (input.schema === "CastControlOperation") validateOperation(this.types, input.value, this.codes);
          return { valid: true };
        }
        case "encode_pair": {
          const value = input.value, decoded = decodeSessionRef(this.types, input.tlv8Hex);
          validateValue(this.types, "CastSessionRef", value);
          validateValue(this.types, "CastSessionRef", decoded); check(same(JSON.parse(JSON.stringify(value)), decoded), "paired encoding mismatch");
          return { valid: true, decoded };
        }
        case "assert_state": return { state: clone(this.state), client: clone(this.client), operations: clone([...this.clientOperations.values()]), cleanups: clone(this.cleanups), retained: this.records.size, needsSnapshot: this.needsSnapshot };
        case "validate_public": {
          const text = JSON.stringify(input.value);
          check(!(input.secrets ?? []).some((secret) => text.includes(secret)), "secret value");
          const visit = (value) => {
            if (!value || typeof value !== "object") return;
            for (const [key, child] of Object.entries(value)) {
              check(!/^(password|passwordHash|pinCode|pinHash|token|sdp|ice|privateKey)$/i.test(key), "secret field"); visit(child);
            }
          };
          visit(input.value); return { safe: true };
        }
        case "legacy_projection": return { result: clone(this.fixture.legacy), sequenceParticipates: false };
        default: throw new Error(`unknown model action ${action}`);
      }
    } catch (error) {
      if (["receive_event", "flush_events", "install_snapshot"].includes(action)) { this.needsSnapshot = true; this.pendingBatch = []; this.beforeSnapshot = []; this.seen.clear(); this.eventBytes = 0; return { resync: true }; }
      return { status: "INVALID_ARGUMENT", valid: false };
    }
  }
}

export function assertSubset(actual, expected) {
  if (expected && typeof expected === "object" && !Array.isArray(expected)) {
    for (const [key, value] of Object.entries(expected)) {
      assert.ok(actual && Object.hasOwn(actual, key), `missing result key ${key}`);
      assertSubset(actual[key], value);
    }
  } else assert.deepEqual(actual, expected);
}

export function runControlCase(testCase, fixture, types, codes, Model = CastControlModel) {
  let count = 0;
  for (const scenario of testCase.scenarios ?? [{ steps: testCase.steps }]) {
    const model = new Model(fixture, types, codes);
    for (const step of scenario.steps) {
      const command = step.control_model;
      check(command, "reference cases require executable control_model steps");
      const output = model.action(command.action, command.input);
      try { assertSubset(output, command.expect); }
      catch (error) { throw new Error(`${testCase.id}/${scenario.name ?? "main"}/${step.id}: ${error.message}`); }
      count++;
    }
  }
  return count;
}
