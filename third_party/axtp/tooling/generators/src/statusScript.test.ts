import { execFileSync } from "node:child_process";
import { mkdtemp, mkdir, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { expect, it } from "vitest";

const checker = path.resolve("../scripts/check-protocol-status.mjs");
for (const newline of ["\n", "\r\n"]) {
  it(`reads protocol frontmatter with ${newline === "\n" ? "LF" : "CRLF"} line endings`, async () => {
    const dir = await mkdtemp(path.join(os.tmpdir(), "axtp-status-"));
    try {
      await Promise.all(["contract/generated", "docs/product", "workspace/protocol/cast"].map((part) => mkdir(path.join(dir, part), { recursive: true })));
      await writeFile(path.join(dir, "contract/generated/protocol.json"), JSON.stringify({ methods: [{ name: "cast.stopPresentation", domain: "cast", capabilities: ["cast.presentation"] }] }));
      await writeFile(path.join(dir, "workspace/protocol/cast/cast.presentation.md"), ["---", "status: generated", "contract: true", "generated: true", "domain: cast", "feature: cast.presentation", "---", "# Feature"].join(newline));
      await writeFile(path.join(dir, "docs/product/domain-status.md"), "草案数和已生成数量由 `tooling/scripts/check-protocol-status.mjs` 校验\n| cast | 1 | generated | 1 |\n");
      expect(execFileSync(process.execPath, [checker, dir], { encoding: "utf8" })).toContain("[OK]");
      const healthChecker = path.resolve("../scripts/report-protocol-draft-health.mjs");
      const health = JSON.parse(execFileSync(process.execPath, [healthChecker, dir, "--json"], { encoding: "utf8" }));
      expect(health.domains.find((domain: any) => domain.domain === "cast").generatedDrafts).toBe(1);
    } finally {
      await rm(dir, { recursive: true, force: true });
    }
  });
}
