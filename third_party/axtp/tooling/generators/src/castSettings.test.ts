import fs from "node:fs";
import path from "node:path";
import { describe, expect, it } from "vitest";
import YAML from "yaml";
import { castSettingsEncodingVectors } from "./emitters/castControlVectors.js";
import { loadProtocolSources } from "./sourceLoader.js";
import { decodeScalarObject, runControlCase } from "../../../conformance/reference/cast-control.mjs";
import { CastSettingsModel } from "../../../conformance/reference/cast-settings.mjs";

const root = path.resolve("../..");
const yaml = (p: string) => YAML.parse(fs.readFileSync(path.join(root, p), "utf8"));
const domain = yaml("contract/registry/domains/cast/domain.yaml");
const fixture = yaml("conformance/fixtures/device_profiles/cast_settings_receiver.yaml");
const codes = Object.fromEntries(yaml("contract/registry/error/error_code.yaml").errors.map((e: any) => [e.name, e.id]));
const c12 = yaml("conformance/cases/castcontrol/c12_settings.yaml");

describe("P1 adoption and reference evidence", () => {
  it("preserves P0 IDs and bindings except adopted unpublished layout replacement", () => {
    const old = JSON.parse(fs.readFileSync(path.join(root, "conformance/fixtures/cast-control/p0-baseline.json"), "utf8"));
    for (const group of ["methods", "events", "capabilities"]) for (const fact of old[group]) {
      expect(domain[group].find((f: any) => f.name === fact.name)).toMatchObject(fact);
    }
    for (const [name, type] of Object.entries(old.types) as Array<[string, any]>) for (const field of type.fields) {
      const current = domain.types[name].fields.find((f: any) => f.name === field.name);
      if (name === "CastPresentationState" && field.name === "layoutMode") {
        expect(current).toMatchObject({id:field.id,type:"enum",enum:["grid","presentation"],required:true});
      } else if (name === "CastControlOperation" && field.name === "kind") {
        const { enum: values, ...shape } = field;
        expect(current).toMatchObject(shape); expect(current.enum.slice(0, values.length)).toEqual(values);
        expect(current.enum.slice(values.length)).toEqual(["setPresentationSettings","setPresentationFocus"]);
      } else expect(current).toMatchObject(field);
    }
  });
  it("binds the one new method/event/capability and append-only optional fields", () => {
    expect(domain.methods.find((m: any) => m.name === "cast.setPresentationSettings")).toMatchObject({ id: 0x1619, bitOffset: 24, capabilities: ["cast.presentationSettings", "cast.controlOperations"] });
    expect(domain.events.find((e: any) => e.name === "cast.presentationSettingsChanged")).toMatchObject({ id: 0x1612, bitOffset: 17, event_schema: "CastPresentationSettingsChangedEvent", capabilities: ["cast.presentationSettings"] });
    expect(domain.capabilities.find((c: any) => c.name === "cast.presentationSettings")).toMatchObject({ id: 0x160B, schema: "CastPresentationSettingsCapability" });
    for (const [schema, field, id] of [["CastStatus", "presentationSettings", 15], ["CastControlCapabilities", "presentationSettings", 10], ["CastControlOperation", "settingsResult", 14]]) {
      expect(domain.types[schema].fields.find((f: any) => f.name === field)).toMatchObject({ id, required: false });
    }
    expect(domain.types.CastSettingsEventContext.fields.map((f: any) => f.name)).toEqual(["controlVersion", "runtimeInstanceId", "settingsRevision", "settingsEventSequence"]);
    expect(domain.types.CastSettingsEventContext.fields.every((f: any) => f.required)).toBe(true);
    expect(domain.types.CastPresentationSettingsChangedEvent.fields.map((f: any) => f.name)).toEqual(["settingsContext", "settings"]);
  });
  it("maps every protocol obligation PS-01 to PS-12 to executable scenarios", () => {
    const expected = Array.from({ length: 12 }, (_, i) => `PS-${String(i + 1).padStart(2, "0")}`);
    expect(c12.given.obligations).toEqual(expected);
    expect([...new Set(c12.scenarios.map((s: any) => s.given.obligation))].sort()).toEqual(expected);
    for (const s of c12.scenarios) expect(s.steps.every((step: any) => step.control_model && Object.keys(step.control_model.expect).length)).toBe(true);
  });
  it("uses strings in JSON and TLV for both independent settings counters", async () => {
    const source = await loadProtocolSources(root), vectors = castSettingsEncodingVectors(source)!;
    expect(vectors.fields).toEqual({ controlVersion: 1, runtimeInstanceId: 2, settingsRevision: 3, settingsEventSequence: 4 });
    const model = new CastSettingsModel(fixture, domain.types, codes);
    for (const pair of vectors.pairs) {
      const decoded = decodeScalarObject(domain.types, "CastSettingsEventContext", pair.tlv8Hex);
      expect(decoded).toEqual(pair.json);
      expect(model.action("validate_value", { schema: "CastSettingsEventContext", value: decoded }).valid, `TLV ${pair.name}`).toBe(pair.accepted);
      expect(model.action("validate_value", { schema: "CastSettingsEventContext", value: pair.json }).valid, `JSON ${pair.name}`).toBe(pair.accepted);
    }
    const upper = vectors.pairs.find((p) => p.name === "uint64_max")!;
    expect(upper.tlv8Hex).toContain("0314" + Buffer.from("18446744073709551615").toString("hex").toUpperCase());
  });
  it("detects a save-ordering implementation defect through the PS-06 oracle", () => {
    class BrokenSaveOrder extends CastSettingsModel {
      completeStore(input: any) {
        if (input.storeFailure && this.pendingWrite) {
          const item = this.pendingWrite.item, field = item.operation.settingsResult.field;
          this.settings.configured[field] = item.params.patch[field];
        }
        return super.completeStore(input);
      }
    }
    const scenario = c12.scenarios.find((s: any) => s.name === "ps06_disk_full");
    expect(() => runControlCase({ ...c12, scenarios: [scenario] }, fixture, domain.types, codes, BrokenSaveOrder)).toThrow(/ps06_disk_full/);
  });
  it("returns global restart-only results without a forged presentation target or control commit", () => {
    const model = new CastSettingsModel(fixture, domain.types, codes);
    model.action("settings_admit", { params: { patch: { capacity: 2 }, persist: false } });
    const { operation } = model.action("settings_execute", {});
    expect(operation).not.toHaveProperty("target"); expect(operation).not.toHaveProperty("appliedRevision");
    expect(operation.settingsResult).toMatchObject({ configurationStatus: "committed", applicationStatus: "pendingRestart", settingsRevision: "8" });
    expect(model.action("validate_value", { schema: "CastControlOperation", value: { ...operation, target: fixture.snapshot.presentation.slots[0].target } }).valid).toBe(false);
    const onlyP0 = model.action("settings_capture", { p0Only: true });
    expect(onlyP0.result).not.toHaveProperty("presentationSettings");
    const onlySettings = model.action("settings_capture", { settingsOnly: true });
    expect(onlySettings.result).toHaveProperty("controlSync");
    expect(onlySettings.result).not.toHaveProperty("sessions");
  });
});
