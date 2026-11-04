#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";
import { runControlCase } from "../../conformance/reference/cast-control.mjs";
import { CastSettingsModel } from "../../conformance/reference/cast-settings.mjs";

const root = path.resolve(process.argv[2] ?? process.cwd());
const requireFromGenerators = createRequire(path.join(root, "tooling", "generators", "package.json"));
const YAML = requireFromGenerators("yaml");
const Ajv2020Module = requireFromGenerators("ajv/dist/2020");
const Ajv2020 = Ajv2020Module.default ?? Ajv2020Module;

const conformanceDir = path.join(root, "conformance");
const manifestPath = path.join(conformanceDir, "manifest.yaml");
const caseSchemaPath = path.join(conformanceDir, "schemas", "conformance-case.schema.json");
const resultSchemaPath = path.join(conformanceDir, "schemas", "conformance-result.schema.json");
const generatedProtocolPath = path.join(root, "contract", "generated", "protocol.json");
const fixturesDir = path.join(conformanceDir, "fixtures", "device_profiles");

const errors = [];
const profileCounts = new Map();

function fail(message) {
  errors.push(message);
}

function readYaml(file) {
  return YAML.parse(fs.readFileSync(file, "utf8"), { merge: true });
}

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

function walkYaml(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walkYaml(full));
    else if (entry.isFile() && entry.name.endsWith(".yaml")) out.push(full);
  }
  return out.sort();
}

function caseIdFromPath(file) {
  const relative = path.relative(path.join(conformanceDir, "cases"), file);
  return relative.replace(/\.yaml$/, "").split(path.sep).join(".");
}

function formatAjvError(error) {
  const pathLabel = error.instancePath || "/";
  return `${pathLabel} ${error.message}`;
}

function expectObject(value, label) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    fail(`${label} must be an object`);
    return false;
  }
  return true;
}

function expectString(value, label) {
  if (typeof value !== "string" || value.length === 0) {
    fail(`${label} must be a non-empty string`);
    return false;
  }
  return true;
}

function statusNames(value) {
  if (typeof value === "string") return [value];
  if (value && typeof value === "object" && Array.isArray(value.oneOf)) return value.oneOf.map(String);
  return [];
}

function validateKnownStatus(label, value, errorNames) {
  for (const name of statusNames(value)) {
    if (!errorNames.has(name)) fail(`${label} references unknown status/error: ${name}`);
  }
}

function validateMethod(label, name, methodNames) {
  if (!name || name.startsWith("vendor.")) return;
  if (!methodNames.has(name)) fail(`${label} references unknown method: ${name}`);
}

function validateEvent(label, name, eventNames) {
  if (!name || name.startsWith("vendor.")) return;
  if (!eventNames.has(name)) fail(`${label} references unknown event: ${name}`);
}

function visitReferences(value, callback) {
  if (!value || typeof value !== "object") return;
  if (!Array.isArray(value) && Object.keys(value).length === 1 && typeof value.ref === "string") callback(value.ref);
  for (const child of Array.isArray(value) ? value : Object.values(value)) visitReferences(child, callback);
}

function capturedFields(step) {
  const payload = step.expect?.jsonrpc ?? step.expect?.rpc ?? step.jsonrpc ?? step.rpc ?? {};
  return new Set(Object.keys(payload));
}

function validateExecutionMetadata(steps, label, report = fail) {
  const stepIds = new Set();
  const captures = new Map();
  for (const [index, step] of (steps ?? []).entries()) {
    if (step.captureAs && !step.expect) report(`${label}.steps[${index}] captureAs is only valid on expectation/output steps`);
    for (const relation of ["responseTo", "triggeredBy"]) {
      if (step[relation] && !stepIds.has(step[relation])) report(`${label}.steps[${index}] ${relation} uses missing or forward step id: ${step[relation]}`);
    }
    if (step.id) {
      if (stepIds.has(step.id)) report(`${label} duplicate step id: ${step.id}`);
      stepIds.add(step.id);
    }
    visitReferences(step, (reference) => {
      const [capture, field, ...rest] = reference.split(".");
      if (rest.length || !capture || !field) return report(`${label}.steps[${index}] invalid reference path: ${reference}`);
      if (!captures.has(capture)) return report(`${label}.steps[${index}] reference uses missing or forward capture: ${capture}`);
      if (!captures.get(capture).has(field)) report(`${label}.steps[${index}] reference field is not captured: ${reference}`);
    });
    if (step.captureAs) {
      if (captures.has(step.captureAs)) report(`${label} duplicate capture: ${step.captureAs}`);
      captures.set(step.captureAs, capturedFields(step));
    }
  }
}

