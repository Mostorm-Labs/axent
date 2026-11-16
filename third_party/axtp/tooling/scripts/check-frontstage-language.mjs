#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(process.argv[2] ?? process.cwd());

const frontstageRoots = [
  "README.md",
  "docs/README.md",
  "docs/guides",
  "docs/product",
  "specs",
];

const forbiddenHeadings = [
  "Product / Architecture Guide",
  "Runtime / SDK Guide",
  "Testing / Conformance Guide",
  "Legacy Migration Guide",
  "Protocol Maintainer Guide",
  "AXTP Product Domain Status",
  "AXTP Protocol Draft Health",
  "Summary",
  "Domain Health Matrix",
  "Product Priority Tuning Queue",
  "Mechanical Example Tuning Queue",
  "Front Door",
  "Implementation Contract",
  "Background Workspace",
  "Generated Files",
  "Generated 文件",
  "AXTP 产品 Domain 状态",
  "Domain 状态矩阵",
];

const forbiddenTableHeaders = [
  "| Metric | Count |",
  "| Domain | Priority | Drafts |",
  "| Domain | 草案数 | Review | Generated |",
  "| Generated 路径 |",
  "| File | Priority | Domain |",
  "| File | Domain | Methods |",
  "| Owner path |",
  "| Parser |",
  "| Next Step |",
];

const forbiddenLeadPatterns = [
  /^This directory is\b/,
  /^This page is\b/,
  /^This report is\b/,
  /^These files are\b/,
  /^Do not browse\b/,
];

function exists(relative) {
  return fs.existsSync(path.join(root, relative));
}

function walk(relative) {
  const absolute = path.join(root, relative);
  if (!fs.existsSync(absolute)) return [];
  const stat = fs.statSync(absolute);
  if (stat.isFile()) return relative.endsWith(".md") ? [relative] : [];
  const out = [];
  for (const entry of fs.readdirSync(absolute, { withFileTypes: true })) {
    const child = path.join(relative, entry.name).replaceAll(path.sep, "/");
    if (entry.isDirectory()) out.push(...walk(child));
    else if (entry.isFile() && entry.name.endsWith(".md")) out.push(child);
  }
  return out.sort();
}

const files = frontstageRoots.flatMap((relative) => (exists(relative) ? walk(relative) : []));
const violations = [];

for (const relative of files) {
  const text = fs.readFileSync(path.join(root, relative), "utf8");
  const lines = text.split(/\r?\n/);
  let inFence = false;

  lines.forEach((line, index) => {
    if (line.startsWith("```")) {
      inFence = !inFence;
      return;
    }
    if (inFence) return;

    const heading = /^(#{1,6})\s+(.+?)\s*$/.exec(line);
    if (heading && forbiddenHeadings.includes(heading[2])) {
      violations.push([relative, index + 1, `英文前台标题应改成中文：${heading[2]}`]);
    }

    for (const header of forbiddenTableHeaders) {
      if (line.includes(header)) {
        violations.push([relative, index + 1, `英文表头应改成中文：${header}`]);
      }
    }

    for (const pattern of forbiddenLeadPatterns) {
      if (pattern.test(line)) {
        violations.push([relative, index + 1, `前台说明应使用中文优先表达：${line}`]);
      }
    }
  });

  if (
    relative === "docs/product/protocol-draft-health.md" &&
    /\bworkspace\/(?:protocol|business|flows|legacy-migration|registry-planning|runtime)\//.test(text)
  ) {
    violations.push([relative, 1, "release artifact 内的草案健康摘要不得列出 workspace/ 后台文件路径"]);
  }
}

if (violations.length > 0) {
  console.error("[FAIL] frontstage language check failed");
  for (const [file, line, message] of violations) {
    console.error(`- ${file}:${line}: ${message}`);
  }
  process.exit(1);
}

console.log("[OK] frontstage docs use Chinese-first navigation language");
