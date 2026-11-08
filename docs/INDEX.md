# 文档索引

按主题排列。文件名沿用原始记录的名字（`handoff-*` 是交给某个并行会话的交接书，
`2026-10-19-*` 是那天的规划文档），没有重命名，方便和 `evidence/` 里的证据对号。

## 先读这三份

| 文件 | 讲什么 |
| --- | --- |
| [`../README.md`](../README.md) | 平台的短版结论：什么能修、什么修不了、什么要写驱动 |
| [`2026-10-19-amd-adaptation-program.md`](2026-10-19-amd-adaptation-program.md) | **总纲**：四大分组（A 内核/框架、B I/O 总线、C 设备驱动、D 依赖功能）、13 个工作流、M0–M5 里程碑、决策门。762 行，最长也最有用。顶部是 2026-10-25 Tahoe 升级后的最新状态 |
| [`virtualization.md`](virtualization.md) | 为什么这台机器永远跑不了虚拟机（Apple HV 只有 Intel 后端），以及为什么 CPU 伪装救不了 |

## 按域

### 引导与固件

| 文件 | 讲什么 |
| --- | --- |
| [`boot-scheme.md`](boot-scheme.md) | 引导方案：三种方案对比、为什么选内置 ESP + `LauncherOption=Full`、源码级安全论证、回滚办法 |
| [`handoff-amd-boot-config.md`](handoff-amd-boot-config.md) | 启动配置基线：真实 ESP 位置、config 哈希、771 文件全量备份、离线还原演练、单写者规则 |
| [`bios-live-vs-static.md`](bios-live-vs-static.md) | BIOS 分析：哪些值是实时读的、哪些不可信、XHCI Hand-off 为什么被隐藏 |
| [`bios/`](bios/) | Setup/CBS 表逐项解码、IFR 全文与结构 JSON、ESP 检查报告、115 个固件变量清单 |
| [`macos-update-checklist.md`](macos-update-checklist.md) | 每次 macOS 更新后要复查什么 |
| [`upstream-watch.md`](upstream-watch.md) | OpenCore / NullMoth 等上游版本追踪 |

### CPU、定时器、电源、传感器

| 文件 | 讲什么 |
| --- | --- |
| [`handoff-amd-timing-nvram.md`](handoff-amd-timing-nvram.md) | Zen5 定时器/TSC 校准：`_tsc_init` 补丁的命中审计、`mach_wait_until` 8 倍超标的消失、NVRAM 写入语义、早期 panic 取证缺口 |
| [`handoff-amd-power-smc.md`](handoff-amd-power-smc.md) | CPU 电源与 SMC 传感器：65 个 key、零真实传感器值、`SMCAMDProcessor` 为什么 No-Go、只读插件为什么有条件可行 |
| [`amd-security-posture.md`](amd-security-posture.md) | 安全姿态只读评估：SIP / AMFI / FileVault 现状与可选收紧路径 |

### I/O 与总线

| 文件 | 讲什么 |
| --- | --- |
| [`usb-mapping-plan.md`](usb-mapping-plan.md) | USB 端口映射：USBToolBox 操作步骤、本机端口表核对结果 |
| [`handoff-usb-portlimit.md`](handoff-usb-portlimit.md) | 15 端口上限：算法反汇编确证、冲突模拟器、两件套为什么缺一不可 |
| [`handoff-usb-portlimit-addendum.md`](handoff-usb-portlimit-addendum.md) | 上面那份的补充 |
| [`handoff-probe-ops.md`](handoff-probe-ops.md) | 外部探针运维（远程只读探针、告警通道、陈旧配置的坑） |
| [`disk-and-nic-plan.md`](disk-and-nic-plan.md) | 磁盘与网卡方案 |
| [`disk-repartition-plan.md`](disk-repartition-plan.md) | 磁盘扩容方案（5 阶段，定稿待执行） |

### 设备驱动（各自有独立仓库）

| 文件 | 讲什么 |
| --- | --- |
| [`device-driver-feasibility.md`](device-driver-feasibility.md) | **三条硬件适配线的完整证据链**：USB 端口上限 / MT7922 WiFi / Mellanox CX3，含反汇编与许可结论 |
| [`handoff-mt7922-wifi.md`](handoff-mt7922-wifi.md) | MT7922 Wi-Fi 驱动交接（→ AirportMtk 仓库） |
| [`handoff-bt-mt7922.md`](handoff-bt-mt7922.md) | MT7922 蓝牙交接（→ mt7922-bt-macos 仓库） |
| [`handoff-cx3-mlx4.md`](handoff-cx3-mlx4.md) | Mellanox ConnectX-3 Pro kext 交接（→ cx3-mlx4-macos 仓库） |
| [`handoff-cx3-freeze-prep.md`](handoff-cx3-freeze-prep.md) | CX3 冻屏调查的准备材料 |
| [`cx3pro-driver-feasibility.md`](cx3pro-driver-feasibility.md) | CX3 Pro 驱动可行性评估 |
| [`cx3-m1-notes.md`](cx3-m1-notes.md) | CX3 第一阶段（M1）的逐条笔记 |
| [`apple-native-rtl8125-driver.md`](apple-native-rtl8125-driver.md) | RTL8125 为什么需要第三方驱动（Apple 的 `AppleEthernetRL` 被 Block） |
| [`handoff-rtl8125-crash.md`](handoff-rtl8125-crash.md) | RTL8125 崩溃调查 |
| [`nullmoth-nvidia-macos-dualboot-plan.md`](nullmoth-nvidia-macos-dualboot-plan.md) | RTX 5060 双系统方案与 NullMoth 驱动路线 |

### 整体

| 文件 | 讲什么 |
| --- | --- |
| [`install-runbook.md`](install-runbook.md) | 主施工手册：各次启动的日志判读、APFS 取证、USB 映射、ESP 现状，最大的一份现场记录 |
| [`todo-open-items.md`](todo-open-items.md) | 未完成项清单（含用户已拍板"不做"的事项与理由） |

## 证据目录

[`../evidence/`](../evidence/) 按日期分目录，每个目录是一次会话的测量记录。命名规则是
`YYYYMMDD-领域`，里面有 baseline、跑过什么、回来什么、否掉了什么。README 里的每条
"measured" 结论都能在里面找到对应报告。
