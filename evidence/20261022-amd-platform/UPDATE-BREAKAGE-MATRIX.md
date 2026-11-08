# macOS 升级破坏面矩阵（版本绑定项全量清点）

一句话：**这台机器上有 30 条内核补丁、23 条注入 kext、1 条 Booter 补丁、35 个 boot-args token、1 组 SIP/AMFI 放宽**，其中真正"跟 macOS 内核版本绑死"的是 **11 条**（§7 有名单），其余是"版本无关但可能被 Apple 改 ABI 打死"。升级当天的判据只有三档，见 §0。

| 项 | 值 |
|---|---|
| 系统 | macOS 15.8.1 (24H32)，`uname -r` = **24.6.0**（实测） |
| 平台 | AMD Ryzen 7 9700X / MAXSUN B850BKB WIFI / SMBIOS MacPro7,1 |
| OpenCore | DBG-107-2026-10-14 |
| 分析对象 | `/tmp/live-config.plist`（56,573 B，sha256 `bb95cd36c24f89bcfff5bc4f51d414ad169f43f486d86f9f629bcf20fd222812`） |
| 同源对照 | `outputs/20261020-amd-boot-config/private-backups/current-20261022/config.plist`（registry 记录的当前副本，sha256 相同） |
| 基准日志 | `opencore-2026-10-22-031825.txt`（摘要 `patchreport-20261022.json`） |
| 数据来源 | 全部由 `plistlib` 直接解析 config + `nm`/`strings` 实测 + 当日启动日志；无一条来自旧文档转抄 |
| 采集边界 | 只读。未写 ESP/EFI、未重启、未加载或卸载任何 kext、未改任何既有文档 |

## 0. 判读三档（全表共用）

OpenCore 对带 `MinKernel`/`MaxKernel` 的补丁只有三种结局，任何一行的"怎么发现"都落在这三档里：

| 档 | 日志形态 | 含义 | 危险度 |
|---|---|---|---|
| **A 门控跳过** | `OC: Kernel patcher skips kernel (<comment>) patch at <idx> due to version` | 新 Darwin 落在窗口外，条目等于不存在 | 低（预期内），但**必须知道哪些是"本来就该跳"** |
| **B 静默失效** | `Success` + `replace count - 0` | 窗口内、字节对不上 —— 最难发现，`patchreport.py` 会标 `ZERO` 并以退出码 1 报警 | **最高** |
| **C 正常命中** | `replace count - N` | N 应与 `Count` 一致（`Count=0` 表示不限次数，实测值即基准） | — |

**今天的基准（Darwin 24.6.0）**：17 条 C、9 条 A（idx 0/1/2/9/11/13/18/22/23）、**0 条 B**。升级后只要出现新的 B，就是一条真失效。

## 1. Kernel > Patch：30 条（26 启用 / 4 禁用）

三条全局事实先摆出来，它们决定了下表的读法：

1. **所有窗口上限都止于 `25.99.99`（= macOS 26.x）**。再往上一代（Darwin 26）没有任何一段 —— `Force cpuid_cores_per_package` 的四段最能说明问题：`17.0.0–18.99.99 / 19.0.0–20.99.99 / 21.0.0–22.3.99 / 22.4.0–25.99.99`。
2. **`Count=0` 是"不限次数"，不是"不生效"**（#16 / #24 / #26）。这三条是全量扫描，内核一改就可能多打或漏打。
3. **窗口号与 Comment 里的 macOS 号不是一套**：Comment 写 "15.0+" 的窗口是 `24.0.0`，写 "26.0+" 的是 `25.0.0` —— 即 **macOS 26.x ↔ Darwin 25.x**。这个换算由 #11/#18/#19/#22/#23 五处的「Comment 版本 ↔ 窗口」配对反推而来，不是猜的。

> 记法：表里 `#N` 是 config 里的序号（1 基）；`[NN]` 是 `patchreport.py` 输出的日志 idx（0 基），**`[NN]` = `#N − 1`**。升级当天对着日志核时用 `[NN]`。

