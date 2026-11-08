# Kernel / Booter 补丁与 kext 注册表（2026-10-22 当前态）

本表由 Python `plistlib` 直接解析**当前 ESP config 副本**生成，不是从旧文档抄的；`Enabled`、字节模式、版本窗口均取自实测配置。

| 项 | 值 |
|---|---|
| 当前 config 副本 | `outputs/20261020-amd-boot-config/private-backups/current-20261022/config.plist` |
| 当前 sha256 | `bb95cd36c24f89bcfff5bc4f51d414ad169f43f486d86f9f629bcf20fd222812` |
| baseline config | `outputs/20261020-amd-boot-config/private-backups/esp-disk0s1-20261020-1146/EFI/OC/config.plist` |
| baseline sha256 | `3dad2596c1b3457c956e0c1308a560a9ca1cd9cc9c1cfd705963ace3f57e4292` |
| 实测日志 | `/tmp/esp-q/opencore-2026-10-22-031825.txt`（摘要见 `outputs/20261022-amd-platform/patchreport-20261022.json`） |
| SMBIOS 身份 | **本文件不含任何身份值**：序列号、主板序列号、系统 UUID、网卡固件区标识均未写入（连字段名也不出现） |

> 采集边界：只读本地副本与日志摘要；未访问 ESP/EFI 设备节点，未修改任何 config，未重启、未加载 kext。

## 0. 规模总览

| 分区 | 条数 | 启用 | 备注 |
|---|---|---|---|
| Kernel/Patch | 30 | 26 | 日志命中 17 条、版本跳过 9 条、「替换 0 处」0 条 |
| Kernel/Add | 23 | 19 | 含 2026-10-21 新增的 MT7922USBWakePolicy |
| Booter/Patch | 1 | 0 | 唯一一条为禁用 |
| Booter/Quirks | 22 | — | 与 baseline 逐键一致 |
| Kernel/Quirks | 23 | — | 与 baseline 逐键一致 |

## 1. Kernel > Patch（30 条）

`C/L/S` = `Count` / `Limit` / `Skip`；OpenCore 语义里 `Count=0` 是**不限次数**（全量扫描），不是「不生效」。`En` 为 config 里的 `Enabled`。

