#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(process.argv[2] ?? process.cwd());
const ignoredDirs = new Set([".git", "node_modules", "dist", "outputs"]);
const ignoredPathSegments = [
  ["docs", "legacy-migration", "evidence"],
  ["docs", "workspace", "legacy-migration", "evidence"],
  ["workspace", "legacy-migration", "evidence"],
];
const markdownLink = /!?\[[^\]]*]\(([^)\s]+)(?:\s+"[^"]*")?\)/g;
const broken = [];

function walk(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const entryPath = path.join(dir, entry.name);
    const relativeParts = path.relative(root, entryPath).split(path.sep);
    if (entry.isDirectory()) {
      const ignoredPath = ignoredPathSegments.some((segments) =>
        segments.every((segment, index) => relativeParts[index] === segment)
      );
      if (!ignoredDirs.has(entry.name) && !ignoredPath) out.push(...walk(entryPath));
    } else if (entry.isFile() && entry.name.endsWith(".md")) {
      out.push(entryPath);
    }
  }
  return out;
}

function stripCodeFences(text) {
  const lines = text.split(/\r?\n/);
  let fenced = false;
  return lines.map((line) => {
    if (/^\s*```/.test(line)) {
      fenced = !fenced;
      return "";
    }
    return fenced ? "" : line;
  }).join("\n");
}

function isExternal(target) {
  return /^(https?:|mailto:|tel:)/i.test(target);
}

function isIgnoredTarget(resolved) {
  const relativeParts = path.relative(root, resolved).split(path.sep);
  return ignoredPathSegments.some((segments) =>
    segments.every((segment, index) => relativeParts[index] === segment)
  );
}

for (const file of walk(root)) {
  const text = stripCodeFences(fs.readFileSync(file, "utf8"));
  for (const match of text.matchAll(markdownLink)) {
    const raw = match[1];
    if (!raw || raw.startsWith("#") || isExternal(raw)) continue;
    if (/^\d+$/.test(raw)) continue;
    if (raw.includes("{{") || raw.includes("<")) continue;

    if (/^file:/i.test(raw)) {
      broken.push(`${path.relative(root, file)} -> ${raw}`);
      continue;
    }

    const [target] = raw.split("#");
    if (!target) continue;
    const resolved = path.resolve(path.dirname(file), decodeURIComponent(target));
    if (isIgnoredTarget(resolved)) continue;
    if (!fs.existsSync(resolved)) {
      broken.push(`${path.relative(root, file)} -> ${raw}`);
    }
  }
}

if (broken.length > 0) {
  console.error(`[FAIL] broken local Markdown links: ${broken.length}`);
  for (const item of broken) console.error(`- ${item}`);
  process.exit(1);
}

console.log("[OK] local Markdown links resolved");
