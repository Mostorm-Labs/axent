# AXTP Runtime Spec Lock

Runtime 仓库应声明自己实现的 AXTP Spec 版本。推荐在仓库根目录放置：

```text
AXTP_SPEC.lock.yaml
```

该文件用于保证 runtime 构建可复现、可审计。Runtime 不得直接依赖 AXTP `main` 分支。

## Lock 文件格式

```yaml
axtp_spec:
  repository: https://github.com/Mostorm-Labs/axtp
  tag: spec/v0.3.0
  version: 0.3.0
  commit: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
  compatibility: ">=0.3.0 <0.4.0"
  updated_at: "YYYY-MM-DD"
```

字段说明：

| 字段 | 含义 |
|---|---|
| `repository` | AXTP spec 仓库 URL。 |
| `tag` | Runtime 当前使用的精确 AXTP Spec tag。 |
| `version` | 去掉 `spec/v` 前缀后的 spec version。 |
| `commit` | tag 解析到的 commit hash，用于审计和可复现构建。 |
| `compatibility` | Runtime 预期支持的 spec 范围。 |
| `updated_at` | lock 文件更新时间。 |

Package metadata 可以重复记录同样信息，但 runtime 源码仓库仍应保留明确的 spec 绑定。

## Runtime Release 版本

Runtime/tool GitHub Release tag 使用已锁定的 Spec version 加一个 runtime revision：

```text
spec/vX.Y.Z -> vX.Y.Z.0
spec/vX.Y.Z -> vX.Y.Z.1
```

首次对齐某个 Spec tag 的 runtime release 使用 revision `0`。后续如果只是实现层修复，并且 `AXTP_SPEC.lock.yaml` 仍锁定同一个 spec，则只递增第四位。这样可以把下一个 `spec/vX.Y.(Z+1)` 留给 spec 仓库使用。

如果某个包生态不支持四段数字版本，应把四段值保存在 runtime 仓库 release metadata、generated manifest 或根目录 `VERSION` 文件中，并单独映射 package-manager version。Package metadata 不应成为记录 AXTP Spec 绑定的唯一位置。

## C++ Runtime

C++ runtime 应依赖固定的 AXTP Spec tag 或 commit。常见方式包括 Git submodule 和 CMake `FetchContent`。

### Git Submodule

```bash
git submodule add https://github.com/Mostorm-Labs/axtp third_party/axtp-spec
git -C third_party/axtp-spec checkout spec/v0.3.0
git add .gitmodules third_party/axtp-spec AXTP_SPEC.lock.yaml
```

如果 tag 不能像 branch 一样跟踪，应 checkout 该 tag 对应的固定 commit，并在 `AXTP_SPEC.lock.yaml` 中同时记录 tag 和 commit。

### CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  axtp_spec
  GIT_REPOSITORY https://github.com/Mostorm-Labs/axtp.git
  GIT_TAG spec/v0.3.0
)

FetchContent_MakeAvailable(axtp_spec)
```

C++ runtime 应优先依赖固定 tag，而不是浮动分支。如果构建系统 vendored generated headers，lock 文件仍应标明生成这些 headers 的 AXTP Spec 快照。

## TypeScript Runtime

短期内，TypeScript runtime 可以在 `package.json` 中记录 AXTP Spec metadata：

```json
{
  "name": "@mostorm/axtp-ts-runtime",
  "version": "0.3.0-runtime.1",
  "axtp": {
    "specVersion": "0.3.0",
    "specTag": "spec/v0.3.0",
    "specRepository": "https://github.com/Mostorm-Labs/axtp"
  }
}
```

如果暂时没有发布 `@mostorm/axtp-spec` package，可以使用 git dependency：

```json
{
  "devDependencies": {
    "@mostorm/axtp-spec": "github:Mostorm-Labs/axtp#spec/v0.3.0"
  }
}
```

长期可以发布机器可读 spec package：

```text
@mostorm/axtp-spec
```

该包应包含与 spec release artifact 一致的可消费合同形态：`contract/`、`specs/`、`conformance/`、角色/产品文档、release 文档、changelog 和 manifest。Runtime package 可以声明：

```json
{
  "peerDependencies": {
    "@mostorm/axtp-spec": "^0.3.0"
  }
}
```

## Flutter / Dart Runtime

Flutter/Dart runtime package version 继续写在 `pubspec.yaml`：

```yaml
name: axtp_flutter_runtime
version: 0.3.0-runtime.1
```

不要把 AXTP Spec version 隐藏进 Dart package version。应使用 `AXTP_SPEC.lock.yaml`、generated release metadata 或项目内的 `axtp_spec.yaml`：

```yaml
axtp_spec:
  repository: https://github.com/Mostorm-Labs/axtp
  tag: spec/v0.3.0
  version: 0.3.0
  compatibility: ">=0.3.0 <0.4.0"
```

长期可以发布 Dart package：

```text
axtp_spec
```

该 package 可以包含从已锁定 spec 派生的 generated Dart types、schema/capability metadata 和 conformance cases。

## Submodule 边界

Submodule 适合 C++、内部 runtime、mock server、conformance runner 和 firmware integration project。它不一定适合 npm 或 pub package 的最终发布。

Runtime 仓库可以在开发期使用 submodule，但 release artifact 必须声明精确的 AXTP Spec version。所有依赖必须指向 tag 或 commit，不允许浮动在 `main`。
