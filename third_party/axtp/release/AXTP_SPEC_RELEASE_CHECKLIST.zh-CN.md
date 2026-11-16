# AXTP Spec 发布检查清单

创建 AXTP Spec tag 和 GitHub Release 前，应使用本清单。

## 发布前检查

1. 确认 `specs/` 中协议文档已经更新。
2. 确认 `contract/registry/` 中 method、event、capability、error、profile 和共享 registry 文件已经更新。
3. 确认 schema 源文件和 domain YAML 中的 schema 定义已经更新。
4. 如果源事实发生变化，确认 generated protocol references、tooling JSON、test vectors 和 runtime generated headers 已刷新。
5. 确认 conformance 或 test-vector 材料覆盖新增或变更的协议行为。
6. 如果 legacy 行为变化，确认 `workspace/legacy-migration/plans/` 中 legacy 映射和迁移说明已经更新。
7. 确认 `release/CHANGELOG.md` 已在 `spec/vX.Y.Z` 下记录本次发布。
8. 确认 `README.md` 已说明 AXTP Spec 版本管理和 runtime 依赖规则。
9. 确认 compatibility notes 明确说明 runtime impact。
10. 确认 `git diff --check` 通过。

## Tag 与 Release

创建 annotated tag：

```bash
git tag -a spec/vX.Y.Z -m "AXTP Spec vX.Y.Z"
git push origin spec/vX.Y.Z
```

创建 GitHub Release，名称为：

```text
AXTP Spec vX.Y.Z
```

GitHub Release body 应包含：

- Protocol changes
- Registry changes
- Schema changes
- Conformance changes
- Migration changes
- Compatibility notes
- Runtime impact
- `release/CHANGELOG.md` 和相关 generated protocol docs 链接

## Runtime 自动化

发布后：

1. 确认 `spec-release-dispatch` workflow 已向所有真实 runtime/mock 仓库发送 `axtp_spec_released`。
2. 确认每个 runtime/tool 仓库打开或更新了 `automation/upgrade-axtp-spec-vX.Y.Z`。
3. 确认每个升级 PR 更新了 `AXTP_SPEC.lock.yaml`、runtime/tool 版本文件、生成代码和 runtime release metadata。
4. 确认每个 runtime generated manifest 或 release metadata 记录 AXTP Spec `X.Y.Z` 和 runtime/tool version `X.Y.Z.0`。
5. 确认每个自动化 PR 只在检查通过后 auto-merge。
6. 确认每个 runtime/tool 仓库创建了 `vX.Y.Z.0` 和 GitHub Release。
7. 不要要求 runtime 依赖 `main`；每个 runtime 依赖都必须指向 tag 或 commit。

## 非目标

- 不从 AXTP Spec release 发布 npm、pub、PyPI、Docker 或其他 package registry 包。
- 不强制所有 runtime 仓库使用同一种依赖机制。
- 在 source facts 与 generated outputs 对齐前，不发布 AXTP Spec tag。