| # | En | 条目（简名） | 当前窗口 | 内核一升级会怎样 | 怎么第一时间发现 | 失效后的处置 |
|---|---|---|---|---|---|---|
| 1 | T | AMD_Vanilla `Force cpuid_cores_per_package` 10.13-10.14 | `17.0.0–18.99.99` | 早已过期，今天本来就跳。升级只影响 B 档判定，不影响现状 | `patchreport.py` 里该条出现在「被版本门控跳过」清单即正常 | 不动（它只是为老系统保留的分段） |
| 2 | T | 同上 10.15-11.0 | `19.0.0–20.99.99` | 同上 | 同上 | 不动 |
| 3 | T | 同上 12.0-13.2 | `21.0.0–22.3.99` | 同上 | 同上 | 不动 |
| 4 | T | 同上 13.3+ **（当前生效那条）** | `22.4.0–25.99.99` | 窗口内仍会尝试；XNU 若改了 `_cpuid_set_info` 附近字节 → **B 档静默失效**，8 核拓扑（`c1e81a0000`→`ba08000000`）不成立 → 早期 panic 或核数判错。升到 Darwin 26 则直接 A 档 | `patchreport.py` 看 `[03]`：`replace=1` 为正常；`replace=0` 即 B 档 | 等 algrey 上游补段；**绝不自己放宽 MaxKernel 或抓宽 Find** |
| 5 | T | `_commpage_populate` Remove rdmsr 10.13+ | `17.0.0–25.99.99` | 窗口内遇 XNU 改动 → B 档。commpage 的 rdmsr 没被摘掉 → 用户态时间/计数器路径异常 | `[04] replace=1` | 同上，等上游 |
| 6 | T | `_cpuid_set_cache_info` Set CPUID proper | `17.0.0–25.99.99` | B 档 → 缓存信息错乱（可能表现为性能异常而非立即崩） | `[05] replace=1` | 等上游 |
| 7 | T | `_cpuid_set_generic_info` Remove wrmsr(0x8B) | `17.0.0–25.99.99` | B 档 → 往 AMD 不存在的 MSR 写值，常见为早期 panic | `[06] replace=1` | 等上游 |
| 8 | T | 同上 Replace rdmsr(0x8B) with 186 | `17.0.0–25.99.99` | B 档 → 读 MSR 0x8B 返回垃圾，CPU 特性判定错 | `[07] replace=1` | 等上游 |
| 9 | T | 同上 Set flag=1 | `17.0.0–25.99.99` | B 档 → 特性位缺失 | `[08] replace=1` | 等上游 |
| 10 | T | 同上 leaf7 检查 10.13+ | `17.0.0–23.99.99` | 已过期，今天正常跳过 | 出现在跳过清单即正常 | 不动 |
| 11 | T | 同上 leaf7 检查 15.0+ **（当前生效）** | `24.0.0–25.99.99` | B 档 → 叶 7 CPUID 被卡，AVX 等特性判定异常 | `[10] replace=1` | 等上游；Darwin 26 需要新段 |
| 12 | T | `_cpuid_set_info` GenuineIntel→AuthenticAMD 10.13-11.0 | `17.0.0–20.99.99` | 已过期 | 跳过清单 | 不动 |
| 13 | T | Goldfish64 Bypass GenuineIntel panic 12.0+ | `21.0.0–25.99.99` | B 档 → **直接 panic**（这条就是拦 GenuineIntel 检查的） | `[12] replace=1`；若 0 则几乎必然起不来 | 等上游，优先级最高 |
| 14 | T | `_cpuid_set_cpufamily` PENRYN 10.13-11.2 | `17.0.0–20.3.0` | 已过期 | 跳过清单 | 不动 |
| 15 | T | 同上 11.3+ **（当前生效）** | `20.4.0–25.99.99` | B 档 → CPU family 判错，影响电源管理与特性门控 | `[14] replace=1`（注意 Base 里带尾随空格，历史实测匹配未受影响） | 等上游 |
| 16 | T | `_i386_init` Remove 3 rdmsr | `17.0.0–25.99.99`，`Count=0`（不限次） | B 档 → 早期启动直接崩；`Count=0` 还可能因新内核多出同类字节而**多打** | `[15] replace=3` 为基准；≠3 就要人看 | 等上游；多打时先看是否为良性重复 |
| 17 | T | Remove version check and panic | `17.0.0–25.99.99` | B 档 → 内核版本检查 panic 复发 | `[16] replace=1` | 等上游 |
| 18 | T | IOPCIFamily `probeBusGated` 关 10-bit tag 12.0-15.x **（当前生效）** | `21.0.0–24.99.99` | **升到 Darwin 25（macOS 26）就自动停**，由 #19 接手 —— 这一对是接力结构，必须成对存在 | `patchreport.py` 里 `[17] replace=1`（今天正常）；升级后若 `[17]` 变跳过、`[18]` 也 0 命中 → 10-bit tag 无人处理 | AM5 上 NVMe/PCIe 设备掉盘或枚举异常；**先确认 #19 是否命中，再决定是否改字节** |
| 19 | T | 同上 26.0+ **（升级后的接棒条）** | `25.0.0–25.99.99` | 今天正常跳过。升到 Darwin 25.x 后由它接：字节对不上就 B 档（`e0117340`→`00000200` 不成立） | 升级后 `[18] replace=1` 才算过；跳过说明窗口算错了 | 按新 IOPCIFamily 反查 `probeBusGated` 重新取字节；不要靠"猜一个相近字节" |
| 20 | T | IOPCIFamily `IOPCIIsHotplugPort` AM5 枚举 13.0+ | `22.0.0–25.99.99` | B 档 → AM5 上 PCI 总线枚举错，表现为设备缺失或 `PCI configuration end` 后卡住。Base 是 demangled C++ 符号，XNU 一改签名整条就挂 | `[19] replace=1`；`patchcheck-all.py` 会因 kext 补丁假阴性，**以日志为准** | 按新符号重取字节；符号改名就先修 Base |
| 21 | T | 调度 `thread_quantum_expire/unblock/invoke` 去非单调时间 panic | `21.0.0–25.99.99`，`Count=3` | B 档 → 随机 panic 复发，backtrace 落在 `thread_invoke` | `[20] replace=3` | 等 Visual 上游；这条与 #22/#23 是一套 |
| 22 | T | 调度 `thread_invoke/dispatch` 12.0-26.3 **（当前生效）** | `21.0.0–25.3.99` | 升到 Darwin 25.4（macOS 26.4）后交给 #23；在此之前字节一变就是 B 档 | `[21] replace=2` | 见 #23，两条必须接力 |
| 23 | T | 调度 `thread_invoke/dispatch` 26.4+ **（接棒条）** | `25.4.0–25.99.99` | 今天正常跳过。升级后由它接，字节不匹配即 B 档 | 升级后 `[22] replace=2` 才算过 | 同上 |
| 24 | T | PAT `_mtrr_update_action` algrey 10.13+ | `17.0.0–23.99.99`，`Count=0` | 已过期（今天正常跳过），Darwin 24+ 由 #26 负责 | 跳过清单 | 不动；**不要因为"它是 algrey 版"就去启用** |
| 25 | F | PAT Shaneee 10.13+ | `17.0.0–23.99.99`，`Count=0` | 禁用项，日志里不出现 | `plutil -p` 看 Enabled=false | 保持禁用（这版会让 NVIDIA 起不来） |
| 26 | T | PAT Algrey/Zormeister 15.0+ **（当前生效）** | `24.0.0–25.99.99`，`Count=0` | B 档 → PAT/MTRR 不对，NVIDIA 驱动起不来或花屏；`Count=0` 还可能多打 | `[25] replace=2` 为基准 | 等上游；改字节前先确认 `nm` 里 `_mtrr_update_action` 范围 |
| 27 | F | PAT Shaneee/Zormeister 15.0+ | `24.0.0–25.99.99`，`Count=0` | 禁用项 | `plutil -p` | 保持禁用 |
| 28 | F | USB 口上限 15→17 `setPortLocation` | `Any`（窗口空 = 不过滤），Find/Replace 写死 `macOS 15.8.1/24H32` 的字节 | **启用后必成 B 档**：注释自带版本号，字节是 24H32 的；升级后 AppleUSBHostPort 一改就没命中 | 启用后用 `tools/research/usbpatch-check.py` 或日志看 `replace=0` | 先保持禁用。要恢复 17 口就用 UTBMap 扩端口；确需此补丁则按新二进制重取 `4183ff0f0f87` |
| 29 | F | USB 口上限 15→17 `AppleUSBXHCI::createPorts` | `Any`，同样写死 24H32 字节 | 同 #28 | 同 #28（`83fb0f0f83`） | 同 #28 |
| 30 | T | 9700X local LAPIC 25MHz 标定（trial） | **`24.6.0–24.6.0`**，`Limit=51` | **升级即失效（A 档）**：Min=Max=24.6.0，新 Darwin 一进来就跳过。时间基准与看门狗行为会悄悄回退，不崩但慢/抖 | 升级后日志里出现 `skips ... 9700X local LAPIC ... due to version` 即为已失效 | 按 `docs/macos-update-checklist.md` §1.3 **重新标定**（取新 `_tsc_init` 51 字节窗口），**不允许放宽版本号** |

