# 引导方案：现状、最优解、以及 ESP 到底要不要动

最后核对：2026-10-08 23:2x　依据：实时读 UEFI 启动变量 + 直读 GPT + 内置 ESP 实测

## 1. 现状（三条硬事实）

### 1.1 固件启动顺序里**没有 OpenCore 条目**

```
BootOrder = Boot0001 -> Boot0080
  Boot0001  "Windows Boot Manager"  ->  内置 ESP  \EFI\Microsoft\Boot\bootmgfw.efi
  Boot0080  "Mac OS X"              ->  APFS 卷  \System\Library\CoreServices\boot.efi   ← 直接引导 macOS，绕过 OpenCore
  Boot0081  "Mac OS X"              ->  （另一个 macOS 入口）
```

⇒ OpenCore 只能通过「F11 一次性启动菜单」选到：
选 **UEFI OS（内部硬盘）** → 内置 ESP 的 `\EFI\BOOT\BOOTx64.efi`；
选 **Kingston U 盘** → U 盘的 `\EFI\BOOT\BOOTx64.efi`。

### 1.2 内置 ESP 的详细情况（直读 GPT + FAT + 文件系统）

| 项 | 值 | 说明 |
| --- | --- | --- |
| 分区类型 GUID | `{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}` | **= EFI System Partition**，正确 |
| GPT 分区**名称** | `"Basic data partition"` | ← **Windows 安装程序写的**（它对 ESP 就写这个名，误导性但无害） |
| FAT **卷标** | `"NO NAME"` | ← Windows 格式化 ESP 时不写卷标，所以是这个 |
| 属性 | `0x8000000000000000` | bit63 = "不自动挂载"（所以没有盘符），ESP/MSR 的正常值 |
| 大小 | 196.0 MB（205,520,896 B） | |
| 可用 | **152.6 MB** | 已用 42.87 MB |
| `\EFI\Microsoft` | 31.32 MB / 147 文件 | Windows 引导，完好 |
| `\EFI\OC` | 8.82 MB / 102 文件 | 整套 OpenCore（含 `Tools\NullMothSafe.efi`） |
| `\EFI\BOOT\BOOTx64.efi` | 24,576 B | **已被换成 OpenCore 的引导壳**（原本这里是 Windows 的 fallback 引导器） |

### 1.3 今天下午的成功启动走的是**内置 ESP**

内置 ESP 根目录有 6 份 OpenCore 日志（`00:56`、`01:19`、`01:26`、**`13:28`、`13:35`、`13:45`**），
其中 5 份结尾是 `[EB|LOG:EXITBS:START]`（boot.efi 成功交给内核）；U 盘的日志停在 `07:30`/`07:32`。
OpenCore 把日志写在自己**启动时所在卷**的根目录 —— 所以日志位置就是判据。

## 2. 「ESP 名字怪怪的」——ESP 就是 EFI System Partition，不用改

三个名字来自三个不同的字段，工具显示哪一个都不一样，容易看着别扭：

| 你看到的 | 来自哪个字段 | 当前值 |
| --- | --- | --- |
| "EFI 系统分区" / "ESP" / "EFI System Partition" | **分区类型**（GPT type GUID） | `{C12A7328-…}` ✅ 正确 |
| "Basic data partition" | **GPT 分区名称** | Windows 安装程序写的，纯标签 |
| "NO NAME" | **FAT 卷标** | Windows 格式化时没写卷标 |

**ESP 就是 EFI System Partition 的缩写，两者是同一个东西。** 你那个分区类型是对的，
所以**不需要做任何调整**。想把 `Basic data partition`/`NO NAME` 改好看一点（比如 GPT 名改成
`EFI system partition`、卷标改成 `EFI`）是可以的，但：

* 纯装饰，对引导零影响；
* 需要挂载并写这块分区，**在系统还没装稳之前，改活着的 ESP 有非零风险**（写坏 FAT 会同时丢 Windows 和 macOS 引导）。

⇒ **建议：别动。**（`IsActive = False` 也是正常的 —— 那是 MBR 的概念，GPT 下无意义。）

## 3. 尺寸要不要动？—— 不用