| # | En | 用途 | Comment | Min–Max | Base | C/L/S | Find → Replace（摘要） | 本次日志实测 |
|---|---|---|---|---|---|---|---|---|
| 1 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| Force cpuid_cores_per_package to constant (user-specified) \| 10.13-10.14 | 17.0.0–18.99.99 | `_cpuid_set_info` | 1/0/0 | `c1e81a000000` → `b80800000000` | 跳过：内核版本不在窗口内（日志 idx 0） |
| 2 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| Force cpuid_cores_per_package to constant (user-specified) \| 10.15-11.0 | 19.0.0–20.99.99 | `_cpuid_set_info` | 1/0/0 | `c1e81a000000` → `ba0800000000` | 跳过：内核版本不在窗口内（日志 idx 1） |
| 3 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| Force cpuid_cores_per_package to constant (user-specified) \| 12.0-13.2 | 21.0.0–22.3.99 | `_cpuid_set_info` | 1/0/0 | `c1e81a000000` → `ba0800000090` | 跳过：内核版本不在窗口内（日志 idx 2） |
| 4 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| Force cpuid_cores_per_package to constant (user-specified) \| 13.3+ | 22.4.0–25.99.99 | `_cpuid_set_info` | 1/0/0 | `c1e81a0000` → `ba08000000` | Success，实际替换 1 处（日志 idx 3） |
| 5 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _commpage_populate \| Remove rdmsr \| 10.13+ | 17.0.0–25.99.99 | `(全局扫描)` | 1/0/0 | `b9a00100000f32` → `66906690669090` | Success，实际替换 1 处（日志 idx 4） |
| 6 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_cache_info \| Set CPUID proper instead of 4 \| 10.13+ | 17.0.0–25.99.99 | `(全局扫描)` | 1/0/0 | `b80400000044…89f14489`（10B） → `b81d00008044…89f14489`（10B） | Success，实际替换 1 处（日志 idx 5） |
| 7 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_generic_info \| Remove wrmsr(0x8B) \| 10.13+ | 17.0.0–25.99.99 | `(全局扫描)` | 1/0/0 | `b98b00000031…31d20f30`（11B） → `669066906690…90669090`（11B） | Success，实际替换 1 处（日志 idx 6） |
| 8 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_generic_info \| Replace rdmsr(0x8B) with constant 186 \| 10.13+ | 17.0.0–25.99.99 | `(全局扫描)` | 1/0/0 | `b98b0000000f32` → `baba0000006690` | Success，实际替换 1 处（日志 idx 7） |
| 9 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_generic_info \| Set flag=1 \| 10.13+ | 17.0.0–25.99.99 | `(全局扫描)` | 1/0/0 | `b9170000000f…1280e207`（13B） → `b201660f1f84…00006690`（13B） | Success，实际替换 1 处（日志 idx 8） |
| 10 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_generic_info \| Disable check to allow leaf7 \| 10.13+ | 17.0.0–23.99.99 | `(全局扫描)` | 1/0/0 | `003a0f82` → `00000f82` | 跳过：内核版本不在窗口内（日志 idx 9） |
| 11 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_generic_info \| Disable check to allow leaf7 \| 15.0+ | 24.0.0–25.99.99 | `(全局扫描)` | 1/0/0 | `00050f82` → `00000f82` | Success，实际替换 1 处（日志 idx 10） |
| 12 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_info \| GenuineIntel to AuthenticAMD \| 10.13-11.0 | 17.0.0–20.99.99 | `(全局扫描)` | 1/0/0 | `47656e75696e…74656c00`（13B） → `41757468656e…414d4400`（13B） | 跳过：内核版本不在窗口内（日志 idx 11） |
| 13 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | Goldfish64, algrey \| Bypass GenuineIntel check panic \| 12.0+ | 21.0.0–25.99.99 | `_cpuid_set_info` | 1/0/0 | `000000000000…31d2b301`（10B） → `909090909090…31d2b301`（10B） | Success，实际替换 1 处（日志 idx 12） |
| 14 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_cpufamily \| Force CPUFAMILY_INTEL_PENRYN \| 10.13-11.2 | 17.0.0–20.3.0 | `(全局扫描)` | 1/0/0 | `31db803d0000…00067500`（11B） → `bbbc4fea78e9…00000090`（11B） | 跳过：内核版本不在窗口内（日志 idx 13） |
| 15 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _cpuid_set_cpufamily \| Force CPUFAMILY_INTEL_PENRYN \| 11.3+ | 20.4.0–25.99.99 | `_cpuid_set_info` | 1/0/0 | `803d000000000675` → `babc4fea7831dbeb` | Success，实际替换 1 处（日志 idx 14） |
| 16 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey \| _i386_init \| Remove 3 rdmsr calls \| 10.13+ | 17.0.0–25.99.99 | `(全局扫描)` | 0/0/0 | `b9990100000f…c94531c0`（42B） → `660f1f840000…1f440000`（42B） | Success，实际替换 3 处（日志 idx 15） |
| 17 | `True` | AMD_Vanilla：CPUID/MSR/早期内核兼容 | algrey, XLNC \| Remove version check and panic \| 10.13+ | 17.0.0–25.99.99 | `(全局扫描)` | 1/0/0 | `25fc00000083f813` → `25fc0000000f1f00` | Success，实际替换 1 处（日志 idx 16） |
| 18 | `True` | IOPCIFamily：10-bit tag（AM5） | CaseySJ \| probeBusGated \| Disable 10 bit tags \| 12.0-15.x | 21.0.0–24.99.99 | `__ZN11IOPCIBridge13probeBusGatedEP14probeBusParams` | 1/0/0 | `e0117200` → `00000300` | Success，实际替换 1 处（日志 idx 17） |
| 19 | `True` | IOPCIFamily：10-bit tag（AM5） | CaseySJ \| probeBusGated \| Disable 10 bit tags \| 26.0+ | 25.0.0–25.99.99 | `__ZN11IOPCIBridge13probeBusGatedEP14probeBusParams` | 1/0/0 | `e0117340` → `00000200` | 跳过：内核版本不在窗口内（日志 idx 18） |
| 20 | `True` | IOPCIFamily：AM5 PCI 总线枚举 | CaseySJ \| IOPCIIsHotplugPort \| Fix PCI bus enumeration on AM5 \| 13.0+ | 22.0.0–25.99.99 | `__ZN17IOPCIConfigurator18IOPCIIsHotplugPortEP16IOPCIConfigEntry` | 1/0/0 | `8400754b` → `0000eb00` | Success，实际替换 1 处（日志 idx 19） |
| 21 | `True` | 调度：非单调时间 panic | Visual \| thread_quantum_expire, thread_unblock, thread_invoke \| Remove non-monotonic time panic \| 12.0+ | 21.0.0–25.99.99 | `(全局扫描)` | 3/0/0 | `480000000200…00000000`（20B） → `000000000000…66906690`（20B） | Success，实际替换 3 处（日志 idx 20） |
| 22 | `True` | 调度：非单调时间 panic | Visual \| thread_invoke, thread_dispatch \| Remove non-monotonic time panic \| 12.0-26.3 | 21.0.0–25.3.99 | `(全局扫描)` | 2/0/0 | `480000800400…00000000`（13B） → `000000000000…66906690`（13B） | Success，实际替换 2 处（日志 idx 21） |
| 23 | `True` | 调度：非单调时间 panic | laobamac \| thread_invoke, thread_dispatch \| Remove non-monotonic time panic \| 26.4+ | 25.4.0–25.99.99 | `(全局扫描)` | 2/0/0 | `480000900400…00000000`（13B） → `000000000000…66906690`（13B） | 跳过：内核版本不在窗口内（日志 idx 22） |
| 24 | `True` | PAT/MTRR 修正 | algrey \| _mtrr_update_action \| fix PAT \| 10.13+ | 17.0.0–23.99.99 | `(全局扫描)` | 0/0/0 | `89c081e2ffff…77020000`（19B） → `b977020000b8…0f1f4000`（19B） | 跳过：内核版本不在窗口内（日志 idx 23） |
| 25 | `False` | PAT/MTRR 修正 | Shaneee \| _mtrr_update_action \| Fix PAT \| 10.13+ | 17.0.0–23.99.99 | `(全局扫描)` | 0/0/0 | `89c081e2ffff…77020000`（19B） → `b977020000b8…0f300f09`（19B） | 未在本次日志中（Enabled=false） |
| 26 | `True` | PAT/MTRR 修正 | Algrey / Zormeister \| _mtrr_update_action \| Fix PAT \| 15.0+ | 24.0.0–25.99.99 | `(全局扫描)` | 0/0/0 | `89c081e2ffff…00000f30`（16B） → `89c0b8060107…0f309090`（16B） | Success，实际替换 2 处（日志 idx 25） |
| 27 | `False` | PAT/MTRR 修正 | Shaneee / Zormeister \| _mtrr_update_action \| Fix PAT \| 15.0+ | 24.0.0–25.99.99 | `(全局扫描)` | 0/0/0 | `89c081e2ffff…00000f30`（16B） → `89c0b8060606…0f309090`（16B） | 未在本次日志中（Enabled=false） |
| 28 | `False` | USB 口上限 15→17 | hl \| USB port limit 15 -> 17 \| setPortLocation cmp 0xF -> 0x11 (macOS 15.8.1/24H32) | Any | `__ZN16AppleUSBHostPort15setPortLocationEj` | 1/0/0 | `4183ff0f0f87` → `4183ff110f87` | 未在本次日志中（Enabled=false） |
| 29 | `False` | USB 口上限 15→17 | hl \| USB port limit 15 -> 17 \| AppleUSBXHCI::createPorts cmp 0xF -> 0x11 (macOS 15.8.1/24H32) | Any | `__ZN12AppleUSBXHCI11createPortsEv` | 1/0/0 | `83fb0f0f83` → `83fb110f83` | 未在本次日志中（Enabled=false） |
| 30 | `True` | Zen5 LAPIC/TSC 标定（trial） | hl \| 9700X local LAPIC 25MHz calibration \| Darwin 24.6.0 trial | 24.6.0–24.6.0 | `_tsc_init` | 1/51/0 | `48b800000000…00000000`（51B） → `000000000000…00000000`（51B） | Success，实际替换 1 处（日志 idx 29） |

