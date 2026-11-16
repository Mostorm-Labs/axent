#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";

const root = path.resolve(process.argv[2] ?? process.cwd());
const protocolRoot = path.join(root, "workspace", "protocol");
const protocolDraftTemplate = path.join(
  root,
  "tooling",
  "skills",
  "20-draft-business-protocol",
  "references",
  "protocol-draft-template.md",
);
const errors = [];

const bannedLinePatterns = [
  {
    pattern: /^\|\s*`supportedMethods`\s*\|/,
    reason: "generic supportedMethods belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*`supportedEvents`\s*\|/,
    reason: "generic supportedEvents belongs in draft-conventions.md",
  },
  {
    pattern: /supportedMethods\s*\/\s*supportedEvents/,
    reason: "generic supportedMethods/supportedEvents schema overview belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*`sections`\s*\|\s*string\[\]\s*\|\s*no\s*\|\s*section name array\s*\|\s*omitted\s*\|\s*需要返回的字段段；省略表示默认摘要。\s*\|$/,
    reason: "generic sections query filter must be removed or replaced with feature-specific query fields",
  },
  {
    pattern: /^\|\s*`constraints`\s*\|\s*object\s*\|\s*no\s*\|\s*feature-specific\s*\|\s*omitted\s*\|\s*设备能力限制、范围、模式或策略摘要。\s*\|$/,
    reason: "generic constraints capability row must be removed or replaced with concrete capability fields",
  },
  {
    pattern: /^\|\s*`target`\s*\|\s*string\s*\|\s*no\s*\|\s*target id\s*\|\s*`default`\s*\|\s*查询对象；具体 target 集合由 capability 声明。\s*\|$/,
    reason: "generic target row must include a sample value or feature-specific target scope",
  },
  {
    pattern: /^\|\s*`state`\s*\|\s*object\s*\|\s*yes\s*\|\s*see schema\s*\|\s*none\s*\|\s*当前状态、配置或查询结果。\s*\|$/,
    reason: "generic state result row must summarize feature-specific fields from the example or schema",
  },
  {
    pattern: /^\|\s*`sampledAt`\s*\|\s*string timestamp\s*\|\s*no\s*\|\s*RFC 3339\s*\|\s*omitted\s*\|\s*结果采样时间。\s*\|$/,
    reason: "generic sampledAt row must explain why the timestamp matters for this response",
  },
  {
    pattern: /正式取值范围由本 feature capability 或 schema 收敛/,
    reason: "generic target adoption reminder belongs in draft-conventions.md, not every params table",
  },
  {
    pattern: /采纳前应展开为正式 schema 字段/,
    reason: "generic schema adoption reminder belongs in draft-conventions.md, not every result table",
  },
  {
    pattern: /用于缓存刷新、事件丢失后的校准或 UI 时间戳/,
    reason: "long generic sampledAt explanation should be shortened in method result tables",
  },
  {
    pattern: /success 示例中的 feature-specific 字段/,
    reason: "fallback state field summary must be replaced with concrete result fields",
  },
  {
    pattern: /见 success 示例/,
    reason: "field descriptions must explain the field instead of pointing readers back to the example",
  },
  {
    pattern: /capability\s*\/\s*supportedTargets\s*\/\s*constraints/,
    reason: "generic capability/constraints schema overview belongs in draft-conventions.md unless constraints are expanded",
  },
  {
    pattern: /^\|\s*`<FEATURE_SPECIFIC_ERROR>`\s*\|/,
    reason: "placeholder feature-specific error must be removed or replaced with a named candidate",
  },
  {
    pattern: /^\|\s*method\/event 命名是否需要与已有 generated 事实合并？\s*\|/,
    reason: "generic registry merge review question belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*legacy 命令和字段是否全部映射清楚？\s*\|/,
    reason: "generic legacy review question belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*happy path\s*\|\s*capability discovery 后调用主要 query\/command\/action method，返回成功响应。\s*\|/,
    reason: "generic test matrix belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*event path\s*\|\s*会改变状态的 method 成功后，按需产生 changed\/progress\/state event；客户端可更新 UI 或调用 get 校准。\s*\|/,
    reason: "generic test matrix belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*boundary case\s*\|\s*省略可选字段、非法 target、非法枚举、越界值、空列表和最大对象数量。\s*\|/,
    reason: "generic test matrix belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*error case\s*\|\s*unsupported feature\/method、permission denied、busy、invalid argument、version\/capability mismatch。\s*\|/,
    reason: "generic test matrix belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*compatibility\s*\|\s*新旧 App \/ 设备组合下，未知可选字段可忽略，未知必填语义必须返回标准错误。\s*\|/,
    reason: "generic test matrix belongs in draft-conventions.md",
  },
  {
    pattern: /^Capability 字段见第 5 章。复杂 capability 对象在 registry review 前需要拆成独立字段表。$/,
    reason: "generic capability section pointer belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*`NOT_SUPPORTED`\s*\|\s*设备不支持 feature、method、target、scope 或 section。\s*\|\s*优先复用通用错误。\s*\|$/,
    reason: "generic NOT_SUPPORTED row belongs in draft-conventions.md unless it explains feature-specific conditions",
  },
  {
    pattern: /^\|\s*`INVALID_ARGUMENT`\s*\|\s*参数非法、枚举非法、范围非法。\s*\|\s*应指出具体字段。\s*\|$/,
    reason: "generic INVALID_ARGUMENT row belongs in draft-conventions.md unless it explains feature-specific fields",
  },
  {
    pattern: /^\|\s*`INVALID_STATE`\s*\|\s*当前状态不允许执行。\s*\|\s*如 lifecycle\/reset\/initialization 冲突。\s*\|$/,
    reason: "generic INVALID_STATE row belongs in draft-conventions.md unless it explains feature-specific states",
  },
  {
    pattern: /^\|\s*`BUSY`\s*\|\s*设备或资源繁忙。\s*\|\s*如已有动作执行中。\s*\|$/,
    reason: "generic BUSY row belongs in draft-conventions.md unless it explains feature-specific contention",
  },
  {
    pattern: /^\|\s*`PERMISSION_DENIED`\s*\|\s*调用方权限不足。\s*\|\s*危险操作或敏感信息读取。\s*\|$/,
    reason: "generic PERMISSION_DENIED row belongs in draft-conventions.md unless it explains feature-specific authorization",
  },
  {
    pattern: /^\|\s*`[^`]+(?:State|Config)`\s*\|\s*表达 `[^`]+` 的(?:当前状态、配置或摘要|可写配置)。\s*\|\s*`\[REVIEW-ASK\]`\s*\|$/,
    reason: "placeholder State/Config schema rows must be removed or replaced with real fields",
  },
  {
    pattern: /本文保留的 `\[REVIEW-ASK\]` 不属于已生成合同/,
    reason: "explanatory text must not contain REVIEW-ASK markers that inflate review counts",
  },
  {
    pattern: /读法：成功响应仍然只展示 RPC `d` block，`id` 必须回显请求 `id`。/,
    reason: "generic success response explanation belongs in draft-conventions.md",
  },
  {
    pattern: /^读法：请求只展示 RPC `d` block；`params` 对应 `[^`]+`，省略字段按上表默认值处理。$/,
    reason: "generic request d-block explanation belongs in draft-conventions.md",
  },
  {
    pattern: /^读法：失败响应仍使用 `op=8`，`d\.id` 回显请求；草案阶段的错误名放在 `status\.details\.candidateError` 中。$/,
    reason: "generic error response explanation belongs in draft-conventions.md",
  },
  {
    pattern: /^读法：失败响应仍使用 `op=8`，`id` 回显请求 `id`。失败时不得携带业务 `result`。$/,
    reason: "generic error response explanation belongs in draft-conventions.md",
  },
  {
    pattern: /^读法：事件不携带 `d\.id`；客户端可按 `data` 更新本地状态，事件丢失或重连后应调用对应 get method 校准。$/,
    reason: "generic event explanation belongs in draft-conventions.md",
  },
  {
    pattern: /^读法：`result` 是 `[^`]+` 的示例快照；正式字段以 registry 采纳后的 schema 为准。$/,
    reason: "generic result snapshot explanation belongs in draft-conventions.md",
  },
  {
    pattern: /^### 7\.1 场景：读取或修改 `/,
    reason: "generic read-or-modify flow example belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*registry\s*\|\s*not generated\s*\|\s*尚未写入正式 registry YAML。\s*\|/,
    reason: "generic registry status table belongs in draft-conventions.md",
  },
  {
    pattern: /^\|\s*registry\s*\|\s*not generated\s*\|\s*`contract\/registry\/domains\/[^`]+` 尚未包含 /,
    reason: "fixed not-generated registry status belongs in frontmatter/domain status, not each draft",
  },
  {
    pattern: /^\|\s*generated\s*\|\s*false\s*\|\s*generated 文档无 /,
    reason: "fixed not-generated status belongs in frontmatter/domain status, not each draft",
  },
  {
    pattern: /^\|\s*conformance\s*\|\s*missing\s*\|\s*需覆盖 /,
    reason: "fixed conformance status table belongs in draft-conventions.md unless it carries adopted case IDs",
  },
  {
    pattern: /^## 10\. Registry \/ Conformance 状态$/,
    reason: "generic registry/conformance status belongs in frontmatter and product domain status, not each draft",
  },
  {
    pattern: /^本草案尚未 generated；状态以 frontmatter、Product Domain Status 和 registry\/generated 事实为准。feature-specific 验收重点见下方测试要点。$/,
    reason: "generic adoption status belongs in frontmatter and product domain status, not each draft",
  },
  {
    pattern: /^采纳状态：本草案尚未 generated；状态以 frontmatter、Product Domain Status 和 registry\/generated 事实为准。/,
    reason: "generic adoption status belongs in frontmatter and product domain status, not each draft",
  },
  {
    pattern: /^\|\s*`[^`]+`\s*采纳前还需确认哪些 schema、事件和 conformance 细节？\s*\|\s*schema \/ conformance\s*\|/,
    reason: "generic adoption open questions belong in draft-conventions.md; drafts should keep only feature-specific questions",
  },
  {
    pattern: /字段见 6\.1/,
    reason: "lazy schema pointers must summarize the returned fields and may reference section 6.1 only as supporting detail",
  },
  {
    pattern: /^\|\s*主要未决问题\s*\|\s*`?\[REVIEW-ASK\]/,
    reason: "速读结论 may summarize open questions but must not duplicate REVIEW-ASK markers already listed in review tables",
  },
  {
    pattern: /^\|\s*(?:Params|Result|Payload) Schema\s*\|\s*`(?:TBD after adoption|0x[0-9A-Fa-f]+|[^`]*[，。；、][^`]*)`\s*\|/,
    reason: "schema slots must contain candidate schema names, not numeric IDs, TBD markers, or prose",
  },
  {
    pattern: /^#### \d+\.\d+\.\d+ (?:请求参数 Params|返回结果 Result|Payload)：`(?:TBD after adoption|0x[0-9A-Fa-f]+|[^`]*[，。；、][^`]*)`$/,
    reason: "schema subsection headings must use candidate schema names, not numeric IDs, TBD markers, or prose",
  },
  {
    pattern: /^\|\s*`(?:TBD after adoption|0x[0-9A-Fa-f]+|[^`]*[，。；、][^`]*)`\s*\|\s*(?:`[^`]+` )?(?:request params|result|Event|event|payload)\s*\|/,
    reason: "schema index rows must use candidate schema names, not numeric IDs, TBD markers, or prose",
  },
  {
    pattern: /^\|\s*`(?:TBD after adoption|0x[0-9A-Fa-f]+|[^`]*[，。；、][^`]*)`\s*\|\s*`[a-z][a-z0-9]*\.[^`]+`\s*\|/,
    reason: "event schema index rows must use candidate schema names, not numeric IDs, TBD markers, or prose",
  },
  {
    pattern: /^\|\s*`[a-z][a-z0-9]*\.[^`]+`\s*\|[^|]*\|\s*`(?:TBD after adoption|0x[0-9A-Fa-f]+|[^`]*[，。；、][^`]*)`\s*\|/,
    reason: "event payload schema columns must use candidate schema names, not numeric IDs, TBD markers, or prose",
  },
  {
    pattern: /^#### (?:\d+\.\d+\.\d+ )?Request d block Example \(op=7\)$/,
    reason: "method request examples must be folded into a single d block 示例 subsection",
  },
  {
    pattern: /^#### (?:\d+\.\d+\.\d+ )?Success Response d block Example \(op=8\)$/,
    reason: "method success examples must be folded into a single d block 示例 subsection",
  },
  {
    pattern: /^#### (?:\d+\.\d+\.\d+ )?Event d block Example \(op=6\)$/,
    reason: "event examples must use the compact Chinese d block 示例 heading",
  },
  {
    pattern: /(?:Request|Success Response|Error Response|Event) d block \(op=[678]\)/,
    reason: "flow examples must use compact labels like request:, success:, event:, or error: instead of old English op headings",
  },
  {
    pattern: /^#### (?:\d+\.\d+\.\d+ )?Error Response d block Example \(op=8\)$/,
    reason: "error examples must use the compact Chinese 错误 d block 示例 heading",
  },
  {
    pattern: /"reason": "example failure"/,
    reason: "example failure is a placeholder; error examples must explain a concrete feature-specific failure",
  },
  {
    pattern: /<(?:STA_INTERFACE_ID|AP_INTERFACE_ID|MISSING_INTERFACE_ID|DEVICE_IP|NEW_DEVICE_IP|GATEWAY_IP|DNS_IP|STA_MAC|AP_MAC|DEVICE_ID|SERIAL_NUMBER|WORKSPACE_ID|ROOM_ENDPOINT_ID|CREDENTIAL_EXPORT_ID|ONE_TIME_EXPORTED_AP_PASSPHRASE|NEW_AP_PASSPHRASE|NA20_AP_BSSID|receiver-id)>/,
    reason: "protocol draft examples must use stable sample values instead of template placeholders",
  },
  {
    pattern: /原草案(?:示例)?中出现的候选(?:方法|事件)/,
    reason: "drafts must describe method/event purpose directly instead of carrying intermediate drafting placeholders",
  },
  {
    pattern: /MVP 字段范围是否完整/,
    reason: "generic MVP field-range review questions belong in draft-conventions.md, not every draft",
  },
];