function validateSemanticContract(value, label, report = fail) {
  const kind = value.semantic?.kind;
  const validateExecution = (steps, executionLabel) => {
    const indexedSteps = (steps ?? []).map((step, index) => ({ step, index }));
    const byId = new Map(indexedSteps.filter(({ step }) => step.id).map(({ step, index }) => [step.id, { step, index }]));
    const select = (role, predicate = () => true) => indexedSteps.filter(({ step }) => step.role === role && predicate(step));
    const exactlyOne = (role, predicate, context) => {
      const matches = select(role, predicate);
      if (matches.length !== 1) report(`${executionLabel} must have exactly one ${context}`);
      return matches[0];
    };
    const exactRoleCount = (role, count) => {
      const actual = select(role).length;
      if (actual !== count) report(`${executionLabel} role ${role} must occur exactly ${count} time(s)`);
    };
    const requireEdge = (child, edge, parent, context) => {
      if (!child || !parent || child.step[edge] !== parent.step.id) {
        report(`${executionLabel} ${context} must use ${edge}=${parent?.step.id}`);
        return;
      }
      if (child.index <= parent.index) report(`${executionLabel} ${context} edge must point backward`);
    };
    const exactStatus = (step, expected, context) => {
      const names = statusNames(step?.step?.expect?.rpc?.statusCode);
      if (names.length !== 1 || names[0] !== expected) report(`${executionLabel} ${context} must have exact ${expected}`);
    };
    const requireLiveness = (trigger) => {
      exactRoleCount("liveness", 2);
      const request = exactlyOne("liveness", (step) => Boolean(step.rpc), "liveness RPC request");
      const response = exactlyOne("liveness", (step) => Boolean(step.expect?.rpc), "liveness RPC response");
      requireEdge(request, "triggeredBy", trigger, "liveness RPC request");
      requireEdge(response, "responseTo", request, "liveness RPC response");
      if (request?.step.rpc.requestId !== response?.step.expect.rpc.requestId) report(`${executionLabel} liveness RPC response requestId mismatch`);
      exactStatus(response, "SUCCESS", "liveness RPC response");
      return { request, response };
    };
    const requireNoEvent = (trigger) => {
      const observation = exactlyOne("observe", (step) => Boolean(step.expect?.no_event), "no_event observation");
      requireEdge(observation, "triggeredBy", trigger, "no_event observation");
      if (observation?.step.expect.no_event.name !== value.given?.event || !(observation?.step.expect.no_event.withinMs > 0)) report(`${executionLabel} no_event must be bounded and match given.event`);
      return observation;
    };
    const requireDegradationChain = (status) => {
      exactRoleCount("trigger", 1);
      exactRoleCount("degraded", 1);
      const request = exactlyOne("trigger", (step) => Boolean(step.rpc), "trigger RPC request");
      const response = exactlyOne("degraded", (step) => Boolean(step.expect?.rpc), "degraded RPC response");
      requireEdge(response, "responseTo", request, "degraded RPC response");
      if (request?.step.rpc.requestId !== response?.step.expect.rpc.requestId) report(`${executionLabel} degraded RPC response requestId mismatch`);
      exactStatus(response, status, "degraded RPC response");
      requireLiveness(response);
      return { request, response };
    };
    switch (kind) {
      case "baseline_handshake": {
        exactRoleCount("input", 1);
        exactRoleCount("trigger", 1);
        exactRoleCount("observe", 1);
        const hello = exactlyOne("input", (step) => step.expect?.jsonrpc?.op === "HELLO", "HELLO input");
        const identify = exactlyOne("trigger", (step) => step.jsonrpc?.op === "IDENTIFY", "IDENTIFY trigger");
        const identified = exactlyOne("observe", (step) => step.expect?.jsonrpc?.op === "IDENTIFIED", "IDENTIFIED response");
        if (hello?.step.expect.jsonrpc.d?.axtpVersion !== undefined) report(`${executionLabel} baseline HELLO must not require axtpVersion`);
        requireEdge(identified, "responseTo", identify, "IDENTIFIED response");
        if (identified?.step.expect.jsonrpc.sid?.minLength < 1) report(`${executionLabel} IDENTIFIED sid must be non-empty`);
        break;
      }
      case "unknown_method_error": requireDegradationChain("RPC_METHOD_NOT_FOUND"); break;
      case "registered_method_unavailable":
      case "registered_feature_degradation": requireDegradationChain("NOT_SUPPORTED"); break;
      case "invalid_params": {
        const pair = requireDegradationChain(value.given?.expected_validation_error);
        if (statusNames(pair?.response?.step.expect?.rpc?.statusCode).includes("NOT_SUPPORTED")) report(`${executionLabel} invalid params cannot use NOT_SUPPORTED`);
        break;
      }
      case "profile_degradation": requireDegradationChain("NOT_SUPPORTED"); break;
      case "unsubscribed_event_sender_suppression": {
        exactRoleCount("precondition", 2);
        const unsubscribe = exactlyOne("precondition", (step) => step.jsonrpc?.op === "REIDENTIFY" && step.jsonrpc?.d?.eventMasks === "", "unsubscribe REIDENTIFY precondition");
        const unsubscribeResponse = exactlyOne("precondition", (step) => step.expect?.jsonrpc?.op === "IDENTIFIED", "unsubscribe IDENTIFIED response");
        requireEdge(unsubscribeResponse, "responseTo", unsubscribe, "unsubscribe IDENTIFIED response");
        exactRoleCount("trigger", 2);
        exactRoleCount("observe", 1);
        const request = exactlyOne("trigger", (step) => Boolean(step.rpc), "trigger RPC request");
        requireEdge(request, "triggeredBy", unsubscribeResponse, "trigger RPC request");
        const response = exactlyOne("trigger", (step) => Boolean(step.expect?.rpc), "trigger RPC response");
        requireEdge(response, "responseTo", request, "trigger RPC response");
        if (request?.step.rpc.requestId !== response?.step.expect.rpc.requestId) report(`${executionLabel} trigger RPC response requestId mismatch`);
        exactStatus(response, "SUCCESS", "trigger RPC response");
        const observation = requireNoEvent(response);
        requireLiveness(observation);
        break;
      }
      case "unsupported_event_sender_suppression": {
        exactRoleCount("trigger", 2);
        exactRoleCount("observe", 1);
        const request = exactlyOne("trigger", (step) => Boolean(step.rpc), "trigger RPC request");
        const response = exactlyOne("trigger", (step) => Boolean(step.expect?.rpc), "trigger RPC response");
        requireEdge(response, "responseTo", request, "trigger RPC response");
        if (request?.step.rpc.requestId !== response?.step.expect.rpc.requestId) report(`${executionLabel} trigger RPC response requestId mismatch`);
        exactStatus(response, "SUCCESS", "trigger RPC response");
        const observation = requireNoEvent(response);
        requireLiveness(observation);
        break;
      }
      case "unknown_event_receiver_tolerance": {
        exactRoleCount("input", 1);
        exactRoleCount("observe", 1);
        const input = exactlyOne("input", (step) => step.direction === "peer_to_runtime" && step.jsonrpc?.op === "EVENT", "peer unknown-event input");
        const disposition = exactlyOne("observe", (step) => Boolean(step.expect?.compatibility_input), "compatibility disposition");
        requireEdge(disposition, "triggeredBy", input, "compatibility disposition");
        const result = disposition?.step.expect.compatibility_input;
        if (result?.kind !== "compatibility_fuzz") report(`${executionLabel} disposition kind must be compatibility_fuzz`);
        const outcomes = statusNames(result?.outcome);
        if (outcomes.length === 0 || outcomes.some((outcome) => !["ignored", "diagnostic_only"].includes(outcome))) report(`${executionLabel} disposition outcome must be ignored or diagnostic_only`);
        if (result?.applicationDispatched !== false) report(`${executionLabel} disposition applicationDispatched must be false`);
        if (result?.sessionState !== "APP_READY") report(`${executionLabel} disposition sessionState must be APP_READY`);
        requireLiveness(disposition);
        break;
      }
      case "advisory_version_handshake": {
        exactRoleCount("input", 1);
        exactRoleCount("trigger", 1);
        exactRoleCount("observe", 1);
        exactRoleCount("liveness", 2);
        exactlyOne("input", (step) => step.jsonrpc?.op === "HELLO", "HELLO input");
        const identify = exactlyOne("trigger", (step) => step.jsonrpc?.op === "IDENTIFY", "IDENTIFY trigger");
        const identified = exactlyOne("observe", (step) => step.captureAs === "identified" && step.expect?.jsonrpc?.op === "IDENTIFIED", "captured IDENTIFIED response");
        requireEdge(identified, "responseTo", identify, "captured IDENTIFIED response");
        if (identified?.step.expect.jsonrpc.sid?.minLength < 1) report(`${executionLabel} IDENTIFIED sid must be non-empty`);
        const reidentify = exactlyOne("liveness", (step) => step.jsonrpc?.op === "REIDENTIFY", "REIDENTIFY request");
        requireEdge(reidentify, "triggeredBy", identified, "REIDENTIFY request");
        if (reidentify?.step.jsonrpc.sid?.ref !== "identified.sid") report(`${executionLabel} REIDENTIFY must use captured sid`);
        const reidentified = exactlyOne("liveness", (step) => step.captureAs === "reidentified" && step.expect?.jsonrpc?.op === "IDENTIFIED", "reidentified response");
        requireEdge(reidentified, "responseTo", reidentify, "reidentified response");
        if (reidentified?.step.expect.jsonrpc.sid?.minLength < 1) report(`${executionLabel} reidentified sid must be non-empty`);
        break;
      }
      default: report(`${executionLabel} unsupported semantic.kind: ${kind}`);
    }
    for (const { step } of indexedSteps) for (const edge of [step.responseTo, step.triggeredBy].filter(Boolean)) if (!byId.has(edge)) report(`${executionLabel} broken semantic edge: ${edge}`);
  };
  if (kind === "advisory_version_handshake") {
    const actualRows = new Set();
    for (const scenario of value.scenarios ?? []) {
      const hello = scenario.steps?.find((step) => step.role === "input" && step.jsonrpc?.op === "HELLO");
      const server = Object.hasOwn(hello?.jsonrpc?.d ?? {}, "axtpVersion") ? hello.jsonrpc.d.axtpVersion : "absent";
      actualRows.add(`${server}->${scenario.given?.client_axtp_version}`);
      validateExecution(scenario.steps, `${label}.scenario.${scenario.name}`);
    }
    const required = value.semantic.requiredRows ?? [];
    if ((value.scenarios ?? []).length !== required.length) report(`${label} scenario count must equal semantic.requiredRows length`);
    if (actualRows.size !== required.length || required.some((row) => !actualRows.has(row))) report(`${label} advisory rows do not match semantic.requiredRows`);
  } else validateExecution(value.steps, label);
}