## 2. Kernel > Add：23 条（19 启用 / 4 禁用）

计数实测：`plistlib` 读出 `Kernel.Add` 共 **23** 条，`Enabled=true` 的 **19** 条（禁用 4 条：`UTBDefault` / `NMProbeAudio` / `NMHDALink` / `NMHDAVol`）。与 registry §5 一致。

**KPI 版本 = Darwin 版本**（本机 `com.apple.kpi.private` 报 `24.6.0`，等于 `uname -r`，见 `docs/apple-native-rtl8125-driver.md` 的依赖对照表）。所以 kext 里那些 `com.apple.kpi.*: 12.0.0 / 15.0.0 / 16.7` 全是"地板值"，今天满足、升级后也满足 —— **它们不是升级风险，别当成窗口**。真正的风险在 Apple 私有类（IO80211 / IOSkywalk / AppleUSBHost* / IOBluetooth）的 ABI 变动。

| # | En | kext | 版本 | 当前窗口 | 内核一升级会怎样 | 怎么第一时间发现 | 失效后的处置 |
|---|---|---|---|---|---|---|---|
| 1 | T | `Lilu.kext` | 1.7.3 | `Any` | 无门控，永远加载。但它是全部插件的符号解析底座：新 XNU 一改就"加载成功、功能全哑" | `kmutil showloaded --list-only \| rg -i lilu`；`log show --last 10m --predicate 'process == "kernel"' \| rg -i 'lilu'` 看 unsupported/panic | 换新版 Lilu（连同全部插件一起）；Lilu 不兼容时**先别动其它 kext** |
| 2 | T | `AppleALC.kext` | 1.9.9 | `Any` | Lilu 插件，随 Lilu 走；AppleHDA 布局随版本变 → `alcid=3` 可能失配 | `kmutil showloaded \| rg AppleALC` + 放一段本地音频听 | 换新版 AppleALC；临时把 `alcid` 试别的 layout |
| 3 | T | `AppleMCEReporterDisabler.kext` | 1.2（**codeless**） | `Any` | 纯 personality：按 `board-id`（三条，含 MacPro7,1 的 `Mac-27AD2F918AE68F61`）拦 `AppleIntelMCEReporter`。Apple 改 nub 类名即静默失效 | codeless kext **不会**出现在 `kmutil showloaded`；用 `kmutil showloaded --list-only \| rg -i AppleIntelMCE` 应为空 | 按新 board-id/类名改 personality；否则 MCE 中断相关 panic 会回来 |
| 4 | T | `NVMeFix.kext` | 1.1.4 | `18.0.0–Any`（只有下限） | 永远加载；Apple NVMe 栈改动后可能空转 | `kmutil showloaded \| rg NVMeFix` + `ioreg -rc IONVMeController` 看 APST 属性 | 换新版；无效就先禁用以排除变量 |
| 5 | T | `RTL812xLucy.kext` | 1.1.100 | `Any` | 依赖 `IONetworkingFamily 1.8.0`/`IOPCIFamily 2.6`（地板）；升级后 Apple 改网络栈 → 网卡消失 | `ifconfig -l`、`ioreg -rc RTL812xLucy`、`log show --last 5m \| rg -i rtl812` | 换新版。别改用原生 `AppleEthernetRL`：本机缺 `AppleSkywalkAVB` 硬依赖且 Skywalk 是降级版，详见 `docs/apple-native-rtl8125-driver.md` |
| 6 | T | `RestrictEvents.kext` | 1.1.7 | `Any` | Lilu 插件；只影响 CPU 名/内存提示，失效不致命 | `kmutil showloaded \| rg RestrictEvents` | 换新版或暂时移除（无功能损失） |
| 7 | T | `USBToolBox.kext` | 1.2.0 | `Any` | 与 `UTBMap` 配对；Apple 改 `AppleUSBHostPort` 端口限制逻辑即失效 | `kmutil showloaded \| rg USBToolBox`；`ioreg` 看端口数 | 换新版；同时复核 §1 #28/#29 是否被误启用 |
| 8 | F | `UTBDefault.kext` | 1.0（codeless） | `Any` | 禁用项（只有 XHC1 一条注入，已被 UTBMap 覆盖） | `plutil -p` | 保持禁用 |
| 9 | T | `VirtualSMC.kext` | 1.3.9 | `Any` | 失效表现为 SMC 读假值 / 传感器全 0 / 启动变慢 | `kmutil showloaded \| rg VirtualSMC`；`ioreg -rc AppleSMC` | 换新版（与 Lilu 同步） |
| 10 | T | `WhateverGreen.kext` | 1.7.2 | `Any` | 消费 `gfxrst=4`；失效 → 显示黑屏/花屏/单头 | `kmutil showloaded \| rg WhateverGreen` | 换新版；先别同时动 `nv*` 参数 |
| 11 | T | `UTBMap.kext` | 1.1（codeless） | `Any` | USB 端口映射（XH00 7 个对象、`port-count=0x11` 即最高端口号 17，与 HS12 的 `0x11` 对应）。**Info.plist 必须是 XML**，binary plist 会被 prelink 拒绝 | `log show`/OC 日志看 `Prelinked injection UTBMap.kext ... - Success`；`ioreg -rc AppleUSBXHCIPCI -l -w0 \| rg 'port-count'` | 保持 XML；端口拓扑变（新硬件/新固件）才重做 map |
| 12 | T | `IOSkywalkFamily.kext`（Apple 原版**降级注入**） | 1.0 | `Any` | **全表最高风险**：这是把 Apple 的 Skywalk 换回旧对象模型，只为让 WiFi 三件套能跑。Apple 每次改 Skywalk 内部，注入的旧版就可能与新 XNU/IO80211 对不上 → WiFi 全灭，重则 panic | `kmutil showloaded --list-only \| rg IOSkywalkFamily` 应只有一份 `1.0`；`log show --last 10m \| rg -i skywalk`；断网即最直观信号 | 换成与新系统匹配的那份拷贝（OCLP 式做法）；**不要靠删 kext 硬顶**，删了 WiFi 直接没了 |
| 13 | T | `IO80211FamilyLegacy.kext` | 1200.12.2b1（旧版回填，带 `PlugIns/`） | `Any` | 系统根里的 `IO80211Family` 已是 `1200.13.1`（Sequoia 把 Wi-Fi 迁到 DriverKit）；注入的是 `1200.12.2b1` 旧实现。升级后 Apple 再改结构 → 加载失败或 `AirportMtk` 依赖断裂 | `kmutil showloaded \| rg IO80211FamilyLegacy` 应显示 `1200.12.2b1`；再 `ls /System/Library/Extensions/IO80211Family.kext/Contents/PlugIns` 对比 | 随新系统换对应旧版回填；加载失败时先确认 SIP 位没变（§5） |
| 14 | T | `AirportMtk.kext` | 0.1.0（自研） | `Any` | 依赖 `IOSkywalkFamily`+`IO80211FamilyLegacy`+`IONetworkingFamily 3.2`+`IOPCIFamily 2.9`。三项里任一失效 → 本 kext 静默不加载（依赖解析不过） | `kmutil showloaded \| rg AirportMtk`；`networksetup -listallhardwareports`、`log show --last 5m \| rg -i airportmtk` | 先修上游三件套（#12/#13/#15），再谈本 kext；改 kext 属另一条工作线 |
| 15 | T | `AMFIPass.kext` | 1.4.1（`com.dhinakg.AMFIPass`） | `Any`（内部 `OSBundleLibraries`: Lilu 1.6.8 + kpi 12.0.0） | 让 AMFI 放行未签名 kext 的那层。真正绑定在 **macOS 26+**（`-amfipassbeta` 就是为那时准备的）。Apple 改 AMFI/CoreTrust 符号 → 它加载成功但不生效 → 全部自研 kext 起不来 | `kmutil showloaded \| rg AMFIPass`（今天 1.4.1 在列）；再看一个未签名 kext 能否加载；`log show \| rg -i 'amfi\|AMFIPass'` | 换 AMFIPass 新版；仍不行就回退系统版本，别靠叠 `amfi=` 位值硬凑 |
| 16 | T | `MT7922USBWakePolicy.kext` | 1.0.0（codeless） | **`24.0.0–24.99.99`** | **升到 Darwin 25（macOS 26）就静默不加载** → MT7922 的 USB 远程唤醒回来，XH00/HS11 恢复风暴可能复发 | codeless → 不在 `kmutil`；用 `ioreg -rc IOUSBHostDevice -r -l -w0 \| rg kUSBRemoteWakeOverride` 必须仍是 **No**（今天实测是 No） | 把窗口改成新 Darwin 段（`25.0.0–25.99.99`）并复验该属性；先别删 personality |
| 17 | T | `SimpleMtkBtFw.kext` | 1.0.0 | `Any` | 蓝牙固件加载器（`mtkbtcsr`）。`IOUSBHostFamily` 一改 → 固件传不进去，蓝牙消失 | `kmutil showloaded \| rg SimpleMtkBtFw`；`ioreg -l \| rg -i 'MtkBtSetupOk'` 应为 Yes（今天实测 Yes） | 换新版；蓝牙链顺序别动 |
| 18 | T | `SimpleMtkBtPatcher.kext` | 1.0.0 | `Any` | Lilu 插件，发 Random-Address HCI / 重发 LE SCAN PARAM。失效 → 能连不能扫 | `kmutil showloaded \| rg SimpleMtkBtPatcher`；`log show \| rg -i 'smbt'` | 换新版 |
| 19 | T | `BlueToolFixup.kext` | 2.7.2（acidanthera） | `Any` | 让 BlueTool 接受非 Apple 控制器。Apple 改 BlueTool 校验即失效 | `kmutil showloaded \| rg BlueToolFixup`；蓝牙开关灰掉即信号 | 换酸果新版 |
| 20 | T | `NMHDA.kext` | 1.0.0（NullMoth） | `Any` | HDMI/DP 音频实验链。AppleHDA/IOAudioFamily 一改即失效，影响面小 | `kmutil showloaded \| rg NMHDA`；`system_profiler SPAudioDataType` | 属实验线，失效可先禁用排除 |
| 21 | F | `NMProbeAudio.kext` | — | `Any` | 一次性探针 | `plutil -p` | 保持禁用 |
| 22 | F | `NMHDALink.kext` | 1.0.0 | `Any` | 显示音频链路（试过） | `plutil -p` | 保持禁用 |
| 23 | F | `NMHDAVol.kext` | 1.0.0 | `Any` | 显示音频软音量 | `plutil -p` | 保持禁用 |

