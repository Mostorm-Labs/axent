# Device Information And System Runtime State 需求

## 背景

设备信息和系统状态监测是设备管理的基础需求。连接到设备后的第一件事通常是确认“这是谁”和“现在状态怎样”：设备 ID、SN、产品、硬件、软件、AXTP runtime / host app 等属于设备身份和版本信息；CPU、内存、在线、运行态、关机、重启、计划任务、恢复默认等属于系统运行状态和生命周期控制。

## 用户目标

用户想在连接到设备后，能够获取到设备的基础信息以及运行时状态参数，同时监控到设备的信息、状态所发生的变化

## 范围

- 设备基础身份和产品信息展示。
- 主设备和级联 / 子设备信息查看。
- 系统运行状态监测，例如 CPU、内存、在线和运行态。
- lifecycle 控制，例如关机、重启、计划重启和计划关机。
- runtime recovery、恢复默认配置和恢复出厂设置的产品目标。

## 非目标

- 不覆盖 camera、audio、video、network 等业务配置本身。
- 不在 business 文档中定义 `device.*` / `system.*` method/event/schema。
- 不定义 UI 展示顺序、图标、文案或具体确认弹窗。

## 场景

- 设备连接完成后，获取设备信息
- 设备连接完成后，获取主设备以及级连设备的信息
- 控制设备的启停
- 设备关机和重启控制
- 系统计划重启、计划关机任务的下发
- 重置设备状态，比如一些mcu设备，需要由上位机来指定重置，否则会处在一些异常的状态中，或者可以让设备从异常中恢复过来
- 重置设备到默认配置状态，属于软重置的一种：恢复到当前已安装版本的默认配置状态，不改变 Launcher 等软件自身版本。
- 重置设备为出厂设置状态，属于软重置的一种：恢复到设备出厂基线，可包含将 Launcher 等软件自身版本恢复到最初出厂版本。

## 约束

- [REVIEW-ASK] 设备信息与系统状态的字段基线、权限和 legacy 映射需在 flow / protocol 草案中继续确认。
- 关机、重启、恢复默认和恢复出厂可能导致断连、数据变化或版本变化，flow 中必须明确权限、确认和重连策略。

## 旧协议线索

- [REVIEW-ASK] 待补充旧设备信息、系统状态、关机/重启、恢复默认和恢复出厂相关命令、日志或 SDK 行为。

## 开放问题

- [REVIEW-ASK] 主设备信息和子设备信息的最小字段基线是什么？
- [REVIEW-ASK] runtime state recovery 与恢复默认 / 恢复出厂的产品边界如何向用户解释？
- [REVIEW-ASK] 哪些 lifecycle 操作需要权限、二次确认或物理在场确认？

## 下一步

- 交互和协议覆盖由 `workspace/flows/device-system-info.md` 承接。
- Flow 识别出的协议缺口再进入 `device.info`、`device.childDevice`、`system.state`、`system.lifecycle`、`system.reset` 或相关 protocol 草案。