const executionFixturePath = path.join(conformanceDir, "validator-fixtures", "execution_metadata.yaml");
const semanticFixturePath = path.join(conformanceDir, "validator-fixtures", "semantic_contracts.yaml");

function validateCaseReferences(label, value, refs) {
  if (!refs.levelNames.has(value.level)) fail(`${label}.level references unknown manifest level: ${value.level}`);

  for (const profile of value.profiles ?? []) {
    profileCounts.set(profile, (profileCounts.get(profile) ?? 0) + 1);
    if (!refs.profileLabels.has(profile)) fail(`${label}.profiles references unknown profile/level: ${profile}`);
  }

  if (value.given?.device_profile && !refs.fixtureNames.has(value.given.device_profile)) {
    fail(`${label}.given.device_profile references missing fixture: ${value.given.device_profile}`);
  }
  validateMethod(`${label}.given.method`, value.given?.method, refs.methodNames);

  const executableSteps = [
    ...(value.steps ?? []).map((step, index) => [`steps[${index}]`, step]),
    ...(value.scenarios ?? []).flatMap((scenario) =>
      (scenario.steps ?? []).map((step, index) => [`scenarios.${scenario.name}.steps[${index}]`, step])
    )
  ];
  for (const [stepPath, step] of executableSteps) {
    const stepLabel = `${label}.${stepPath}`;
    validateMethod(`${stepLabel}.rpc.method`, step.rpc?.method, refs.methodNames);
    validateMethod(`${stepLabel}.jsonrpc.d.method`, step.jsonrpc?.d?.method, refs.methodNames);
    validateKnownStatus(`${stepLabel}.control.statusCode`, step.control?.statusCode, refs.errorNames);
    validateKnownStatus(`${stepLabel}.error.code`, step.error?.code, refs.errorNames);
    validateEvent(`${stepLabel}.event.name`, step.event?.name, refs.eventNames);
    if (step.control_model) {
      if (!["cast-control", "cast-settings"].includes(value.level) || step.direction !== "runtime_local") fail(`${stepLabel}.control_model requires cast-control or cast-settings/runtime_local`);
      validateMethod(`${stepLabel}.control_model.input.method`, step.control_model.input?.method, refs.methodNames);
      validateEvent(`${stepLabel}.control_model.input.event.name`, step.control_model.input?.event?.name, refs.eventNames);
      const schema = step.control_model.input?.schema;
      if (schema && !refs.schemaNames.has(schema)) fail(`${stepLabel} references unknown model schema: ${schema}`);
      validateKnownStatus(`${stepLabel}.control_model.expect.status`, step.control_model.expect?.status, refs.errorNames);
    }

    if (step.registry_lookup?.capability && !refs.capabilityNames.has(step.registry_lookup.capability)) {
      fail(`${stepLabel}.registry_lookup.capability references unknown capability: ${step.registry_lookup.capability}`);
    }
    for (const method of step.registry_lookup?.methods ?? []) {
      validateMethod(`${stepLabel}.registry_lookup.methods`, method, refs.methodNames);
    }

    const expect = step.expect ?? {};
    validateMethod(`${stepLabel}.expect.jsonrpc.d.method`, expect.jsonrpc?.d?.method, refs.methodNames);
    validateKnownStatus(`${stepLabel}.expect.rpc.statusCode`, expect.rpc?.statusCode, refs.errorNames);
    validateKnownStatus(`${stepLabel}.expect.control.statusCode`, expect.control?.statusCode, refs.errorNames);
    validateKnownStatus(`${stepLabel}.expect.error.code`, expect.error?.code, refs.errorNames);
    validateEvent(`${stepLabel}.expect.event.name`, expect.event?.name, refs.eventNames);
  }
}