**加载次序契约（换版本时必须保住）**：`Lilu` 最先 → `AirportMtk` 依赖 `IOSkywalkFamily`+`IO80211FamilyLegacy`+`AMFIPass` → 蓝牙链 `SimpleMtkBtFw` → `SimpleMtkBtPatcher` → `BlueToolFixup`；`MT7922USBWakePolicy` 位于 `AMFIPass` 之后、蓝牙链之前。

## 3. Booter > Patch：1 条（0 启用）

**这条的关键结论：OpenCore 的 `Booter/Patch` 没有版本窗口**。实测该条目的全部键为 `Arch / Comment / Count / Enabled / Find / Identifier / Limit / Mask / Replace / ReplaceMask / Skip` —— **不存在 `MinKernel`/`MaxKernel`**（`plistlib` 读出来是 `None`，不是空串）。所以它不属于"版本绑定"，而属于"字节绑定"。

| # | En | Comment | Identifier | Find → Replace | 当前窗口 | 升级会怎样 | 怎么发现 | 失效处置 |
|---|---|---|---|---|---|---|---|---|
| 1 | F | `macOS to hacOS` | `Apple` | `6d61634f53` → `6861634f53`（`macOS`→`hacOS`） | 无（schema 无版本键） | 禁用项，升级不改变任何行为。**若有人启用**，它会在任何 macOS 版本上按 Count/Limit 扫，命中不了就静默 0 处 | 禁用态不出现在 OC 日志；启用后看日志 `OC: Booter patcher result ...` 的 replace 计数 | 保持禁用。要真启用，先确认本版 OpenCore 里 `Apple` 是有效 identifier（**待确认**） |

## 4. boot-args：35 个 token（表内 32 行，同族合并 3 处）

当前生效值（`nvram boot-args` 实测，与 config `NVRAM/Add/7C4361…/boot-args` 逐字一致）：

