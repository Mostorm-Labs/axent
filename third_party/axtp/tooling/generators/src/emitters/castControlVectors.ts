import type { SpecModel } from "../models.js";

const decimalSamples: Array<[string, string, boolean]> = [
  ["zero", "0", true], ["beyond_js_precision", "9007199254740993", true],
  ["uint64_max", "18446744073709551615", true], ["negative", "-1", false],
  ["exponent", "1e3", false], ["fraction", "1.5", false], ["leading_zero", "01", false],
  ["overflow", "18446744073709551616", false], ["empty", "", false],
  ["whitespace", " 1", false], ["plus", "+1", false]
];

// Scalar object vectors only: use existing TLV8 string bytes and source field IDs.
// This is a vector emitter, not a new transport or a runtime business codec.
export function castControlEncodingVectors(spec: SpecModel) {
  const schema = spec.schemas.find((item) => item.name === "CastSessionRef");
  if (!schema) return undefined;
  const id = schema.fields.find((field) => field.name === "id");
  const generation = schema.fields.find((field) => field.name === "generation");
  if (id?.type !== "string" || generation?.type !== "string" || generation.maxLength !== 20) {
    throw new Error("CastSessionRef vectors require the adopted string field shapes");
  }
  const scalar = (fieldId: number, value: string) => {
    const bytes = Buffer.from(value, "utf8");
    return Buffer.concat([Buffer.from([fieldId, bytes.length]), bytes]);
  };
  return {
    schema: schema.name, encoding: "json+tlv8", scope: "schema-local object body",
    fields: { id: id.id, generation: generation.id },
    pairs: decimalSamples.map(([name, value, accepted]) => ({
      name, accepted, json: { id: "session-ref", generation: value },
      tlv8Hex: Buffer.concat([scalar(id.id, "session-ref"), scalar(generation.id, value)]).toString("hex").toUpperCase()
    }))
  };
}

export function castSettingsEncodingVectors(spec: SpecModel) {
  const schema = spec.schemas.find((item) => item.name === "CastSettingsEventContext");
  if (!schema) return undefined;
  const fields = Object.fromEntries(schema.fields.map((field) => [field.name, field.id]));
  for (const name of ["settingsRevision", "settingsEventSequence"]) {
    const field = schema.fields.find((f) => f.name === name);
    if (field?.type !== "string" || field.maxLength !== 20) throw new Error(`Settings vectors require string ${name}`);
  }
  const uuid = "11111111-1111-4111-8111-111111111111";
  return { schema: schema.name, encoding: "json+tlv8", scope: "schema-local object body", fields,
    pairs: decimalSamples.map(([name, value, accepted]) => {
      const json = { controlVersion: 1, runtimeInstanceId: uuid, settingsRevision: value, settingsEventSequence: value };
      const body = schema.fields.map((field) => {
        const bytes = field.name === "controlVersion" ? Buffer.from([0, 0, 0, 1]) : Buffer.from(json[field.name as keyof typeof json] as string, "utf8");
        return Buffer.concat([Buffer.from([field.id, bytes.length]), bytes]);
      });
      return { name, accepted, json, tlv8Hex: Buffer.concat(body).toString("hex").toUpperCase() };
    }) };
}