if (!fs.existsSync(manifestPath)) fail(`Missing ${path.relative(root, manifestPath)}`);
if (!fs.existsSync(caseSchemaPath)) fail(`Missing ${path.relative(root, caseSchemaPath)}`);
if (!fs.existsSync(resultSchemaPath)) fail(`Missing ${path.relative(root, resultSchemaPath)}`);
if (!fs.existsSync(generatedProtocolPath)) fail(`Missing ${path.relative(root, generatedProtocolPath)}`);

const caseSchema = readJson(caseSchemaPath);
const resultSchema = readJson(resultSchemaPath);
const ajv = new Ajv2020({ allErrors: true, strict: false });
const validateCaseSchema = ajv.compile(caseSchema);
ajv.compile(resultSchema);

function validateFixturePipeline(fixture, fixtureType) {
  const { name, valid, ...candidate } = fixture;
  const schemaErrors = [];
  const metadataErrors = [];
  const semanticErrors = [];
  if (!validateCaseSchema(candidate)) {
    for (const error of validateCaseSchema.errors ?? []) schemaErrors.push(formatAjvError(error));
  }
  validateExecutionMetadata(candidate.steps, `fixture.${name}`, (message) => metadataErrors.push(message));
  for (const scenario of candidate.scenarios ?? []) {
    validateExecutionMetadata(scenario.steps, `fixture.${name}.scenario.${scenario.name}`, (message) => metadataErrors.push(message));
  }
  if (fixtureType === "semantic contract") validateSemanticContract(candidate, `fixture.${name}`, (message) => semanticErrors.push(message));
  if (schemaErrors.length > 0) fail(`${fixtureType} fixture ${name} must be schema-valid: ${schemaErrors.join(" | ")}`);
  if (fixtureType === "semantic contract" && metadataErrors.length > 0) fail(`semantic contract fixture ${name} must have valid execution metadata: ${metadataErrors.join(" | ")}`);
  const focusedErrors = fixtureType === "semantic contract" ? semanticErrors : metadataErrors;
  if ((focusedErrors.length === 0) !== valid) fail(`${fixtureType} fixture ${name} expected valid=${valid}, errors=${focusedErrors.join(" | ")}`);
  return { kind: candidate.semantic?.kind, valid };
}

