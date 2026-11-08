# 主机 / 启动 / 补丁基线

采样时间：2026-10-21 09:33:58 CST 至 09:36:08 CST。全程只读。

## 运行态

```text
Darwin ... 24.6.0 ... RELEASE_X86_64 x86_64
ProductVersion: 15.8.1
BuildVersion:   24H32
kern.boottime: { sec = 1792545411, usec = 753215 } Wed Oct 21 09:16:51 2026
hw.model: MacPro7,1
machdep.cpu.brand_string: AMD Ryzen 7 9700X 8-Core Processor
machdep.cpu.vendor: AuthenticAMD
machdep.xcpm.mode: 0
hw.cpufrequency: 5140000000
hw.cpufrequency_max: 5140000000
hw.busfrequency: 3424000000
machdep.tsc.frequency: 5140336049
machdep.tsc.nanotime.scale: 835542123
machdep.tsc.nanotime.shift: 0
hw.tbfrequency: 1000000000
machdep.cpu.thermal.invariant_APIC_timer: 1
```

`hw.cpufrequency=5.14 GHz` 是当前 TSC 校准得到的报告值，不等于 Zen5 的实际 P-state。
`hw.busfrequency=3.424 GHz` 也不能替代 OpenCore 的 FSB 读数；本机校准证据以 OpenCore
日志中的 `FSBFrequency=856722674 Hz` 为准。

## 文件身份

| 项 | 大小 | SHA-256 |
| --- | ---: | --- |
| `/Volumes/OPENCORE/EFI/OC/config.plist` | 56,063 B | `3dad2596c1b3457c956e0c1308a560a9ca1cd9cc9c1cfd705963ace3f57e4292` |
| `/Volumes/OPENCORE/EFI/OC/OpenCore.efi` | 872,448 B | `7754f6d66b23ad2410e1a6ef9ee63b7d527ce2dd8e8dbf58bca9c657f87273c8` |
| `/Volumes/OPENCORE/EFI/BOOT/BOOTx64.efi` | 53,248 B | `d3a4854c82f183e2a73976e69490ce334bbaf8055c8efaa78f4d4626fddb10f3` |
| 最新 OpenCore 日志 | 262,144 B | `810eac50c9e5bb050b658a58b79a840c413d470c5847bfbaaa0bd158e9411679` |
| 当前 BootKC | 66,125,824 B | `cd013c14a7f21dbb8181fb06c44e079de8646b6a674abe21f6e47f42e8979af3` |

最新 OpenCore 日志：

```text
/Volumes/OPENCORE/opencore-2026-10-21-011630.txt
mtime: 2026-10-21 01:16:50 UTC = 09:16:50 CST
```

日志载入配置 56,063 B，与当前 config 大小一致；当前 config 的 mtime 虽为 09:27:30，
但内容哈希仍等于 2026-10-20 冻结基线，说明本次启动后没有发生内容变化。

## CPU / TSC / FSB 日志值

```text
00:109 OCCPU: FID 6 DID 0 Divisor 0 MaxBR 6
00:110 OCCPU: CPUFrequencyFromTSC  5140336049Hz  5140MHz
00:111 OCCPU: CPUFrequency  5140336049Hz  5140MHz
00:111 OCCPU: FSBFrequency   856722674Hz   856MHz
14:838 OCAK: BusFreq = 856722674Hz, BusFCvtt2n = 5013252743, BusFCvtn2t = 856722674
14:841 OCAK: TscFreq = 5140336049Hz, TscFCvtt2n = 835542123, TscFCvtn2t = 5140336049
```

这里的 `FID=6 / MaxBR=6` 是 OpenCore 对 Zen5 12-bit FID 的截断后读数；不能把它直接解释为
CPU 真实倍频 6。`busFCvtn2t=856722674` 是 quirk 阶段写入值，随后被用户 Kernel/Patch
覆写；见 `PATCH-HIT.md`。

## 关键 quirk / 策略

| 键 | 当前值 |
| --- | --- |
| `Kernel.Quirks.ProvideCurrentCpuInfo` | `true` |
| `Kernel.Quirks.LapicKernelPanic` | `false` |
| `Kernel.Quirks.PanicNoKextDump` | `true` |
| `Kernel.Quirks.PowerTimeoutKernelPanic` | `true` |
| `Booter.Quirks.DisableVariableWrite` | `true` |
| `Booter.Quirks.DevirtualiseMmio` | `true` |
| `Booter.Quirks.RebuildAppleMemoryMap` | `true` |
| `Booter.Quirks.SyncRuntimePermissions` | `true` |
| `Misc.Security.ExposeSensitiveData` | `7` |
| `Misc.Debug.Target` | `71` |
| `Misc.Debug.LogModules` | `*` |

## 测量时的系统负担

09:36 的 `uptime` 为 `load averages: 4.90 5.85 6.96`。本轮只解释线程时约束模式的
短等待倍率；非实时模式仍有正常调度/合并抖动，不能据其绝对上界判失败。
