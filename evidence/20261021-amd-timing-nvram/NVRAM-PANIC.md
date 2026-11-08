# NVRAM 与早期 panic 可观测性

第一轮只读采样：2026-10-21 09:33–09:36 CST。

## NVRAM 当前策略

| 项 | 当前值 | 含义 |
| --- | --- | --- |
| `Booter.Quirks.DisableVariableWrite` | `true` | 阻止 macOS runtime 真写固件 NVRAM |
| `NVRAM.WriteFlash` | `true` | OpenCore 自己的变量写入策略仍是写 flash |
| `Misc.Security.ExposeSensitiveData` | `7` | 包含 boot path 暴露位 |
| `Misc.Debug.Target` | `71` | 屏显 + Data Hub + 写 OpenCore 文件日志 |

`NVRAM.Add` 明确写入 `boot-args`、`csr-active-config`、`AirportMtkAttach`、
`bluetooth*`、`prev-lang:kbd` 和 `run-efi-updater`。`NVRAM.Delete` 包含
`boot-args`、`csr-active-config`、`prev-lang:kbd`、`nullmoth-remove` 等 OpenCore
每次启动先删后加的键。

`nvram -p` 当前只有 14 个普通名变量：

```text
boot-args
nullmoth-nv-ok
fm-spstatus
bluetoothExternalDongleFailed
ForceDisplayRotationInEFI
bluetoothInternalControllerInfo
prev-lang:kbd
run-efi-updater
SystemAudioVolumeDB
bluetoothHostControllerSwitchBehavior
csr-active-config
AirportMtkAttach
SystemAudioVolume
LocationServicesEnabled
```

当前输出 795 B，行数 14，SHA-256：

```text
c9fee2807ad1f380b2b36f12c8689c8d47e5317c57e0e487bf99ea5f5db5b2ea
```

09:36:08 间隔 10 秒重取两次，哈希相同。只是静默态稳定，不是“所有写路径均被拦截”
的充分证明。

`nvram -p` 没有列出 vendor GUID 键，但显式查询存在：

```text
4D1FDA02-38C7-4A6A-9CC6-4BCCA8B30102:boot-path
4D1FDA02-38C7-4A6A-9CC6-4BCCA8B30102:opencore-version = DBG-107-2026-10-14
4D1FDA02-38C7-4A6A-9CC6-4BCCA8B30102:revpatch = sbvmm,cpuname,pci
```

因此以后做 NVRAM 盘点时不能只看 `nvram -p` 的键名列表；要额外检查已知 vendor GUID。

## 写者归属

`NVRWLog.kext` 目前不在 ESP，也不在 `Kernel.Add`，当前运行态没有加载；
最近一小时 unified log 没有 `nvrwlog`/`AppleEFIRuntime setVariable` 记录。

结论：当前只能确认策略值；不能确认本轮每次变化的具体写者。历史 2026-10-14 的受控音量
测试曾确认音量写路径被挡住，但同一时期也观察到 `SystemAudioVolume` 被未知写者改过；
这不等于当前仍在发生写入。

## NVRAM 风险矩阵

| 方案 | 启动/固件风险 | 双系统一致性 | 可观测性收益 | 本轮建议 |
| --- | --- | --- | --- | --- |
| 保持 `DisableVariableWrite=true` | 低；保留已知稳定档 | macOS 变量持久化不完整 | 低 | **保持** |
| 改成 `false` 直接放行 | 高；AM5 已知有写坏/冻死先例 | 真共享 | 高但有不可逆风险 | 不推荐 |
| `OpenVariableRuntimeDxe` 模拟 NVRAM | 中高；需 LoadEarly/OpenRuntime 排序与持久化链 | Windows/macOS 不共享 | 中 | 不启动 |
| 恢复 NVRWLog 第一阶段“只记录后转发” | 低–中；需 ESP 写入与重启 | 无变化 | 可拿到 GUID/变量/调用者 | 如需证据，单独立项 |

## 早期启动 / panic 落盘

### 已有

- OpenCore `Debug.Target=71`，日志写 ESP 根目录。当前共 329 个日志，82 MiB；
  最新日志在 `20.479s` 写 `AAPL: #[EB|LOG:EXITBS:START]` 后结束，不覆盖内核接管后的崩溃。
- ESP 当前使用 156 MiB / 196 MiB，约 80%，剩 40 MiB；每份日志 256 KiB，长期有填满风险。
- `boot-args` 包含 `-v debug=0x100 keepsyms=1`，磁盘/panic reporter 可用时有助于屏显与符号化。
- `/Library/Logs/DiagnosticReports/.contents.panic` 仍保留 2026-10-13 的早期 panic 摘要，
  其中 panic string、backtrace 和 boot args 都在。

### 缺口

- `/var/db/PanicReporter` 当前为空。
- `.contents.panic` 引用的
  `/Library/Logs/DiagnosticReports/Kernel-2026-10-13-005400.panic` 已不存在，只剩摘要。
- `Kernel.Quirks.PanicNoKextDump=true` 会减少 panic 时的 kext dump。
- 本次启动后前 3 分钟 unified log 没有 panic/fault/watchdog 命中；unified log 不能覆盖
  disk logging 初始化前的内核早期故障。
- 09:16:01 的 `shutdown_stall_...` 是上一次关机阶段的 spindump，不是本次启动 panic。

### 判定

当前有“启动前/OpenCore 阶段”的证据链，但**没有**“内核刚接管、磁盘 stack 尚未就绪”阶段的
可靠持久化通道。若以后需要真正闭环，优先考虑串口/专用 crash capture；只加
`PanicNoKextDump=false` 无法覆盖 reporter 尚未启动的故障。

## 本轮阈值

- 不改 `DisableVariableWrite`。
- 不把 NVRAM 接管与 timer A/B 混在同一启动。
- 不清理 ESP 旧日志；ESP 清理是独立窗口。