if (fs.existsSync(executionFixturePath)) {
  for (const fixture of readYaml(executionFixturePath).tests ?? []) validateFixturePipeline(fixture, "execution metadata");
}
const semanticFixtureCoverage = [];
if (fs.existsSync(semanticFixturePath)) {
  for (const fixture of readYaml(semanticFixturePath).tests ?? []) semanticFixtureCoverage.push(validateFixturePipeline(fixture, "semantic contract"));
}

const manifest = readYaml(manifestPath);
expectObject(manifest, "manifest");
expectObject(manifest.conformance, "manifest.conformance");
expectObject(manifest.levels, "manifest.levels");
expectString(manifest.conformance?.version, "manifest.conformance.version");
expectString(manifest.conformance?.spec_min, "manifest.conformance.spec_min");

const generated = readJson(generatedProtocolPath);
const refs = {
  methodNames: new Set((generated.methods ?? []).map((item) => item.name)),
  eventNames: new Set((generated.events ?? []).map((item) => item.name)),
  errorNames: new Set((generated.errors ?? []).map((item) => item.name)),
  capabilityNames: new Set((generated.capabilities ?? []).map((item) => item.name)),
  schemaNames: new Set((generated.schemas ?? []).map((item) => item.name)),
  levelNames: new Set(Object.keys(manifest.levels ?? {})),
  profileLabels: new Set(Object.keys(manifest.levels ?? {})),
  fixtureNames: new Set(),
  fixtureValues: new Map()
};

