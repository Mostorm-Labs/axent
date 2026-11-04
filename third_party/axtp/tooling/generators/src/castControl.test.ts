import fs from "node:fs";
import path from "node:path";
import { describe, expect, it } from "vitest";
import YAML from "yaml";
import Ajv2020 from "ajv/dist/2020.js";
import { castControlEncodingVectors } from "./emitters/castControlVectors.js";
import { loadProtocolSources } from "./sourceLoader.js";
import { CastControlModel, runControlCase, decodeSessionRef } from "../../../conformance/reference/cast-control.mjs";
import { CastSettingsModel } from "../../../conformance/reference/cast-settings.mjs";

const root = path.resolve("../..");
const readYaml = (file: string) => YAML.parse(fs.readFileSync(path.join(root, file), "utf8"));
const source = readYaml("contract/registry/domains/cast/domain.yaml");
const fixture = readYaml("conformance/fixtures/device_profiles/cast_control_receiver.yaml");
const codes = Object.fromEntries(readYaml("contract/registry/error/error_code.yaml").errors.map((e: any) => [e.name, e.id]));
const files = fs.readdirSync(path.join(root, "conformance/cases/castcontrol")).filter((f) => f.endsWith(".yaml")).sort();
const ajv = new Ajv2020({ strict: false });
const validateCase = ajv.compile(JSON.parse(fs.readFileSync(path.join(root, "conformance/schemas/conformance-case.schema.json"), "utf8")));

describe("cast control P0/P1 executable reference conformance", () => {
  for (const file of files) {
    const value = readYaml(`conformance/cases/castcontrol/${file}`);
    it(`${value.id} executes all positive and negative scenarios`, () => {
      expect(validateCase(value), JSON.stringify(validateCase.errors)).toBe(true);
      const caseFixture = readYaml(`conformance/fixtures/device_profiles/${value.given.device_profile}.yaml`);
      expect(runControlCase(value, caseFixture, source.types, codes, caseFixture.settings ? CastSettingsModel : undefined)).toBeGreaterThan(0);
    });
  }
  it("keeps C01-C11 independently claimable and binds C01-C12 to combined settings", () => {
    const manifest = readYaml("conformance/manifest.yaml");
    expect(manifest.levels["cast-control"].required_cases).toEqual(files.filter((f) => !f.startsWith("c12_")).map((file) => `castcontrol.${file.slice(0, -5)}`));
    expect(manifest.levels["cast-settings"].required_cases).toEqual(files.map((file) => `castcontrol.${file.slice(0, -5)}`));
    expect(files).toHaveLength(12);
  });
  it("fails an intentionally incorrect oracle expectation", () => {
    const value = readYaml("conformance/cases/castcontrol/c05_fencing.yaml");
    value.scenarios[0].steps[1].control_model.expect.operation.state = "applied";
    expect(() => runControlCase(value, fixture, source.types, codes)).toThrow(/c05_fencing/);
  });
  it("rejects invalid model actions and incomplete DSL rather than silently skipping them", () => {
    const value = readYaml("conformance/cases/castcontrol/c01_capability.yaml");
    value.scenarios[0].steps[0].control_model.action = "guess_success";
    expect(validateCase(value)).toBe(false);
    delete value.scenarios[0].steps[0].control_model.expect;
    expect(validateCase(value)).toBe(false);
  });
  it("generates and independently decodes paired JSON/TLV8 boundary vectors", async () => {
    const sources = await loadProtocolSources(root);
    const vectors = castControlEncodingVectors(sources)!;
    expect(vectors.fields).toEqual({ id: 1, generation: 2 });
    expect(vectors.pairs).toHaveLength(11);
    const model = new CastControlModel(fixture, source.types, codes);
    for (const pair of vectors.pairs) {
      expect(model.action("encode_pair", { value: pair.json, tlv8Hex: pair.tlv8Hex }).valid, pair.name).toBe(pair.accepted);
      const decoded = decodeSessionRef(source.types, pair.tlv8Hex);
      expect(decoded).toEqual(pair.json);
      expect(model.action("validate_value", { schema: "CastSessionRef", value: decoded }).valid, `TLV ${pair.name}`).toBe(pair.accepted);
      expect(model.action("validate_value", { schema: "CastSessionRef", value: pair.json }).valid, `JSON ${pair.name}`).toBe(pair.accepted);
    }
    const upper = vectors.pairs.find((p) => p.name === "uint64_max")!;
    expect(upper.tlv8Hex).toContain("0214" + Buffer.from("18446744073709551615").toString("hex").toUpperCase());
    expect(() => decodeSessionRef(source.types, "0214FF")).toThrow(/TLV length/);
    expect(() => decodeSessionRef(source.types, "0201FF")).toThrow();
  });
  it("binds the exact P0 surface, errors and required event contexts", () => {
    const names = ["getControlCapabilities", "stopPresentation", "setSessionAudioEnabled", "getControlOperation"];
    for (const [index, name] of names.entries()) {
      expect(source.methods.find((m: any) => m.name === `cast.${name}`)).toMatchObject({ id: 0x1615 + index, bitOffset: 20 + index, status: "draft" });
    }
    const events = ["receiversChanged", "sessionsChanged", "presentationChanged", "audioPolicyChanged", "controlOperationChanged"];
    for (const [index, name] of events.entries()) {
      const event = source.events.find((e: any) => e.name === `cast.${name}`);
      expect(event).toMatchObject({ id: 0x160D + index, bitOffset: 12 + index });
      expect(source.types[event.event_schema].fields[0]).toMatchObject({ name: "context", required: true, type: "CastControlEventContext" });
    }
    expect(source.methods.find((m: any) => m.name === "cast.getStatus").errors).toContain("NOT_SUPPORTED");
    expect(source.methods.find((m: any) => m.name === "cast.stopPresentation").capabilities).toEqual(["cast.presentation", "cast.controlOperations"]);
    expect(source.methods.find((m: any) => m.name === "cast.setSessionAudioEnabled").capabilities).toEqual(["cast.audioPolicy", "cast.controlOperations"]);
  });
  it("retains every legacy identifier, field shape and enum from the exact baseline", () => {
    const baseline = JSON.parse(fs.readFileSync(path.join(root, "conformance/fixtures/cast-control/legacy-baseline.json"), "utf8"));
    for (const group of ["methods", "events", "capabilities"]) {
      for (const old of baseline[group]) {
        const current = source[group].find((item: any) => item.name === old.name);
        expect(current, `${group}/${old.name}`).toBeDefined();
        // getStatus alone adds a new error binding; all original bindings survive.
        const { errors, ...facts } = old;
        expect(current).toMatchObject(facts);
        if (errors) expect(current.errors).toEqual(expect.arrayContaining(errors));
      }
    }
    for (const [name, schema] of Object.entries(baseline.types) as Array<[string, any]>) {
      for (const field of schema.fields) expect(source.types[name].fields.find((f: any) => f.name === field.name), `${name}.${field.name}`).toMatchObject(field);
    }
    expect(source.methods.filter((m: any) => m.name === "cast.setPresentationSettings")).toHaveLength(1);
    expect(source.events.some((e: any) => e.name === "cast.statusChanged")).toBe(false);
  });
});