两条注记：

- `#15` 的 `Base` 在 config 里带一个尾随空格（`_cpuid_set_info `），但本次日志对它是 Success + 1 处替换，说明实际匹配未受影响；保留原样即可，不必为它单独改配置。
- `patchreport` 的 `applied` 里另有 `PanicKextDump` / `PowerStateTimeout` / `CustomSmbiosGuid` / `Set core count to thread count` / `ProvideCurrentCpuInfoZeroMsrThreadCoreCount` —— 属于 quirk 与 CPU 信息注入的生效计数，不是 Kernel/Patch 条目。

## 2. Booter > Patch（1 条）

| # | En | Comment | Identifier | Find → Replace | 说明 |
|---|---|---|---|---|---|
| 1 | `False` | macOS to hacOS | `Apple` | `6d61634f53` → `6861634f53` | 禁用项（boot 期标识改写试验），不影响启动；保留仅为注册完整性 |

`Booter > MmioWhitelist` 共 1 条：`0xFD000000`（MMIO 0xFD000000 (PCIe MMIO high window 0xF7000000-0xFEE00000)，Enabled=True）。

## 3. Booter > Quirks（22 项）

| Quirk | 当前值 | 与 baseline |
|---|---|---|
| `AllowRelocationBlock` | `False` | 一致 |
| `AvoidRuntimeDefrag` | `True` | 一致 |
| `ClearTaskSwitchBit` | `False` | 一致 |
| `DevirtualiseMmio` | `True` | 一致 |
| `DisableSingleUser` | `False` | 一致 |
| `DisableVariableWrite` | `True` | 一致 |
| `DiscardHibernateMap` | `False` | 一致 |
| `EnableSafeModeSlide` | `True` | 一致 |
| `EnableWriteUnprotector` | `False` | 一致 |
| `FixupAppleEfiImages` | `True` | 一致 |
| `ForceBooterSignature` | `False` | 一致 |
| `ForceExitBootServices` | `False` | 一致 |
| `ProtectMemoryRegions` | `False` | 一致 |
| `ProtectSecureBoot` | `False` | 一致 |
| `ProtectUefiServices` | `False` | 一致 |
| `ProvideCustomSlide` | `True` | 一致 |
| `ProvideMaxSlide` | `0` | 一致 |
| `RebuildAppleMemoryMap` | `True` | 一致 |
| `ResizeAppleGpuBars` | `0` | 一致 |
| `SetupVirtualMap` | `False` | 一致 |
| `SignalAppleOS` | `False` | 一致 |
| `SyncRuntimePermissions` | `True` | 一致 |