```
-v debug=0x100 keepsyms=1 amfi=0x80 mtkwlan-attach ipc_control_port_options=0 cx3raw=1 cx3debug=1
-amfipassbeta mtknoreassert -smbtcompatdbg -liludbg mtkascan itlwm_cc=CN mtkscan1ch=1 mtkbtcsr
mtkrecover=5 mtkaspm=2 mtkscanhon=0 mtkampdu mtkvht=5 mtkunicastwd=0 alcid=3 nvfb=1 nvaccel=1
nvfbheads=4 -nvkmsnosmooth nvfbhidpi mtkbsshe mtkstahe mtkdatafix=3 mtkhecap mtkhe mtkhedefer gfxrst=4
```

「消费方」一列是**实测 grep**（`grep -rl -a <token>` 扫 `EFI/OC/Kexts` 全部二进制 + `/Library/Extensions`），不是按名字猜的。

| token | 消费方（实测） | 分类 | 内核一升级会怎样 | 怎么发现 / 怎么处置 |
|---|---|---|---|---|
| `-v` | OpenCore / boot.efi | 长期调试 | 不受内核版本影响 | 保留（排障必需） |
| `debug=0x100` | 内核（Apple 私有位） | 长期调试 | 位含义未在本地文档核实（**待确认**），不随版本失效 | 保留；具体位表见 `docs/todo-open-items.md` §1 的待办 |
| `keepsyms=1` | 内核 | 长期调试 | 不受版本影响；失效只影响 panic 可读性 | 保留 |
| `amfi=0x80` | `AMFIPass`（ESP grep 实测命中）；内核 AMFI 侧为社区既有结论，本地未反汇编核实 | **长期必需** | 见 §5：AMFI 改二进制即可能失效，表现为所有未签名 kext 拒绝加载 | `kmutil showloaded \| rg AMFIPass`；试装一个自签 kext |
| `mtkwlan-attach` | `AirportMtk` | 长期必需（自研） | 只被自研 kext 读，升级不失效；kext 不加载时它等于废参数 | `ioreg`/`ifconfig` 看 WiFi 是否 attach |
| `ipc_control_port_options=0` | **无消费者**（ESP 与 `/Library/Extensions` 全无命中） | 未确认 | 不随版本失效；但来源与作用无本地证据 | 记入待确认；动它之前先确认是否 Apple 私有 boot-arg |
| `cx3raw=1` | `Cx3Mlx4`（AuxKC，`/Library/Extensions`） | 长期必需（CX3 线） | AuxKC 不走 ESP，系统更新可能重装/失效 AuxKC → 参数在、驱动没了 | `kmutil showloaded \| rg Cx3Mlx4` |
| `cx3debug=1` | **无消费者**（`Cx3Mlx4` 里也没有该串） | **临时调试（疑似遗留）** | 不失效也无效果；升级后可考虑清掉 | 先与 CX3 会话确认再删（**待确认**） |
| `-amfipassbeta` | `AMFIPass` | 长期预留 | 现在就有效；真正起作用要等 macOS 26/27（`docs/todo-open-items.md` §4 已定"保留"） | 保留；升级到 26 时它是第一个要验证的 |
| `mtknoreassert` | `AirportMtk` | 长期必需（自研） | 抑制 MT7922 反复重断言 —— 少了它会看到恢复风暴 | `log show \| rg -i 'mtk\|XH00'` 看是否反复 reassert |
| `-smbtcompatdbg` | `SimpleMtkBtPatcher` | **临时调试** | 无版本依赖；调试完可摘 | 摘之前先留一版基线日志 |
| `-liludbg` | `Lilu` + 全部 Lilu 插件（AppleALC/NVMeFix/RestrictEvents/VirtualSMC/WhateverGreen） | **临时调试** | 无版本依赖；但它会放大日志量，升级排障期反而有用 | 排障期留，稳定后摘 |
| `mtkascan` | `AirportMtk` | 功能项（自研） | 无版本依赖 | 随 WiFi 功能保留 |
| `itlwm_cc=CN` | `AirportMtk` | 长期必需 | 国家码；升级不影响，但驱动重写时要带走 | 保留 |
| `mtkscan1ch=1` | `AirportMtk` | 功能项 | 同上 | 保留 |
| `mtkbtcsr` | `SimpleMtkBtFw` | 长期必需（蓝牙链） | 无版本依赖；固件加载失败时它也没用 | `ioreg -l \| rg MtkBtSetupOk` |
| `mtkrecover=5` | `AirportMtk` | 功能项 | 恢复重试次数；升级后若恢复逻辑变，值可能要调 | 观察 `log show` 里 recover 次数 |
| `mtkaspm=2` | `AirportMtk` | 功能项 | 无版本依赖 | 保留 |
| `mtkscanhon=0` | `AirportMtk` | 功能项 | 同上 | 保留 |
| `mtkampdu` | `AirportMtk` | 功能项 | 同上 | 保留 |
| `mtkvht=5` | `AirportMtk` | 功能项 | 同上 | 保留 |
| `mtkunicastwd=0` | `AirportMtk` | 功能项 | 同上 | 保留 |
| `mtkbsshe` / `mtkstahe` | `AirportMtk` | 功能项 | 同上 | 保留 |
| `mtkdatafix=3` | `AirportMtk` | 功能项 | 同上 | 保留 |
| `mtkhecap` / `mtkhe` / `mtkhedefer` | `AirportMtk` | 功能项（HE 相关） | 同上 | 保留 |
| `alcid=3` | `AppleALC` | 长期必需 | AppleHDA 布局随版本变 → 可能要换 layout id | 放本地音频听；`system_profiler SPAudioDataType` |
| `nvfb=1` | `NVRM` + `NVRMFB`（AuxKC） | 长期必需（NVIDIA 线） | AuxKC 与 ESP 无关；系统更新可能让 AuxKC 失效 → 参数成死值 | `kmutil showloaded \| rg -i 'NVRM\|NVAccel'` |
| `nvaccel=1` | `NVRM` + `NVAccel` | 长期必需 | 同上 | 同上 |
| `nvfbheads=4` | `NVRM` | 长期必需 | 同上 | 同上 |
| `-nvkmsnosmooth` | `NVRM` | 长期必需 | 同上 | 同上 |
| `nvfbhidpi` | `NVRMFB` | 长期必需 | 同上 | 同上 |
| `gfxrst=4` | `WhateverGreen` + `NVRMFB` | 长期必需 | 双消费者；升级后两边任一改语义都可能冲突 | 看开机是否多一次 reset；必要时只留一边 |

分类计数（合计 35，与 `nvram boot-args` 的 token 数逐一对齐）：