const templateOnlyBannedLinePatterns = [
  {
    pattern: /^#### 3\.1\.2 Request d block Example \(op=7\)$/,
    reason: "template must not require generic request examples for every method",
  },
  {
    pattern: /^#### 3\.1\.4 Success Response d block Example \(op=8\)$/,
    reason: "template must not require generic success examples for every method",
  },
  {
    pattern: /^#### 3\.1\.7 Error Response d block Example \(op=8\)$/,
    reason: "template must not require generic error examples for every method",
  },
  {
    pattern: /^### 7\.1 场景：<客户端要完成什么>$/,
    reason: "template must not include generic read/modify flow boilerplate",
  },
];

function walk(dir) {
  const out = [];
  if (!fs.existsSync(dir)) return out;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const entryPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      out.push(...walk(entryPath));
      continue;
    }
    if (entry.isFile() && entry.name.endsWith(".md")) out.push(entryPath);
  }
  return out.sort();
}

function shouldSkip(file) {
  const base = path.basename(file);
  return base === "README.md" || base === "draft-conventions.md";
}

const files = walk(protocolRoot);
if (fs.existsSync(protocolDraftTemplate)) files.push(protocolDraftTemplate);

function checkMethodExampleCoverage(file, relative, text) {
  const methodMatches = [...text.matchAll(/^### 3\.\d+ `[^`]+`.*$/gm)];
  for (let index = 0; index < methodMatches.length; index += 1) {
    const start = methodMatches[index].index;
    const nextMethodStart = index + 1 < methodMatches.length ? methodMatches[index + 1].index : text.length;
    const nextH2Match = text.slice(start).match(/\n## [4-9]\./);
    const h2Start = nextH2Match ? start + nextH2Match.index + 1 : text.length;
    const end = Math.min(nextMethodStart, h2Start);
    const section = text.slice(start, end);
    const heading = methodMatches[index][0];
    const compactMatches = [...section.matchAll(/^#### \d+\.\d+\.\d+ d block 示例$/gm)];
    if (compactMatches.length !== 1) {
      errors.push(`${relative}:${lineNumber(text, start)}: ${heading} must contain exactly one compact d block 示例 subsection`);
      continue;
    }
    const compactStart = compactMatches[0].index;
    const nextH4 = section.slice(compactStart + compactMatches[0][0].length).match(/\n#### /);
    const compactEnd = nextH4 ? compactStart + compactMatches[0][0].length + nextH4.index : section.length;
    const compactSection = section.slice(compactStart, compactEnd);
    if (!/^request:$/m.test(compactSection) || !/^success:$/m.test(compactSection)) {
      errors.push(`${relative}:${lineNumber(text, start + compactStart)}: method d block 示例 must label request: and success: examples`);
    }
    const methodEventExample = section.match(/^#### (?:\d+\.\d+\.\d+ )?Event d block Example \(op=6\)$/m);
    if (methodEventExample) {
      errors.push(`${relative}:${lineNumber(text, start + methodEventExample.index)}: method-level event examples belong in the Events section`);
    }
  }
}

function lineNumber(text, offset) {
  return text.slice(0, offset).split(/\r?\n/).length;
}

function checkMethodNumbering(file, relative, text) {
  if (file === protocolDraftTemplate) return;
  const lines = text.split(/\r?\n/);
  let sawOverview = false;
  let expectedMethod = 0;
  let currentMethod = null;
  let expectedSubsection = null;

  lines.forEach((line, index) => {
    const h2Match = line.match(/^## (\d+)\./);
    if (h2Match && Number(h2Match[1]) !== 3) {
      currentMethod = null;
      expectedSubsection = null;
    }

    const methodMatch = line.match(/^### 3\.(\d+)\s+(.*)$/);
    if (methodMatch) {
      const methodNumber = Number(methodMatch[1]);
      if (methodNumber === 0) {
        if (sawOverview) {
          errors.push(`${relative}:${index + 1}: duplicate Methods overview heading 3.0`);
        }
        sawOverview = true;
        expectedMethod = 1;
        currentMethod = 0;
        expectedSubsection = null;
        return;
      }

      if (!sawOverview) {
        errors.push(`${relative}:${index + 1}: Methods section must include 3.0 overview before 3.${methodNumber}`);
        expectedMethod = methodNumber + 1;
      } else if (methodNumber !== expectedMethod) {
        errors.push(`${relative}:${index + 1}: method headings must be consecutive; expected 3.${expectedMethod}, found 3.${methodNumber}`);
        expectedMethod = methodNumber + 1;
      } else {
        expectedMethod += 1;
      }
      currentMethod = methodNumber;
      expectedSubsection = 1;
      return;
    }

    const subsectionMatch = line.match(/^#### 3\.(\d+)\.(\d+)\s+(.*)$/);
    if (!subsectionMatch) return;

    const methodNumber = Number(subsectionMatch[1]);
    const subsectionNumber = Number(subsectionMatch[2]);
    if (currentMethod === null || currentMethod === 0) {
      errors.push(`${relative}:${index + 1}: method subsection 3.${methodNumber}.${subsectionNumber} appears outside a method heading`);
    } else if (methodNumber !== currentMethod) {
      errors.push(`${relative}:${index + 1}: method subsection belongs to 3.${methodNumber}, but current method heading is 3.${currentMethod}`);
    }
    if (expectedSubsection !== null && subsectionNumber !== expectedSubsection) {
      errors.push(`${relative}:${index + 1}: method subsections must be consecutive; expected 3.${currentMethod}.${expectedSubsection}, found 3.${methodNumber}.${subsectionNumber}`);
    }
    expectedSubsection = subsectionNumber + 1;
  });
}

function checkKnownExampleValueDrift(relative, text) {
  const checks = [
    {
      pattern: /"method": "audio\.closeStream"[\s\S]{0,320}"reason": "user_request"/,
      reason: "audio.closeStream example reason must use the closeStream enum such as user_stop",
    },
    {
      pattern: /"method": "video\.closeStream"[\s\S]{0,320}"reason": "user_request"/,
      reason: "video.closeStream example reason must use the closeStream enum such as user_stop",
    },
    {
      pattern: /"method": "audio\.closeStream"[\s\S]{0,900}"state": "streaming"/,
      reason: "audio.closeStream success example must not return streaming; close result state is closing/closed/failed",
    },
    {
      pattern: /"method": "video\.closeStream"[\s\S]{0,900}"state": "streaming"/,
      reason: "video.closeStream success example must not return streaming; close result state is closing/closed/failed",
    },
    {
      pattern: /"event": "audio\.streamSourceStateChanged"[\s\S]{0,500}"state": "streaming"/,
      reason: "audio stream source state example must use source states such as receiving, not stream lifecycle state streaming",
    },
    {
      pattern: /"event": "video\.streamSourceStateChanged"[\s\S]{0,500}"state": "streaming"/,
      reason: "video stream source state example must use source states such as receiving, not stream lifecycle state streaming",
    },
    {
      pattern: /"event": "audio\.streamSourceStateChanged"[\s\S]{0,650}"(?:reason|lastOpenRejectedReason)": "user_request"/,
      reason: "audio stream source state reason examples must use the source-state reason enum",
    },
    {
      pattern: /"event": "video\.streamSourceStateChanged"[\s\S]{0,650}"(?:reason|lastOpenRejectedReason)": "user_request"/,
      reason: "video stream source state reason examples must use the source-state reason enum",
    },
    {
      pattern: /"method": "network\.disconnectWifi"[\s\S]{0,900}"state": "connected"/,
      reason: "network.disconnectWifi success example must not report connected as the resulting state",
    },
    {
      pattern: /"event": "network\.wifiStateChanged"[\s\S]{0,650}"reason": "user_request"/,
      reason: "network.wifiStateChanged reason examples must use wifi state reasons such as connect_completed or disconnect",
    },
    {
      pattern: /"event": "network\.wifiScanResultReported"[\s\S]{0,300}"reason": "user_request"/,
      reason: "network.wifiScanResultReported reason examples must use scan reasons such as result or complete",
    },
    {
      pattern: /"method": "software\.(?:getConfig|setConfig|resetConfig)"[\s\S]{0,900}"target": "runtime"/,
      reason: "software.config examples must use supported targets such as launcher instead of the old runtime placeholder",
    },
    {
      pattern: /"method": "software\.(?:getConfig|setConfig|resetConfig)"[\s\S]{0,900}"(?:profile|logLevel|networkMode)":/,
      reason: "software.config examples must use the current SoftwareConfig shape instead of legacy runtime/profile fields",
    },
    {
      pattern: /"method": "software\.(?:getUpdatePolicy|setUpdatePolicy|resetUpdatePolicy)"[\s\S]{0,900}"(?:mode|allowedWindow|autoInstall)":/,
      reason: "software.updatePolicy examples must use the current policy.updateMode/schedule/channel/conditions shape",
    },
  ];

  for (const { pattern, reason } of checks) {
    const match = text.match(pattern);
    if (match?.index !== undefined) {
      errors.push(`${relative}:${lineNumber(text, match.index)}: ${reason}`);
    }
  }
}

function checkEmptyErrorSections(relative, text) {
  const emptyErrorSectionPattern =
    /^## \d+\. 错误\n\n\| 错误 \| 适用场景 \| 说明 \|\n\|---\|---\|---\|\n(?=\n## )/gm;
  for (const match of text.matchAll(emptyErrorSectionPattern)) {
    errors.push(`${relative}:${lineNumber(text, match.index)}: empty error sections must be removed or filled with feature-specific errors`);
  }
}

for (const file of files) {
  if (shouldSkip(file)) continue;
  const relative = path.relative(root, file);
  const text = fs.readFileSync(file, "utf8");
  const lines = text.split(/\r?\n/);
  const h2Numbers = new Map();
  lines.forEach((line, index) => {
    const h2Match = line.match(/^## (\d+)\./);
    if (h2Match) {
      const previous = h2Numbers.get(h2Match[1]);
      if (previous !== undefined) {
        errors.push(`${relative}:${index + 1}: duplicate H2 section number ${h2Match[1]} also used on line ${previous}`);
      } else {
        h2Numbers.set(h2Match[1], index + 1);
      }
    }
    for (const { pattern, reason } of bannedLinePatterns) {
      if (pattern.test(line)) {
        errors.push(`${relative}:${index + 1}: ${reason}`);
      }
    }
    if (file === protocolDraftTemplate) {
      for (const { pattern, reason } of templateOnlyBannedLinePatterns) {
        if (pattern.test(line)) {
          errors.push(`${relative}:${index + 1}: ${reason}`);
        }
      }
    }
  });
  checkMethodNumbering(file, relative, text);
  checkMethodExampleCoverage(file, relative, text);
  checkKnownExampleValueDrift(relative, text);
  checkEmptyErrorSections(relative, text);
}

if (errors.length > 0) {
  console.error(`[FAIL] protocol draft noise check failed: ${errors.length}`);
  for (const error of errors) console.error(`- ${error}`);
  process.exit(1);
}

console.log("[OK] protocol drafts do not contain repeated template noise");