有效项：`AvoidRuntimeDefrag` / `DevirtualiseMmio` / `DisableVariableWrite` / `EnableSafeModeSlide` / `FixupAppleEfiImages` / `ProvideCustomSlide` / `RebuildAppleMemoryMap` / `SyncRuntimePermissions`；数值项 `ProvideMaxSlide=0`、`ResizeAppleGpuBars=0`。

## 4. Kernel > Quirks（23 项）

| Quirk | 当前值 | 与 baseline |
|---|---|---|
| `AppleCpuPmCfgLock` | `False` | 一致 |
| `AppleXcpmCfgLock` | `False` | 一致 |
| `AppleXcpmExtraMsrs` | `False` | 一致 |
| `AppleXcpmForceBoost` | `False` | 一致 |
| `CustomPciSerialDevice` | `False` | 一致 |
| `CustomSMBIOSGuid` | `True` | 一致 |
| `DisableIoMapper` | `False` | 一致 |
| `DisableIoMapperMapping` | `False` | 一致 |
| `DisableLinkeditJettison` | `True` | 一致 |
| `DisableRtcChecksum` | `False` | 一致 |
| `ExtendBTFeatureFlags` | `False` | 一致 |
| `ExternalDiskIcons` | `False` | 一致 |
| `ForceAquantiaEthernet` | `False` | 一致 |
| `ForceSecureBootScheme` | `False` | 一致 |
| `IncreasePciBarSize` | `False` | 一致 |
| `LapicKernelPanic` | `False` | 一致 |
| `LegacyCommpage` | `False` | 一致 |
| `PanicNoKextDump` | `True` | 一致 |
| `PowerTimeoutKernelPanic` | `True` | 一致 |
| `ProvideCurrentCpuInfo` | `True` | 一致 |
| `SetApfsTrimTimeout` | `-1` | 一致 |
| `ThirdPartyDrives` | `False` | 一致 |
| `XhciPortLimit` | `False` | 一致 |

