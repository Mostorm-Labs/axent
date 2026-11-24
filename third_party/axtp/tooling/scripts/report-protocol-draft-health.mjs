#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const args = process.argv.slice(2);
let rootArg = null;
let writePath = null;
let checkPath = null;
let json = false;

for (let index = 0; index < args.length; index += 1) {
  const arg = args[index];
  if (arg === "--write") {
    writePath = args[index + 1] ?? "docs/product/protocol-draft-health.md";
    index += 1;
  } else if (arg === "--check") {
    checkPath = args[index + 1] ?? "docs/product/protocol-draft-health.md";
    index += 1;
  } else if (arg === "--json") {
    json = true;
  } else if (!arg.startsWith("--") && rootArg === null) {
    rootArg = arg;
  } else {
    usage(2);
  }
}

const root = path.resolve(rootArg ?? process.cwd());
const protocolRoot = path.join(root, "workspace", "protocol");
const generatedPath = path.join(root, "contract", "generated", "protocol.json");
const domainStatusPath = path.join(root, "docs", "product", "domain-status.md");

function usage(exitCode) {
  console.error("Usage: node tooling/scripts/report-protocol-draft-health.mjs [root] [--write PATH] [--check PATH] [--json]");
  process.exit(exitCode);
}

function walk(dir) {
  const out = [];
  if (!fs.existsSync(dir)) return out;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const entryPath = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walk(entryPath));
    else if (entry.isFile() && entry.name.endsWith(".md")) out.push(entryPath);
  }
  return out.sort();
}

function readFrontmatter(file) {
  const text = fs.readFileSync(file, "utf8").replace(/\r\n/g, "\n");
  if (!text.startsWith("---\n")) return { text, data: {} };
  const end = text.indexOf("\n---", 4);
  if (end < 0) return { text, data: {} };
  const data = {};
  for (const line of text.slice(4, end).trim().split(/\r?\n/)) {
    const match = /^([A-Za-z0-9_-]+):\s*(.*)$/.exec(line);
    if (match) data[match[1]] = match[2].trim();
  }
  return { text, data };
}

function boolValue(value) {
  return value === "true";
}

function countMatches(text, pattern) {
  return (text.match(pattern) ?? []).length;
}

function countJsonFences(text) {
  const invalid = [];
  let count = 0;
  const regex = /```json\n([\s\S]*?)\n```/g;
  let match;
  while ((match = regex.exec(text)) !== null) {
    count += 1;
    try {
      JSON.parse(match[1]);
    } catch (error) {
      invalid.push(error.message);
    }
  }
  return { count, invalid: invalid.length };
}

function countGenericExampleHints(text) {
  const patterns = [
    /"state": \{\n\s+"target": "default",\n\s+"status": "ok"\n\s+\}/g,
    /"config": \{\n\s+"mode": "auto"\n\s+\}/g,
    /"params": \{\n\s+"target": "default",\n\s+"sections": \[\n\s+"summary"\n\s+\]\n\s+\}/g,
    /"result": \{\n\s+"state": \{\n\s+"target": "default",\n\s+"status": "ok"\n\s+\}\n\s+\}/g,
  ];
  return patterns.reduce((total, pattern) => total + countMatches(text, pattern), 0);
}