- **长期必需 14 个**：`amfi=0x80` `mtkwlan-attach` `cx3raw=1` `-amfipassbeta` `mtknoreassert` `itlwm_cc=CN` `mtkbtcsr` `alcid=3` `nvfb=1` `nvaccel=1` `nvfbheads=4` `-nvkmsnosmooth` `nvfbhidpi` `gfxrst=4`
- **长期功能项 14 个**（自研 MTK 驱动的行为开关，长期保留但非硬依赖）：`mtkascan` `mtkscan1ch=1` `mtkrecover=5` `mtkaspm=2` `mtkscanhon=0` `mtkampdu` `mtkvht=5` `mtkunicastwd=0` `mtkbsshe` `mtkstahe` `mtkdatafix=3` `mtkhecap` `mtkhe` `mtkhedefer`
- **长期调试 3 个**：`-v` `debug=0x100` `keepsyms=1`
- **临时调试 3 个**：`-smbtcompatdbg` `-liludbg` `cx3debug=1`
- **未确认 1 个**：`ipc_control_port_options=0`

即：**真正"临时"的只有 3 个调试 token**，其余都是驱动运行所必需或长期行为开关 —— 升级排障时不要把它们当噪音一并删掉。

## 5. SIP / AMFI

### 5.1 `csr-active-config = 430a0000` → `0x00000A43`

实测两处一致：`nvram csr-active-config` 回 `C%0a%00%00`；`csrutil status` 回 `unknown (Custom Configuration)`。位展开（与 `docs/amd-security-posture.md` §1 一致）：

| 位 | 名称 | 状态 |
|---|---|---|
| `0x001` | CSR_ALLOW_UNTRUSTED_KEXTS | **ON** |
| `0x002` | CSR_ALLOW_UNRESTRICTED_FS | **ON** |
| `0x040` | CSR_ALLOW_UNRESTRICTED_NVRAM | **ON** |
| `0x200` | CSR_ALLOW_UNAPPROVED_KEXTS | **ON** |
| `0x800` | CSR_ALLOW_UNAUTHENTICATED_ROOT | **ON** |
| 其余（TASK_FOR_PID / KERNEL_DEBUGGER / APPLE_INTERNAL / DTRACE / DEVICE_CONFIG / RECOVERY_OS / EXEC_POLICY） | — | off |

| 四列 | 内容 |
|---|---|
| **当前窗口** | 无版本窗口：由 OpenCore `NVRAM/Add` 每次开机写入，系统更新**不会**清掉它 |
| **升级会怎样** | 值本身不变，但**位的语义由 Apple 定义**，不同系统版本的判定范围并不保证一致（本地只实测了 24.6.0 这一版，未逐位跨版本核对）。若新版新增位或调整判定，会出现"`csrutil` 说关着、实际操作被拒"的错位 |
| **怎么发现** | `csrutil status` 逐项对照上表**前五行（五条 ON 位）**为 disabled；`nvram csr-active-config` 必须仍为 `C%0a%00%00` |
| **失效处置** | 先确认是"值被改"还是"语义变了"：值变了查 `NVRAM/Delete` 与 `DisableVariableWrite`；语义变了才动位，且一次只加一位，改完必须重跑 §2 的 kext 加载检查 |

### 5.2 `amfi=0x80` + `-amfipassbeta` + `AMFIPass.kext`

| 四列 | 内容 |
|---|---|
| **当前窗口** | `amfi=0x80` 无窗口（内核 boot-arg）；`AMFIPass.kext` 配置层 `Any`，其 `OSBundleLibraries` 为 `Lilu 1.6.8` + 全部 `com.apple.kpi.* 12.0.0`（地板值，今天 kpi 是 24.6.0）；`-amfipassbeta` 为 **macOS 26/27 预留** |
| **升级会怎样** | 三层都可能断：① AMFI 二进制改结构 → `amfi=0x80` 的位不再对应原检查项；② AMFI/CoreTrust 符号变动 → AMFIPass 加载成功但拦截失效；③ 新系统把 AMFI 检查挪进 BootKC 前置阶段 → 连加载机会都没有。结果是**全部未签名 kext 一起起不来**（表现为很多 kext 消失，而不是单个 kext 报错） |
| **怎么发现** | `kmutil showloaded --list-only \| rg -i 'AMFIPass'` 必须有 `com.dhinakg.AMFIPass (1.4.1)`；再看自研 kext 是否都在：`kmutil showloaded --list-only \| rg -i 'nullmoth\|Cx3Mlx4\|AirportMtk\|RTL812xLucy\|SimpleMtk'`；`log show --last 10m \| rg -i 'amfi\|AMFIPass\|code signature'` |
| **失效处置** | 先换 AMFIPass 新版（上游按系统版本跟），再谈改 `amfi=` 值。**不要**用"加大 SIP 位"来绕过 —— 两套机制不互相替代（`docs/todo-open-items.md` §6 已证 AMFIPass 不可替代 `amfi=0x80`，反之亦然） |

### 5.3 顺带：两条同样带版本语义的 `Kernel > Block`

不在必答项里，但它们直接决定上面 kext 能否加载，漏了会误判：

| 条目 | 策略 | 窗口 | 升级会怎样 | 怎么发现 / 处置 |
|---|---|---|---|---|
| `com.apple.iokit.IOSkywalkFamily`（Allow IOSkywalk Downgrade） | `Exclude`，En=True | **`MinKernel=23.0.0`**（macOS 14 起一直屏蔽系统那份） | 屏蔽永远生效 —— 升级后系统的新 IOSkywalkFamily 照样被排掉，注入的 `1.0` 顶上。若 Apple 把 Skywalk 挪出 kext，这条就变成空转 | `kmutil showloaded \| rg IOSkywalk` 只应有一份；两份同时出现说明屏蔽没生效 |
| `com.apple.iokit.IONDRVSupport`（boot framebuffer 抢占 NVRMFB 索引 0） | `Exclude`，En=True | `Any` | 无版本绑定；NVIDIA 线必需，去掉会让 NVRMFB 抢不到 index 0 | `kmutil showloaded \| rg IONDRVSupport` 应为空 |

## 6. 复核记录（对着 registry 逐条比）

| 复核项 | 实测 | registry | 结论 |
|---|---|---|---|
| `Kernel/Patch` 条数 / 启用数 | 30 / 26 | 30 / 26 | ✅ |
| `Kernel/Add` 条数 / 启用数 | 23 / 19 | 23 / 19 | ✅ |
| `Booter/Patch` 条数 / 启用数 | 1 / 0 | 1 / 0 | ✅ |

