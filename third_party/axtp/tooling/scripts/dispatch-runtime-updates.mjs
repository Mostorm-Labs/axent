#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, "..", "..");
const repositoryConfigPath = path.join(repoRoot, "tooling", "release", "runtime-repositories.json");
const specRepositoryDefault = "https://github.com/Mostorm-Labs/axtp";
const eventType = "axtp_spec_released";

function usage() {
  console.error(`Usage: node tooling/scripts/dispatch-runtime-updates.mjs --spec-tag spec/vMAJOR.MINOR.PATCH --spec-version MAJOR.MINOR.PATCH [--spec-commit SHA] [--spec-repository URL] [--dry-run]`);
}

function parseArgs(argv) {
  const out = {
    dryRun: false,
    specRepository: specRepositoryDefault,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--dry-run") {
      out.dryRun = true;
      continue;
    }
    const next = argv[i + 1];
    if (!next || next.startsWith("--")) {
      throw new Error(`Missing value for ${arg}`);
    }
    if (arg === "--spec-tag") out.specTag = next;
    else if (arg === "--spec-version") out.specVersion = next;
    else if (arg === "--spec-commit") out.specCommit = next;
    else if (arg === "--spec-repository") out.specRepository = next;
    else throw new Error(`Unknown argument: ${arg}`);
    i += 1;
  }
  return out;
}

function validateInput(options) {
  if (!options.specTag || !/^spec\/v[0-9]+\.[0-9]+\.[0-9]+$/.test(options.specTag)) {
    throw new Error("spec-tag must match spec/vMAJOR.MINOR.PATCH");
  }
  if (!options.specVersion || !/^[0-9]+\.[0-9]+\.[0-9]+$/.test(options.specVersion)) {
    throw new Error("spec-version must match MAJOR.MINOR.PATCH");
  }
  const expectedTag = `spec/v${options.specVersion}`;
  if (options.specTag !== expectedTag) {
    throw new Error(`spec-tag must equal spec/v<spec-version>; expected ${expectedTag}`);
  }
}

function resolveSpecCommit(options) {
  if (options.specCommit) return options.specCommit;
  const refs = [`refs/tags/${options.specTag}^{}`, `refs/tags/${options.specTag}`];
  for (const ref of refs) {
    let output = "";
    try {
      output = execFileSync("git", ["ls-remote", `${options.specRepository}.git`, ref], {
        encoding: "utf8",
        stdio: ["ignore", "pipe", "ignore"],
      }).trim();
    } catch {
      output = "";
    }
    const commit = output.split(/\s+/)[0];
    if (commit) return commit;
  }
  throw new Error(`Could not resolve ${options.specTag} from ${options.specRepository}`);
}

function loadRepositories() {
  const data = JSON.parse(fs.readFileSync(repositoryConfigPath, "utf8"));
  if (!Array.isArray(data.repositories) || data.repositories.length === 0) {
    throw new Error(`${path.relative(repoRoot, repositoryConfigPath)} must contain a non-empty repositories array`);
  }
  for (const repository of data.repositories) {
    if (!/^[-A-Za-z0-9_.]+\/[-A-Za-z0-9_.]+$/.test(repository)) {
      throw new Error(`Invalid repository entry: ${repository}`);
    }
  }
  return data.repositories;
}

async function dispatch(repository, payload, token) {
  const response = await fetch(`https://api.github.com/repos/${repository}/dispatches`, {
    method: "POST",
    headers: {
      Accept: "application/vnd.github+json",
      Authorization: `Bearer ${token}`,
      "X-GitHub-Api-Version": "2022-11-28",
      "Content-Type": "application/json",
    },
    body: JSON.stringify(payload),
  });
  if (!response.ok) {
    const body = await response.text();
    throw new Error(`Dispatch to ${repository} failed with ${response.status}: ${body}`);
  }
}

try {
  const options = parseArgs(process.argv.slice(2));
  validateInput(options);
  const specCommit = resolveSpecCommit(options);
  const repositories = loadRepositories();
  const payload = {
    event_type: eventType,
    client_payload: {
      spec_tag: options.specTag,
      spec_version: options.specVersion,
      spec_repository: options.specRepository,
      spec_commit: specCommit,
    },
  };

  if (!options.dryRun && !process.env.AXTP_RUNTIME_DISPATCH_TOKEN) {
    throw new Error("Missing AXTP_RUNTIME_DISPATCH_TOKEN secret for repository_dispatch");
  }

  for (const repository of repositories) {
    if (options.dryRun) {
      console.log(`Dry run: would dispatch ${eventType} to ${repository}`);
      console.log(JSON.stringify(payload));
      continue;
    }
    console.log(`Dispatching ${eventType} to ${repository}`);
    await dispatch(repository, payload, process.env.AXTP_RUNTIME_DISPATCH_TOKEN);
  }
} catch (error) {
  usage();
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
