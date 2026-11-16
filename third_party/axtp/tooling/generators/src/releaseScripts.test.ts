import { readFile } from "node:fs/promises";
import path from "node:path";
import { describe, expect, it } from "vitest";

const repoRoot = path.resolve("../..");

describe("release script contracts", () => {
  it("uses a default release artifact version accepted by the artifact builder", async () => {
    const checkReleaseArtifact = await readFile(path.join(repoRoot, "tooling/scripts/check-release-artifact.sh"), "utf8");
    const buildSpecArtifact = await readFile(path.join(repoRoot, "tooling/scripts/build-spec-artifact.sh"), "utf8");

    const defaultVersion = checkReleaseArtifact.match(/^version="\$\{1:-([^}]+)\}"$/m)?.[1];
    const acceptedVersionPattern = buildSpecArtifact.match(/\[\[ ! "\$version" =~ (.+) \]\]/)?.[1];

    expect(defaultVersion).toBeDefined();
    expect(acceptedVersionPattern).toBeDefined();
    expect(defaultVersion).toMatch(new RegExp(acceptedVersionPattern!));
  });
});