**三条窗口值逐条复核**（要求 ≥3 条）：

| # | 条目 | `/tmp/live-config.plist` 实测 | `PATCH-REGISTRY-20261022.md` | 一致 |
|---|---|---|---|---|
| 4 | `Force cpuid_cores_per_package 13.3+` | `MinKernel=22.4.0`, `MaxKernel=25.99.99`, `Count=1` | §1 #4 同 | ✅ |
| 19 | `CaseySJ probeBusGated 26.0+` | `MinKernel=25.0.0`, `MaxKernel=25.99.99`, `Count=1` | §1 #19 同 | ✅ |
| 30 | `9700X local LAPIC 25MHz calibration` | `MinKernel=24.6.0`, `MaxKernel=24.6.0`, `Count=1`, `Limit=51` | §1 #30 同（`24.6.0–24.6.0` + `Limit=51`） | ✅ |
| 16(Add) | `MT7922USBWakePolicy.kext` | `24.0.0–24.99.99`, `Enabled=True`, `ExecutablePath=""` | §7 逐字段同 | ✅ |

## 7. 最该盯的 11 条（版本绑死 / 已过期 / 自带版本号）

按"升级后必须人工确认"排序：

| 序 | 条目 | 为什么 |
|---|---|---|
| 1 | Kernel/Patch **#30** LAPIC 标定 | `Min=Max=24.6.0`，升级即失效，且不会崩 —— 最容易漏 |
| 2 | Kernel/Add **#16** `MT7922USBWakePolicy` | 窗口封在 `24.99.99`，升 Darwin 25 静默不加载，恢复风暴可能复发 |
| 3 | Kernel/Add **#12/#13** `IOSkywalkFamily` + `IO80211FamilyLegacy` | 降级注入 Apple 私有栈，Apple 一改就整条 WiFi 链断 |
| 4 | Kernel/Patch **#18↔#19** IOPCIFamily 10-bit tag | 接力对：Darwin 25 上必须由 #19 接住，接不住就 AM5 PCIe 枚举异常 |
| 5 | Kernel/Patch **#22↔#23** 调度非单调时间 | 同样的接力对，窗口在 `25.3.99 / 25.4.0` 交界 |
| 6 | Kernel/Patch **#1–#4** `cpuid_cores_per_package` | 四段全部止于 `25.99.99`；Darwin 26 无段，8 核拓扑会失守 |
| 7 | Kernel/Patch 其余 AMD_Vanilla 生效条（**#5–#9 / #16**） | 与 #4/#11/#13/#15/#17 同源，都是"能开机"的前提，任一条 B 档都很可能是 panic |
| 8 | Kernel/Patch **#20** `IOPCIIsHotplugPort` | demangled C++ 符号当 Base，XNU 一改签名整条挂 |
| 9 | Kernel/Patch **#26** PAT（Algrey/Zormeister） | NVIDIA 起不来的直接原因；`Count=0` 还可能多打 |
| 10 | Kernel/Patch **#28/#29** USB 15→17 | 禁用中，但字节写死 24H32 —— 一旦有人启用就是定时失效 |
| 11 | §5.2 AMFI 三层 | 断了是"全部自研 kext 一起消失"，不是单点故障 |

## 8. 升级当天执行顺序（可直接复制）

**升级前（旧系统上）**

```bash
sw_vers; uname -r
shasum -a 256 /tmp/live-config.plist
python3 tools/patchcheck-all.py \
    --config outputs/20261020-amd-boot-config/private-backups/current-20261022/config.plist \
    --json /tmp/patchcheck-before.json
```

判据：`patchcheck-before.json` 里的 MISS 应为 0（A7 §2 当版结论：`适用 20 / 字节扫描 16 / kext 补丁 4 / MISS 0`）。有 MISS 就先别升级。

**升级后第一次启动（不改任何 EFI）**

```bash
# 1) 权威口径：OpenCore 自己的结论
python3 tools/patchreport.py /Volumes/OPENCORE/opencore-<最新>.txt --json /tmp/patchreport-after.json

# 2) 版本是否真的变了；窗口判定全靠它
uname -r

# 3) kext 是否都还在
kmutil showloaded --list-only | rg -i 'Lilu|AMFIPass|AirportMtk|IOSkywalk|IO80211FamilyLegacy|RTL812xLucy|SimpleMtk|BlueTool|MT7922'

# 4) codeless kext 的唯一验证方式（MT7922USBWakePolicy）
ioreg -rc IOUSBHostDevice -r -l -w0 | rg kUSBRemoteWakeOverride     # 期望 No

# 5) SIP / AMFI 现状
csrutil status; nvram csr-active-config; nvram boot-args
```

**通过判据**

| 项 | 通过 | 失败时先看 |
|---|---|---|
| `patchreport.py` | 没有新的 `ZERO` 行；退出码 0 | 该条 idx 的 `Comment` 与 §1 表对照，确认是 A 档（预期跳过）还是 B 档 |
| 版本门控跳过清单 | 只应比基准（idx 0/1/2/9/11/13/18/22/23）"多出该跳的"，不应少 | 少的那些意味着窗口写错（比如 #19 该接棒却没接） |
| `kmutil showloaded` | Lilu / AMFIPass / AirportMtk / IOSkywalkFamily / IO80211FamilyLegacy / RTL812xLucy / SimpleMtkBt* / BlueToolFixup 全在 | 缺哪个就按 §2 该行的"处置"列走 |
| `kUSBRemoteWakeOverride` | `No` | 出现 `Yes`/属性消失 → `MT7922USBWakePolicy` 未生效（§2 #16） |
| `csrutil status` / `nvram` | 与 §5.1 五行一致 | 见 §5.1 失效处置 |

## 9. 待确认清单（没有证据就不写结论）

