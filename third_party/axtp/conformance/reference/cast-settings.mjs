// Optional P1 protocol oracle. Store, policy owner, crash and scheduling are substitutes.
// No filesystem writes, media pipeline implementation or product restart API live here.
import { CastControlModel, decimal, validateValue, validateOperation } from "./cast-control.mjs";

const copy = (v) => structuredClone(v);
export const settingFields = ["layoutMode", "contentAdaptation", "presentationMode", "presentationSidebar", "presentationAudioFollow", "presentationFocusOnDoubleClick", "presentationGridOnDoubleClick", "presentationAutoEnter", "audioControlMode", "audioMixMode", "capacity", "enabledProtocols", "profileHint"];
const canonical = (v) => JSON.stringify(v, (key, value) => key === "enabledProtocols" && Array.isArray(value) ? [...value].sort()
  : value && typeof value === "object" && !Array.isArray(value) ? Object.fromEntries(Object.entries(value).sort(([a], [b]) => a.localeCompare(b))) : value);
const equal = (a, b) => canonical(a) === canonical(b);
const sameSetting = (field, a, b) => field === "enabledProtocols" && Array.isArray(a) && Array.isArray(b)
  ? equal([...a].sort(), [...b].sort()) : equal(a, b);
const check = (ok, reason) => { if (!ok) throw new Error(reason); };
const unique = (v) => Array.isArray(v) && new Set(v).size === v.length;
const next = (v) => { check(decimal(v) < 18446744073709551615n, "counter exhaustion"); return String(decimal(v) + 1n); };
const reject = (status, reason) => { const error = new Error(reason); error.status = status; throw error; };
const knownValues = (cap, values) => Object.fromEntries(cap.fields.filter((f) => Object.hasOwn(values, f)).map((f) => [f, copy(values[f])]));

export function validateSettingsCapability(types, descriptor) {
  validateValue(types, "CastControlCapabilities", descriptor);
  const cap = descriptor.presentationSettings;
  validateValue(types, "CastPresentationSettingsCapability", cap);
  for (const key of ["fields", "liveFields", "restartOnlyFields"]) check(unique(cap[key]), `unique ${key}`);
  check(cap.fields.length > 0 && cap.fields.every((f) => settingFields.includes(f)), "supported field set");
  const partition = [...cap.liveFields, ...cap.restartOnlyFields];
  check(unique(partition) && equal([...partition].sort(), [...cap.fields].sort()), "effect partition");
  const requireChoices = (field, key, predicate) => {
    if (cap.fields.includes(field)) check(unique(cap[key]) && cap[key].length > 0 && cap[key].every(predicate), `conditional ${key}`);
  };
  const identifier = (v) => typeof v === "string" && v.length > 0 && Buffer.byteLength(v) <= 64;
  requireChoices("capacity", "capacityChoices", (v) => Number.isInteger(v) && v > 0 && v <= 0xffffffff);
  requireChoices("layoutMode", "layoutModes", identifier); requireChoices("contentAdaptation", "contentAdaptations", identifier); requireChoices("profileHint", "profileHints", identifier);
  if (cap.fields.includes("enabledProtocols")) check(Number.isInteger(cap.minEnabledProtocols) && cap.minEnabledProtocols >= 0 && cap.minEnabledProtocols <= descriptor.presentation.protocols.length, "conditional protocol minimum");
  if (cap.fields.includes("audioControlMode")) check(descriptor.audioPolicy.controlModes.length > 0 && descriptor.audioPolicy.controlModes.every((m) => ["automatic", "manual"].includes(m)), "control modes");
  if (cap.fields.includes("audioMixMode")) check(descriptor.audioPolicy.mixModes.length > 0 && descriptor.audioPolicy.mixModes.every((m) => ["exclusive", "mix"].includes(m)), "mix modes");
  check(["cast.setPresentationSettings", "cast.getControlCapabilities", "cast.getControlOperation", "cast.getStatus"].every((name) => descriptor.methods.includes(name)) && descriptor.events.includes("cast.presentationSettingsChanged") && descriptor.statusSections.includes("presentationSettings") && descriptor.operations?.supportsQuery && descriptor.operations.supportsDedup, "complete P1 binding");
  return cap;
}