## 5. Kernel > Add（23 条，含新增 kext）

| # | En | BundlePath | ExecutablePath | Min–Max | 版本 / 用途 |
|---|---|---|---|---|---|
| 1 | `True` | `Lilu.kext` | `Contents/MacOS/Lilu` | Any | V1.7.3 |
| 2 | `True` | `AppleALC.kext` | `Contents/MacOS/AppleALC` | Any | V1.9.9 |
| 3 | `True` | `AppleMCEReporterDisabler.kext` | （codeless：纯 Info.plist） | Any | V1.2 |
| 4 | `True` | `NVMeFix.kext` | `Contents/MacOS/NVMeFix` | 18.0.0–Any | V1.1.4 |
| 5 | `True` | `RTL812xLucy.kext` | `Contents/MacOS/RTL812xLucy` | Any | V1.1.100 |
| 6 | `True` | `RestrictEvents.kext` | `Contents/MacOS/RestrictEvents` | Any | V1.1.7 |
| 7 | `True` | `USBToolBox.kext` | `Contents/MacOS/USBToolBox` | Any | V1.2.0 |
| 8 | `False` | `UTBDefault.kext` | （codeless：纯 Info.plist） | Any | V1.0 |
| 9 | `True` | `VirtualSMC.kext` | `Contents/MacOS/VirtualSMC` | Any | V1.3.9 |
| 10 | `True` | `WhateverGreen.kext` | `Contents/MacOS/WhateverGreen` | Any | V1.7.2 |
| 11 | `True` | `UTBMap.kext` | （codeless：纯 Info.plist） | Any | V1.0 \| NullMoth USB map |
| 12 | `True` | `IOSkywalkFamily.kext` | `Contents/MacOS/IOSkywalkFamily` | Any | AirportMtk (MT7922 IO80211 bring-up) |
| 13 | `True` | `IO80211FamilyLegacy.kext` | `Contents/MacOS/IO80211FamilyLegacy` | Any | AirportMtk (MT7922 IO80211 bring-up) |
| 14 | `True` | `AirportMtk.kext` | `Contents/MacOS/AirportMtk` | Any | AirportMtk (MT7922 IO80211 bring-up) |
| 15 | `True` | `AMFIPass.kext` | `Contents/MacOS/AMFIPass` | Any | AMFIPass (NullMoth driver) |
| 16 | `True` | `MT7922USBWakePolicy.kext` | （codeless：纯 Info.plist） | 24.0.0–24.99.99 | Disable MT7922 USB remote wake to prevent XH00/HS11 recovery storm **← 2026-10-21 新增** |
| 17 | `True` | `SimpleMtkBtFw.kext` | `Contents/MacOS/SimpleMtkBtFw` | Any | SimpleMtkBtFw 1.0.0 (MediaTek USB BT firmware loader) |
| 18 | `True` | `SimpleMtkBtPatcher.kext` | `Contents/MacOS/SimpleMtkBtPatcher` | Any | SimpleMtkBtPatcher (Lilu plugin: sends the Random-Address HCI command, resends LE SCAN PARAM) |
| 19 | `True` | `BlueToolFixup.kext` | `Contents/MacOS/BlueToolFixup` | Any | BlueToolFixup 2.7.2 (acidanthera) - lets BlueTool accept a non-Apple/Broadcom controller; needed because no IOBluetoothHCIController is created for the MT7922 |
| 20 | `True` | `NMHDA.kext` | `Contents/MacOS/NMHDA` | Any | NullMoth NMHDA 1.0.0 (HDMI/DP audio test) |
| 21 | `False` | `NMProbeAudio.kext` | `Contents/MacOS/NMProbeAudio` | Any | NM audio EDID probe (throwaway) |
| 22 | `False` | `NMHDALink.kext` | `Contents/MacOS/NMHDALink` | Any | NullMoth NMHDALink 1.0.0 (display-audio link for AppleHDA) |
| 23 | `False` | `NMHDAVol.kext` | `Contents/MacOS/NMHDAVol` | Any | NullMoth NMHDAVol 1.0.0 (software volume for display audio) |

