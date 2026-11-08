# 本机实装 runbook（9700X / B850BKB / RTX 5060 → macOS 15 + NullMoth 驱动）

日期：2026-10-08　状态：**准备完成**；方案已定为「压缩 C: 出 300 GB + 共用现有 ESP」

## 1. 现在已就绪的东西（都验证过）

| 项 | 状态 |
| --- | --- |
| EFI | `Documents/Hackintosh/efi`，config.plist SHA-256 `05919F3B…5D4DCE`，ocvalidate **0 issues** |
| EFI 与 v1.0.9 的一致性 | 用 1401 **v1.0.9** 的引擎在同一份本机报告上重建，整树 diff **只差随机 SMBIOS 与声卡 layout-id** → 功能等价 |
| 1401 Windows | `Documents/Hackintosh/tools/1401`（**v1.0.14**；包 SHA-256 `2EB8D99C…C5DA` 与官方公布值一致；内含 1401-Mac-1.0.8 + 驱动 1.0.5） |
| 驱动包 | 包内自带 `NullMoth/nullmoth-nvidia-1.0.5.tar.gz`，其 SHA-256 `F1B5CFFE…B742` 与官方 `SHA256SUMS.txt` 一致 |
| 目标 U 盘 | **Kingston DataTraveler 3.0，57.8 GB，磁盘 2，盘符 E:**（MBR + 29.3 GB FAT32） |
| 该盘原内容 | 2026-09-26 做的 1401-probe 启动盘；REPORTS 目录里的报告**已备份**到 `tools/probe-stick-backup-20261008` |
| 机器基线 | Secure Boot 已关、BitLocker 未加密、SMBIOS `iMacPro1,1`、序列号已固定（见 EFI 内 PlatformInfo） |
| 磁盘 | 已压缩出 **300.00 GB 未分配空间**（`LargestFreeExtent = 307200 MB`），位于 C: 与 Recovery 之间 |
| 有线网卡 | **Realtek 2.5G 已 Up（2.5 Gbps）** —— 装 macOS 期间的唯一网络 |

### 1.1 启动盘已做好并验证（2026-10-08）

1401 v1.0.14 写盘完成，Kingston 现在是 **MBR + 16 GB FAT32，卷标 `1401`，盘符 Y:**。盘上内容逐项核对：

| 盘上项目 | 状态 |
| --- | --- |
| `EFI/BOOT/BOOTx64.efi` | 24,576 B ✅ |
| `EFI/OC/OpenCore.efi` + `config.plist` | 630,784 B / 42,980 B ✅ |
| `EFI/OC/ACPI/` | 5 张 SSDT（EC / PLUG-ALT / USB-Reset / USBX / Disable_Network_GPP7）✅ |
| `EFI/OC/Drivers/` | HfsPlus / OpenRuntime / ResetNvramEntry ✅ |
| `EFI/OC/Kexts/` | **10 个 kext 全部启用且带可执行文件** ✅ |
| `EFI/OC/Resources/` | 字体 / 图标 / 标签 ✅ |
| `com.apple.recovery.boot/` | `BaseSystem.dmg` **847.45 MB** + chunklist ✅ |
| `NullMoth/` | 驱动 1.0.5（269,254,042 B）+ Mac app 1.0.8 + README ✅ |

**盘上 config.plist 的关键项全部实测通过**（SHA-256 `50FD7A38…4ECA2`）：

```
SetupVirtualMap        = False      ✅ AM5 必须
ResizeAppleGpuBars     = 0          ✅ 安装态
UEFI.ResizeGpuBars     = -1         ✅ 安装态
csr-active-config      = 430a0000   ✅
ProvideCurrentCpuInfo  = True       ✅
DummyPowerManagement   = True       ✅
SecureBootModel        = Disabled   ✅
Debug.Target           = 67 (含 0x40，日志落盘) ✅
SMBIOS                 = iMacPro1,1 ✅
boot-args              = nvfb=1 nvaccel=1 nvfbheads=4 -nvkmsnosmooth amfi_get_out_of_my_way=0x1 amfi=0x80 ✅
AMD: 27 条补丁，4× core-count = BA08…（8 核），Algrey PAT=True，Shaneee PAT=False ✅
```

驱动包哈希 `F1B5CFFE…B742` 与官方 `SHA256SUMS.txt` 一致 ✅

> **注意：盘上这份 config 才是权威版本。** 它是 1401.exe 新建的，SMBIOS 序列号/UUID 与
> `Documents/Hackintosh/efi/` 里那份（我早先构建的）**不同**（两者只差这些随机值和 ALC1220 layout-id）。
> 别再拿项目里那份去覆盖盘上的 —— 就按盘上的用。

## 1.2 首次启动结果：**失败**，已定位并修复

2026-10-08 04:30 / 04:32 你启动了两次，U 盘上留下两份日志（各 256 KB）：
`opencore-2026-10-08-043024.txt`、`opencore-2026-10-08-043253.txt`。

**日志说的事**（两次完全一样）：

```
正常部分：
  #[EB|LIMG:FP] \System\Library\CoreServices\boot.efi   ← 从盘上 BaseSystem.dmg 加载了引导器 ✓
  #[EB|BRD:NV] Mac-7BA5B2D9E42DDD94                     ← SMBIOS 生效（这就是 iMacPro1,1）✓
  #[EB|MBA:OUT] <-v debug=0x100 ... slide=119>          ← boot-args 生效 ✓
失败部分：
  @[EB|MMD] … 66 个内存区域转储 …            ← boot.efi 在找地方放内核
  #[EB|STOP] 0x16
  Displaying boot failure graphic for 10 seconds...
```

这个签名（**转储内存映射 → 立刻 STOP**）就是 Dortania 记录的
**「Couldn't allocate runtime area」**：内核在内存映射里找不到足够大的连续落点。

**根因**：硬件嗅探器把这块主板的内核芯片组读成裸 `"AMD"`（AM4/AM5 的 FCH 都是同一个 PCI ID，
Windows 也报不出具体型号）。而 OpCore-Simplify 的规则只在芯片组**能被命名**时才开
`DevirtualiseMmio`（Intel 名单，或 AMD 里仅 TRX40），所以我们的配置里它是 `False`。
Dortania 的 KASLR 修复页给的解法正是：**开 `DevirtualiseMmio`**（把 MMIO 区域的 runtime
属性去掉，腾出空间给内核），并配 `ProvideCustomSlide`（我们已经是 `True`）。

### 本次已改的三项（备份：`tools/stick-config-backup-20261008-044251.plist`）

| 项 | 改前 | 改后 | 目的 |
| --- | --- | --- | --- |
| `Booter > Quirks > DevirtualiseMmio` | `False` | **`True`** | 修「Couldn't allocate runtime area」 |
| `NVRAM > Add > prev-lang:kbd` | 空 | **`zh-Hans:252`**（ASCII） | 安装器默认简体中文 |
| `NVRAM > Delete` | — | 追加 `prev-lang:kbd` | 防止固件里的旧值压过新值 |
| `UEFI > Drivers` | 3 个 | **追加 `OpenCanopy.efi`（启用）** | 图形化引导界面 |
| `Misc > Boot > PickerMode` | `Builtin` | **`External`** | 同上（OpenCanopy 需要 External） |

改动后 **ocvalidate 1.0.9：No issues found**。