function derived(cap, settings) {
  const differing = (keys, values) => keys.filter((f) => !Object.hasOwn(values, f) || !sameSetting(f, settings.configured[f], values[f]));
  const unsavedFields = settings.persistedValues ? differing(cap.fields, settings.persistedValues) : [...cap.fields];
  return { pendingRestart: differing(cap.restartOnlyFields, settings.activeStartup),
    unsavedFields, unappliedFields: differing(cap.liveFields, settings.applied),
    persisted: !!settings.persistedValues && unsavedFields.length === 0 };
}

export function validateSettingsState(types, descriptor, settings) {
  const cap = validateSettingsCapability(types, descriptor);
  validateValue(types, "CastPresentationSettingsState", settings);
  const safe = (v) => { if (v && typeof v === "object") for (const [key, child] of Object.entries(v)) {
    check(!/^(password|passwordHash|pinCode|pinHash|token|sdp|ice|privateKey|privateKeyPath|sessionKey|slotId|bindingGeneration|lifecycleGeneration|enabledSessions|effectiveAudioSessions)$/i.test(key), "forbidden settings material"); safe(child);
  } }; safe(settings);
  const fieldsOnly = (values, complete = false, keys = cap.fields) => {
    check(Object.keys(values).every((f) => keys.includes(f)), "unsupported value field");
    if (complete) check(keys.every((f) => Object.hasOwn(values, f)), "incomplete value projection");
  };
  fieldsOnly(settings.configured, true); fieldsOnly(settings.applied);
  fieldsOnly(settings.activeStartup, true, cap.restartOnlyFields);
  if (settings.persistedValues) fieldsOnly(settings.persistedValues, true);
  for (const key of ["pendingRestart", "unsavedFields", "unappliedFields", "lockedFields"]) check(unique(settings[key]) && settings[key].every((f) => cap.fields.includes(f)), "field name set");
  for (const issue of settings.applicationIssues) check(cap.fields.includes(issue.field), "issue field");
  for (const f of cap.fields) if (!Object.hasOwn(settings.applied, f)) check(settings.applicationIssues.some((i) => i.field === f), "unconfirmed policy issue required");
  const expected = derived(cap, settings);
  for (const key of ["pendingRestart", "unsavedFields", "unappliedFields"]) check(equal([...settings[key]].sort(), [...expected[key]].sort()), `derived ${key}`);
  check(settings.persisted === expected.persisted, "persisted equality");
  if (["invalid", "unavailable"].includes(settings.storageState)) check(!Object.hasOwn(settings, "persistedValues") && !Object.hasOwn(settings, "storeRevision"), "unknown storage projection");
  if (["loaded", "saved"].includes(settings.storageState)) check(settings.persistedValues && settings.storeRevision !== undefined, "known store metadata");
  if (settings.storageState === "absent") check(settings.storeRevision === undefined, "absent store revision");
  for (const values of [settings.configured, settings.applied, settings.activeStartup, settings.persistedValues].filter(Boolean)) {
    if (values.enabledProtocols !== undefined) check(unique(values.enabledProtocols) && values.enabledProtocols.length >= cap.minEnabledProtocols && values.enabledProtocols.every((p) => descriptor.presentation.protocols.includes(p)), "protocol set");
    for (const [field, choices] of [["layoutMode", cap.layoutModes], ["profileHint", cap.profileHints], ["audioControlMode", descriptor.audioPolicy.controlModes], ["audioMixMode", descriptor.audioPolicy.mixModes]]) {
      if (values[field] !== undefined) check(choices?.includes(values[field]), "undeclared state mode");
    }
    if (values !== settings.applied && values.capacity !== undefined) check(cap.capacityChoices.includes(values.capacity), "undeclared capacity intent");
  }
  return settings;
}