function countGenericOpenQuestions(text) {
  return countMatches(
    text,
    /^\| `[^`]+` 采纳前还需确认哪些 schema、事件和 conformance 细节？ \| schema \/ conformance \| 按本文 method\/event 示例逐项确认字段、边界错误和测试用例；确认后再进入 registry review。 \| open \|$/gm,
  );
}

function countGenericFieldPlaceholders(text) {
  const patterns = [
    /^\| `sections` \| string\[\] \| no \| section name array \| omitted \| 需要返回的字段段；省略表示默认摘要。 \|$/gm,
    /^\| `constraints` \| object \| no \| feature-specific \| omitted \| 设备能力限制、范围、模式或策略摘要。 \|$/gm,
    /^\| `target` \| string \| no \| target id \| `default` \| 查询对象；具体 target 集合由 capability 声明。 \|$/gm,
    /^\| `state` \| object \| yes \| see schema \| none \| 当前状态、配置或查询结果。 \|$/gm,
    /^\| `sampledAt` \| string timestamp \| no \| RFC 3339 \| omitted \| 结果采样时间。 \|$/gm,
    /正式取值范围由本 feature capability 或 schema 收敛/g,
    /采纳前应展开为正式 schema 字段/g,
    /用于缓存刷新、事件丢失后的校准或 UI 时间戳/g,
    /success 示例中的 feature-specific 字段/g,
    /见 success 示例/g,
    /capability \/ supportedTargets \/ constraints/g,
  ];
  return patterns.reduce((total, pattern) => total + countMatches(text, pattern), 0);
}

function generatedDomainCounts() {
  const generated = JSON.parse(fs.readFileSync(generatedPath, "utf8"));
  const counts = new Map();
  for (const item of [...(generated.methods ?? []), ...(generated.events ?? [])]) {
    counts.set(item.domain, (counts.get(item.domain) ?? 0) + 1);
  }
  return counts;
}

function productPriorities() {
  const priorities = new Map();
  if (!fs.existsSync(domainStatusPath)) return priorities;
  const text = fs.readFileSync(domainStatusPath, "utf8");
  const rows = text.matchAll(/^\|\s*([a-z][a-z0-9]*)\s*\|\s*\d+\s*\|\s*[^|]*\|\s*\d+\s*\|\s*([^|]+?)\s*\|/gm);
  for (const match of rows) {
    priorities.set(match[1], match[2].trim());
  }
  return priorities;
}

function priorityRank(priority) {
  if (!priority) return 99;
  if (/P0/.test(priority)) return 0;
  const numbered = priority.match(/P(\d+)(b)?/);
  if (numbered) return Number(numbered[1]) + (numbered[2] ? 0.5 : 0);
  if (/旁路高覆盖/.test(priority)) return 1.8;
  if (/待排期/.test(priority)) return 20;
  return 50;
}

function emptyDomainStats(domain) {
  return {
    domain,
    priority: "",
    priorityRank: 99,
    drafts: 0,
    generatedDrafts: 0,
    generatedFacts: 0,
    methods: 0,
    compactExamples: 0,
    exampleGaps: 0,
    jsonExamples: 0,
    invalidJsonExamples: 0,
    genericExampleHints: 0,
    genericOpenQuestions: 0,
    genericFieldPlaceholders: 0,
    reviewAsk: 0,
    reviewDraft: 0,
    reviewFix: 0,
    reviewBlocker: 0,
    tbdAfterAdoption: 0,
    lines: 0,
  };
}

function analyze() {
  const generatedCounts = generatedDomainCounts();
  const priorities = productPriorities();
  const domains = new Map();
  const files = [];

  for (const [domain, count] of generatedCounts) {
    const stats = emptyDomainStats(domain);
    stats.priority = priorities.get(domain) ?? "";
    stats.priorityRank = priorityRank(stats.priority);
    stats.generatedFacts = count;
    domains.set(domain, stats);
  }

  for (const file of walk(protocolRoot)) {
    const relative = path.relative(root, file).replaceAll(path.sep, "/");
    if (relative.endsWith("/README.md") || relative.endsWith("/draft-conventions.md")) continue;
    const { text, data } = readFrontmatter(file);
    const domain = data.domain || path.relative(protocolRoot, file).split(path.sep)[0];
    const stats = domains.get(domain) ?? emptyDomainStats(domain);
    stats.priority ||= priorities.get(domain) ?? "";
    stats.priorityRank = priorityRank(stats.priority);
    domains.set(domain, stats);

    const methods = countMatches(text, /^### 3\.\d+ `/gm);
    const compactExamples = countMatches(text, /^#### 3\.\d+\.\d+ d block 示例$/gm);
    const jsonExamples = countJsonFences(text);
    const fileStats = {
      file: relative,
      domain,
      priority: priorities.get(domain) ?? "",
      priorityRank: priorityRank(priorities.get(domain) ?? ""),
      feature: data.feature ?? "",
      status: data.status ?? "",
      generated: boolValue(data.generated),
      contract: boolValue(data.contract),
      methods,
      compactExamples,
      exampleGaps: Math.max(0, methods - compactExamples),
      reviewAsk: countMatches(text, /\[REVIEW-ASK\]/g),
      reviewDraft: countMatches(text, /\[REVIEW-DRAFT\]/g),
      reviewFix: countMatches(text, /\[REVIEW-FIX\]/g),
      reviewBlocker: countMatches(text, /\[REVIEW-BLOCKER\]/g),
      tbdAfterAdoption: countMatches(text, /TBD after adoption/g),
      genericExampleHints: countGenericExampleHints(text),
      genericOpenQuestions: countGenericOpenQuestions(text),
      genericFieldPlaceholders: countGenericFieldPlaceholders(text),
      jsonExamples: jsonExamples.count,
      invalidJsonExamples: jsonExamples.invalid,
      lines: text.split(/\r?\n/).length,
    };

    stats.drafts += 1;
    stats.generatedDrafts += fileStats.generated ? 1 : 0;
    stats.methods += fileStats.methods;
    stats.compactExamples += fileStats.compactExamples;
    stats.exampleGaps += fileStats.exampleGaps;
    stats.jsonExamples += fileStats.jsonExamples;
    stats.invalidJsonExamples += fileStats.invalidJsonExamples;
    stats.genericExampleHints += fileStats.genericExampleHints;
    stats.genericOpenQuestions += fileStats.genericOpenQuestions;
    stats.genericFieldPlaceholders += fileStats.genericFieldPlaceholders;
    stats.reviewAsk += fileStats.reviewAsk;
    stats.reviewDraft += fileStats.reviewDraft;
    stats.reviewFix += fileStats.reviewFix;
    stats.reviewBlocker += fileStats.reviewBlocker;
    stats.tbdAfterAdoption += fileStats.tbdAfterAdoption;
    stats.lines += fileStats.lines;
    files.push(fileStats);
  }

  return {
    domains: [...domains.values()].sort((a, b) => a.domain.localeCompare(b.domain)),
    files,
  };
}

