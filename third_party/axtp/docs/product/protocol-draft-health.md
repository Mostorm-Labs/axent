# AXTP 协议草案健康度

本页是产品和协议维护者查看草案健康度、示例质量和待确认问题密度的报告。它不是 runtime 实现合同；可实现事实仍以 `contract/**`、`specs/**` 和 `conformance/**` 为准。

本页是 release artifact-safe 摘要：只展示 domain 级计数和治理状态，不列后台 `workspace/` 文件路径。维护者需要文件级队列时，可运行脚本的 `--json` 模式在本地分析。

本页由脚本生成并校验：

```bash
node tooling/scripts/report-protocol-draft-health.mjs --write docs/product/protocol-draft-health.md
node tooling/scripts/report-protocol-draft-health.mjs --check docs/product/protocol-draft-health.md
```

## 摘要

| 指标 | 数量 |
|---|---:|
| 草案文件 | 117 |
| 已生成草案文件 | 28 |
| 已生成 method/event 事实 | 142 |
| Method 小节 | 455 |
| 紧凑 method 示例 | 455 |
| Method 示例缺口 | 0 |
| JSON 示例 | 1237 |
| 无效 JSON 示例 | 0 |
| 模板化示例提示 | 0 |
| 模板化开放问题 | 0 |
| 模板化字段占位 | 0 |
| REVIEW-ASK | 132 |
| REVIEW-DRAFT | 139 |
| REVIEW-FIX | 1 |
| REVIEW-BLOCKER | 0 |
| 采纳后占位残留 | 25 |

## 领域健康矩阵

| 领域 | 优先级 | 草案 | 已生成草案 | 已生成事实 | Method 数 | 示例覆盖 | Review 标记 | 模板示例 | 模板问题 | 建议动作 |
|---|---|---:|---:|---:|---:|---:|---|---:|---:|---|
| audio | 旁路高覆盖 / P0 stream | 12 | 2 | 13 | 54 | 54/54 | ASK 2 / DRAFT 7 / FIX 0 / BLOCKER 0 | 0 | 0 | 确认 REVIEW-ASK |
| auth | 待排期 | 3 | 0 | 0 | 12 | 12/12 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| camera | P3/P4 | 7 | 0 | 0 | 40 | 40/40 | ASK 28 / DRAFT 52 / FIX 0 / BLOCKER 0 | 0 | 0 | 确认 REVIEW-ASK |
| capability | 待排期 | 1 | 0 | 0 | 4 | 4/4 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| cast | P0/P1 多路控制已采纳 | 12 | 12 | 47 | 25 | 25/25 | ASK 9 / DRAFT 4 / FIX 0 / BLOCKER 0 | 0 | 0 | 确认 REVIEW-ASK |
| device | P1 | 6 | 2 | 5 | 20 | 20/20 | ASK 19 / DRAFT 19 / FIX 0 / BLOCKER 0 | 0 | 0 | 确认 REVIEW-ASK |
| diagnostic | P5 | 10 | 0 | 0 | 39 | 39/39 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| display | 待排期 | 6 | 0 | 0 | 24 | 24/24 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| file | 待排期 | 2 | 0 | 0 | 8 | 8/8 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| firmware | 旁路高覆盖 | 3 | 1 | 6 | 7 | 7/7 | ASK 5 / DRAFT 1 / FIX 0 / BLOCKER 0 | 0 | 0 | 确认 REVIEW-ASK |
| input | 待排期 | 5 | 0 | 0 | 20 | 20/20 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| log | 待排期 | 3 | 0 | 0 | 9 | 9/9 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| network | 旁路高覆盖 | 6 | 4 | 26 | 24 | 24/24 | ASK 12 / DRAFT 18 / FIX 0 / BLOCKER 0 | 0 | 0 | 确认 REVIEW-ASK |
| output | P2b | 1 | 0 | 0 | 3 | 3/3 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| privacy | 待排期 | 3 | 0 | 0 | 10 | 10/10 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| room | P7 | 5 | 0 | 0 | 20 | 20/20 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| signage | P7 | 2 | 1 | 7 | 10 | 10/10 | ASK 12 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 确认 REVIEW-ASK |
| software | P7 | 2 | 2 | 8 | 6 | 6/6 | ASK 37 / DRAFT 24 / FIX 1 / BLOCKER 0 | 0 | 0 | 先处理 blocker/fix |
| sport | P2 | 2 | 2 | 9 | 7 | 7/7 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 维护 generated 合同 |
| storage | 待排期 | 6 | 0 | 0 | 24 | 24/24 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| stream | P0 data-plane plumbing | 2 | 1 | 12 | 10 | 10/10 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 维护 generated 合同 |
| system | P1 | 6 | 0 | 0 | 28 | 28/28 | ASK 0 / DRAFT 0 / FIX 0 / BLOCKER 0 | 0 | 0 | 可排采纳评审 |
| video | P0 stream | 12 | 1 | 9 | 51 | 51/51 | ASK 8 / DRAFT 14 / FIX 0 / BLOCKER 0 | 0 | 0 | 确认 REVIEW-ASK |