| 需求 | 用量 | 判断 |
| --- | --- | --- |
| Windows 引导 | 31.3 MB | |
| OpenCore 整套（含 Resources 图标） | 8.8 MB | |
| 合计 | 42.9 MB / 196 MB | **剩 152.6 MB** |
| Dortania 对 ESP 的最低要求 | 200 MB | 你正好 196 MiB ≈ 200 MB（十进制），达标 |

⇒ 放得下，**不用扩容、不用给 macOS 单开一个 ESP**（同一块盘共享一个 ESP 是标准做法；
给同一块盘再建第二个 ESP 反而会让固件和 macOS 困惑）。

## 4. 三种引导方案对比（为什么推荐 B）

Hackintosh 的硬要求：**OpenCore 必须先于 macOS 运行**（它负责 kext 注入、ACPI 补丁、NVRAM boot-args；
少了它 macOS 根本起不来）。Windows 则完全不需要 OpenCore。

| | A. 只走 U 盘 | **B. 内置 ESP + 专用启动项（推荐）** | C. 现状：两处都有、靠 fallback 路径 |
| --- | --- | --- | --- |
| 怎么启 | 每次按 F11 选 U 盘 | 开机直接进 OpenCore 图形菜单 | 每次按 F11 选「UEFI OS」或 U 盘 |
| 优点 | 内置 ESP 完全不碰，Windows 最安全；换/重建只动 U 盘 | 不用插 U 盘；OpenCore 必然先跑 → boot-args/补丁一致；macOS 更新后的 bless 正常 | 现在能用 |
| 缺点 | **每次都得插 U 盘**；U 盘坏了/丢了 macOS 就起不来；USB 磨损 | 单盘单点（用 U 盘当救援即可） | **两处同源必须手动同步**；**依赖被劫持的 `\EFI\BOOT\BOOTx64.efi`** |
| 风险 | 低 | 低（配好之后） | **高 —— 这个"劫持 fallback"正是弄丢 Windows 引导的那条链** |

**结论：安装期用 A（就是上游现在的建议：插着启动盘、别做 USB→内置 的 promotion），
装完稳定后切到 B。** 现状 C 是要消除的中间态。

### 为什么 B 需要"专用启动项"而不是继续用 fallback

`\EFI\BOOT\BOOTx64.efi` 是**可移动介质/无启动项时的兜底路径**。把 OpenCore 塞在那里有两个后果：

1. 它替掉了 **Windows 自己的 fallback 引导器** —— 一旦 Windows 的 `Boot0001` 条目丢了
   （NVRAM 重置、Windows 更新、或那个 bug），你连 fallback 都没得救；
2. 某些固件只在"没有别的可启动项"时才走它，行为不可靠。

正确做法是给 OpenCore 建一个**独立的 `Boot####` 条目**，指向内置 ESP 的 `\EFI\OC\OpenCore.efi`，
并放在 BootOrder 最前。这样：

```
开机 → OpenCore（图形菜单，中文）
         ├─ macOS  → 带全套 kext/补丁
         └─ Windows → 交给 \EFI\Microsoft\Boot\bootmgfw.efi
```

即使 OpenCore 配置写坏，也能在 F11 里直接选 `Windows Boot Manager` 进系统修。

## 5. ⚠️ 一个现成的陷阱：那两个 "Mac OS X" 条目

`Boot0080` / `Boot0081` 是 macOS 安装时 `bless` 建的**直连条目**，指向 APFS 卷上的 `boot.efi`。

**从 F11 里选它们 = 绕过 OpenCore 启动 macOS = 没有 kext 注入 = 必然失败（panic 或卡住）。**
它们是你之前安装尝试留下的产物。

### 5.1 设备路径已解析（2026-10-08 23:5x）——它们各自指向什么