function reviewSummary(stats) {
  return `ASK ${stats.reviewAsk} / DRAFT ${stats.reviewDraft} / FIX ${stats.reviewFix} / BLOCKER ${stats.reviewBlocker}`;
}

function focus(stats) {
  if (stats.exampleGaps > 0) return "补 method 示例覆盖";
  if (stats.invalidJsonExamples > 0) return "修 JSON 示例语法";
  if (stats.reviewBlocker > 0 || stats.reviewFix > 0) return "先处理 blocker/fix";
  if (stats.genericOpenQuestions > 0) return "删除模板 open question";
  if (stats.reviewAsk > 0 && stats.genericExampleHints > 0) return "确认 REVIEW-ASK + 调真实业务示例";
  if (stats.reviewAsk > 0) return "确认 REVIEW-ASK";
  if (stats.genericExampleHints > 0) return "调真实业务示例";
  if (stats.generatedFacts > 0) return "维护 generated 合同";
  return "可排采纳评审";
}

function renderMarkdown(report) {
  const total = report.domains.reduce(
    (acc, domain) => {
      for (const key of Object.keys(acc)) acc[key] += domain[key] ?? 0;
      return acc;
    },
    {
      drafts: 0,
      generatedDrafts: 0,
      generatedFacts: 0,
      methods: 0,
      compactExamples: 0,
      exampleGaps: 0,
      jsonExamples: 0,
      invalidJsonExamples: 0,
      genericExampleHints: 0,
      genericOpenQuestions: 0,
      genericFieldPlaceholders: 0,
      reviewAsk: 0,
      reviewDraft: 0,
      reviewFix: 0,
      reviewBlocker: 0,
      tbdAfterAdoption: 0,
      lines: 0,
    },
  );

  return `# AXTP 协议草案健康度

本页是产品和协议维护者查看草案健康度、示例质量和待确认问题密度的报告。它不是 runtime 实现合同；可实现事实仍以 \`contract/**\`、\`specs/**\` 和 \`conformance/**\` 为准。

本页是 release artifact-safe 摘要：只展示 domain 级计数和治理状态，不列后台 \`workspace/\` 文件路径。维护者需要文件级队列时，可运行脚本的 \`--json\` 模式在本地分析。

本页由脚本生成并校验：

\`\`\`bash
node tooling/scripts/report-protocol-draft-health.mjs --write docs/product/protocol-draft-health.md
node tooling/scripts/report-protocol-draft-health.mjs --check docs/product/protocol-draft-health.md
\`\`\`

## 摘要

| 指标 | 数量 |
|---|---:|
| 草案文件 | ${total.drafts} |
| 已生成草案文件 | ${total.generatedDrafts} |
| 已生成 method/event 事实 | ${total.generatedFacts} |
| Method 小节 | ${total.methods} |
| 紧凑 method 示例 | ${total.compactExamples} |
| Method 示例缺口 | ${total.exampleGaps} |
| JSON 示例 | ${total.jsonExamples} |
| 无效 JSON 示例 | ${total.invalidJsonExamples} |
| 模板化示例提示 | ${total.genericExampleHints} |
| 模板化开放问题 | ${total.genericOpenQuestions} |
| 模板化字段占位 | ${total.genericFieldPlaceholders} |
| REVIEW-ASK | ${total.reviewAsk} |
| REVIEW-DRAFT | ${total.reviewDraft} |
| REVIEW-FIX | ${total.reviewFix} |
| REVIEW-BLOCKER | ${total.reviewBlocker} |
| 采纳后占位残留 | ${total.tbdAfterAdoption} |

## 领域健康矩阵

| 领域 | 优先级 | 草案 | 已生成草案 | 已生成事实 | Method 数 | 示例覆盖 | Review 标记 | 模板示例 | 模板问题 | 建议动作 |
|---|---|---:|---:|---:|---:|---:|---|---:|---:|---|
${report.domains
  .map((domain) => `| ${domain.domain} | ${domain.priority || "未排期"} | ${domain.drafts} | ${domain.generatedDrafts} | ${domain.generatedFacts} | ${domain.methods} | ${domain.compactExamples}/${domain.methods} | ${reviewSummary(domain)} | ${domain.genericExampleHints} | ${domain.genericOpenQuestions} | ${focus(domain)} |`)
  .join("\n")}
`;
}

const report = analyze();
const rendered = renderMarkdown(report);

if (json) {
  process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
} else if (writePath) {
  const absolute = path.resolve(root, writePath);
  fs.mkdirSync(path.dirname(absolute), { recursive: true });
  fs.writeFileSync(absolute, rendered);
  console.log(`[OK] wrote ${path.relative(root, absolute)}`);
} else if (checkPath) {
  const absolute = path.resolve(root, checkPath);
  const current = fs.existsSync(absolute) ? fs.readFileSync(absolute, "utf8").replace(/\r\n/g, "\n") : "";
  if (current !== rendered) {
    console.error(`[FAIL] ${path.relative(root, absolute)} is out of date`);
    console.error(`Run: node tooling/scripts/report-protocol-draft-health.mjs --write ${path.relative(root, absolute)}`);
    process.exit(1);
  }
  console.log(`[OK] ${path.relative(root, absolute)} is up to date`);
} else {
  process.stdout.write(rendered);
}
