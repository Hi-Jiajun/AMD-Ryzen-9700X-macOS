# AMD 定时器 / NVRAM 工作流交接

更新时间：2026-10-21 09:37 CST。

## 状态

第一轮只读基线完成；未写 ESP、未改 EFI/BIOS、未重启。

- timer 补丁：**已部署且已命中**，当前只允许 Darwin 24.6.0。
- 当前行为：**8× 等待上界未复现**；100/500/1000 µs 三轮中位
  `1.027 / 1.007 / 1.004×`，各轮最坏 `1.074 / 1.013 / 1.009×`。
- 单一因果：**尚未验收**。当前启动同时具备 timer 补丁与 USB S2 稀疏 map；
  没有 timer-only A/B。
- NVRAM：`DisableVariableWrite=true`；当前保持，不建议改成 `false`。
- 早期 panic：OpenCore 文件日志可用到 `EXITBS:START`；内核接管后、
  PanicReporter 就绪前的故障仍无可靠持久化通道。

## 关键证据

| 项 | 值 |
| --- | --- |
| 启动 | `2026-10-21 09:16:51 CST`，macOS 15.8.1 / 24H32 / Darwin 24.6.0 |
| config | `3dad2596c1b3457c956e0c1308a560a9ca1cd9cc9c1cfd705963ace3f57e4292` |
| OpenCore | `7754f6d6…273c8`，`DBG-107-2026-10-14` |
| BootKC | `cd013c14a7f21dbb8181fb06c44e079de8646b6a674abe21f6e47f42e8979af3` |
| 最新 OpenCore 日志 | `opencore-2026-10-21-011630.txt`，`810eac50…11679` |
| timer 日志 | `replace count - 1`，随后 `Kernel patcher result 29 ... Success` |
| TSC | `5,140,336,049 Hz` |
| FSB | `856,722,674 Hz`（`FID 6 DID 0 MaxBR 6`） |
| `busFCvtn2t` 原值 | `856,722,674`，等效 `199.4712916 MHz` |
| trial 立即数 | `0x06666666 = 107,374,182`，等效 `25.000000 MHz` |
| 离线 A/B 差异 | 仅零基偏移 `3388996..3388999` 四字节 |
| `nvram -p` | 795 B，14 行，`c9fee280…db5b2ea` |

完整目录：`outputs/20261021-amd-timing-nvram/`。

## 补丁判断

当前 `Kernel.Patch[29]`：

```text
Comment = hl | 9700X local LAPIC 25MHz calibration | Darwin 24.6.0 trial
Enabled = true
Base = _tsc_init
MinKernel = MaxKernel = 24.6.0
Count = 1
Limit = 51
```

本次启动日志在 `15.213s` 命中一次、`15.262s` 返回 `Success`。离线用当前 BootKC
和当前 config 的卷外副本做 `Enabled=false/true` A/B，结果只有目标立即数四字节变化。

结论排序：

1. 补丁应用/命中：**已生效**。
2. 当前等待行为：**8× 已消失**。
3. timer 对音频/调度的独立贡献：**待 A/B，不能声称已证明**。

## NVRAM / panic 结论

- `DisableVariableWrite=true` 与 `NVRAM.WriteFlash=true` 并存；前者拦 macOS runtime，
  后者是 OpenCore 自己的启动期写策略。
- 当前 `nvram -p` 只列普通名变量；vendor GUID 的 `boot-path`、`opencore-version`、
  `revpatch` 需显式查询，`nvram -p` 不是完整枚举器。
- `NVRWLog` 已从 ESP 和 `Kernel.Add` 移除，当前无法知道具体写者。
- `.contents.panic` 有一份 2026-10-13 的早期 panic 摘要，但对应完整
  `Kernel-*.panic` 缺失；PanicReporter 当前为空。
- OpenCore 日志 329 个、82 MiB，最新日志止于 `EXITBS:START` 后；ESP 已用 80%，
  剩 40 MiB。

## 最小下一步

推荐顺序：

1. **timer-only A/B**：冻结当前 config，只将 `Kernel.Patch[29].Enabled=false`，
   保持 USB S2、NVRAM、BootKC 和其他补丁不变；重启后跑同一
   `deadline-probe` 三轮，预期实时模式恢复约 8×。记录后立即恢复当前 config，
   再重启一次确认回到约 1×。
2. 该 A/B 需要 ESP 写入和两次重启，必须由 `amd-boot-config` 开窗口；本轮不执行。
   更保守的替代是制作外部启动介质承载 disable 版本，仍然需要重启。
3. NVRAM 本轮不改。只有确实需要写者归属时，才另开 `NVRWLog` 第一阶段窗口：
   保持 `DisableVariableWrite=true`，只记录、不转发。
4. panic 可观测性本轮维持现状。若要求覆盖内核接管后的早期故障，应优先设计串口或
   专用 crash capture，不能把普通 `PanicNoKextDump=false` 当作完整方案。

## 禁止 / 注意事项

- 不要改 `busFCvtn2t` 为其它值；当前 25 MHz 常量与行为一致。
- 不要把 NVRAM 接管和 timer A/B 放在同一启动。
- 不要用仓库旧 `efi/` 覆盖实际 ESP。
- 升级 macOS 后 timer 补丁会因版本限制跳过，必须重新审计，不能沿用本结论。
