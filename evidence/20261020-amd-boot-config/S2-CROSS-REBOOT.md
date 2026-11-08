# S2 跨重启验收（2026-10-22 23:55 CST）

结论：**PASS**。S2 稀疏 map 在一次普通重启后完整保持，USB 硬失败签名 0，蓝牙持久。
S2 窗口据此关闭。

## 本次重启

| 项 | 值 |
|---|---|
| `kern.boottime` | 2026-10-22 20:34:50 CST |
| 对应日志 | `opencore-2026-10-22-123428.txt`，`Boot timestamp 2026.10.22 12:34:28`（UTC）= 20:34:28 CST |
| 对齐误差 | 22 秒（OpenCore 交接 → 内核 boottime） |
| 日志配置 | `Loaded configuration of 56573 bytes`，与当前 config 大小一致 |
| 引导器 | `OpenCore DBG-107-2026-10-14` |
| `EXITBS:START` | 有（21.438s） |

## 逐项判据

| 判据 | 结果 |
|---|---|
| UTBMap 注入 | `Prelinked injection UTBMap.kext ... - Success` + `UTBMap.kext v1.1` ✅ |
| USB 端口 | 16 个端口对象（XH00 7 + XHC0 4 + XHC1 4 + XHC2 1）✅ |
| 蓝牙 | `State: On`、`Address: 02:1A:7D:DA:71:2A` ✅ |
| 补丁 | `patchreport.py`：无 enabled 且 0 替换的补丁 ✅ |
| USB 硬失败签名 | 最近 3 小时 `hardwareException` / `CommandRing timeout` / `createDevice: failed` / `powerOn: deadline` = **0** ✅ |
| ESP 哈希 | config `bb95cd36…`、UTBMap `998c7fbb…`、OpenCore `7754f6d6…` 均未变 ✅ |
| 探针连续性 | 317 条快照，10-21 07:59:39 → 10-22 23:52:59，跨重启无断档；端口集合 100% 恒定 ✅ |

## 需要如实记录的噪声

`audio.log` 里有 **14 条 `0xE00002EE`（kIOReturnIsoTooOld）**，集中在两次 `load1 ≈ 78`
的极端负载瞬态，属 CoreAudio 周期预算超限（麦克风路径），**不是 USB 控制器层故障**，
也不在这四类硬失败签名里。当前系统在正常负载下音频无断音（与 C5 已解决一致）。

## 剩余开放项

- R2：DBG-107 与 1.0.9 时代 config 的 schema 不匹配（`HideVerbose` 告警），待决策。
- R10：ESP 约 80%、356 个日志 89 MiB，清理另立任务（已有 `R10-LOG-INVENTORY.md`）。
- A4：SuperIO 型号未识别，等 MTKCAP 盘上的 `superio-probe.sh` 结果。