```
Boot0001  "Windows Boot Manager"
    HD(part1, sig=6D351E57-EE46-4CE7-AACC-0BDA08532F8C, start=2048, size=409600, GPT)   ← 内置 ESP
    File("\EFI\Microsoft\Boot\bootmgfw.efi")
    OptionalData=136 B

Boot0080  "Mac OS X"
    ACPI(...) → Pci(1,2) → Pci(0,0) → Type3/Sub22 → 
    HD(part4, sig=116B20BA-5217-4705-986F-354C44816C78, start=3369791488, size=631005832, GPT)  ← APFS 分区
    File("\551904A2-6BDB-4914-A842-D482C37AAF05\System\Library\CoreServices\boot.efi")

Boot0081  "Mac OS X"
    Type3/Sub1 → HD(part1, sig=01E47D9D-8F1A-458F-9C44-087F38B28F68, start=409640, size=5619224, GPT)
    （没有 File 节点 —— 残缺条目）
```

* **Boot0080** = 直接加载 APFS 卷（`551904A2-…`）里的 `boot.efi` → **绕过 OpenCore**。
  没有 kext 注入、没有 ACPI 补丁、没有 boot-args → 必然失败。
* **Boot0081** = `sig=01E47D9D-8F1A-458F-9C44-087F38B28F68` 正是
  **BaseSystem.dmg 里的那个合成分区**（OpenCore 日志里出现过同一 sig：
  `…DMG_0000000034F7316B.dmg/VenMedia(004B07E8…)/HD(Part1,Sig01E47D9D-…)`），
  且**没有 File 节点** → 安装器留下的**残缺条目**，选了必然失败。

两者都是 **macOS 安装器 `bless` 时自动注册的**（Apple 的安装器会给自己建启动项）。
**应该删**，但时机见 §6；删除办法见 §6.4。

⇒ 装完系统、切到方案 B 之后，**建议把这两个条目删掉**，免得以后误选。
（删 UEFI 启动项要么在 macOS 里用 `bless`/`efibootmgr`，要么从 Windows 写 NVRAM 变量 —— 见下。）

## 6. 切到方案 B 的具体动作（**等装完系统再做**）

1. **备份**：把内置 ESP 的 `EFI\OC` 整树拷到 U 盘/U 盘备目录（已有 `_backup-*`）；
   记下当前 `BootOrder`（`Boot0001 -> Boot0080`）以便回滚。
2. **建 OpenCore 启动项 —— 用 OpenCore 自带的机制，不要手写 NVRAM**：

   `Misc > Boot > LauncherOption` 现在是 **`Disabled`**。改成 **`Full`**
   （`LauncherPath` 保持 `Default`，即 `\EFI\OC\OpenCore.efi`）即可：

   | 值 | 行为 |
   | --- | --- |
   | `Disabled` | 什么都不做（现状：只能靠 F11） |
   | `Full` | 启动时**自动注册**一个名为 `OpenCore` 的 `Boot####` 条目（路径 = `LauncherPath`），**并把它挪到 `BootOrder` 第一位**；每次启动都会校正 |
   | `Short` | 同上，但用短路径（`\EFI\BOOT\BOOTx64.efi`），适合 NVRAM 受限的固件 |

   ⚠️ **关键**：它注册的是**当前启动的那个 ESP 的路径**。所以做这件事时必须
   **从内置 ESP 启动**（F11 → 选「UEFI OS（内部硬盘）」），不要从 U 盘启 ——
   否则会注册成 U 盘的路径，拔掉 U 盘就无法引导。

   ⚠️ OpenCore 1.0.9 官方包里**没有** `Bootstrap.efi`（已逐个核对 zip 内容），
   网上那套"用 Bootstrap.efi 注册启动项"的老办法在 1.0.9 上不适用；`LauncherOption` 就是它的替代品。
   另外**不推荐**从 Windows 手写 `Boot####` 变量（要求严格的 `EFI_LOAD_OPTION` 结构，
   写错会让固件列出坏条目）—— 有这个官方机制就不必冒险。
3. **调整 BootOrder**：`LauncherOption=Full` 会自动把 OpenCore 放第一，
   `Windows Boot Manager` 保留在后（不要删，它是你的退路）。