加载次序契约：Lilu 必须最先；`AirportMtk` 依赖 `IOSkywalkFamily` + `IO80211FamilyLegacy` + `AMFIPass`；MT7922 蓝牙链为 `SimpleMtkBtFw` → `SimpleMtkBtPatcher` → `BlueToolFixup`。新增的 `MT7922USBWakePolicy` 位于 `AMFIPass` 之后、蓝牙链之前，不打断上述依赖。

## 6. 其它未被本次改动触及的固定项（完整性补充）

| 分区 | 内容 |
|---|---|
| Kernel > Block（2 条，均为 Exclude） | `com.apple.iokit.IOSkywalkFamily`（Allow IOSkywalk Downgrade；Strategy=Exclude；Min=23.0.0；Enabled=True）；`com.apple.iokit.IONDRVSupport`（boot framebuffer IONDRVFramebuffer steals index 0 from NVRMFB；Strategy=Exclude；Min=Any；Enabled=True） |
| Kernel > Emulate | `DummyPowerManagement=True`；`Cpuid1Data` / `Cpuid1Mask` 为空；版本窗口不限 |
| Kernel > Force | 0 条（空） |
| Kernel > Scheme | `CustomKernel=False`；`FuzzyMatch=True`；`KernelArch=Auto`；`KernelCache=Auto` |

## 7. baseline → 当前：唯一差异

**一句话：只多了一条 codeless kext（`MT7922USBWakePolicy.kext`，插在 `Kernel/Add` 第 16 位），补丁、quirk、其它全部未动。**

逐键 diff 结论（`plistlib` 把整棵树展开成「路径 → 值」再比对）：

| 分区 | 结果 |
|---|---|
| `Kernel > Patch`（30 条） | 完全一致 —— `Comment` / `Find` / `Replace` / `Count` / 版本窗口逐字节相同 |
| `Kernel > Add` | 22 → 23 条；把当前第 16 条剔除后与 baseline 列表**逐项相等**，即其余 22 条字段与相对顺序均未变，只是其后条目下标整体后移一位 |
| `Booter`（Patch / Quirks / MmioWhitelist） | 完全一致 |
| `Kernel > Quirks` / `Block` / `Emulate` / `Scheme` / `Force` | 完全一致 |
| 顶层其它段（ACPI / DeviceProperties / Misc / NVRAM / PlatformInfo / UEFI） | 完全一致 |

