#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(process.argv[2] ?? process.cwd());
const ignoredDirs = new Set([".git", "node_modules", "dist", "outputs"]);
const textExtensions = new Set([
  ".csv",
  ".json",
  ".md",
  ".sh",
  ".toml",
  ".txt",
  ".yaml",
  ".yml",
]);
const errors = [];
const legacySpecsDirs = ["0-principles", "1-core", "2-registry", "3-codec", "4-tooling"];
const legacySpecsPattern = new RegExp(`specs/(?:${legacySpecsDirs.join("|")})/[^\\s\`"'<>),\\]]+`, "g");
const legacyRegistryAppendixPattern = new RegExp(`specs/${"2-registry"}/appendix/[^\\s\`"'<>),\\]]+`, "g");
const unprefixedSpecs = ["glossary", "contract", "core", "registry", "codec", "tooling"];
const unprefixedSpecsPattern = new RegExp(`specs/(?:${unprefixedSpecs.join("|")})\\.md`, "g");
const removedLegacyPathPattern =
  /workspace\/(?:business|flows)\/(?:cast-reciever-uxplay|cast-rxtx-paring)\.md/g;
const activeRepoPathPattern =
  /(?:docs\/(?!archive\/)[^\s`"'<>),\]]+\.(?:md|yaml|yml|json)|workspace\/(?:business|flows|legacy-migration\/classification|legacy-migration\/generated|protocol|registry-planning|runtime)\/[^\s`"'<>),\]]+\.(?:csv|json|md|pdf|txt|xlsx|yaml|yml)|tooling\/(?:release|scripts|skills)\/[^\s`"'<>),\]]+\.(?:json|md|mjs|sh|yaml|yml)|contract\/registry\/domains\/[A-Za-z0-9_.-]+\/domain\.yaml|specs\/[^\s`"'<>),\]]+\.md|release\/[^\s`"'<>),\]]+\.md|conformance\/[^\s`"'<>),\]]+\.(?:json|md|yaml|yml))/g;

function walk(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const entryPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      if (!ignoredDirs.has(entry.name)) out.push(...walk(entryPath));
      continue;
    }
    if (!entry.isFile()) continue;
    const ext = path.extname(entry.name).toLowerCase();
    if (textExtensions.has(ext) || ["README", "CHANGELOG", "CODEOWNERS", "LICENSE"].includes(entry.name)) {
      out.push(entryPath);
    }
  }
  return out;
}

function stripDecorators(raw) {
  return raw
    .replace(/^['"`(<\[]+/, "")
    .replace(/[>'"`)\].,;，。；、:]+$/, "")
    .split("#")[0];
}

function report(file, raw, reason) {
  errors.push(`${path.relative(root, file)} -> ${raw}: ${reason}`);
}

function checkExists(file, raw) {
  const candidate = stripDecorators(raw);
  if (!candidate || candidate.includes("*") || candidate.includes("<") || candidate.includes(">")) return;
  if (isLegacyMigrationCandidatePath(file, candidate)) return;
  const evidencePath = candidate.includes(":")
    ? candidate.slice(0, candidate.indexOf(":"))
    : candidate;
  const target = path.join(root, evidencePath);
  if (!fs.existsSync(target)) report(file, raw, "missing referenced path");
}

function isLegacyMigrationCandidatePath(file, candidate) {
  const relative = path.relative(root, file);
  return (
    relative.startsWith(path.join("workspace", "legacy-migration") + path.sep) &&
    /^contract\/registry\/domains\/[A-Za-z0-9_.-]+\/domain\.yaml$/.test(candidate)
  );
}

const files = walk(root);
for (const file of files) {
  const relative = path.relative(root, file);
  if (relative.startsWith(`docs${path.sep}archive${path.sep}`)) continue;
  if (relative.startsWith(path.join("workspace", "legacy-migration", "evidence") + path.sep)) continue;
  if (relative === path.join("release", "CHANGELOG.md")) continue;
  const text = fs.readFileSync(file, "utf8");

  for (const match of text.matchAll(/docs\/workspace\/[^\s`"'<>),\]]+/g)) {
    report(file, match[0], "legacy docs/workspace path is no longer valid");
  }

  for (const match of text.matchAll(legacySpecsPattern)) {
    report(file, match[0], "legacy nested specs path is no longer valid");
  }

  for (const match of text.matchAll(legacyRegistryAppendixPattern)) {
    report(file, match[0], "registry planning appendix moved out of specs");
  }

  for (const match of text.matchAll(unprefixedSpecsPattern)) {
    report(file, match[0], "current specs files must use numeric prefixes");
  }

  for (const match of text.matchAll(removedLegacyPathPattern)) {
    report(file, match[0], "removed legacy misspelled path must not be linked from active docs");
  }

  for (const match of text.matchAll(/workspace\/protocol\/[^\s`"'<>),\]]+\.md(?:#[^\s`"'<>),\]]+)?/g)) {
    checkExists(file, match[0]);
  }

  for (const match of text.matchAll(/workspace\/legacy-migration\/evidence\/[^\s`"'<>),\]]+\.(?:md|pdf|xlsx)(?::[^\s`"'<>),\]]+)?/g)) {
    checkExists(file, match[0]);
  }

  for (const match of text.matchAll(activeRepoPathPattern)) {
    checkExists(file, match[0]);
  }
}

if (errors.length > 0) {
  console.error(`[FAIL] plain-text repository paths failed: ${errors.length}`);
  for (const error of errors) console.error(`- ${error}`);
  process.exit(1);
}

console.log("[OK] plain-text repository paths resolved");
