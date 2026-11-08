# Apple 原生 RTL8125 驱动（AppleEthernetRL）能不能移植到本机？

结论：**不能。保持现状（Sequoia 15.8.1 + OpenCore 注入 `RTL812xLucy` 1.1.1）。**

日期：2026-10-12。起因：M6 Mac mini 的 2.5G 网卡就是 RTL8125（`10ec:8125`），
驱动栏是 `com.apple.driver.AppleEthernetRL`——Apple 自己给这颗芯片出了驱动。

> ⚠️ **不要把这个驱动或其反编译产物放进仓库、也不要推送。**
> 它来自 Apple 的 Kernel Debug Kit（Apple 机密材料）。本文只记录**结论与匹配规则**，
> 不含 Apple 的二进制、符号表或反编译代码。

---

## 1. 取证来源（可复现）

| 步骤 | 命令 |
| --- | --- |
| 取 KDK（官方 CDN，约 1.2 GB；**比 17 GB 的 InstallAssistant.pkg 省得多**） | `ipsw download kdk --build 25G241` |
| 挂载 | `hdiutil attach -nobrowse -readonly Kernel_Debug_Kit_26.7.1_build_25G241.dmg` |
| 展开 pkg | `pkgutil --expand-full "/Volumes/Kernel Debug Kit/KernelDebugKit.pkg" <out>` |
| kext 位置 | `<out>/KDK.pkg/Payload/System/Library/Extensions/IONetworkingFamily.kext/Contents/PlugIns/AppleEthernetRL.kext` |

`ipsw` 从 `blacktop/ipsw` GitHub release 直接取（**不要** `brew install blacktop/tap/ipsw`，
那个 tap 未受信任；直接下 `ipsw_<ver>_macOS_x86_64.tar.gz` 更干净）。

本机下载速率实测：走代理 0.24 MB/s、直连 Apple CDN 0.01 MB/s →
17 GB 的 InstallAssistant.pkg 不可行，1.2 GB 的 KDK 可行。

## 2. 驱动的权威事实（读自 KDK 内 Info.plist / lipo）

| 项 | 值 |
| --- | --- |
| Bundle ID | `com.apple.driver.AppleEthernetRL` |
| 自述 | **"Apple RTL8125 Ethernet, Copyright 2023 Apple Inc."** |
| 版本 | 1.0（CFBundleVersion `1.0.0d1`） |
| **架构** | **`x86_64` + `arm64e`**（通用二进制 → 架构**不是**障碍） |
| 最低系统 | **`LSMinimumSystemVersion = 26.7`**（用 `macosx26.7.internal` SDK 构建） |
| 匹配 | `IOPCIClassMatch = 0x02000000&0xffffff00`、`IOPCIMatch = 0x812410ec&0xfffcffff` |
| 命中范围 | device id `0x8124 / 0x8125 / 0x8126 / 0x8127`（vender `0x10ec`）→ **本机 `10ec:8125` 会被它抢走** |
| 所属 | 已并入 `IONetworkingFamily.kext/Contents/PlugIns/`（与 Aquantia 同层，即 Dortania 说的"原生网卡"机制） |

## 3. 为什么不能移植（按致命程度排序）

1. **缺一个硬依赖的 kext。** `OSBundleLibraries` 要求
   **`com.apple.driver.AppleSkywalkAVB >= 1.0`**，而本机 Sequoia **完全没有这个 kext**：
   - Sequoia 的 `IONetworkingFamily/PlugIns` 只有 **8** 个 kext；
     Tahoe 有 **10** 个，多出来的正是 `AppleEthernetRL.kext` + `AppleSkywalkAVB.kext`。
   - `find /System/Library/Extensions -iname "AppleSkywalkAVB*"` → 空。
   - kext 依赖解析不了 → 内核**根本不会加载**它。这是第一道也是无法绕过的一道墙。
2. **父类不存在。** 反编译与符号显示 `AppleEthernetRL` 的父类是
   **`AppleSkywalkAVBEthernetInterface`**，而该类正由上面那个缺失的
   `AppleSkywalkAVB.kext` 导出（该 kext 自己也是 `LSMinimumSystemVersion 26.7`）。