4. **删除 `Boot0080`/`Boot0081`**（那两个直连 macOS/残缺条目），三选一：
   * **BIOS UI**：本机 BIOS 的 IFR 里存在 `Boot Option Priorities` 与
     `AddBootOption`/`DelBootOption` 两个 VarStore，说明引导菜单里有
     "Add/Delete Boot Option" 之类的界面（Maxsun PTM UI 里通常在 Boot 页）。
   * **OpenCore 的 UEFI Shell**（最标准）：把 `OpenCore-1.0.9-RELEASE.zip` 里的
     `X64/EFI/OC/Tools/OpenShell.efi` 放进 `EFI/OC/Tools/` 并在 config 的
     `Misc > Tools` 里启用，然后在 OpenCore 菜单里运行它，执行：

     ```
     fs0:                              # 切到内置 ESP
     bcfg boot dump -v                 # 看清各条目编号
     bcfg boot rm 80                   # 删 Boot0080
     bcfg boot rm 81                   # 删 Boot0081
     ```

     （工具箱里另有 `BootKicker.efi` 可直接调用固件启动项，用于临时引导。）
   * **macOS 里**：`brew install efibootmgr` 后 `sudo efibootmgr -b 0080 -B`。
5. **收尾**：把 `\EFI\BOOT\BOOTx64.efi` 还原成 Windows 的 fallback（可选，更"干净"）；
   U 盘保留一份更新过的 OpenCore 作救援。
6. **验证**：重启 → 应直接进 OpenCore 图形菜单（**不再需要 F11**）→
   分别选 macOS 与 Windows 各启动一次。

## 7. 现在（安装期）该怎么操作

| 事项 | 做法 |
| --- | --- |
| 启动方式 | **保持插着 1401 启动盘**，F11 选它（或选「UEFI OS（内部硬盘）」——两者内容一致） |
| 改 config | **两边都改**（内置 ESP + U 盘），否则会出现"哪边好用哪边不好用"。改完跑 `ocvalidate` |
| ESP | **不要动**（不改名、不扩容、不重格式化） |
| 那两个 "Mac OS X" 条目 | **别选**。选了会绕过 OpenCore 而失败 |
| 备份 | U 盘上保留 `EFI\OC` 与内置 ESP 的 `_backup-*` 两份 |

## 8. ✅ 已实施：`LauncherOption = Full`（2026-10-08 23:56）

### 8.1 改了什么

| 位置 | 键 | 改前 | 改后 | sha256(前16) |
| --- | --- | --- | --- | --- |
| **内置 ESP** `\EFI\OC\config.plist` | `Misc > Boot > LauncherOption` | `Disabled` | **`Full`** | `863abe1a5220647e` → **`87c047be4047fa5e`** |
| **U 盘** `D:\EFI\OC\config.plist` | 同上 | `Disabled` | **保持 `Disabled`**（故意） | `863abe1a5220647e`（未变） |

`LauncherPath` 保持 `Default`（→ `\EFI\OC\OpenCore.efi`）；
前提 `Uefi > Quirks > RequestBootVarRouting` **本来就是 `True`**（已核对，无需改）。
备份：ESP 上 `config.plist.bak-launcher-20261008-235653`。
`ocvalidate` 1.0.9 对**两份**都返回 **No issues found**。

### 8.2 为什么两份 config 现在不同（这是唯一一处**故意**的差异）

* **内置 ESP = `Full`** → 它注册/维护启动项，成为"主"。
* **U 盘 = `Disabled`** → **救援盘绝不写 NVRAM**。

原因是 `Full` 注册的是"**当前启动的那个 ESP 的路径**"。若 U 盘也开 `Full`，
你一旦用 U 盘启动，那条启动项就会被改写成 U 盘路径，拔掉 U 盘后它就成了死条目
（固件会跳到下一个条目，通常是 Windows）—— 不难恢复，但很让人困惑。
把 U 盘保持 `Disabled`，它就永远是干净的救援盘。

> 想临时反过来（比如从 U 盘当主），把 U 盘那份改成 `Full` 即可：
> `python patch_launcher.py D:\EFI\OC\config.plist Full`。
> **注意：除了这个键，两份其余内容仍需保持一致。**

### 8.3 对 Windows 引导安全吗？—— 安全，且有源码级依据

读 OpenCore 1.0.8 源码（`Library/OcBootManagementLib/DefaultEntryChoice.c`，
函数 `InternalRegisterBootstrapBootOption`，行 1248-1495），它**只做两件事**：

