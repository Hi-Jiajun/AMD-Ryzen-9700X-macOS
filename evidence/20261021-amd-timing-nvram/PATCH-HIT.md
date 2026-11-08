# timer 补丁命中审计

## 结论

判定：**已生效**。

这里的“已生效”指：

1. 本次启动的 OpenCore 在真实 BootKC 上命中 timer 补丁 1 次并返回 `Success`。
2. 当前 config 与当前 BootKC 的离线 A/B 只改变目标立即数 `_busFCvtn2t` 的 4 个字节。
3. 当前行为探针与 25 MHz 常量一致，7.93–7.99× 上界没有复现。

“已生效”不等于“音频改善已单因果归功于 timer”；后者仍需 timer-only A/B。

## 本次启动日志

日志：`/Volumes/OPENCORE/opencore-2026-10-21-011630.txt`

```text
14:797 OCAK: Read kernel version 24.6.0 (240600)
14:838 OCAK: BusFreq = 856722674Hz, BusFCvtt2n = 5013252743, BusFCvtn2t = 856722674
14:841 OCAK: TscFreq = 5140336049Hz, TscFCvtt2n = 835542123, TscFCvtn2t = 5140336049
15:213 OCAK: 64-bit hl | 9700X local LAPIC 25MHz calibration | Darwin 24.6.0 trial replace count - 1
15:262 OC: Kernel patcher result 29 for kernel (hl | 9700X local LAPIC 25MHz calibration | Darwin 24.6.0 trial) - Success
```

`BusFCvtn2t=856722674` 是 `ProvideCurrentCpuInfo` 在用户补丁之前写入的原始值，等效：

```text
856722674 × 1e9 / 2^32 = 199.4712916 MHz
```

用户补丁随后只覆写 `_tsc_init` 中给 `_busFCvtn2t` 的立即数：

```text
107374182 × 1e9 / 2^32 = 25.000000 MHz
```

因此不能在日志里期待后面再出现一行新的 `BusFCvtn2t=107374182`；该日志行发生在用户
Kernel/Patch 执行之前，后面紧跟的 `replace count - 1` 才是覆写证据。

## 当前 BootKC 离线 A/B

输入：

```text
BootKC sha256 cd013c14a7f21dbb8181fb06c44e079de8646b6a674abe21f6e47f42e8979af3
config sha256  3dad2596c1b3457c956e0c1308a560a9ca1cd9cc9c1cfd705963ace3f57e4292
```

在线读取当前 config，复制到卷外，并仅把 `Kernel.Add` 清空以便 ProcessKernel fixture 运行。
两组唯一语义差异是第 29 条 `Kernel.Patch.Enabled`：

```text
baseline config sha256 b01155700f3cc40b1d2ac97d1d393b595df5b616fee7302b939320864512204c
trial    config sha256 a93060ee87068ffb78227a8c8b9421b55bbd0c4e3c61c208b00f5cfd55b2dc6d
baseline patched kernel sha256 26ab0a3173fcebbd8b189b4950d4ee20377751cf166a44091a0b124b7eab0358
trial    patched kernel sha256 5d907f39481ee03794fdc83fbc72d14cdfe79d1458f3ab8c64747d43a6c2e22d
```

`cmp -l` 差异只有 4 个字节（位置为一基）：

```text
3388997 010 146
3388998 225 146
3388999 020 146
3389000 063 006
```

对应零基偏移 `3388996..3388999`：

```text
baseline: 08 95 10 33 00 00 00 00   # 0x33109508 = 856724744
trial:    66 66 66 06 00 00 00 00   # 0x06666666 = 107374182
```

offline fixture 的 DummyCpuInfo 固定使用 2026-10-17 日志的 FSB 856,724,744；当前启动的
原始 FSB 是 856,722,674。两个值只相差约 2 kHz，且 A/B 的唯一差异仍是 `_busFCvtn2t`
立即数，不影响“命中位置与替换值正确”的结论。

旧但对应同一 trial 二进制的反汇编保存于
`outputs/20261017-audio-current/offline-after/tsc-init-disassembly.txt`：

```text
_tsc_init:
ffffff8000453620: movabsq $0x33109508, %rax
ffffff800045362a: movq %rax, _busFreq(%rip)
...
ffffff8000453642: movabsq $0x06666666, %rax
ffffff800045364c: movq %rax, _busFCvtn2t(%rip)
```

当前 trial 的 patch 二进制哈希与该旧反汇编所用二进制同为
`5d907f39481ee03794fdc83fbc72d14cdfe79d1458f3ab8c64747d43a6c2e22d`。

## 版本与回滚边界

```text
Base       = _tsc_init
MinKernel  = 24.6.0
MaxKernel  = 24.6.0
Count      = 1
Limit      = 51
```

升级 macOS 后该补丁会自动跳过，不能沿用“当前仍生效”的结论；必须重新做 KC 命中审计。
单条回滚是把该条 `Enabled` 改为 `false`，但本轮不写 ESP。