取值依据：`OpenCanopy.efi`/`PickerMode` 的写法与 `prev-lang:kbd` 的格式取自 OpenCore 1.0.9 官方
`Docs/Sample.plist` 与 `Docs/Configuration.pdf`（"Format is lang-COUNTRY:keyboard…"）；
`DevirtualiseMmio` 取自 Dortania 的
[Fixing KASLR slide values](https://dortania.github.io/OpenCore-Install-Guide/extras/kaslr-fix.html)。

> 顺带发现：你在 04:36 用 **OCAT** 保存过配置（文件从 42,980 → 69,891 B）。它只加了 kext 版本注释
> 和一段空的 `Misc > Serial` 默认值，**功能没有变化**（`SetupVirtualMap` 仍是 False）。

### 下次启动怎么做

1. 插盘开机 → 这次引导界面应该是**图形化**的（最坏情况：OpenCanopy 没加载成功，
   OpenCore 会自动退回文字界面，不影响启动，只是没图标）
2. 选 **macOS Installer**（结尾带 `(dmg)`）
3. 如果仍然黑屏/重启，请记下**屏幕最后显示的那几行**
4. **若安装界面仍是英文**：在 OpenCore 界面按空格键调出菜单，选 **Reset NVRAM**，再启动一次
   （`prev-lang:kbd` 是 NVRAM 变量，固件里的旧值可能压过它）

### 如果还是失败（按顺序试）

| 顺序 | 动作 | 理由 |
| --- | --- | --- |
| 1 | BIOS 里把 **Resizable BAR 关掉** | 你刚开了 ReBAR，显卡 BAR 变大 → MMIO 区变大 → 内核可用的连续内存更少。驱动 1.0.5 起**不再要求** ReBAR 开启（它会自己放大 BAR），所以关掉是安全的 |
| 2 | 给 `Booter > MmioWhitelist` 加条目 | 若 `DevirtualiseMmio` 反而弄坏了必须保留的 MMIO 区域 |
| 3 | boot-args 加 `slide=1` | 手工约束内核落点（老办法） |

## 1.3 第二次启动：修复生效，**已经进到内核**（2026-10-08 05:2x 复核）

盘上现在共 6 份日志。把文件名时间和实际时间对上之后（见下面「时间戳之谜」），真实时间线是：

| 本地时间 | 日志文件 | 结果 |
| --- | --- | --- |
| 04:30 | `opencore-2026-10-08-043024.txt` | ❌ `STOP 0x16`（改之前） |
| 04:32 | `…-043253.txt` | 空日志 |
| 04:46 | `…-044629.txt` | 空日志 |
| 04:48 | `…-044734.txt` | ✅ 到 `[EB\|#LOG:EXITBS:START]` |
| 04:53 | `opencore-2026-10-07-205238.txt` | ✅ 到 `[EB\|#LOG:EXITBS:START]` |
| 05:17 | `opencore-2026-10-07-211758.txt` | 空日志 |

**`STOP 0x16` 消失了。** 现在两次都走到了 `[EB|#LOG:EXITBS:START]` —— 这是 boot.efi 把控制权交给内核的那一瞬间，
也就是说 `DevirtualiseMmio=True` 这个修复**生效了**。

### 时间戳之谜（解释了那两份「10-07」日志）

Windows 本地时间 05:22 时，UTC 是 **10-07 21:22**（时区 +480 分钟）。而那两份日志内部时间戳是
`2026-10-07T20:53` / `T21:17` —— 正好等于本地的 04:53 / 05:17。也就是说：

**录制日志用的 RTC 从「本地时间」变成了「UTC」。谁改的？macOS 内核。**

macOS 把硬件时钟当 UTC 用（Windows 当本地时间用），内核启动后会回写 RTC。所以这反过来证明
**内核真的跑起来了**（04:48–04:53 那两次）。

> 副作用：现在双系统切回来，**Windows 时间会显示成 UTC（差 8 小时）**。
> 以后修：Windows 侧加注册表 `HKLM\SYSTEM\CurrentControlSet\Control\TimeZoneInformation\RealTimeIsUniversal = 1`(DWORD)
> 让 Windows 也把 RTC 当 UTC。**现在别改**，等 macOS 装好一起处理。

### 磁盘状态：macOS 已经建好 APFS 容器

```
磁盘0 分区4：偏移 1606.84 GB，大小 300.89 GB，类型 {7c3457ef-0000-11aa-aa11-00306543ecac} = APFS
```

**这是 macOS 磁盘工具建的**（Windows 做不出 APFS）。说明你已经进到安装器界面、把那块空间抹成 APFS 了。
所以进度不是「进不去」，而是**安装过程中/安装后内核 panic**。

⚠️ **一个必须知道的副作用**：原来盘尾那个 907 MB 的 Windows 恢复分区不见了 ——
`300.89 = 300 + 0.89`，它被并进 APFS 容器了。Windows 还能正常启动，但
**「重置此电脑」和 WinRE 自动修复没了**。这个可以以后修（重建 WinRE 分区），不影响本次安装，
先记在账上。

### 现在卡在哪：内核 panic

`[EB|#LOG:EXITBS:START]` 之后就是内核的事，这段**不会再写进 OpenCore 日志**（控制权已交出去），
所以日志到此为止是正常的。屏幕上有 `-v`（verbose）文字，panic 会直接打在屏幕上。
盘上没有 `panic-*.txt`：OpenCore 的 `ApplePanic` 需要在**下一次启动**时从 NVRAM 取回 panic 信息，
而这次没取到（可能被 NVRAM 重置吞掉了），所以**屏幕上的文字是唯一证据**。

### 下一步（按性价比排序）

| # | 动作 | 理由 |
| --- | --- | --- |
| **1** | **BIOS 把 Resizable BAR 关掉**（Above 4G 保持 On） | 现在 ReBAR 开着，RTX 5060 的 BAR 窗口有 8 GB+，正好落在 MMIO 区里 —— 而 `DevirtualiseMmio` 会把 MMIO 区的 runtime 属性抹掉交给内核当普通内存用。**让内核把显卡的 BAR 当内存用，很容易直接崩**。关掉 ReBAR 后 BAR 缩到 256 MB，风险大幅下降；而驱动 1.0.5 起会自己放大 BAR，不依赖这个。 |
| 2 | 若仍 panic：同时把 `DevirtualiseMmio` 改回 `False` | 也许当初的「Couldn't allocate runtime area」本来就是 ReBAR 造成的，关掉 ReBAR 后就不再需要虚拟化 MMIO，从而彻底避开上面那个隐患。 |
| 3 | 把 panic 文字拍下来 | panic 会指向具体原因（CPUID/补丁 / 某个 kext / slide 等），比继续猜快得多。 |

## 1.4 录像解码结果 + 「那两个补丁」为什么不适用（2026-10-08 06:0x）

### 从视频帧里读出的完整 panic（`VID_20261008_055808.mp4`）

```
launchd|05:58:17| <Notice>: System shutdown begin
launchd|05:58:17| <Notice>: Will be calling reboot(2) with flags: 0x0
launchd|05:58:17| <Notice>: Quiescing queues
launchd|05:58:17| shutdown LAUNCH_SHUTDOWN_TEARDOWN -> WAITING_ON_QUIESCE
launchd|05:58:17| <Notice>: quiesce complete / Flushed 46 logs [limit: 20]

panic(cpu 2 caller 0xffffff801ab3d64a): Kernel trap at 0xffffff801ae1e8ad, type 14=page fault
CR2: 0x00000000FEA802B0   CR3: 0x000000001D9BCE000
Fault CR2: 0x00000000FEA802B0, Error code: 0x0, Fault CPU: 0x2

Panicked task 0xffffff8722e3f980: 6 threads: pid 1: launchd
Backtrace (CPU 2): _handle_debugger_trap / _kernel_trap / _panic / _sync_iss_to_iks ...
No mapping exists for frame pointer; Backtrace terminated-invalid frame pointer

Process name corresponding to current thread: launchd
Boot args: -v debug=0x100 keepsyms=1 nvfb=1 nvaccel=1 ... amfi=0x80
Mac OS version: 24G90        Kernel: Darwin 24.6.0 ... RELEASE_X86_64
System model name: iMacPro1,1 (Mac-7BA5B2D9E42DDD94)
System shutdown begin: YES
Panic diags file unavailable, panic occurred prior to initialization
System uptime in nanoseconds: 288288676813      <- 288 秒 = 4.8 分钟
```

**结论**：panic 确实在 `reboot(2)` 那一刻（不是安装阶段）；系统此前已正常运行 288 秒；
backtrace 全是 `mach_kernel`，**没有任何第三方 kext**。

### 关键：故障地址落在一个「被我们 devirtualize 掉的 MMIO 区」里

```
CR2 (出错的地址) = 0xFEA802B0

04:30 日志里的内存映射（DevirtualiseMmio 尚未开启时拍的）：
  [# 57] type=0x0B(MMIO)  0x00000000E0000000  0x10000 页
  [# 58] type=0x0B(MMIO)  0x00000000F7000000  0x7E00 页   覆盖 0xF7000000 - 0xFEE00000
  [# 59] type=0x0B(MMIO)  0x00000000FEE00000  0x1 页
  [# 60] type=0x0B(MMIO)  0x00000000FEE01000  0x11FF 页

0xFEA802B0 落在 region#58 区间内 ✓
```

OpenCore 文档对 `DevirtualiseMmio` 的定义与警告：

> **Remove runtime attribute from certain MMIO regions.** … This quirk typically frees between 64 and 256
> megabytes of memory … **On certain firmware, a list of addresses that need virtual addresses for proper
> NVRAM and hibernation functionality may be required. Use the MmioWhitelist section for this.**

即：DevirtualiseMmio 是**拿固件的 MMIO 映射换内核空间**。我们用它在 04:48 成功开启了机（治好了
`STOP 0x16`），但它同时把某些固件仍需要的 MMIO 映射交了出去 —— 这与「能开机、关机会崩」的现象吻合。

### 为什么 Dortania 的 FixShutdown 补丁**不能照装**

补丁本体（官方原文）：

```asl
External (_SB_.PCI0.XHC_.PMEE, FieldUnitObj)
External (ZPTS, MethodObj)
Method (_PTS, 1) { ZPTS (Arg0)
    If ((0x05 == Arg0)) { \_SB.PCI0.XHC.PMEE = Zero } }
```

对你这块板子的实测核查：

| 需要的东西 | 你的 DSDT | 结果 |
| --- | --- | --- |
| `Device (XHC_)` | ❌ 只有 `XHC0` / `XHC1` / `XHC2`，且都是**只有 `_ADR` 的空壳** | 路径不存在 |
| 字段 `PMEE` | ❌ 全表搜不到 | 无从清零 |
| `_PTS` 存在 | ✅ 4504 行 | rename 本身可行 |

所以：**USB 那半不成立**（编译不过或指向不存在的对象）；而官方文档自己写着
*"Without it this patch is worthless"* —— 只做 `_PTS`→`ZPTS` 改名、不加能加载的 SSDT，
会让 `_PTS` 彻底消失（关机/睡眠预处理全部不执行），**比现在更糟**。故未安装。

### 顺带查到的另一条线索：每次关机都会经 SMI 进 SMM

你的 `_PTS`（4504 行）在关机时（Arg0=5）会调用：

```
Method (_PTS, 1) { If (Arg0) { \_SB.TPM.TPTS (Arg0)  ... } }
```

而 `TPTS`（5707 行）在 S5 分支里：

```
Case (0x05) { RQST = 0; FLAG = 0x09; SRSP = 0; SMI = OFST }   // SMI 是 IO 端口
Sleep (0x012C)                                                 // 300 ms
```

其中 `SMI` 属于 `OperationRegion (TSMI, SystemIO, SMIA, 0x02)`，而 `SMIA = 0xB2` ——
**0xB2 正是 ACPI 的 SMI 命令端口**。也就是说每次关机/重启，固件都会被 SMI 拉进 SMM 跑一段。
（注：故障地址 0xFEA802B0 并**不**落在 DSDT 任何 OperationRegion 里，所以这条是**旁证**而非直接原因。）

## 1.5 读了 NullMoth 的 Discord（2026-10-08 06:3x 起）—— 拿到官方修复

### 怎么读到的

Discord 消息只能靠登录态读。三条路实测：

| 方式 | 结果 |
| --- | --- |
| 直接 fetch / Discord API（无会话） | SPA 外壳 73 KB / API 401 |
| Codex 内置浏览器 + Chrome 扩展桥 | `Codex auth token is unavailable`（本机已知故障） |
| **用户手动登录的调试实例 + CDP** | ✅ 用 `--remote-debugging-port=9222` + 独立 `--user-data-dir` 起一个实例，用户登录后经 CDP 抓到的 `Authorization` 头直接调 API |

> 为什么不用 SaseQ/discord-mcp：它用**机器人 token**（JDA 库），必须把机器人**邀请进服务器**；
> NullMoth Systems 不是我们的服务器，普通成员没权限加机器人 —— 这条路不通。

### 🔴 最重要：1401 v1.0.15 已发布，官方修的就是 AMD 的 0x16

v1.0.15（2026-10-07T21:22Z = 本地 10-08 05:22）发布说明：

> **Ryzen and other AMD PCs:** the macOS installer no longer stops right after OpenCore (Apple's boot error
> 0x16 / "EB.MM.AKM"). **DevirtualiseMmio is now off on AMD boards** except Threadripper TRX40/TRX50, which need it.
> … The stick carries **NVIDIA driver 1.0.6** and **1401 Mac 1.0.9** …
> SHA-256: `708398017ee943cd1b2599ac4d9a1910431e70d3f19d6b9398738d3ab529b88d`

包已下载并校验通过（289,030,658 B，哈希与官方一致）。`policy.py` 的实测 diff（v1.0.14 → v1.0.15）只加了一条 AMD 规则：

```python
# AMD platforms whose MMIO layout needs DevirtualiseMmio (Dortania: TRx40); everything else boots with it off.
AMD_DEVMMIO_ON = {"TRX40", "TRX50"}
...
if cpu.Manufacturer == "AMD" and chipset not in AMD_DEVMMIO_ON and quirks.DevirtualiseMmio is True:
    quirks["DevirtualiseMmio"] = False
```

**作者的原始依据**（写在代码注释里，引用了真实上报）：

> ⛔ 10-07 (stick log NM-Z1VR8TR9 + config NM-EPAV1SHF, **1401 1.0.14**): a Ryzen board with SetupVirtualMap already
> off **still stopped at EB.MM.AKM / STOP 0x16 — with DevirtualiseMmio ON** (and a 2-entry MmioWhitelist).
> **Every Ryzen stick that passed boot.efi had it off** (NM-9D6C0VP7, NM-7QZKR9WS). Dortania AMD Zen config:
> DevirtualiseMmio NO, YES only on TRx40.

⚠️ 注意方向：官方结论是 **AMD 要关** `DevirtualiseMmio`；而我们在 §1.2 里是**手动打开**它才过的 0x16。
也就是说**我们这块板子可能与作者样本相反**（也可能只是两种不同的 0x16 成因）。

### driver 1.0.6（随 v1.0.15 附带）里与我们直接相关的修复

> - **Cards that start with a small memory window (Resizable BAR off in the BIOS):** the driver enlarges the
>   window to at most 8 GB. 16 GB stopped some RTX 3060 and RTX 5080 cards during startup.
> - **OpenCL apps** (Geekbench OpenCL and others) no longer quit while building their kernels.
> - **RTX 20/30/40 series:** WindowServer no longer crashes when you change the resolution.
> - **eGPUs and cards behind a PCIe switch or Thunderbolt:** the driver no longer moves the card's memory window.
> - **Remove NVIDIA driver now removes it.**

第一条说明：**BIOS 关掉 ReBAR 是被官方支持的路**（驱动会自己把窗口放大到最多 8 GB）。

### 社区里的 AMD 成功案例（#it-boots）

| 机器 | 原话 |
| --- | --- |
| **R9 9950X3D + RTX 5080 + X870E** | "DevirtualiseMmio: true + I had to add a few entries to MmioWhitelist" |
| **Ryzen 9 7950X3D + RTX 5070 Ti** | "Needed some MmioWhitelist changes. It boots!" |

两条都提到 `MmioWhitelist`，但**都没给具体地址**；Dortania 的 KASLR 页（`#using-devirtualisemmio`）是社区推荐的做法。

### 作者的工作方式（#general，nullmoth 本人）

> "just keep posting the logs will get fixed" / "there are logs that get sent when theres a crash or issue"

即：他靠 **1401 Mac app 的 Send logs** 上报来定位和修复。所以我们的 reboot panic 最有效的上报路径就是
进 macOS 后跑 1401 Mac app → Crash report → **Send logs**，把 report ID 发到 Discord。

## 1.6 用 v1.0.15 重建启动盘（2026-10-08 06:4x）

这次没有用 GUI，而是**复刻 1401.exe 的调用链**无界面完成（源头：`Engine.cs`
`python -u -m <args>`，工作目录 `engine\app`；`MainForm.cs` 的三步参数）：

```
p1401.scan   <WORK>\scan
p1401 build  <WORK>\scan\Report.json <WORK>\scan\ACPI <WORK>\efi --json
p1401.usbwriter write 1 <WORK>\efi 24 --driver <...1.0.6.tar.gz> --extra <...1401-Mac-1.0.9.zip>
```

`WORK = %LOCALAPPDATA%\NullMoth\1401`（与 GUI 相同）。提权用自建的 `elevate.ps1`
（`Start-Process -Verb RunAs` + 输出落文件）。

### 过程与两个坑

1. **引擎在 `PickerMode=Builtin` 时不会把 `OpenCanopy.efi` 放进 EFI**（构建末尾的
   「Cleaning up unused drivers」把它删了）。所以打完 config 补丁后必须**把文件补回去** ——
   从 `~/.cache/1401/OpenCore-1.0.9-RELEASE.zip` 里的 `X64\EFI\OC\Drivers\OpenCanopy.efi`
   解出来放进 `EFI\OC\Drivers\`（114,688 B）。
2. **盘符冲突**：Kingston 被分到 `Z:`，而 `Z:` 是一个已连接的网络盘
   `\\192.0.2.50\Data`（你的视频就在那）。网络映射在路径解析上优先，所以
   `Z:\` 看到的是网络盘；启动盘的真实内容要用卷 GUID 路径
   `\\?\Volume{8b5cd61c-…}\` 访问。已核对：**网络盘未被误写**，视频完好。
   下次取盘内容时注意这一点。

### 盘上结果（全部核对通过）

| 项 | 值 |
| --- | --- |
| 卷标 / 文件系统 | `1401` / FAT32 16 GB（MBR，与 1401 的设计一致） |
| `EFI\BOOT\BOOTx64.efi` | 24,576 B |
| `EFI\OC\OpenCore.efi` | 630,784 B |
| `EFI\OC\Drivers\` | HfsPlus / OpenRuntime / **OpenCanopy（新增）** / ResetNvramEntry |
| `EFI\OC\config.plist` | 43,319 B，sha256 `a6ac0c0b8572003e…` |
| `com.apple.recovery.boot\BaseSystem.dmg` | 888,615,275 B |
| `NullMoth\` | 驱动 **1.0.6** + Mac app **1.0.9** + README |
| **ocvalidate 1.0.9** | **No issues found** |

### 这次 config 的关键值（就是我们上一轮讨论的结果）

| 键 | 值 | 来源 |
| --- | --- | --- |
| `Booter > Quirks > DevirtualiseMmio` | **`False`** | 官方 v1.0.15 的 AMD 策略（唯一变化） |
| `Booter > Quirks > SetupVirtualMap` | `False` | 官方（v1.0.7 起） |
| `ProvideCustomSlide` | `True` | 官方 |
| `ResizeAppleGpuBars` / `UEFI.ResizeGpuBars` | `0` / `-1` | 官方（安装态小 BAR） |
| `boot-args` | `-v debug=0x100 keepsyms=1 nvfb=1 nvaccel=1 nvfbheads=4 -nvkmsnosmooth amfi_get_out_of_my_way=0x1 amfi=0x80` | 官方 + 驱动要求 |
| `csr-active-config` | `430a0000` | 官方 |
| SMBIOS | `iMacPro1,1` | 官方 |
| `Misc > Debug > Target` | `67`（+AppleDebug/ApplePanic） | 官方：失败日志写盘 |
| **`NVRAM > Add > prev-lang:kbd`** | **`zh-Hans:252`** | **我们加的：安装器默认简体中文** |
| **`NVRAM > Delete`** | 含 `prev-lang:kbd` | **我们加的：防固件旧值盖过它** |
| **`Misc > Boot > PickerMode`** | **`External`** | **我们加的：图形化引导** |
| **`UEFI > Drivers`** | 含 `OpenCanopy.efi`（启用） | **我们加的：图形化引导** |

> 注意方向：官方 v1.0.15 把 AMD 的 `DevirtualiseMmio` **关掉**；而我们上次是**手动打开**它才越过
> `STOP 0x16`。这次按官方走，**用实机结果判定我们这块板子属于哪一类**。

### 下次启动要看的

1. 引导界面是否是**图形化**的（OpenCanopy 生效）；最坏情况自动退回文字界面，不影响启动
2. 安装器界面是否**简体中文**；若仍英文 → 引导界面按空格 → **Reset NVRAM** → 再启动
3. **会不会又卡 `EB.MM.AKM` / `STOP 0x16`** —— 这一条决定我们是否要退回「手动开 DevirtualiseMmio」
4. 失败时 U 盘上会有 `opencore-*.txt` / `panic-*.txt`（日志已开），回 Windows 可用 1401 的 Scan for logs

## 1.7 把 APFS 分区读出来了（2026-10-08 06:5x）—— 但里面没有 panic 报告

### 终于走通的路：dump 分区 + apfs-fuse

前几次失败的原因与最终解法：

| 尝试 | 结果 |
| --- | --- |
| `wsl --mount \\.\PHYSICALDRIVE0 --partition 4 --bare` | ❌ `ERROR_SHARING_VIOLATION`（系统盘正被 Windows 占用） |
| 7-Zip 直接读 `\\.\PhysicalDrive0` | 能认出 GPT 与「3.Basic data partition.apfs」，但**不会进 APFS 内部** |
| VSS 卷影副本 | ❌ 拿不到（路径拼错 + 删除方法不对） |
| **dump 分区到镜像 + Linux 的 apfs-fuse** | ✅ **成功** |

做法（全部需管理员，原始灵感来自 `apfs-fuse` 支持读镜像文件）：

1. 管理员直读物理盘（只读，`FileShare::ReadWrite`），把 APFS 分区**按字节拷成镜像**
   （实测读速 **1,700–2,500 MB/s**，2GB 用时 0.8s、8GB 用时 4.8s）
2. WSL 里编译好的 `apfs-fuse` 读镜像（老代码在 GCC 15 上缺 `<cstdint>`，
   用 `-include cstdint -DCMAKE_POLICY_VERSION_MINIMUM=3.5` 编过）

第一次只 dump 2GB 不够：`Invalid omap block @ oid 0x9c31a`（≈2.44 GB 处）。
dump 到 **8GB** 后容器正常打开。

### 容器内容

```
Volume 0  551904A2-…  no role      "Macintosh HD"   consumed 3,496,161,280 B
Volume 1  B30ADA2F-…  Preboot     "Preboot"        consumed    10,944,512 B
Volume 2  2BE2969A-…  Recovery    "Recovery"       consumed        20,480 B
Volume 3  1515F67E-…  VM          "VM"             consumed        20,480 B
```

### 🔴 关键结论：macOS 根本没装完

卷 0（Macintosh HD）根目录**只有两项**：

```
drwx------  .fseventsd
drwxr-xr-x  macOS Install Data
```

`macOS Install Data` 里：

```
      181 B  index.sproduct
   53,852 B  InstallAssistant.pkg.integrityDataV1
3,491,758,080 B  InstallAssistant.pkg.partial     ← 3.25 GB，注意是 .partial
          d  Locked Files/（.disk_label / Boot Files）
```

**卷 0 的已用容量 3,496,161,280 B ≈ 正好等于那个 `.partial` 文件**，说明整个卷上只有它。
`System/Library`、`usr/lib/dyld`、`Applications`、`Library/Logs` **全都不存在** ——
安装停在「把 InstallAssistant.pkg 写进目标卷」这一步，**从未写入真正的系统**。

### 所以：没有 panic 报告可拿

整卷找不到任何 `DiagnosticReports`、`*.panic`、`*.ips`：

| 卷 | 有无 panic 报告 |
| --- | --- |
| Macintosh HD | ❌（只有 macOS Install Data） |
| Preboot | ❌（只有 `var/db/{AdminUserRecoveryInfo,CryptoUserInfo}.plist` + `ProgressMarkers`） |
| Recovery / VM | ❌（各 20,480 B，几乎空） |

这与屏幕上那句 **`Panic diags file unavailable, panic occurred prior to initialization`** 一致 ——
panic 发生在 diags 子系统起来之前，**从未落盘**。唯一记录仍是我们从视频帧里读出来的那份文本（§1.4）。

### 下次怎么拿到

1. **新盘已开 `ApplePanic=True` + `Target=67`**：一旦再 panic，会直接写成
   `panic-YYYY-MM-DD-HHMMSS.txt` 到 U 盘根目录。
2. panic 信息由内核写进 **NVRAM**，OpenCore 在**下一次启动**时取回并落盘。
   所以即使这次的 panic 还在 NVRAM 里，**只要插新盘启动一次**，也可能直接出现 `panic-*.txt`。
3. 若系统真装起来了：进 macOS 后看 `/Library/Logs/DiagnosticReports/`，或用 1401 Mac app
   的 **Crash report → Send logs**。

### 残留物

dump 出来的镜像在 `C:\Users\hiliang\.codex\tmp\apfs-img\`（`head2g.img` 2GB + `head8g.img` 8GB，
共 10GB，**可以随时删**）。整套读盘工具（`dump_apfs.ps1`、`try_apfs_img.sh`、`explore_apfs*.sh`）
留在同一个临时目录，以后要再读 APFS 可直接复用。

## 1.8 ⚠️ 更正：1401 报的「Resizable BAR: Disabled」是**误报**（2026-10-08 07:0x）

### BIOS 两项其实都开着（实测证据）

```
nvidia-smi -q
    Product Name : NVIDIA GeForce RTX 5060
    FB Memory Usage   Total : 8151 MiB   Reserved : 256 MiB
    BAR1 Memory Usage Total : 8192 MiB   Used : 8163 MiB
```

**`BAR1 = 8192 MiB` 是决定性证据**：RTX 5060 的 BAR1 默认只有 **256 MB**，只有
**BIOS 的 Resizable BAR 打开**、窗口被放大到 8 GB 才会显示 8192 MiB；而 8 GB 的窗口
**不可能映射在 4 GB 以下**，所以这同时证明 **Above 4G Decoding 也是 On**。
两项都开着 —— 先前那句「还剩一项没改」是错的，**BIOS 不需要再动**。

### 误报根因：它读的是注册表，不是硬件

`upstream/Hardware-Sniffer/Scripts/platforms/windows.py` 的 `gpu()`：

```python
if reg_key == "KMD_RebarControlMode" and value == 1:
    is_resize_bar_enabled = True      # 没有这个键 → 保持 False → 报 "Disabled"
```

实测注册表（`HKLM\SYSTEM\ControlSet001\Control\Class\{4d36e968-…}`）：

| 子键 | MatchingDeviceId | 含 bar/rebar 的值 |
| --- | --- | --- |
| `0000` | `pci\ven_10de&dev_2d05`（**RTX 5060**） | **一个都没有** |
| `0001` | `PCI\VEN_1002&DEV_13C0`（AMD 核显） | `KMD_RebarControlMode: 0` |

`KMD_RebarControlMode` 是 **NVIDIA 驱动自己的 ReBAR 偏好覆盖值**，**默认不写**
（除非在 NVIDIA 控制面板里手动改过）。缺键 ≠ 关闭，代码却把「缺键」当 `False` → **假阴性**。

另外 **`Above 4G Decoding` 在 Windows 侧根本没有探测代码**（只有 `platforms/linux.py` 有），
所以 `Report.json` 的 `BIOS` 段里没有这个键，`guide.py` 永远读到空字符串，只能无条件列成「must」。

### 对配置的实际影响：**零**

`config_prodigy.py:636` 本该用这个读数决定 `ResizeAppleGpuBars = 0 / -1`，误报会把它带到 `-1`；
**但 `p1401/policy.py:189-197` 会无条件覆盖**（注释就是为 ReBAR 显卡写的）：

```python
driver = nullmoth.apply(cfg, result, change)
# , measured on an RTX 5060 with Resizable BAR on: ... The Mac helper sets the 8 GB BAR back after install.
if driver:
    cfg["Booter"]["Quirks"]["ResizeAppleGpuBars"] = 0
    cfg["UEFI"]["Quirks"]["ResizeGpuBars"] = -1
```

盘上 config 实测：`ResizeAppleGpuBars = 0`、`UEFI.ResizeGpuBars = -1` —— **正是 ReBAR 打开时该有的值**。
所以误报只影响屏幕上那段 BIOS 提示文字，**没有写进 EFI**。

## 1.9 第三次启动（v1.0.15 官方配置）：又卡 `STOP 0x16` —— 正是 §1.6 预判的那条（2026-10-08 07:0x）

盘上只有一份日志：`opencore-2026-10-08-065655.txt`（256 KB，06:56:54 建立、06:59:16 收尾）。
**一份会话里含 3 次尝试**（三个 AAPL 块：01.9s / 43.5s / 97.2s），三次签名完全相同：

```
AAPL: #[EB.MM.AKM|!] Err(0xE) <- EB.MM.MKP
AAPL: #[EB.BST.FBS|MM!] 0 <- EB.MM.AKMr2 0x16a0 0x0
AAPL: @[EB|MMD][# 0..#65] … 66 条内存映射转储 …
AAPL: #[EB|STOP] 0x16
AAPL: Displaying boot failure graphic for 10 seconds...
AAPL: Displaying text console for 10 seconds...
OC: Boot failed - Aborted          ← OpenCore 收回控制权，退回选择界面
OCB: StartImage failed - Aborted
```

这就是 §1.2 里 04:30 那次**同一个故障**（Dortania 的 *Couldn't allocate runtime area*）：
boot.efi 拿着固件给的内存映射**找不到地方放内核的 runtime 区**，于是放弃。**内核从未启动** ——
没有 panic、没有 kext、与 NVIDIA 驱动无关。选中的是安装器
（`[EB|LIMG:DP] … DMG_0000000034F7316B.dmg` → `\System\Library\CoreServices\boot.efi`），
启动失败图能画出来说明 OpenCanopy 图形界面这一层是好的。

### 判定：官方 v1.0.15 的 AMD 策略在我们这块板子上是反的（A/B 已做）

| 时间 | `DevirtualiseMmio` | 其它内存映射相关项 | 结果 |
| --- | --- | --- | --- |
| 04:30 | False | SetupVirtualMap False / MmioWhitelist 空 | ❌ `STOP 0x16` |
| 04:48 | **True** | 同上 | ✅ 到 `[EB|#LOG:EXITBS:START]`（进内核） |
| 04:53 | **True** | 同上 | ✅ 到 `[EB|#LOG:EXITBS:START]` |
| 06:57 | False（v1.0.15 官方输出） | 同上 | ❌ `STOP 0x16`（本次） |

差异逐项核对过（`plistlib` 全量扁平化 diff）：

- `tools/stick-config-backup-20261008-044251.plist`（04:30 那次失败时盘上的配置，后被 OCAT 存过一次）
  → 官方 v1.0.15 输出（`E:\EFI\OC\config.plist.bak-20261008-064045`）之间**只差** kext 注释字段、
  `Misc > Serial` 的默认段、以及 SMBIOS 的 `MLB` / `ROM` / 序列号 / UUID（重新随机）——
  **没有一个与内存映射有关**。
- 也就是说 04:30 与 06:57 两次失败的配置在内存映射相关项上等价；两次成功 / 两次失败之间
  **唯一的变量就是 `DevirtualiseMmio`**。

上游 `tools/1401/engine/app/p1401/policy.py` 自己就写着这条错误码（提交 10-07，工单 NM-5WSVHVK1 /
NM-EQB69GYK / NM-V1W41NGH / NM-ZW53KZWZ）：AMD 板关 `SetupVirtualMap`；而另一段注释
（NM-Z1VR8TR9，1.0.14）记录过**另一块** Ryzen 板开着 `DevirtualiseMmio`（还带 2 条白名单）仍然挂 ——
所以这一项是**逐板**的，以本机实测为准。Dortania 的 KASLR 页给的解法正是"开 `DevirtualiseMmio`，
不行再用 `MmioWhitelist`"；Discord `#it-boots` 里 Zen4 用户的结论也一样（"zen 4 needs
DevirtualizeMmio and appropriate mmio whitelist"，另有人回"had the exact same issue"）。

### 本次已改回的两项（备份：`E:\EFI\OC\config.plist.bak2-20261008-070359`）

| 键 | 改前 | 改后 | 目的 |
| --- | --- | --- | --- |
| `Booter > Quirks > DevirtualiseMmio` | False | **True** | 修 `STOP 0x16`（本机实测唯一有效项） |
| `Misc > Debug > AppleDebug` | True | **False**（临时） | 让 OpenCore 自己的日志留在盘上。`OCABC: MMIO devirt 0x…` 那些行此前被 boot.efi 的 `AAPL:` 日志**从头覆盖**了（这份日志从偏移 0 就是 `AAPL:`，`OCABC:` 一行都没有），而那份 MMIO 列表正是建 `MmioWhitelist` 的唯一原料 |

备份 = 官方 v1.0.15 原样（sha256 `a6ac0c0b8572003e…`）。`ApplePanic` 仍为 `True`、`Target = 67`
（`1|2|64`，含写盘位）→ 真出 panic 仍会写成 `panic-*.txt` 到 U 盘根目录。

盘上当前值（读回核对，改动只有上表两项 + 之前那 4 项语言/图形化改动）：
`DevirtualiseMmio=True`、`SetupVirtualMap=False`、`ProvideCustomSlide=True`、`RebuildAppleMemoryMap=True`、
`MmioWhitelist=[]`、`ResizeAppleGpuBars=0`、`UEFI.ResizeGpuBars=-1`、`PickerMode=External`、
`Drivers=[HfsPlus, OpenRuntime, OpenCanopy, ResetNvramEntry]`、`prev-lang:kbd=zh-Hans:252`、
`csr-active-config=430a0000`、`SecureBootModel=Disabled`、`DmgLoading=Signed`、SMBIOS `iMacPro1,1`。

### 下一次启动看什么

1. 引导界面应仍是**图形化 + 简体中文**（这次失败图上已经出现过，说明 OpenCanopy 生效）。
2. **会不会再出现 `EB.MM.AKM` / `STOP 0x16`** —— 按 `DevirtualiseMmio=True` 的两次历史记录，应该不会。
3. **这次日志开头会有 `OCABC: MMIO devirt 0x…` 行**（AppleDebug 已临时关掉）。拿到它就等于拿到建
   `MmioWhitelist` 的原料；白名单定下来后要把 `AppleDebug` 改回 `True`。
4. 若进了内核、又在 `reboot(2)` 那一步 panic（§1.4 的 `CR2=0xFEA802B0`，落在 region#58
   `0xF7000000–0xFEE00000`）：处置顺序是**先用第 3 条的地址表建 `MmioWhitelist`**（把固件仍需要
   虚拟映射的区段保留），**不必**先去 BIOS 关 ReBAR —— 官方策略在安装期已把显卡 BAR 压到 256 MB
   （`ResizeAppleGpuBars=0`；旁证：本次日志里 MMIO 段最大的一块 region#57 = `0xE0000000`，长
   `0x10000` 页 = 256 MB），而驱动 1.0.6 会在 Mac 侧自己把 BAR 放回去。

## 1.10 第四次启动：**过了 `STOP 0x16`**，日志给出「boot.efi 没返回」的硬证据（2026-10-08 07:0x）

盘上新增 `opencore-2026-10-08-070935.txt`（262,144 B，07:09:34 建立）。**内容 262,144 字节全是 0x00**
（实算 `nonzero bytes = 0`），而上一份日志同一位置有 48,873 B 有效内容。

### 全零日志为什么等于「过了 0x16」

OpenCore 的写盘路径（源码实证，`OpenCorePkg 1.0.8`，与盘上 1.0.9 同源）：

| 事实 | 出处 |
| --- | --- |
| `Target = 67 = 0x43` = `0x01 启用 + 0x02 屏幕 + 0x40 写文件`，**没有** `0x80`（不安全快写） | 盘上 config；`Docs/Configuration.tex` 的 Target 位表 |
| 没有 `0x80` 时走"安全"分支：**每次日志都整块重写 256 KB 缓冲** | `Library/OcLogAggregatorLib/OcLog.c` 写文件分支 |
| 缓冲 = `Private->AsciiBuffer`（256 KB），文件内容就是它**从偏移 0 起的整块** | `OcLogInternal.h`（`OC_LOG_BUFFER_SIZE = BASE_256KB`）+ `OcSetFileData()`（`OcFileLib/FileProtocol.c`，`File->Write(File, &Size, Buffer)`） |
| 协议刚配置完时缓冲是空的 → 第一次落盘 = 256 KB 全零（文件就此诞生，大小 256 KB） | `OcConfigureLogProtocol()` 末尾那次 `OcSetFileData` |
| 之后每一条日志（含 `OC: Boot failed - Aborted`）都会再整块重写一次 | 同上 |

这个 RELEASE 版还有个关键性质（**实测**）：除下面那两条失败提示外，**OpenCore 自己不会往文件里写别的**。
证据是 §1.9 那份日志（`AppleDebug=True`）通篇只有 `AAPL:` 行 + **3 条** `OC: Boot failed - Aborted` /
`OCB: StartImage failed - Aborted`（偏移 16214 / 32503 / 48797，恰好对应三次失败）——
**一条 `OC: OpenCore 1.0.9 is loading in RELEASE mode` 之类的启动横幅都没有**，
即 RELEASE 构建把普通 `DEBUG ((...))` 全部编译掉了，写文件的只有 AppleDebug 转发和这两条失败提示。

所以：

* **只要 boot.efi 返回失败**（`STOP 0x16` 就是这种），OpenCore 就会写下 `OC: Boot failed - Aborted` /
  `OCB: StartImage failed - Aborted` → 文件**不可能全零**（§1.9 那份就是这么写的）。
* **文件全零 = 初始那次零写入之后再没有任何一次写盘 = 这一轮 OpenCore 一条错误都没报**。

⇒ 07:09 这次**已经越过 `STOP 0x16`**（与 04:48/04:53 两次 `DevirtualiseMmio=True` 的结果一致），
不再出现"找不到内核运行时区"这一类错误。**这也说明上一轮 `DevirtualiseMmio=True` 的补丁是对的。**

> 还没法从日志区分"内核起来后很快死掉"和"在更早的地方挂住" —— 因为这一轮 `AppleDebug` 是关的，
> 所以也**不要再关它**（已改回 True）。下一步的判据变成：U 盘上有没有 `panic-*.txt`、
> 屏幕最后停在哪一行、以及 APFS 分区有没有新写入（见 1.10.4）。

> 副作用（下次取证注意）：`AppleDebug=True` 时 boot.efi 的 `AAPL:` 行会把 OpenCore 自己的早期行
> **从偏移 0 覆盖掉**（§1.9 那份日志从字节 0 就是 AAPL，`OCABC:` 一条都没有），
> 所以那份 `OCABC: MMIO devirt 0x…` 列表**从文件里拿不到**。别再用「读日志」这条路找白名单，
> 改用下面 §1.10.2 的静态证据链。

### 1.10.1 已改回盘上（备份 `E:\EFI\OC\config.plist.bak3-20261008-072227`）

| 键 | 改前 | 改后 | 目的 |
| --- | --- | --- | --- |
| `Booter > MmioWhitelist` | `[]` | `[{Address: 4244635648 (0xFD000000), Enabled: true, Comment: "MMIO 0xFD000000 (PCIe MMIO high window 0xF7000000-0xFEE00000)"}]` | 修 §1.4 那个 **reboot(2) panic**（见 1.10.2） |
| `Misc > Debug > AppleDebug` | False | **True** | 0x16 已过，现在需要 AAPL 行判断卡在哪；失败时也有内容可读 |

`ocvalidate`（1.0.9，`~/.cache/1401/3c5845b74060-ocvalidate.exe`）= **No issues found**。
其余保持：`DevirtualiseMmio=True`、`SetupVirtualMap=False`、`ProvideCustomSlide=True`、
`RebuildAppleMemoryMap=True`、`ResizeAppleGpuBars=0`、`Target=67`、`ApplePanic=True`、
`PickerMode=External` + OpenCanopy、`prev-lang:kbd=zh-Hans:252`、SMBIOS `iMacPro1,1`。

### 1.10.2 为什么白名单地址是 0xFD000000（三条互相独立的证据）

1. **本机 ACPI MCFG 实测**：`mcfg.aml` 解析出 ECAM/MMCONFIG = **0xE0000000**，0–255 号总线
   → 0xE0000000–0xF0000000，正好等于 boot.efi 内存映射 dump 里的 region #57
   （`0xE0000000`，`0x10000` 页 = 256 MB，type `0x0B` = MMIO）。
2. **panic 地址落在同一个 126 MB 窗口**：region #58 = `0xF7000000` + `0x7E00` 页
   = 0xF7000000–0xFEE00000（126 MB），而 §1.4 那次 `reboot(2)` panic 的
   `CR2 = 0xFEA802B0` **正落在它里面**。本机 DSDT 的多个 OperationRegion 也都在这个窗口：
   `TMMB 0xFED40000`（TPM）、`GSMM 0xFED80000`、`PMRG 0xFED80300`、`FACR 0xFED81E00`、
   `LUIE 0xFEDC0020`、`HPET 0xFED00000` —— 全是**关机/重启路径（`_PTS` → `TPM.TPTS` → SMI）要碰的 MMIO**。
   （Windows 侧 `Win32_PnPAllocatedResource` 也把这些地址列在 `ACPI\PNP0C02\700` / `AMDIF031` /
   `PNP0103`(HPET) / `AMDI0030` / `AMDI0010` 名下。）
3. **上游自己的 AM5 配方**：`OpCore-Simplify/Scripts/config_prodigy.py` 的 `mmio_whitelist()` 对
   **B650/X670 就是给 `0xFD000000` 这一条**，并且同一文件里「白名单非空 ⇒ 打开 DevirtualiseMmio」。
   0xFD000000 落在本机 region #58 内（0xF7000000 ≤ 0xFD000000 < 0xFEE00000）。
   上游 1401 v1.0.16 的 changelog 也证实"AMD 板本来会拿到白名单条目"（1.0.15 把它关掉才导致构建报错）——
   本机没拿到，纯粹因为嗅探器把芯片组读成裸 `"AMD"`。

**匹配语义**（`Library/OcAfterBootCompatLib/ServiceOverrides.c` + `Include/Acidanthera/Library/OcMemoryLib.h`）：

```c
if (AREA_WITHIN_DESCRIPTOR (Desc, Whitelist[Index2], 1)) { Skipped = TRUE; }
// AREA_WITHIN_DESCRIPTOR: Area >= Desc->PhysicalStart && Area+Size-1 <= LAST_DESCRIPTOR_ADDR(Desc)
```

即**"地址只要落在某个 descriptor 内，整个 descriptor 就跳过"** —— 给 0xFD000000 等于
「保留 region #58 这 126 MB 的 runtime 属性」，其余 MMIO 照旧被 devirtualise：

| region | 范围 | 页数 | 处置 |
| --- | --- | --- | --- |
| #57 | 0xE0000000（ECAM 256 MB） | 0x10000 | 仍然 devirtualise |
| **#58** | **0xF7000000–0xFEE00000** | 0x7E00（126 MB） | **被白名单跳过（保留 runtime）** |
| #60 | 0xFEE01000 | 0x11FF | 仍然 devirtualise |
| #64 | 0xC8000000 | 0x20200（514 MB） | 仍然 devirtualise |
| #65 | 0xFD00000000 | 0x300000（3 GB） | 仍然 devirtualise |

即仍然有 **≈3.8 GB** 可省（远多于 04:48 成功那次所需的量），所以「白名单把 0x16 带回来」的风险低；
万一真回来了，boot.efi 会返回、日志非零，"`OC: Boot failed - Aborted`" 一眼可辨，回退就是删掉这一条。

### 1.10.3 下次启动要看什么

1. **又出现 `EB.MM.AKM` / `STOP 0x16` 吗** —— 不该出现；出现就删掉 MmioWhitelist 那一条。
2. **安装器能不能跑到「重新启动」那一步、并且重启时不 panic** —— 这是这一轮唯一要验的假设。
   判据：如果 panic 再来，`ApplePanic=True` + `Target=67` 会把它写成 `panic-*.txt` 到 U 盘根目录
   （§1.7 解释过为什么之前拿不到）；**重启失败时请一定让人看着屏幕并把最后几行拍下来**。
3. **进度是否真的推进**：`AppleDebug=True` 时文件会以 AAPL 行开头，末尾若出现
   `OC: Boot failed` 就是 boot.efi 失败（可直接定位）；若只有 AAPL 而无 OC 行，就是内核阶段。

### 1.10.4 顺带同步的上游载荷（无需重建整只盘）

上游 07:12 发 1401 **v1.0.16**、07:26 又发 1401 **v1.0.17** + 驱动仓 **v1.0.11**。盘里的载荷升级到
**驱动 1.0.7 + 1401 Mac 1.0.11**，每一步都按官方 `SHA256SUMS.txt` 逐字节校验（旧的成对移进 `old-*/` 作回滚）：

```
5d66e1b6…ebf7  nullmoth-nvidia-1.0.7.tar.gz   269,260,020 B   本地实算 = 官方值 ✓（v1.0.10/v1.0.11 字节相同）
bf5eaf36…36bc  1401-Mac-1.0.11.zip            2,071,195 B     本地实算 = 官方值 ✓
（已被替换的：eaf9e4d4…285b Mac 1.0.10 → old-1.0.10\，驱动 1.0.6 + Mac 1.0.9 → old-1.0.6\）
```

`tools\1401-Windows-1.0.17.zip`（289,077,093 B，`ef544c67…52f8` = 官方公布值 ✓）只是留档，
**没有解包、没有重建盘**；解包出来的 `policy.py` 与 main 一致 —— 对 AMD 仍然会关
`DevirtualiseMmio`，所以将来若用它重建，`patch_stick_v3.py` 那一步**不能省**。

**1.0.16 没有推翻「AMD 上关掉 DevirtualiseMmio」**（git log 里三次提交均无回退），所以本机仍需
自己保留 `DevirtualiseMmio=True`；也正因为如此，**不重建整只盘**（重建会重新随机 SMBIOS 四项并
清掉我们三轮补丁）。细节与逐条影响见 `docs/upstream-watch.md` §9/§10。

另外补了两件**不重建盘**就能做的：

1. **Mac 侧 per-system 规则的前置文件**：上游 main 已经走到 1.0.17 / Mac 1.0.11（**只有 commit，
   还没发 release**），Mac 1.0.11 会按 `/Volumes/*/NullMoth/system-profile.json` 里的真实芯片组匹配
   驱动规则。我们没重建 1.0.17 的盘 → 这个文件本来缺失。已按上游同一份代码
   （`p1401/nullmoth.py: system_profile`）从本机真实报告生成：

   ```
   E:\NullMoth\system-profile.json   366 B   board=MAXSUN MS-TERMINATOR B850BKB WIFI
                                            chipset=AMD  platform=Desktop  cpu_vendor=amd
                                            gpu=10DE-2D05  rebar=Disabled(上游探测的假阴性，照抄不改)
   ```

   目前唯一带开关的规则 `nvenc-blackwell-only` 只匹配 turing/ampere/ada，本机 Blackwell **不命中**，
   所以现在装了也没差别；这是为将来铺路，零风险。
2. **APFS 分区只读比对确认「内核根本没动过盘」**：把 `E:\` 之外的那 300 GB APFS 分区的前 2 GiB
   与 06:45 的镜像逐字节比对（提权只读，`~/.codex/tmp/hackintosh-macos-setup/apfs_diff.ps1`）：

   ```
   compared = 2,147,483,648 bytes
   diff bytes = 0            ← 与 06:45 完全一致，一个字节都没变
   ```

   即 07:09 那次**在写盘之前就停了**（安装器都没来得及往 APFS 里写 `InstallAssistant.pkg`），
   与"文件全零 = boot.efi 没返回"互为佐证：卡点在内存映射分配修好之后的**更早阶段**，
   不是 macOS 侧的磁盘问题。也说明 `InstallAssistant.pkg.partial`（3.49 GB）还是 06:45 之前留下的那份，
   不用再看它。

## 2. 方案决定（已定）

## 1.11 ⚠️ 现在到底哪一份 OpenCore 在生效（2026-10-08 23:1x 实测定案）

**结论：两处都有 OpenCore，而且内容已经一致；但"哪份生效"取决于固件从哪儿启动。**

### 固件启动顺序（实时读 NVRAM）

```
BootOrder = Boot0001 -> Boot0080
  Boot0001  "Windows Boot Manager"  ->  内置 ESP  \EFI\Microsoft\Boot\bootmgfw.efi
  Boot0080  "Mac OS X"              ->  APFS 卷 551904A2-…\System\Library\CoreServices\boot.efi
  Boot0081  "Mac OS X"              ->  （较短，另一种 macOS 入口）
```

**固件启动项里没有任何 OpenCore 条目。** 也就是说 OpenCore 只会在两种情况下运行：

1. 你按 **F11/F8** 一次性启动菜单里选「UEFI OS（内部硬盘）」→ 走内置 ESP 的
   `\EFI\BOOT\BOOTx64.efi`；
2. 你选 **Kingston U 盘** → 走 U 盘的 `\EFI\BOOT\BOOTx64.efi`。

### 决定性证据：内置 ESP 才是最近在用的那一份

| 事实 | 出处 |
| --- | --- |
| 内置 ESP 的 `\EFI\BOOT\BOOTx64.efi` 与 U 盘的**逐字节相同**（24,576 B，sha256 `19fa90b9…`）—— 两边都是 OpenCore 的引导壳 | 实测哈希 |
| **内置 ESP 根目录有 6 份 OpenCore 日志**（`00:56`、`01:19`、`01:26`、**`13:28`、`13:35`、`13:45`**），其中 5 份结尾是 `[EB\|LOG:EXITBS:START]`（boot.efi 成功交给内核） | 读 ESP |
| U 盘根目录的日志停在 `07:30`/`07:32`，之后再没有新的 | 读 U 盘 |
| 固件变量 `BootFromUSB = 0` | Maxsun 专有变量 |

⇒ **今天下午的三次启动（13:28 / 13:35 / 13:45）走的是内置 ESP，不是 U 盘。**
OpenCore 把日志写在自己**启动时所在的那个卷**的根目录，所以日志位置就是判据。

### 为什么内置 ESP 会变成这个样子（和那个 bug 对上了）

内置 ESP 根目录里有 `.fseventsd` / `.Spotlight-V100` / `.Trashes` —— **macOS 挂载并写入过它**。
再加上 Mac 端 1401 的 "USB-to-internal OpenCore promotion"，可以确认：
**那个 promotion 把 U 盘的 OpenCore 整套复制进了内置 ESP，并且顺手把内置 ESP 原本的
`\EFI\Boot\BOOTx64.efi`（Windows 的 fallback 引导器）替换成了 OpenCore 的引导壳。**
这正是上游 v1.0.16 承认的 *"could replace the destination EFI tree and remove active Windows
or vendor boot files"* 的机制 —— 也是你 Windows 引导消失的那条链。

### 以后改哪边？（重要）

| 你打算怎么启动 macOS | 该改哪份 config |
| --- | --- |
| F11 选「UEFI OS（内部硬盘）」 | **内置 ESP** `\EFI\OC\config.plist` |
| F11 选 Kingston U 盘 | **U 盘** `\EFI\OC\config.plist` |

**现行做法：两边保持同步**（2026-10-08 22:49 已把 UTBMap + config 同步过一次，两边 sha 都是
`863abe1a5220647e`）。好处是不管从哪边启行为一致；代价是**每次改都得改两处**，
忘了改一边就会出现"U 盘上好用、内置 ESP 上不好用"的鬼故事。

**建议（等 macOS 装稳之后再动）**：二选一收敛成单一来源 ——
要么彻底回到 U 盘启动（把内置 ESP 的 `EFI\OC` 与 `\EFI\Boot\BOOTx64.efi` 还原成 Windows 原状，
并给 OpenCore 建一个正规的 UEFI 启动项）；要么把内置 ESP 作为唯一家，U 盘只留作救援。
**现在先别动**，先把系统装完。


### 2.1 macOS 装到哪 —— **已决定：压缩 C: 出 300 GB**

详细方案、命令与依据见 `docs/disk-and-nic-plan.md`。要点：

- C 盘实际有 **1055.7 GB 空闲**（我 10-07 报的 254 GB 是错读数，已更正）；
- 但**整盘零未分配空间**（`LargestFreeExtent` = 1 MB），所以必须先压缩 C:；
- 压出来的空间位于 **C: 与 Recovery 之间**，不用移动任何分区；
- 压缩前若空间不够，先 `powercfg /h off`（`hiberfil.sys` 19 GB）+ 关分页文件（`pagefile.sys` 60 GB），重启再压；
- 这一步**可逆**：删掉 macOS 分区后右键 C: 扩展卷即可还回来。

### 2.1b EFI 分区：测试阶段一个都不动

- 1401 的设计就是 OpenCore 待在 U 盘上，测试阶段**内部 ESP 完全不参与**；
- 想脱离 U 盘启动时，**共用现有 200 MB ESP**（`\EFI\OC` 与 `\EFI\Microsoft` 并存），
  我们 EFI 只有 8.3 MB，ESP 还剩约 159 MB；
- **不要在同一个盘上建第二个 ESP**：部分固件只认第一个，或产生难以分辨的重复启动项，且毫无收益。

### 2.1c 驱动版本：必须用 1.0.5，不能用 1.0.1（重要）

2026-10-08 上游更新把这条从「可选」变成了「必须」。driver **1.0.5** 的发布说明原文：

> **Most Intel and Ryzen PCs:** the card's memory window is now placed inside the PCI window your firmware
> provides (**the old placement only worked on Core Ultra 200 boards**), and **enlarged by the driver itself
> when Resizable BAR is off in the BIOS**. Turning Resizable BAR on is still recommended.

也就是说 **1.0.1 的 BAR 放置逻辑只在作者的 Core Ultra 200 测试板上成立** —— 我们这是 AM5 锐龙，
用 1.0.1 很可能**装上驱动后屏幕点不亮**。1.0.5 修的就是这个，而且它在 BIOS 没开 ReBAR 时会自己放大 BAR。

结论：**用 1401 v1.0.14（自带驱动 1.0.5）**，不要用我们先前准备的 v1.0.9（钉的是 1.0.1）。

### 2.2 你的 Kingston（E:）会被整只擦掉

1401 做盘会 clean 整只盘（只留一个 16 GiB 的 FAT32 分区）。里面的 probe 报告我已备份，
probe 镜像本身也能随时从 GitHub 重新下载 —— 但**擦除不可逆**，所以确认一下这只盘可以清空。

## 3. 你需要先在 BIOS / 手上做的（和磁盘无关，现在就能做）

| # | 项 | 要求 | 备注 |
| --- | --- | --- | --- |
| 1 | **Resizable BAR / Re-Size BAR** | **On**（仍建议改） | 驱动 1.0.5 起会在 BIOS 关闭时**自己放大 BAR**，所以不再是硬性前提；但开着更稳。1401 的说明仍标为 must |
| 2 | **Above 4G Decoding** | **On** | 可能在 PCI/Advanced 里；有些板子要先开它 ReBAR 才出现 |
| 3 | CSM | Off | 已是纯 UEFI，保持 |
| 4 | Secure Boot | Off | 已关，不用动 |
| 5 | Fast Boot（BIOS 里那个） | Off | 否则可能跳过 USB 初始化、认不到启动盘 |
| 6 | XHCI Hand-off（有 EHCI 也一起） | On（**建议项，找不到就跳过**） | 1401 原话："If your BIOS doesn't have this setting, skip it." 找不到不影响安装 |
| 7 | IOMMU | Off | AMD IOMMU 是 macOS 早期启动挂起的常见原因 |
| 8 | **网线** | 插到 **Realtek 2.5G 口** | 该口现在是 Disconnected；板载 RZ616 Wi-Fi 在 macOS 无驱动，Mellanox 也不被支持（见 §7），装系统期间只能靠这个口 |
| 9 | 磁盘 | 压缩 C: 出 ~300 GB，建成一个简单卷 | 见 `disk-and-nic-plan.md`；Recovery 分区在盘尾**不挡路**，别删 |

## 4. 施工步骤

### 步骤 1 — 做启动盘（必须在管理员权限下）

我这个会话**不是管理员**（已验证 elevated: False），diskpart 那步要提权，所以这步得你来发起：

1. 打开 `C:/Users/you/Documents/Hackintosh/tools/1401`，右键 `1401.exe` → **以管理员身份运行**
2. 按界面顺序：**Check this PC** → **Build the Mac setup** → 读 **Your BIOS steps** → **Create the macOS stick**
3. 选盘时**认准 Kingston DataTraveler 3.0（57.8 GB）**。注意：盘号会变——它现在是**磁盘 1、盘符 Z:**
   （之前是磁盘 2、E:）。1401 会按「盘号+序列号+容量+型号」在擦盘前复核，但仍建议你先跑一次 `Get-Disk` 对一下
4. 这步会联网：从 Apple 下 macOS 恢复镜像，并写入 OpenCore + EFI + NullMoth 驱动包

做完后我会核对盘上产物：`EFI/OC/config.plist` 的关键项（SetupVirtualMap=False、驱动 boot-args、
csr-active-config=430A0000、ResizeAppleGpuBars=0），以及 NullMoth 目录里的驱动包哈希。

### 步骤 2 — 改 BIOS，从 U 盘启动

按 §3 的表改完 → Save & Exit → 启动菜单选 U 盘（**带 UEFI 前缀那条**）→ OpenCore 菜单里选
**macOS Installer**（条目结尾带 (dmg)）。

### 步骤 3 — 装 macOS

1. 进安装器 → **磁盘工具** → 菜单 View → **Show All Devices**
2. 选中**整块**目标磁盘（不是 Windows 那块）→ 抹掉 → 名称 `Macintosh HD`、格式 **APFS**、方案 **GUID**
3. 退出磁盘工具 → **Reinstall macOS** → 选刚抹的盘
4. 会重启若干次，每次仍从 U 盘启动、选 macOS Installer，直到条目变成 `Macintosh HD`
5. **U 盘一直插着** —— 现阶段它是这台机器的启动来源

### 步骤 4 — 装 NVIDIA 驱动

进桌面后：打开 U 盘上的 NullMoth 文件夹 → 解压 `1401-Mac-1.0.2.zip` → 运行 **1401.app** →
它会安装驱动（1.0.1）并把 OpenCore 从「安装态小 BAR」切成「8 GB 全 BAR」→ 按提示允许扩展 → 重启（可能要两次）。

### 步骤 5 — 验收（我来跑）

```
kmutil showloaded --list-only | grep nullmoth      # 期望 4 行
system_profiler SPDisplaysDataType | head -20      # 期望认出 GeForce RTX 5060，Metal: supported
```

## 5. 出问题时的回退点

| 卡在哪 | 怎么办 |
| --- | --- |
| OpenCore 就到不了菜单 | U 盘上会有 `opencore-*.txt`（1.0.7+ 默认开日志）。回 Windows 用 1401 的 Scan for logs and send them |
| 卡在 PCI configuration end / Couldn't allocate runtime area | 前者是 BAR 没开（回 §3 第 1、2 项）；后者是 SetupVirtualMap —— 我们的 config 已是 False，若仍复现请把 U 盘 config 发我 |
| 内核 panic | U 盘上有 `panic-*.txt`（ApplePanic 已开） |
| 装完驱动后屏幕停在启动文字 | 就是 ReBAR/Above 4G 没开 |
| 完全进不去、想回到原样 | 物理拔掉 macOS 那块盘 / 把 BIOS 启动顺序改回 Windows；**Windows 盘在 A/C 方案下从未被改动** |
| 选了 B 方案想撤销 | 磁盘管理里删掉 APFS 分区、扩展卷把空间还给 C: |

## 6. 已知的粗糙处（装完再收拾）

| 项 | 现状 | 收拾方式 |
| --- | --- | --- |
| USB 端口 | `UTBDefault.kext` 是占位端口图 | 用 1401 Mac app 的 USB map 功能生成真的 UTBMap.kext（1.0.2 才修好这功能） |
| 声卡 | layout-id 是引擎随机给的（我见过 1/2/3/5/7） | 没声音就换值试 |
| Wi-Fi / 蓝牙 | RZ616 在 macOS 无驱动，已在 config 里禁用 | 换 BCM94360NG 之类，或只用有线 |
| WhateverGreen | 现在启用的；这台只有一张 N 卡，用不上它 | 装完驱动若有异常，第一个把它 Enabled 改 false |
| 驱动版本 | 1.0.1（1.0.2 的包上游还没传） | 等 v1.0.3 release 出现后升级，能修 Geekbench OpenCL / Hackintool 崩溃 |
| 电源管理 | AMD 平台无 XCPM，睡眠大概率不正常 | 别指望 |
| 安全姿态 | amfi=0x80 + SIP 430A0000 | 当研究机用，别放重要数据 |