export class CastSettingsModel extends CastControlModel {
  constructor(fixture, types, codes) {
    super(fixture, types, codes);
    this.settings = copy(fixture.settings); this.store = copy(fixture.settingsStore);
    this.knownStore = { revision: this.store.revision, overrides: copy(this.store.overrides) };
    this.defaults = copy(fixture.settingsDefaults); this.pendingWrite = undefined;
    this.policyCalls = {}; this.controlEvents = []; this.lastSettingsEvent = undefined;
    this.clientSettings = undefined; this.settingsBuffer = []; this.settingsSeen = new Map();
    this.settingsNeedsSnapshot = false; this.unpublishedSettings = false;
    validateSettingsState(types, this.capabilities, this.settings);
  }
  get settingsCap() { return this.capabilities.presentationSettings; }
  settingsParams(input) {
    return { ...this.params(input), expectedSettingsRevision: this.fixture.settings.settingsRevision, ...(input.params ?? {}) };
  }
  recompute() { Object.assign(this.settings, derived(this.settingsCap, this.settings)); }
  factView() { const { settingsRevision, settingsEventSequence, ...facts } = this.settings; return facts; }
  commit(before) {
    this.recompute();
    if (!equal(before, this.factView())) { this.settings.settingsRevision = next(this.settings.settingsRevision); this.unpublishedSettings = true; }
  }
  patchCheck(p, constraints = true) {
    validateValue(this.types, "CastSetPresentationSettingsParams", { ...p, patch: {} });
    check(p.patch && typeof p.patch === "object" && !Array.isArray(p.patch), "patch object");
    const keys = Object.keys(p.patch); check(keys.length === 1 && settingFields.includes(keys[0]), "exactly one known field");
    const field = keys[0], value = p.patch[field];
    if (field === "capacity") check(Number.isInteger(value) && value >= 0 && value <= 0xffffffff, "uint32 capacity");
    else if (field === "enabledProtocols") check(unique(value) && value.every((v) => typeof v === "string"), "duplicate or malformed protocols");
    else check(typeof value === "string" && value.length > 0 && Buffer.byteLength(value) <= 64, "setting string");
    if (!constraints) return field;
    if (!this.settingsCap.fields.includes(field) || (p.persist && !this.settingsCap.supportsPersistence)) reject("NOT_SUPPORTED", "SETTINGS_UNSUPPORTED");
    const choices = { layoutMode: this.settingsCap.layoutModes, profileHint: this.settingsCap.profileHints,
      audioControlMode: this.capabilities.audioPolicy.controlModes, audioMixMode: this.capabilities.audioPolicy.mixModes };
    if (choices[field] && !choices[field].includes(value)) reject("NOT_SUPPORTED", "SETTINGS_UNSUPPORTED_MODE");
    if (field === "capacity" && !this.settingsCap.capacityChoices.includes(value)) reject("OUT_OF_RANGE", "SETTINGS_OUT_OF_RANGE");
    if (field === "enabledProtocols") {
      if (value.some((v) => !this.capabilities.presentation.protocols.includes(v))) reject("NOT_SUPPORTED", "SETTINGS_UNSUPPORTED_PROTOCOL");
      if (value.length < this.settingsCap.minEnabledProtocols) reject("OUT_OF_RANGE", "SETTINGS_OUT_OF_RANGE");
    }
    return field;
  }
  admitSettings(input) {
    const p = this.settingsParams(input), field = this.patchCheck(p, false);
    if (p.controlVersion !== 1 || input.supported === false || !this.settingsCap) return { status: "NOT_SUPPORTED" };
    if (this.closed || p.runtimeInstanceId !== this.state.controlSync.runtimeInstanceId) return { status: "INVALID_STATE" };
    if (input.authorized === false) return { status: "INVALID_STATE", outcomeReason: "SETTINGS_LOCKED" };
    this.expire(); const key = this.key(p), intent = { kind: "setPresentationSettings", expectedSettingsRevision: p.expectedSettingsRevision, patch: p.patch, persist: p.persist };
    const old = this.records.get(key);
    if (old) return equal(old.intent, intent) ? this.success({ operation: copy(old.operation) }) : { status: "INVALID_ARGUMENT", outcomeReason: "COMMAND_ID_CONFLICT" };
    this.patchCheck(p);
    if (this.pendingWrite || input.writerBusy) return { status: "BUSY" };
    const limits = { ...this.capabilities.operations.limits, ...(input.limits ?? {}) };
    const queued = [...this.records.values()].filter((r) => r.operation.state === "queued");
    if (queued.length >= limits.maxPendingOperations || queued.filter((r) => r.operation.clientInstanceId === p.clientInstanceId).length >= limits.maxPendingPerClient || this.records.size >= limits.maxRetainedResults || Buffer.byteLength(JSON.stringify(p)) > limits.maxRequestBytes) return { status: "BUSY" };
    if (decimal(this.settings.settingsRevision) === 18446744073709551615n) return { status: "UNAVAILABLE" };
    if (p.persist && !sameSetting(field, this.store.overrides[field], p.patch[field]) && decimal(this.store.revision) === 18446744073709551615n) return { status: "UNAVAILABLE" };
    const operation = { operationId: `settings-operation-${this.nextOperation++}`, runtimeInstanceId: p.runtimeInstanceId, clientInstanceId: p.clientInstanceId,
      commandId: p.commandId, kind: "setPresentationSettings", state: "queued", submittedAt: this.state.sampledAt,
      settingsResult: { field, settingsRevision: this.settings.settingsRevision, configurationStatus: "notCommitted", applicationStatus: "notAttempted", persistenceStatus: p.persist ? "pending" : "notRequested" } };
    this.records.set(key, { operation, intent, params: copy(p), expires: this.clock + limits.replayWindowMs });
    return this.success({ operation: copy(operation) });
  }
  finish(item, state, status, reason, result) {
    Object.assign(item.operation, { state, completedAt: this.state.sampledAt, statusCode: this.codes[status], outcomeReason: reason });
    Object.assign(item.operation.settingsResult, result, { settingsRevision: this.settings.settingsRevision });
    if (this.settings.appliedControlRevision !== undefined) item.operation.settingsResult.appliedControlRevision = this.settings.appliedControlRevision;
    validateOperation(this.types, item.operation, this.codes);
    return this.success({ operation: copy(item.operation) });
  }
  issue(field, kind) {
    this.settings.applicationIssues = this.settings.applicationIssues.filter((i) => !(i.field === field && i.kind === kind));
    this.settings.applicationIssues.push({ field, kind });
  }
  reflectPolicy(field, value) {
    this.settings.applied[field] = copy(value);
    let section;
    if (field === "layoutMode") { this.state.presentation.layoutMode = value; section = "presentation"; }
    else if (field === "audioControlMode") {
      this.state.audioPolicy.controlMode = value;
      this.state.audioPolicy.enabledSessions = value === "manual" ? [] : this.state.sessions.slice(0, 1).map((s) => copy(s.session));
      this.state.audioPolicy.effectiveAudioSessions = []; section = "audioPolicy";
    } else if (field === "audioMixMode") {
      this.state.audioPolicy.mixMode = value;
      if (value === "exclusive") this.state.audioPolicy.enabledSessions = this.state.audioPolicy.enabledSessions.slice(-1);
      this.state.audioPolicy.effectiveAudioSessions = []; section = "audioPolicy";
    } else if (field === "capacity") {
      this.state.presentation.configuredCapacity = this.settings.configured.capacity;
      this.state.presentation.appliedCapacity = value; this.state.presentation.effectiveCapacity = value; section = "presentation";
    }
    if (!section) return false;
    const revision = this.state.controlSync.revision = next(this.state.controlSync.revision);
    const eventSequence = this.state.controlSync.eventSequence = next(this.state.controlSync.eventSequence);
    this.settings.appliedControlRevision = revision;
    this.controlEvents.push({ name: `cast.${section === "audioPolicy" ? "audioPolicyChanged" : "presentationChanged"}`, data: {
      context: { ...copy(this.state.controlSync), batchId: eventSequence, partIndex: 0, partCount: 1 }, [section]: copy(this.state[section]) } });
    return true;
  }
  applyConfiguration(item, input, persistenceStatus) {
    const before = input.beforeStoreFacts ?? copy(this.factView()), p = item.params, field = item.operation.settingsResult.field, value = p.patch[field];
    const configurationStatus = sameSetting(field, this.settings.configured[field], value) ? "unchanged" : "committed";
    this.settings.configured[field] = copy(value);
    let applicationStatus, committedControl = false;
    if (this.settingsCap.restartOnlyFields.includes(field)) applicationStatus = sameSetting(field, value, this.settings.activeStartup[field]) ? "unchanged" : "pendingRestart";
    else if (sameSetting(field, this.settings.applied[field], value)) applicationStatus = "unchanged";
    else {
      this.policyCalls[field] = (this.policyCalls[field] ?? 0) + 1;
      if (input.applyFailure) {
        if (Object.hasOwn(input, "observedPolicy")) committedControl = this.reflectPolicy(field, input.observedPolicy);
        this.issue(field, "liveApplyFailed"); applicationStatus = "failed";
      } else {
        committedControl = this.reflectPolicy(field, value);
        this.settings.applicationIssues = this.settings.applicationIssues.filter((i) => !(i.field === field && i.kind === "liveApplyFailed"));
        applicationStatus = "applied";
      }
    }
    this.commit(before);
    if (committedControl) item.operation.appliedRevision = this.state.controlSync.revision;
    return this.finish(item, applicationStatus === "failed" ? "failed" : "applied", applicationStatus === "failed" ? "UNAVAILABLE" : "SUCCESS",
      applicationStatus === "failed" ? "SETTINGS_LIVE_APPLY_FAILED" : applicationStatus === "pendingRestart" ? "SETTINGS_PENDING_RESTART" : persistenceStatus === "stored" ? "SETTINGS_SAVED" : "SETTINGS_ACCEPTED",
      { configurationStatus, applicationStatus, persistenceStatus });
  }
  refreshStore() {
    const before = copy(this.factView());
    if (this.store.schemaVersion !== 1 || ["invalid", "unavailable"].includes(this.store.state)) {
      this.settings.storageState = this.store.state === "unavailable" ? "unavailable" : "invalid";
      delete this.settings.persistedValues; delete this.settings.storeRevision;
    } else {
      this.settings.storageState = this.store.state;
      this.settings.persistedValues = knownValues(this.settingsCap, { ...this.defaults, ...this.store.overrides });
      if (this.store.state === "absent") delete this.settings.storeRevision;
      else this.settings.storeRevision = this.store.revision;
      this.knownStore = { revision: this.store.revision, overrides: copy(this.store.overrides) };
    }
    this.commit(before);
  }
  completeStore(input) {
    const pending = this.pendingWrite;
    if (!pending || pending.epoch !== this.state.controlSync.runtimeInstanceId) return { status: "INVALID_STATE" };
    const item = pending.item, field = item.operation.settingsResult.field;
    this.pendingWrite = undefined;
    if (this.store.schemaVersion !== 1 || ["invalid", "unavailable"].includes(this.store.state) || input.storeFailure) {
      const before = copy(this.factView()); this.settings.persistError = "SETTINGS_PERSIST_FAILED";
      if (this.store.schemaVersion !== 1 || ["invalid", "unavailable"].includes(this.store.state)) {
        this.settings.storageState = this.store.state === "unavailable" ? "unavailable" : "invalid";
        delete this.settings.persistedValues; delete this.settings.storeRevision;
      }
      this.commit(before);
      return this.finish(item, "failed", "INTERNAL_ERROR", "SETTINGS_PERSIST_FAILED", { configurationStatus: "notCommitted", applicationStatus: "notAttempted", persistenceStatus: "failed" });
    }
    if (!equal(pending.observedStore, { revision: this.store.revision, overrides: this.store.overrides })) {
      this.refreshStore();
      return this.finish(item, "rejected", "INVALID_STATE", "SETTINGS_STORE_CHANGED", { configurationStatus: "notCommitted", applicationStatus: "notAttempted", persistenceStatus: "notRequested" });
    }
    const value = item.params.patch[field];
    const unchanged = Object.hasOwn(this.store.overrides, field) && sameSetting(field, this.store.overrides[field], value);
    if (!unchanged) { const revision = next(this.store.revision); this.store.overrides[field] = copy(value); this.store.revision = revision; this.store.writeCount++; this.store.state = "saved"; }
    this.knownStore = { revision: this.store.revision, overrides: copy(this.store.overrides) };
    if (input.crashAfterStore) return this.restartSettings(input.runtimeInstanceId);
    // Store and configured/apply observations become one host settings commit below.
    const before = copy(this.factView());
    this.settings.persistedValues = knownValues(this.settingsCap, { ...this.defaults, ...this.store.overrides });
    this.settings.storeRevision = this.store.revision; this.settings.storageState = this.store.state;
    delete this.settings.persistError;
    return this.applyConfiguration(item, { ...pending.input, ...input, beforeStoreFacts: before }, unchanged ? "unchanged" : "stored");
  }
  executeSettings(input) {
    const item = this.records.get(this.key(this.settingsParams(input)));
    if (!item) return { status: "NOT_FOUND" };
    if (item.operation.state !== "queued") return this.success({ operation: copy(item.operation) });
    const p = item.params, field = item.operation.settingsResult.field;
    const fail = (status, reason) => this.finish(item, "rejected", status, reason, { configurationStatus: "notCommitted", applicationStatus: "notAttempted", persistenceStatus: "notRequested" });
    if (p.runtimeInstanceId !== this.state.controlSync.runtimeInstanceId || this.closed) return fail("INVALID_STATE", "SETTINGS_REVISION_CONFLICT");
    if (p.expectedSettingsRevision !== this.settings.settingsRevision) return fail("INVALID_STATE", "SETTINGS_REVISION_CONFLICT");
    if (this.settings.lockedFields.includes(field)) return fail("INVALID_STATE", "SETTINGS_LOCKED");
    if (this.settingsCap.liveFields.includes(field) && !equal(this.settings.applied[field], p.patch[field]) && decimal(this.state.controlSync.revision) === 18446744073709551615n) return fail("UNAVAILABLE", "SETTINGS_CONTROL_UNAVAILABLE");
    try { this.patchCheck(p); } catch (error) { return fail(error.status ?? "INVALID_ARGUMENT", error.message); }
    if (!p.persist) return this.applyConfiguration(item, input, "notRequested");
    if (this.pendingWrite) return { status: "BUSY" };
    if (input.writerBusy) return fail("BUSY", "SETTINGS_WRITER_BUSY");
    this.pendingWrite = { item, epoch: p.runtimeInstanceId, input: copy(input), observedStore: copy(this.knownStore) };
    if (input.waitStore) return this.success({ operation: copy(item.operation), waitingStore: true });
    return this.completeStore(input);
  }
  publishSettings() {
    if (!this.unpublishedSettings) return { published: false };
    validateSettingsState(this.types, this.capabilities, this.settings);
    this.settings.settingsEventSequence = next(this.settings.settingsEventSequence); this.unpublishedSettings = false;
    this.lastSettingsEvent = { name: "cast.presentationSettingsChanged", data: { settingsContext: {
      controlVersion: 1, runtimeInstanceId: this.state.controlSync.runtimeInstanceId,
      settingsRevision: this.settings.settingsRevision, settingsEventSequence: this.settings.settingsEventSequence }, settings: copy(this.settings) } };
    return { published: true, sequence: this.settings.settingsEventSequence, event: copy(this.lastSettingsEvent) };
  }
  coherent(control, settings) {
    validateSettingsState(this.types, this.capabilities, settings);
    if (settings.appliedControlRevision !== undefined) check(decimal(settings.appliedControlRevision) <= decimal(control.controlSync.revision), "policy version ahead of snapshot");
    for (const [field, value] of Object.entries(settings.applied)) {
      if (field === "layoutMode") check(value === control.presentation.layoutMode, "split layout capture");
      if (field === "audioControlMode") check(value === control.audioPolicy.controlMode, "split control-mode capture");
      if (field === "audioMixMode") check(value === control.audioPolicy.mixMode, "split mix-mode capture");
      if (field === "capacity") check(value === control.presentation.appliedCapacity, "split capacity capture");
    }
  }
  installSettings(input) {
    const settings = input.state ? copy(input.state) : { ...copy(this.settings), ...(input.patch ?? {}) };
    if (input.combined !== false) { const control = { ...copy(this.state), ...(input.controlPatch ?? {}) }; this.coherent(control, settings); this.install(control); }
    else validateSettingsState(this.types, this.capabilities, settings);
    const buffered = this.settingsBuffer; this.settingsBuffer = []; this.settingsSeen.clear();
    this.clientSettings = settings; this.settingsNeedsSnapshot = false;
    for (const event of buffered) if (event.data.settingsContext.runtimeInstanceId === this.state.controlSync.runtimeInstanceId && decimal(event.data.settingsContext.settingsEventSequence) > decimal(settings.settingsEventSequence)) this.receiveSettings({ event });
    return { installed: true, settingsSequence: settings.settingsEventSequence };
  }
  receiveSettings(input) {
    if (this.settingsNeedsSnapshot) return { resync: true };
    const event = copy(input.event ?? this.lastSettingsEvent);
    check(event?.name === "cast.presentationSettingsChanged", "settings event");
    Object.assign(event.data.settingsContext, input.context ?? {}); Object.assign(event.data.settings, input.patch ?? {});
    validateValue(this.types, "CastPresentationSettingsChangedEvent", event.data);
    const { settingsContext: ctx, settings } = event.data;
    check(ctx.controlVersion === 1 && ctx.runtimeInstanceId === this.state.controlSync.runtimeInstanceId, "settings epoch/version");
    check(ctx.settingsRevision === settings.settingsRevision && ctx.settingsEventSequence === settings.settingsEventSequence, "settings context mismatch");
    validateSettingsState(this.types, this.capabilities, settings);
    const limits = { ...this.capabilities.operations.limits, ...(input.limits ?? {}) };
    if (!this.clientSettings) {
      check(this.settingsBuffer.length + 1 <= limits.maxEventBufferEvents && this.settingsBuffer.reduce((n, e) => n + Buffer.byteLength(JSON.stringify(e)), 0) + Buffer.byteLength(JSON.stringify(event)) <= limits.maxEventBufferBytes, "settings buffer overflow");
      this.settingsBuffer.push(event); return { applied: false, buffered: this.settingsBuffer.length };
    }
    const seq = decimal(ctx.settingsEventSequence), barrier = decimal(this.clientSettings.settingsEventSequence);
    if (this.settingsSeen.has(ctx.settingsEventSequence)) { check(equal(this.settingsSeen.get(ctx.settingsEventSequence), event), "conflicting settings duplicate"); return { ignored: true }; }
    if (seq <= barrier) return { ignored: true };
    check(seq === barrier + 1n && decimal(ctx.settingsRevision) >= decimal(this.clientSettings.settingsRevision), "settings sequence gap/revision");
    this.clientSettings = copy(settings); this.settingsSeen.clear(); this.settingsSeen.set(ctx.settingsEventSequence, event);
    return { applied: true, awaitControl: settings.appliedControlRevision !== undefined && (!this.client || decimal(this.client.controlSync.revision) < decimal(settings.appliedControlRevision)) };
  }
  restartSettings(runtimeInstanceId) {
    check(typeof runtimeInstanceId === "string" && runtimeInstanceId !== this.state.controlSync.runtimeInstanceId, "fresh restart epoch");
    super.action("restart", { runtimeInstanceId }); this.pendingWrite = undefined;
    this.capabilities.runtimeInstanceId = runtimeInstanceId;
    const known = this.store.schemaVersion === 1 && !["invalid", "unavailable"].includes(this.store.state);
    const values = knownValues(this.settingsCap, known ? { ...this.defaults, ...this.store.overrides } : this.defaults);
    this.settings = { ...copy(this.fixture.settings), settingsRevision: "1", settingsEventSequence: "0", configured: copy(values), applied: copy(values),
      activeStartup: Object.fromEntries(this.settingsCap.restartOnlyFields.map((f) => [f, copy(values[f])])), storageState: known ? this.store.state : "invalid", applicationIssues: [], lockedFields: [] };
    if (known) { this.settings.persistedValues = copy(values); if (this.store.state !== "absent") this.settings.storeRevision = this.store.revision; else delete this.settings.storeRevision; }
    else { delete this.settings.persistedValues; delete this.settings.storeRevision; }
    this.state.controlSync.revision = "1"; this.state.controlSync.eventSequence = "0"; this.settings.appliedControlRevision = "1";
    const adoptedCapacity = Math.min(values.capacity, this.state.presentation.validatedCapacity ?? this.capabilities.presentation.maxPresentationCapacity);
    this.state.presentation.layoutMode = values.layoutMode; this.state.presentation.configuredCapacity = values.capacity;
    this.state.presentation.appliedCapacity = adoptedCapacity; this.state.presentation.effectiveCapacity = adoptedCapacity;
    this.settings.applied.capacity = adoptedCapacity;
    if (adoptedCapacity < values.capacity) this.settings.applicationIssues.push({ field: "capacity", kind: "limitedByValidation" });
    this.state.audioPolicy.controlMode = values.audioControlMode; this.state.audioPolicy.mixMode = values.audioMixMode;
    this.clientSettings = undefined; this.settingsBuffer = []; this.settingsSeen.clear(); this.settingsNeedsSnapshot = false; this.unpublishedSettings = false; this.recompute();
    return { restarted: true, synchronized: false, settings: copy(this.settings) };
  }
  action(action, input) {
    try {
      switch (action) {
        case "settings_discover": {
          const descriptor = { ...copy(this.capabilities), ...(input.descriptor ?? {}) };
          if (input.capability) descriptor.presentationSettings = input.capability;
          try { validateSettingsCapability(this.types, descriptor); return { supported: true }; } catch { return { supported: false }; }
        }
        case "settings_admit": return this.admitSettings(input);
        case "settings_execute": return this.executeSettings(input);
        case "settings_complete_store": return this.completeStore(input);
        case "settings_publish": return this.publishSettings();
        case "settings_receive": return this.receiveSettings(input);
        case "settings_install": return this.installSettings(input);
        case "settings_capture": {
          if (input.p0Only) return super.action("capture", input);
          if (!this.settingsCap || input.supported === false || (input.controlVersion !== undefined && input.controlVersion !== 1)) return { status: "NOT_SUPPORTED" };
          this.coherent(this.state, this.settings);
          const result = input.settingsOnly ? { receiver: copy(this.state.receiver), sampledAt: this.state.sampledAt, controlSync: copy(this.state.controlSync), presentationSettings: copy(this.settings) }
            : { ...copy(this.state), presentationSettings: copy(this.settings) };
          if (input.unavailable || Buffer.byteLength(JSON.stringify(result)) > (input.maxSnapshotBytes ?? this.capabilities.operations.limits.maxSnapshotBytes)) return { status: "UNAVAILABLE" };
          return this.success({ result });
        }
        case "settings_flush": check(this.settingsBuffer.length === 0, "settings buffer timeout"); return { complete: true };
        case "settings_observe": {
          const before = copy(this.factView()); Object.assign(this.settings, copy(input.patch ?? {}));
          if (input.capabilityPatch) Object.assign(this.settingsCap, copy(input.capabilityPatch));
          if (input.presentation) Object.assign(this.state.presentation, copy(input.presentation));
          for (const field of input.omitPresentation ?? []) delete this.state.presentation[field];
          if (input.applied) {
            for (const [field, value] of Object.entries(input.applied)) { this.settings.applied[field] = copy(value); if (field === "capacity") this.state.presentation.appliedCapacity = value; }
          }
          if (input.omitApplied) for (const field of input.omitApplied) delete this.settings.applied[field];
          this.commit(before); return { observed: true, revision: this.settings.settingsRevision };
        }
        case "settings_store": {
          Object.assign(this.store, copy(input.patch ?? {}));
          if (input.refresh) this.refreshStore();
          return { observed: true };
        }
        case "settings_restart": return this.restartSettings(input.runtimeInstanceId);
        case "settings_validate": {
          const value = input.state ?? { ...copy(this.settings), ...(input.patch ?? {}) };
          validateSettingsState(this.types, this.capabilities, value); return { valid: true };
        }
        case "settings_state": return { settings: copy(this.settings), store: copy(this.store), policyCalls: copy(this.policyCalls), control: copy(this.state), writing: !!this.pendingWrite,
          clientSettings: copy(this.clientSettings), settingsNeedsSnapshot: this.settingsNeedsSnapshot,
          readyForPolicy: !!this.clientSettings && !!this.client && (this.clientSettings.appliedControlRevision === undefined || decimal(this.client.controlSync.revision) >= decimal(this.clientSettings.appliedControlRevision)) };
        case "receive_event": return super.action(action, { ...input, event: input.event ?? (input.source === "settings" ? this.lastSettingsEvent : this.controlEvents.at(-1)) });
        case "validate_value": if (input.schema === "CastPresentationSettingsState") { validateSettingsState(this.types, this.capabilities, input.value); return { valid: true }; } break;
      }
      return super.action(action, input);
    } catch (error) {
      if (["settings_receive", "settings_install", "settings_flush"].includes(action)) {
        this.settingsNeedsSnapshot = true; this.settingsBuffer = []; this.settingsSeen.clear(); return { resync: true };
      }
      if (action === "settings_capture") return { status: "UNAVAILABLE" };
      return { status: error.status ?? "INVALID_ARGUMENT", valid: false, outcomeReason: error.status ? error.message : "INVALID_SETTINGS" };
    }
  }
}