即：2026-10-21 10:50 那次改动是一次**纯增量 kext 注册**——没有任何补丁被改写、启用或停用，也没有任何 quirk 被拨动。

新增条目逐字段：

| 字段 | 值 |
|---|---|
| `BundlePath` | `MT7922USBWakePolicy.kext` |
| `Comment` | `Disable MT7922 USB remote wake to prevent XH00/HS11 recovery storm` |
| `Enabled` | `True` |
| `Arch` | `Any` |
| `ExecutablePath` | （空：codeless kext，无副本可执行文件） |
| `PlistPath` | `Contents/Info.plist` |
| `MinKernel` | `24.0.0` |
| `MaxKernel` | `24.99.99` |

用途（读自 `outputs/20261021-mt7922-wake-policy/MT7922USBWakePolicy.kext/Contents/Info.plist`，`CFBundleIdentifier=local.hiliang.MT7922USBWakePolicy`，v1.0.0）：两个 `AppleUSBHostMergeProperties` personality，`IOProviderClass=IOUSBHostDevice` 且 `IOPropertyMatch.locationID=0x600000`，经 `IOProviderMergeProperties` 给该设备打上 `kUSBRemoteWakeOverride=0`，即**关闭 MT7922 的 USB 远程唤醒**，用来掐掉 XH00/HS11 反复重入的恢复风暴。两条 personality 分别匹配 `0x0A12/0x0616`（CSR 形态）与 `0x0E8D/0x0616`（原厂 MediaTek 形态），覆盖同一 locationID 下的两种枚举结果。

版本窗口 `24.0.0–24.99.99` 只在 Darwin 24（macOS 15.x）生效，与本次实测内核 24.6.0 对齐；升到 Darwin 25+ 会自动跳过。

## 8. 下一次 macOS 更新要重点复核的 5 条

| # | 条目 | 一句话理由 |
|---|---|---|
| 1 | Kernel/Patch #30 `hl \| 9700X local LAPIC 25MHz calibration` | `Min=Max=24.6.0` 加 `Limit=51` 的窄窗口 trial，升级后自动失效，必须确认新内核 `_tsc_init` 是否仍需标定，否则时间与看门狗行为会悄悄回退。 |
| 2 | Kernel/Add #16 `MT7922USBWakePolicy.kext`（本轮新增） | 窗口封在 `24.99.99`，升到 Darwin 25 后静默不加载，XH00/HS11 恢复风暴可能复发，升级后要用 `ioreg` / `log show` 确认它是否仍生效。 |
| 3 | Kernel/Patch #24–#27 `_mtrr_update_action` PAT 四条 | 生效条 `Count=0` 是不限次数的全局模式匹配，内核一改就可能在多处误打或漏打，需用 `patchcheck` 重新核对命中数。 |
| 4 | Kernel/Patch #1–#4 `Force cpuid_cores_per_package` 分段 | 四段为 `17.0.0–18.99.99 / 19.0.0–20.99.99 / 21.0.0–22.3.99 / 22.4.0+`，上限都停在 `25.99.99`，说明 Darwin 26 的段还没补，升级当天要先把这段补上。 |
| 5 | Kernel/Patch #18–#20 IOPCIFamily 三条（`probeBusGated` / `IOPCIIsHotplugPort`） | `Base` 用的是 demangled C++ 符号名，XNU 一改实现或签名就整条匹配不上；本次日志里 `probeBusGated \| 26.0+` 已因版本窗口外被跳过，升级后要确认三条各命中一次。 |

附注：`#28/#29` 的 USB 15→17 口上限两条仍为 `Enabled=false`，本机 USB 映射走 `UTBMap.kext`；若更新后出现掉口再考虑启用。