for (const file of walkYaml(path.join(conformanceDir, "profiles"))) {
  const value = readYaml(file);
  const label = path.relative(root, file);
  expectObject(value, label);
  expectString(value?.id, `${label}.id`);
  expectString(value?.since, `${label}.since`);
  if (typeof value?.id === "string") refs.profileLabels.add(value.id);
}

if (fs.existsSync(fixturesDir)) {
  for (const entry of fs.readdirSync(fixturesDir, { withFileTypes: true })) {
    if (entry.isFile() && entry.name.endsWith(".yaml")) {
      const name = entry.name.replace(/\.yaml$/, "");
      refs.fixtureNames.add(name);
      refs.fixtureValues.set(name, readYaml(path.join(fixturesDir, entry.name)));
    }
  }
}

const caseFiles = walkYaml(path.join(conformanceDir, "cases"));
const casesById = new Map();
let controlScenarios = 0;
let controlObservations = 0;
for (const file of caseFiles) {
  const label = path.relative(root, file);
  const value = readYaml(file);
  if (!validateCaseSchema(value)) {
    for (const error of validateCaseSchema.errors ?? []) fail(`${label}: ${formatAjvError(error)}`);
  }
  const expectedId = caseIdFromPath(file);
  if (value?.id !== expectedId) fail(`${label} id ${value?.id} does not match path id ${expectedId}`);
  if (typeof value?.id === "string") {
    if (casesById.has(value.id)) fail(`Duplicate case id: ${value.id}`);
    casesById.set(value.id, { file, value });
  }
  validateCaseReferences(label, value, refs);
  validateExecutionMetadata(value.steps, label);
  if (value.semantic?.kind) validateSemanticContract(value, label);
  const scenarioNames = new Set();
  for (const scenario of value.scenarios ?? []) {
    if (scenarioNames.has(scenario.name)) fail(`${label} duplicate scenario name: ${scenario.name}`);
    scenarioNames.add(scenario.name);
    validateExecutionMetadata(scenario.steps, `${label}.scenarios.${scenario.name}`);
  }
  if (["cast-control", "cast-settings"].includes(value.level)) {
    try {
      const source = readYaml(path.join(root, "contract/registry/domains/cast/domain.yaml"));
      const codes = Object.fromEntries(readYaml(path.join(root, "contract/registry/error/error_code.yaml")).errors.map((item) => [item.name, item.id]));
      const fixture = refs.fixtureValues.get(value.given?.device_profile);
      if (!fixture) throw new Error("missing cast control fixture");
      controlObservations += runControlCase(value, fixture, source.types, codes, fixture.settings ? CastSettingsModel : undefined);
      controlScenarios += value.scenarios?.length ?? 1;
    } catch (error) { fail(`${label}: cast control reference oracle failed: ${error.message}`); }
  }
}