| 项 | 为什么没定 |
|---|---|
| macOS 15.8.1 的**下一个**版本的 Darwin 号 | 未安装、无镜像，无法实测；`24.7.0` 或 `24.8.0` 都是猜 |
| Darwin 26（macOS 27）的任何一段补丁 | 上游 AMD_Vanilla 未发布，本地无资料；这是 §1 里四条 `cpuid_cores_per_package` 与 IOPCIFamily/调度各组的共同缺口 |
| `cx3debug=1` 的作用 | ESP 与 `/Library/Extensions` 全无该字符串的消费者，也无本地文档；疑为历史遗留 |
| `ipc_control_port_options=0` 的作用 | 同上，无任何消费者命中；可能是 XNU 私有 boot-arg |
| `Booter/Patch` 里 `Identifier=Apple` 是否有效 | 该条 `Enabled=false` 从未在日志中出现过，无实证 |
| `IO80211FamilyLegacy.kext` 的 `1200.12.2b1` 出自哪个系统版本 | release 里没有来源记录；只知道系统根当前是 `1200.13.1` |
| 各 `mtk*` 参数的语义细节 | 只有参数名与消费方（`AirportMtk`/`SimpleMtkBtFw`/`SimpleMtkBtPatcher`）实证，没有逐项文档 |
| `debug=0x100` 的位含义 | `docs/todo-open-items.md` §1 明确标注"未开始"，不在此编造 |

---

附：本文件只写版本绑定与破坏面，不重复 registry 的逐条 Find/Replace 字节与 baseline diff；那部分以 `PATCH-REGISTRY-20261022.md` 为准。

## 10. 本次实际执行的命令（全部只读）

下面这些是**生成这张表时真跑过的**命令，不是"建议这么跑"；§8 那节才是升级当天要跑的。

| 目的 | 命令 | 关键结论 |
|---|---|---|
| 确认分析对象与 registry 同源 | `shasum -a 256 /tmp/live-config.plist` | `bb95cd36c24f89bc…fd222812`，与 registry 头部一致 |
| 清点 Patch / Add / Booter / Block | `/usr/bin/python3 - <<'EOF'` + `plistlib.load(open('/tmp/live-config.plist','rb'))` | Patch 30（启用 26）、Add 23（启用 19）、Booter/Patch 1（启用 0）、Block 2 |
| 取 boot-args / SIP | `nvram boot-args; nvram csr-active-config; csrutil status; sw_vers; uname -r` | `24.6.0` / `15.8.1 (24H32)` / `C%0a%00%00` / Custom Configuration |
| 核对 Booter/Patch 字段集 | 同上 `plistlib`，打印 `sorted(x.keys())` | 只有 `Arch…Skip` 十个键，**无 MinKernel/MaxKernel** |
| 查每个 boot-arg 的消费者 | `for t in …; do grep -rl -a "$t" EFI/OC/Kexts; done`（挂载点 `/tmp/esp_ro.KWRIKE`） | 35 个 token 的归属：**24 个**由 ESP 内 kext 消费、**6 个**只在 AuxKC kext（`cx3raw` / `nvfb` / `nvaccel` / `nvfbheads` / `nvkmsnosmooth` / `nvfbhidpi`）、**3 个**属内核/OpenCore 层（`-v` `debug=0x100` `keepsyms=1`）、**2 个**无处命中（`cx3debug`、`ipc_control_port_options=0`） |
| 确认 AuxKC 侧消费者 | `grep -rl -a cx3raw cx3debug nvfb … /Library/Extensions/*.kext` | `cx3raw`→Cx3Mlx4；`nv*`→NVRM/NVRMFB/NVAccel；`cx3debug`→无 |
| 取 kext 版本与依赖 | `plistlib` 读各 `Contents/Info.plist` 的 `CFBundleVersion` / `OSBundleLibraries` | AMFIPass 1.4.1（Lilu 1.6.8 + kpi 12.0.0）、IO80211FamilyLegacy 1200.12.2b1、AirportMtk 0.1.0 等 |
| 确认 codeless kext 的匹配条件 | 同上，打印 `IOKitPersonalities` | `MT7922USBWakePolicy` 两条 personality 均 `locationID=6291456`(0x600000) → `kUSBRemoteWakeOverride=False` |
| 验证 MT7922 唤醒策略当前生效 | `ioreg -rc IOUSBHostDevice -r -l -w0 \| grep -E 'kUSBRemoteWakeOverride\|MtkBtSetupOk'` | `kUSBRemoteWakeOverride` = **No**、`MtkBtSetupOk` = **Yes** |
| 确认已加载 kext 版本 | `kmutil showloaded --list-only \| grep -iE 'nullmoth\|Cx3Mlx4\|AirportMtk\|RTL812xLucy\|AMFIPass\|Lilu\|SimpleMtk\|BlueTool\|IOSkywalk\|IO80211FamilyLegacy'` | 见下表 |
| 确认系统侧 IO80211 版本 | `plistlib` 读 `/System/Library/Extensions/IO80211Family.kext/Contents/Info.plist` | `1200.13.1`（注入的是 `1200.12.2b1`，差一代） |
| 确认 SMBIOS | `plistlib` 读 `PlatformInfo/Generic/SystemProductName` | `MacPro7,1` → 命中 `AppleMCEReporterDisabler` 的 `Mac-27AD2F918AE68F61` |
| 复核 registry 的 `Base` 尾随空格 | `repr(pat[14]['Base'])` | `'_cpuid_set_info '`（确有尾随空格，实际匹配未受影响） |
| 行数与 token 数自检 | `awk` 数表行 + `nvram boot-args \| tr ' ' '\n' \| wc -l` | Patch 30 行、Add 23 行、token 35 个 |

`kmutil showloaded` 实测（节选，本文件所有 kext 侧结论的来源）：

```
 55  com.apple.iokit.IOSkywalkFamily (1.0)
 57  as.vit9696.Lilu (1.7.3)
 62  com.dhinakg.AMFIPass (1.4.1)
 63  com.laobamac.SimpleMtkBtPatcher (1.0.0)
 64  as.acidanthera.BlueToolFixup (2.7.2)
 65  com.nullmoth.NMHDA (1.0.0)
 97  com.insanelymac.RTL812xLucy (1.1.100)
 99  com.apple.iokit.IO80211FamilyLegacy (1200.12.2b1)
100  local.hiliang.AirportMtk (0.1.0)
109  com.laobamac.SimpleMtkBtFw (1.0.0)
120  com.hijiajun.driver.Cx3Mlx4 (0.44.47)     ← AuxKC
132  com.nullmoth.NVRMFB (2.5.2)               ← AuxKC
144  com.nullmoth.NVRM (2.5.2)                 ← AuxKC
154  com.nullmoth.NVAccel (2.5.2)              ← AuxKC
```

注：`MT7922USBWakePolicy` / `UTBMap` / `AppleMCEReporterDisabler` / `UTBDefault` 是 codeless（无 `ExecutablePath`），**不会**出现在这张表里 —— 所以 §2 给它们的验证方式都是 `ioreg` / `kmutil \| rg <被屏蔽的类>`，而不是查自己。