3. **用的是新一代 Skywalk 对象模型。** 它按 `IOSkywalkPacket` /
   `IOSkywalkRxSubmissionQueue` / `IOSkywalkTxCompletionQueue` / `IOSkywalkNetworkKDPPoller`
   这套对象工作。本机 `IOSkywalkFamily` 虽然版本号同为 `1.0`，但对象模型是旧的
   （同一版本号 ≠ 同一 API）——这正是社区那句注释的由来：
   *"AppleEthernetRL depends on IOSkywalkFamily, crashes with downgraded Skywalk"*。
4. **设计上依赖协处理器。** `AppleEthernetRLIPC : IOService` 通过
   `psaCall` / `mailboxClientCall` / `writeToSRAM` / `copyFromSRAM` 与协处理器信箱通信，
   还有带 secure / non-secure slot 的 `FirmwareUpdater`。**x86_64 切片里同样带着这套 IPC
   （实测 x86_64 slice 含 `AppleEthernetRLIPC` / `SRAM` 字符串）**。黑苹果没有那个协处理器，
   固件/NVRAM/MAC 那一段起不来。
5. **数字上"看似满足"的那些依赖是假象。** 逐条对照本机 Sequoia：

   | 依赖 | Apple 要求 | 本机 Sequoia | 数值判定 |
   | --- | --- | --- | --- |
   | `com.apple.driver.AppleSkywalkAVB` | 1.0 | **不存在** | ❌ **致命** |
   | `com.apple.iokit.IONetworkingFamily` | ≥ 1.5 | 3.4 | ✓ |
   | `com.apple.iokit.IOPCIFamily` | ≥ 1.7 | 2.9 | ✓ |
   | `com.apple.iokit.IOSkywalkFamily` | ≥ 1.0 | 1.0 | ✓ 版本号，但 API 面不同（见第 3 条） |
   | `com.apple.iokit.IOTimeSyncFamily` | ≥ 500.10 | 1340.15 | ✓ |
   | `com.apple.kpi.private` | ≥ 22.0 | 24.6.0 | ✓ |
   | `com.apple.kpi.{bsd,iokit,libkern,mach}` | ≥ 13.0.0 | 24.6.0 | ✓ |

   （`OSBundleLibraries_arm64e` 另加 `AppleARMPlatform`，x86_64 不需要。）
   所以**只有第 1 条是硬墙**，但也正是它一票否决。

## 4. 就算硬移植成功，对我们也只会更差

它按 `IOPCIMatch` 抢 `10ec:8124/8125/8126/8127`。在 Tahoe 黑苹果上，
社区的做法是**在 `Kernel → Block` 里屏蔽 `com.apple.driver.AppleEthernetRL`**
（RapidEFI 已自动化这条），否则 Lucy 系列 kext 拿不到设备。
也就是说 Apple 这个驱动在非 Apple 硬件上既抢设备又崩（"crashes with downgraded Skywalk"）。

## 5. 顺带确认的另一件事

它**不在 Sequoia 里**，所以本机现在**不存在"Apple 原生驱动 vs Lucy"抢设备的问题**——
这正是 `RTL812xLucy` 1.1.1 在本机能稳定工作的原因之一。
升到 macOS 26 才会遇到这个冲突（届时需要 Block 它，且要面对 VT-d / SMBIOS 2019+ /
AppleHDA 缺失等一堆 Tahoe 门槛）。

## 6. 建议

- **不动。** 继续用 Sequoia 15.8.1 + `RTL812xLucy` 1.1.1（OpenCore 注入）。
- 想借鉴 Apple 的实现也没价值：它的寄存器级代码同样是 OpenBSD `re(4)` 血统
  （`re_check_mac_version` / `re_hw_start_unlock_8125` / `re_get_default_imr_8125`），
  Mieze 那边本来就有；Apple 的真正增量在固件/协处理器那一侧，那部分**搬不过来**。
- 只有升到 macOS 26 时才需要重新评估，并且第一步就是**确认要不要 Block 它**。

## 7. 证据留存

KDK 展开物在 `/tmp/kdk_expand/out`（约 4.7 GB）、dmg 在 `/tmp/ipswdl/`（1.18 GB）。
**结论已记录在本文，这些临时文件用完即可删（本机磁盘 816 GB 富余，留着也不碍事）。**
再次强调：不要把它们提交进仓库或外发。
