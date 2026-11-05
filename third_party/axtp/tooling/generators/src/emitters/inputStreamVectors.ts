import { pathToFileURL } from "node:url";
import path from "node:path";
import type { SpecModel } from "../models.js";
import { toJsonStable, writeTextFile } from "../util.js";

export async function emitInputStreamVectors(spec: SpecModel, dir: string): Promise<Record<string, string> | undefined> {
  if (!spec.inputStream) return undefined;
  const url = pathToFileURL(path.join(spec.specRoot, "conformance/reference/input-vectors.mjs")).href;
  const { createInputVectors } = await import(url);
  const vectors = createInputVectors(spec.inputStream);
  await writeTextFile(path.join(dir, "input_stream.json"), toJsonStable(vectors));
  return {format:spec.inputStream.format,file:"input_stream.json"};
}