1. 读现有 `BootOrder`（行 1333-1339）；
2. 找"设备路径以 `\EFI\OC\OpenCore.efi` 结尾"的启动项：
   * 找到且路径等同 → 直接复用它的编号；
   * 找到但路径不同 → **就地改写**那一条（行 1427 `SetVariable`）；
   * 没找到 → **找一个空闲编号**新建（行 1389-1406，从 1 开始找未被占用的）；
3. 把**自己的编号**插到 `BootOrder` 最前（行 1453 `BootOrder[0] = BootOptionIndex`），
   并只摘掉**自己编号**在后面的重复项（行 1456-1468 的 `while` 里比较的正是 `BootOptionIndex`）。

**全程没有任何针对其它条目的删除或改写。** 所以：

| 条目 | 改动后 |
| --- | --- |
| `Boot0001` Windows Boot Manager | **原样保留**（只是 BootOrder 里向后挪一位） |
| `Boot0080` / `Boot0081` Mac OS X | 原样保留 |
| 新增 `Boot####` "OpenCore" | 指向内置 ESP 的 `\EFI\OC\OpenCore.efi`，排在 BootOrder 第一位 |

另外，官方文档 `Configuration.tex:3549` 说明 `Full` 需要 `RequestBootVarRouting` 开启；
而该 quirk 的作用是"**把 `Boot` 开头的变量写入重定向到 `OC_VENDOR_VARIABLE_GUID`**"
（`9120:9277-9281`），即**保护性**地隔离，而不是改动别人的条目。
官方文档（`6884`）本身就是这么推荐的：*"Make sure `RequestBootVarRouting` and
`LauncherOption` are enabled in `config.plist`"*。

**唯一要留意的**是文档两处提醒（不是我们的问题，但值得知道）：

* `3571-3575` Note 1：个别固件有 NVRAM 实现缺陷，建议只在已知兼容的板子上用 ——
  我们已做完整备份 + 回滚能力验证（见 8.4），风险可控。
* `3336-3338` Note 3：Windows 首次启动或 NVRAM 重置后可能把自己的条目标成最前；
  但 OpenCore 每次启动都会重新把 `OpenCore` 提到第一位，所以下次启动即自动校正。

### 8.4 回滚能力 —— 已实测（不是纸面承诺）

| 能力 | 实测结果 |
| --- | --- |
| 读固件变量 | ✅ 已验证（115 个变量枚举成功） |
| **写固件变量** | ✅ `SetFirmwareEnvironmentVariableEx` 返回 True，读回逐字节一致 |
| **删固件变量** | ✅ 关键在于**长度 0 且属性必须匹配原值**；用 `attrs=0x7` 删除成功（`err=87` 是因为属性写成了 0x0/0x6） |

**备份位置**（写回所需的原始字节）：

```
C:\Users\hiliang\Documents\Hackintosh\tools\bootvars-backup-20261008-235630\
    BootOrder.bin    4 B   f7b6a50a08842978
    Timeout.bin      2 B   47dc540c94ceb704
    Boot0001.bin   300 B   574a45039c234583   ← Windows Boot Manager
    Boot0080.bin   300 B   915d95926044a89d   ← Mac OS X（应删）
    Boot0081.bin    78 B   bb3d33980465b470   ← Mac OS X（残缺，应删）
    index.json
```

要回滚时：把 `BootOrder.bin` 原样写回，并删掉新增的 `Boot####`（或按 8.3 的办法
把 `LauncherOption` 改回 `Disabled`，OpenCore 就不再维护它）。

### 8.5 下一步怎么启（关键：第一次必须从内置 ESP 启）

```
1) 开机按 F11 → 选「UEFI OS (内部硬盘)」（不是 U 盘！）
2) OpenCore 会自注册启动项并把 OpenCore 排到第一位
3) 之后重启就【不用再按 F11】了，直接进 OpenCore 图形菜单
   （菜单里选 macOS 或 Windows 各试一次）
```

从**内置 ESP** 启是唯一要求 —— 否则注册的路径会指向 U 盘。
顺带：OpenCore 菜单用的是 UEFI 键盘支持，不受 macOS 那份 USB 映射影响。