for (const [level, value] of Object.entries(manifest.levels ?? {})) {
  if (!expectObject(value, `manifest.levels.${level}`)) continue;
  const requiredCases = value.required_cases;
  if (!Array.isArray(requiredCases)) {
    fail(`manifest.levels.${level}.required_cases must be an array`);
    continue;
  }
  for (const id of requiredCases) {
    if (!casesById.has(id)) fail(`manifest level ${level} references missing case: ${id}`);
  }
}

const casesBySemantic = new Map();
for (const { value } of casesById.values()) {
  if (!value.semantic?.kind) continue;
  if (casesBySemantic.has(value.semantic.kind)) fail(`Duplicate semantic contract: ${value.semantic.kind}`);
  casesBySemantic.set(value.semantic.kind, value);
}
for (const kind of casesBySemantic.keys()) {
  if (!semanticFixtureCoverage.some((fixture) => fixture.kind === kind && fixture.valid)) fail(`semantic kind ${kind} lacks a focused positive fixture`);
  if (!semanticFixtureCoverage.some((fixture) => fixture.kind === kind && !fixture.valid)) fail(`semantic kind ${kind} lacks a focused negative fixture`);
}
if (errors.length > 0) {
  console.error("[FAIL] conformance validation failed");
  for (const error of errors) console.error(`- ${error}`);
  process.exit(1);
}

console.log(`[OK] conformance manifest: ${manifest.conformance.version} (${manifest.conformance.spec_min})`);
console.log(`[OK] conformance cases: ${casesById.size}`);
if (controlScenarios) console.log(`[OK] cast-control reference: ${controlScenarios} scenarios, ${controlObservations} observations (substitute owners only)`);
for (const [profile, count] of [...profileCounts.entries()].sort(([a], [b]) => a.localeCompare(b))) {
  console.log(`[OK] profile coverage: ${profile}=${count}`);
}
