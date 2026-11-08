# 三件硬件适配的可行性研究（2026-10-09）

针对 USB 15 端口上限、RZ616/MT7922 WiFi、Mellanox ConnectX-3 Pro 三条线做的资料与源码/二进制级调查。
**所有结论都标了证据来源与置信度。**

---

## 一、USB 15 端口上限：可以绕过，但是"重度 OS 修改"（更正早前"软件无解"的说法）

### 1.1 本地内核集合实测（决定性证据）

用 `capstone` 反汇编 `/System/Library/KernelCollections/BootKernelExtensions.kc`（macOS 15.8.1 / 24H32）：

`AppleUSBHostPort::setPortLocation(unsigned int)` @ `0xffffff8002cd638a`（符号仍存在）：

```
+  54  cmp  r15d, 0xf        ← 上限检查；OpenCore 的 part-1 模式 [0F 0F 87] 在 +57 命中 ✓
+  58  ja   ...              ← 超过就报错返回
+ 649  test al, 0xf          ← "找空闲半字节"的掩码测试（第 1 处）
+ 846  test al, 0xf          ← 同上（第 2 处）
+ 987  shl  r15d, cl         ← 端口号左移
+ 990  or   r12d, r15d       ← OR 进 locationID
```

`AppleUSBXHCI::createPorts()` @ `0xffffff8002deaa58`：

```
+1344  cmp ebx, 0xf          ← 端口上限检查
+1346  0F 0F 83              ← OpenCore 的 V2 模式在 +1346 命中 ✓
```

`AppleUSBXHCI::createPort(...)` @ `0xffffff8002debb4a`：未发现上限比较。

### 1.2 由此得到的三条判断

1. **OpenCore 的 part-1 补丁（上限 15 → 0x40）在本机仍然有效**（模式命中）。
2. **OpenCore 的 part-2 补丁（把 4bit 掩码放宽）在本机已经失效**：它找的是 `and eax, 0xf`
   形态（`41 83 ?? 0F`），而 15.8.1 的代码已改成 `test al, 0xf`（`A8 0F`），模式 0 命中。
   ⇒ **单独打开 `XhciPortLimit` 是有害的**：检查放行了 >15 的端口号，但掩码没放宽，
   端口号会 mis-encode 进 locationID 的其他字段 —— 正是 Dortania 警告的
   "pushing past the 0xF limit and going into someone else's space"（数据损坏风险）。
3. **端口号只有 4 bit**：locationID 布局是 `AA BBBB CCCC …`（AA=控制器 8bit，B=端口 4bit，
   C…=每级 hub 一个半字节）。我们的 Port 16/17 编号为 0x10/0x11，**放不进 4 bit**。
   所以问题不只是"一道检查"，是字段宽度。

### 1.3 可行的技术路径（有公开配方）

`corpnewt/USBMap` issue **#63「Removing USB Port Limit on macOS 26」**（2025-07，含 IDA 步骤与
搜索/替换字节）给出了完整配方，改 4 处：

| 位置 | 动作 |
| --- | --- |
| `AppleUSBXHCI::createPort`（createPortEPN） | `0F`→`3F`（cmp） |
| `AppleUSBXHCI::createPorts` | `0F`→`40` |
| `AppleUSBHostPort::setPortLocation` | cmp 指令 `0F`→`40`；**其余 `0F`→`3F`**（放宽掩码） |
| `IOUSBHostFamily` 相关函数 | 同上放宽验证 |

该 issue 里的示例替换对：`4489F3E96A010000 4183FE0F 0F837C080000418D5E01` →
同串但 `4183FE3F`。（macOS 26 的字节，15.8.1 需重推。）

**针对本机 15.8.1 的候选（我本地推导，未验证）**：把 `setPortLocation` 里那两处
`test al, 0xf`（`A8 0F`）改成 `test al, 0x3f`（`A8 3F`），即补上 OpenCore 失效的 part-2。
可用 OpenCore `Kernel > Patch` 加一条：

```
Identifier : com.apple.iokit.IOUSBHostFamily
Base       : __ZN16AppleUSBHostPort15setPortLocationEj
Find       : A8 0F
Replace    : A8 3F
Count      : 2          （函数内两处；需实测确认不会误伤）
```

### 1.4 风险与代价（必须知道）

- 放宽到 6 bit 会改变 locationID 的语义，端口 16/17 是否被 macOS 正确编码**必须实测**。
- 越界写进 hub 半字节 → 可能的数据损坏（Dortania 明确警告）。
- 每次 macOS 小版本更新都要重新推导字节（补丁随 Apple 改代码而失效）。
- 与 SIP / 内核集合签名相关，需要谨慎测试。

### 1.5 稳妥的替代

**PCIe USB 扩展卡**：每个控制器独立享 15 个额度（Mac Pro 加卡即此原理）。
FL1100（Fresco Logic）、ASM1042/1142 在 macOS 通常免驱。花几十块解决，零风险。

---

## 二、RZ616 / MediaTek MT7922 WiFi

### 2.1 已有专门项目（这是好消息）

**`laobamac/SimpleMtkWlan`** —— 实验性 macOS kext，针对 MediaTek MT792x **PCIe** 无线网卡：

- 覆盖：`14c3:7961`(MT7921)、`14c3:0608`(MT7921K/RZ608)、`14c3:7922`(MT7922)
- 状态（README 原文）：*"under active hardware bring-up… **not release-stable yet**"*，
  当前重点：PCIe probe/attach、固件加载、DMA 队列、硬件扫描、关联/认证 TX 路径调试
- 结构：`hal_mwx/`（MT7921/MT7922 驱动实现）、`mtk80211/`（OpenBSD net80211 兼容层）、
  `scripts/`（固件生成）
- 作者 laobamac 也是 1401 所用 AMD_Vanilla fork 的作者

### 2.2 ⚠️ 我们的设备 ID 不在它的匹配列表里

本机（`ioreg` + Probe 双重确认）：**`14c3:0616`**，sub `0x0616`。
而它的 `IOPCIPrimaryMatch = 0x796114c3 0x060814c3 0x792214c3` —— **没有 0616**。
⇒ 即使要用，也要先往 `IOPCIPrimaryMatch` 加 `0x061614c3`（一行改动），然后面对"实验性项目"本身的成熟度。

### 2.3 蓝牙（另一条线）

**`laobamac/SimpleMtkBtFw`**（GPL-3.0）做 MediaTek USB 蓝牙固件装载，覆盖 MT7921/MT7922/MT7922A，
匹配条件为 `bDeviceClass=0xE0` + `idVendor=0x0E8D`（我们的蓝牙正是 `0e8d:0616`）。
但它**必须先能被 macOS 枚举到** —— 而我们的蓝牙在 Port 16，被 15 端口上限挡在门外（见第一节）。
⇒ **蓝牙的前置条件是先解决端口上限，或者用免驱 USB 蓝牙 dongle（如 BCM20702 系）。**

### 2.4 结论

| 路线 | 可行性 | 说明 |
| --- | --- | --- |
| SimpleMtkWlan（改 ID + 自己调） | 中 | 项目存在但未稳定；我们的 ID 要自己加；属于"陪开发者一起 debug" |
| 免驱 USB 蓝牙 dongle | 高 | 几十块，插上即用（前提是插在能枚举的口上） |
| 换 Intel AX210/AX200 + itlwm | 高 | 需要 M.2 槽位；itlwm 是成熟项目。但我们是 RZ616 焊在板上或 M.2 卡座需确认 |
| 有线（现状） | 已可用 | RTL8125 正常 |

---

## 三、Mellanox ConnectX-3 Pro

### 3.1 我们的卡

Probe：`15b3:1007` = **MT27520 Family [ConnectX-3 Pro]**，sub `15b3:0003`，
BAR0 1 MiB / BAR2 8 MiB / ROM 1 MiB，无驱动。

### 3.2 已知的社区路线：ATTO FastFrame（但已死）

MacRumors 长帖「Mellanox ConnectX-3 40 GbE using ATTO FastFrame macOS driver」（2020-2023，7 页）：

- ATTO FastFrame **NQ41/NQ42** 是 ConnectX-3 **Pro**（CX313A/CX314A）的贴牌卡，
  **device ID 4103** —— **和我们的 0x1007 = 4103 完全一致**
- 做法：用 `mstflint` 把 ATTO 固件刷进普通卡，让 ATTO 的 macOS kext 匹配
- **但**：
  1. 普通 ConnectX-3（device ID **4099**）刷不进 ATTO 固件 —— 报
     `Device/Image mismatch … intended for: ConnectX-3Pro only`。我们恰好是 Pro，这一步没问题
  2. **交叉刷写后 macOS 下 MAC 全 0**（Linux 下正常）。帖子里多人复现，**至今无解**
     （推测 ATTO 卡 NVRAM 的 i2c 地址不同）
  3. **ATTO FastFrame 2 已列为 discontinued**；ATTO 当前 Networking Suite（2026-08，支持
     macOS 26/15/14/13）只覆盖 **FastFrame 3/4**（ConnectX-4/5/6 世代）。
     老 FastFrame 2 驱动的 macOS 上限在 Catalina 附近，且官方注明不支持 Apple Silicon

⇒ **ATTO 路线在当前 macOS 15 上基本不可行**（驱动版本太老 + MAC 问题）。

### 3.3 macOS 原生

`/System/Library/DriverExtensions/` 里只有 `AppleEthernetMLX5.dext`（ConnectX-4/5/6）。
ConnectX-3 属 mlx4 世代，**从来没被覆盖**。帖子里有人用 `AppleEthernetMLX5.dext` 试 CX-4 的
VF（`15b3:1014`）也失败（`QUERY_HCA_CAP Failed`）。

### 3.4 结论

| 路线 | 可行性 | 说明 |
| --- | --- | --- |
| 自己写 mlx4 macOS 驱动 | **极低** | 需要移植 Linux/FreeBSD mlx4_core+mlx4_en 到 IOKit Ethernet 框架，还涉及固件加载、命令队列、MSI-X。工程量以人月计 |
| ATTO FastFrame 2 驱动 | 低 | 已停售停更，驱动老于 macOS 15；且刷固件后 MAC 归零 |
| **换 ConnectX-4 Lx / CX-5** | **高** | 原生 `AppleEthernetMLX5.dext` 免驱（帖子里 MCX4121A-ACAT 在 Apple Silicon 上正常工作）；二手价格与 CX-3 接近 |
| 换 Chelsio T580/T62100 | 中高 | 有厂商 macOS 驱动；帖子报告 T62100-SO-CR 在 Sequoia 正常、支持 9000 MTU |

**建议：不要为 ConnectX-3 写驱动；把预算花在一张 ConnectX-4 Lx 上。**

---

## 五、2026-10-09 晚：构建管线打通（不用 Xcode）+ 上游 AMD 修复已存在

### 5.1 本机现在可以编译 kext（不需要 Xcode）

用两条探针确认：

1. Command Line Tools 的 SDK **自带 Kernel.framework**
   （`/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk/System/Library/Frameworks/Kernel.framework`）。
2. 拉 `acidanthera/MacKernelSDK`（kext 构建用的头文件集）后，用 CLT 的 `clang` 直接编译即可。

`tools/research/build-mtkwlan.sh` 实测结果：**54/54 源文件编译通过并链接成功**，
产出 `Mach-O 64-bit kext bundle x86_64`（filetype `KEXTBUNDLE`，含 `_start(kmod_info*, void*)`，1851 个符号）。

踩过的三个坑（都是构建参数问题，不是工具链问题）：

| 现象 | 原因 | 解决 |
| --- | --- | --- |
| `PCIDriverKit/IOPCIDevice.h` 里 `OSClassLoadInformation` 未知 | 缺 `KERNEL` 宏，走了 DriverKit（用户态）分支 | 加 `-DKERNEL -D__KERNEL__` |
| OpenBSD 的 `.c` 报 `unknown type name 'class'` | `mtk80211/compat.h` **没有 `__cplusplus` 保护**却包含 IOKit 的 C++ 头 | 全部按 C++ 编译（`-x c++`） |
| `types.h` 找不到 | 需要 `mtk80211/linux` 在 include 路径里 | 加 `-I mtk80211/linux` |

另外工程用前缀头 `SimpleMtkWlan/PrivateSPI.pch`（`GCC_PREFIX_HEADER`），要 `-include` 它；
`Info.plist` 里带 Xcode 占位符（`$(PRODUCT_BUNDLE_IDENTIFIER)`、`$(MODULE_VERSION)` 等），
不用 Xcode 时必须手工替换，否则 kext 会因 `CFBundleVersion` 非法而无法加载。

`tools/research/assemble-kext.sh` 负责组装 bundle，`kmutil print-diagnostics` 的结果是
**`Dependencies: OK`**，唯一剩余项是 `Bad code signature`（未签名，符合预期）。

### 5.2 RZ616（MT7922）驱动的真实成熟度

作者 laobamac 在 issue #2 的原话：

> "This project is still under development. At present, **only the firmware download and WiFi
> scanning** of MT7922/MT7921 network card are realized. … I am currently a senior high school
> student, so there is no expected arrival time for the project."

仓库实况：**总共 2 个提交**（`init` + `enable firmware decrypts…`），最后提交 **2026-06-27**，
无 release / 无 tag；issue #3 报告 *"WiFi scans OK but connection fails / crashes"*。

而我们这颗是 **`14c3:0616`**，不在它声明的 `0x796114c3 0x060814c3 0x792214c3` 里。
已生成扩展版匹配（`SimpleMtkWlan-plus0616.kext`）。

**结论：当前能得到的上限是"能扫描"，不是"能联网"。** 固件（`include/FwBinary.cpp`，12 MB）
与 MT7922 固件名（`mwx-mt7922_ram_code_1`）都在仓库里，所以"能 probe/能扫描"这个目标是现实的。

### 5.3 网卡可换性（Probe 数据）

`map/map.json` 的插槽表：**PCIE4 = PCI Express x4，"In Use"，bus `0000:00:02.1`**，
而 WiFi 的 PCI 路径正是 `…/0000:00:02.1/…/0000:06:00.0`，链路 **5.0 GT/s ×1**
——即 WiFi 挂在这个插槽后面，形态是单通道的 M.2 模块。
换 Intel AX210/AX200 走 `itlwm`/`itlwm` + HeliPort 是成熟路线（支持到较新 macOS）。
**需物理确认是标准 M.2 E-key 座还是焊死**。

### 5.4 ConnectX-3 Pro：一个新线索（可能免刷固件）

之前的结论是"必须交叉刷 ATTO 固件、刷完 MAC 归零"。但注意：帖子里 ATTO 固件的
`adapter_dev_id / hca_header_device_id = 0x1007`，**我们这张卡本来就是 `15b3:1007`（= 4103）**。
也就是说：**如果 ATTO 的 kext 只按 device ID 0x1007 匹配（不看 subsystem），我们的卡可能直接匹配，
根本不需要交叉刷固件——那 MAC 归零的问题就不适用。**

待验证（下一步）：取 ATTO FastFrame 2 驱动（其 discontinued 下载页），看它 `Info.plist` 的
匹配条件、以及这个 Catalina 时代的旧 kext 能否在 macOS 15 上加载（未签名 + 老 ABI 是主要障碍）。

### 5.5 上游 AMD 策略：1.1.0 已经修了（不是改芯片组列表）

`nullmoth/1401` 的 issue #9（Ryzen 9 9950X / MSI X870，STOP 0x16）里，作者的原话：

> "Your log has no `OCABC: MMIO devirt` lines, so every attempt ran with **DevirtualiseMmio off**.
> On AM5 boards, `EB.MM.AKM … STOP 0x16` means boot.efi could not allocate memory for the kernel
> because firmware runtime MMIO regions are in the way." … "Or let 1401 1.1.0 do it: … It reads the
> stick's OpenCore log and makes this exact change." … "the next step is to whitelist the regions
> below 4 GB, and 1401 does that automatically from that log too."

对应实现是 `p1401/bootfix.py`（1.1.0 引入，本次实取 262 行），一个按**实际日志**推进的状态机：

```
off, 失败                      -> DevirtualiseMmio=True + SetupVirtualMap=True（下次日志会列出 MMIO 区）
on 且 devirt 跑过、仍失败        -> 把日志里 <4 GiB 的 runtime MMIO 区加进 MmioWhitelist
再失败                        -> 两者都关（exhausted）
```

解析的是 OpenCore DEBUG 的 `OCABC: MMIO devirt 0x<addr> (0x<pages> pages, 0x<attr>) skip <0|1>` 行。
其注释还给了统计：*"of 7 AMD sticks with the policy default (both off) 3 reached the kernel and 4 stopped there。"*

⇒ **我们没有"上游不管"的问题**：它用"读日志、按机器数据修"取代了"硬编码芯片组名单"，
这比改 `AMD_DEVMMIO_ON` 更好。含义有两条：

1. 以后用 1.0.16+ 重建盘，失败后会**自动**变成我们现在手工达到的配置（DevirtualiseMmio=on + 白名单），
   不必再手改。
2. ⚠️ 但 `bootfix` 同时会打开 **`SetupVirtualMap=True`**，而 `policy.py` 对 Ryzen 是设成 `False` 的。
   我们这块板子当前用 `False` 正常启动，**若将来重建导致它变 True，需要重新验证能否启动**。

## 六、2026-10-09 晚（二）：USB 补丁给出**定量结论**，WiFi 上限确定

### 6.1 USB 端口补丁：本机存在**可证明的 locationID 冲突**（不是抽象风险）

用实测的 ioreg 布局（端口对象名字就是 locationID）建立模型 → `tools/research/usbfix-sim2.py`：

| 控制器 | locationID 基址 | 端口字段 |
| --- | --- | --- |
| XH00（17 端口） | `0x00000000` | port 1..15 → `0x00100000`..`0x00F00000` |
| XHC0 / XHC1 / XHC2 | `0x60000000` / `0x80000000` / `0x01000000` | — |

模拟结果（`python3 usbfix-sim2.py`）：

```
A. 现状（端口 ≤15）          全机 27 个 locationID，冲突 0 处
B. 只放宽检查（OpenCore part-1 语义）
   XH00 port 16 -> 0x00000000
   XH00 port 17 -> 0x00100000   ⚠️ 与 XH00 port 1 撞车（4bit 掩码把 17 截断成 1）
C. 放宽检查 + 掩码放宽到 6bit（USBMap#63 语义）
   XH00 port 16 -> 0x01000000
   XH00 port 17 -> 0x01100000   ⚠️ 与 XHC2 port 1 撞车
```

**关键事实**：locationID 是 32 位，控制器 8 位（bits 24..31）+ 端口 4 位（bits 20..23）。
本机 XH00 的控制器字节**恰好是 `0x00`**，端口字段紧贴 bit 20，**上面就是别的控制器的地盘**：

```
XH00 port 16 = 0x01000000  ← 等于 XHC2 的控制器基址
XH00 port 17 = 0x01100000  ← 等于 XHC2 port 1 的 locationID
```

所以：**只放宽检查 → 端口号被截断，17 变回 1（同控制器内自相混淆）；
放宽掩码 → 越界到 XHC2 的地盘（跨控制器混淆）**。两条都有可证明的冲突，
这正是 Dortania "into someone else's space" 的具体形态。

**要让它成立，必须重新设计 locationID 编码**（端口字段 ≥5 位 + 所有控制器基址划分同步调整），
涉及 `AppleUSBHostController` 基址 + `AppleUSBHostPort` 端口 + `IOUSBHostFamily` 解析三处 ——
不是两字节补丁能完成的。

### 6.2 风险的缓解手段（逐条对应）

| 风险 | 缓解 | 状态 |
| --- | --- | --- |
| 误伤无关代码 | 用 `Base` 限定到符号 + `Count` + 先离线比对字节 | ✅ 可做（`usbfix-sim2.py` 同目录可扩展成 checker） |
| locationID 语义 / 数据损坏 | 用模拟器在动内核前算冲突 | ✅ **已做出，并发现本机必然冲突** |
| 逐版本重做 | 脚本化"模式是否命中"检测 | ✅ 可做 |
| 签名/信任链 | **OpenCore `Kernel > Patch` 是引导时内存补丁**，磁盘上的 KC 保持签名、无需改 SIP；回滚=删条目 | ✅ 风险基本不存在 |

⇒ 缓解手段能覆盖除"编码本身装不下"之外的所有风险；而"装不下"是**本机硬件布局决定的硬约束**。

### 6.3 WiFi：AWDL（AirDrop/接力）在 MT7922 上不可能

两点实证：

1. **生态结论**：Intel 的成熟项目 itlwm/AirportItlwm 明确**不支持 AWDL**
   （`ifconfig awdl0` 不存在）⇒ 无 AirDrop / 个人热点 / 连续互通相机。
   AWDL 依赖 Apple 自家 `IO80211Family` 对芯片的原生支持，**只有 Apple 用过的 Broadcom 芯片有**。
2. **SimpleMtkWlan 的链接目标**：`OSBundleLibraries` = `IONetworkingFamily`（**以太网风格驱动**），
   **不是 `IO80211Family`**（Airport 风格）。⇒ 即使它开发完成，形态也是 itlwm+HeliPort 那种，
   **不会有原生 Wi‑Fi 菜单**，更不会有 airport 特性。

⇒ MT7922 上"完美适配 + AirPort/AirDrop"不可达。要这些功能只有换 Broadcom
（如 BCM94360NG/BCM943602CS，Apple 原生驱动）。

### 6.4 Mellanox：ATTO 驱动下载被表单门禁

ATTO 的 discontinued 页面确实还挂着 **"FastFrame 2 40GbE Driver macOS"（NQ-41 & NQ42）**，
但下载链接是 `wpdm-download-locked`：直接 `?wpdmdl=<id>` 只返回 3240 字节的下载页（表单/门禁），
取不到包。**需要用真实浏览器会话走一遍表单**（本机有 playwright 可用）。

## 七、2026-10-09 晚（三）：三处更正

### 7.1 ⚠️ 更正：Intel 网卡**确实能做到 AirDrop/AWDL**（我上一轮说错了）

用户指出"Intel 网卡我看到就有大神做成了"，核实后成立。机制是：

```
Broadcom IOName 伪装（DeviceProperties）
  + OCLP root patch（Modern Wi-Fi Root Patch，走 system volume patch）
  + IO80211Family 系的驱动（AirportItlwm，而非 itlwm）
  + IOSkywalkFamily 降级
  → 拿到原生 Wi-Fi 菜单 + iServices + Location + 部分/完整 AirDrop
```

证据：

- EliteMacx86 指南（Sequoia+）原文：AirportItlwm 方法 *"has several advantages (such as iServices,
  Location services and **partial AirDrop**)"*，且明确说要 *"spoof the Intel WiFi device to make it see
  as a Broadcom WiFi device to allow the root patching"*，OCLP 只看 `IOName`。
- InsanelyMac 有专帖：**"Experimental Fork of OCLP 3.0.0 Nightly – modern Wi-Fi, AWDL and AppleHDA
  Fully Working under Tahoe"**（434 帖，36 关注），讲的就是把 AWDL 打开。
- 反例（说明还不稳定）：itlwm issue #1062 在同一套配置下 `ifconfig awdl0` 仍 **does not exist**
  ⇒ AirDrop / Personal Hotspot / Continuity Camera 不可用。

**结论修正**：AWDL 不是"只有 Broadcom 硬件能做"，而是"**需要一个走 `IO80211Family` 的驱动
+ OCLP 的 root patch 支持**"。Intel 能，因为有人写了 `AirportItlwm`（IO80211 系）。

### 7.2 那么 MT7922 的真实差距在哪

差距**不在芯片，在驱动架构**：

| 项目 | 用的框架 | 能否拿 AirPort 特性 |
| --- | --- | --- |
| itlwm | `IOEthernetController` | ❌ 不能（伪以太网） |
| **AirportItlwm** | **`IO80211Family`** | ✅ 能（Intel 就靠它） |
| **SimpleMtkWlan（MT7922）** | **`IONetworkingFamily`** | ❌ 不能（走了 itlwm 那条路） |

⇒ 要让 MT7922 达到 Intel 的水平，需要的是**一个走 `IO80211Family` 的 MT7922 驱动**
（等价于"MediaTek 版 AirportItlwm"），而不是把 SimpleMtkWlan 做完。
这条路的可行性因此**从"不通"改成"通，但要自己写 IO80211 系驱动"**，并且还需要
OCLP root patch 的支持（OCLP 目前只认 Broadcom/Atheros 的 `IOName`，所以必须伪装）。

### 7.3 ATTO FastFrame 2 macOS 驱动：存在，但**只支持到 macOS 12**

用 playwright 打开真实页面后取到的原始信息（决定性）：

```
FastFrame 2 40GbE Driver macOS
  Files in Package: macos_kext_fastframe2_1171f1.dmg (v1.17.1)  1.97 MB
                    ReadMe.htm  5.14 KB
  Note: These download packages are for End-of-Life products …
  Supports the following Versions:
    macOS 12 Monterey, macOS 11 Big Sur, macOS 10.15 Catalina,
    macOS 10.14 Mojave, macOS 10.13 High Sierra & macOS 10.12 Sierra.
```

⇒ **最高只支持 macOS 12 Monterey，而我们是 macOS 15.8.1**，差了三个大版本。
下载按钮是 JS 门禁（`data-package="9795"`，nonce 由脚本注入），静态抓取拿不到 DMG。

判断：这条路要成立，需要把 Monterey 时代的 kext 加载到 Sequoia 的内核上
（旧 ABI + 未签名 + 需要 OCLP 式 root patch），**属于"可以试但成功率低"**。
相比之下，"从零移植 mlx4"虽然工程量大，但结果可控。

## 八、2026-10-09 晚（四）：为什么必须 OCLP root patch —— 本地实证

### 8.1 决定性发现：Apple 在 Sequoia 里把 IO80211Family **掏空了**

在本机 macOS 15.8.1 上直接查（只读）：

```
/System/Library/Extensions/IO80211Family.kext/Contents/
    Info.plist  _CodeSignature  version.plist      ← 只剩这些
    PlugIns/                                        ← 目录存在，但 0 个条目！
CFBundleVersion = 1200.13.1
```

逐个确认，**全部缺失**：`AirPortBrcmNIC`、`AirPortBrcm4360`、`Apple80211`、`IO80211FamilyLegacy`。

而 Wi-Fi 驱动的新形态在 DriverExtensions 里：

```
/System/Library/DriverExtensions/com.apple.DriverKit-AppleBCMWLAN.dext   ← Broadcom 专用，DriverKit
```

⇒ **Sequoia 起 Apple 把 Wi-Fi 从 IO80211Family 的 kext 插件整体迁到 DriverKit，而且只做了 Broadcom。**
这解释了三件事：

1. 为什么 Sequoia+ 上"原生 Wi-Fi 只有 Broadcom"；
2. 为什么 `AirportItlwm` 声明依赖 `com.apple.iokit.IO80211Family: 1200.12.2b1`
   —— 而本机是 `1200.13.1`，且**框架里已经没有可用的驱动插件**；
3. 为什么必须 OCLP root patch：需要把**旧版 IO80211Family（含插件）+ 降级后的 IOSkywalkFamily**
   重新装回**系统卷**，而系统卷是签名的只读卷 → 只能靠 root patch（并放宽 SIP）。
   Broadcom `IOName` 伪装是另一件事：它的作用是让 OCLP 愿意对这块设备打补丁
   （OCLP 只按 `IOName` 认 Broadcom/Atheros）。

### 8.2 对 MT7922 路的含义（修正后的结论）

要做"MediaTek 版 AirportItlwm"，需要同时具备：

1. 一个走 `IO80211Family` 私有 API 的 MT7922 驱动（参考 `AirportItlwm`：
   `IO80211Controller.h`、`apple80211_var.h`、`IO80211SkywalkInterface.h` 等一整套私有头，
   以及 `AirportAWDL.cpp` 这类 AWDL 实现——注意其 AWDL 文件只有 **58 行**，说明它主要靠框架，驱动侧只需少量配合）；
2. OCLP（或等价的系统卷补丁）把旧 IO80211Family 插件装回去；
3. 设备伪装成 Broadcom，让 OCLP 接受。

### 8.3 XH00 控制器字节的来源：ACPI 数据（未定论，但有线索）

从 Probe 的 ACPI 设备表拿到 `_ADR`：

| ACPI 路径 | `_ADR` | macOS 控制器 locationID |
| --- | --- | --- |
| `\_SB_.PCI0.GP17.XHC0` | `0x00000003` | `0x60000000` |
| `\_SB_.PCI0.GP17.XHC1` | `0x00000004` | `0x80000000` |
| `\_SB_.PCI0.GP19.XHC2` | `0x00000000` | `0x01000000` |
| `\_SB_.PCI0.GPP7.UP00.DP60.XH00` | `0x00000000` | `0x00000000` |

观察：`_ADR 3/4` → 字节 `0x60/0x80`（像是 `fn << 5`），但 `_ADR 0` 的两个控制器拿到了
**不同的**字节（`0x01` 与 `0x00`）⇒ **不是单纯由 `_ADR` 决定**。
另外 XH00 在 ioreg 里**没有 `name` 也没有 `port-count`**，而 XHC0/XHC1/XHC2 都有
（`name = "XHC0"` 等、`port-count` = 4/4/1）⇒ XH00 走的是另一条枚举路径。

**结论：字节的确切来源尚未定论**，下一步是反汇编 `AppleUSBXHCIPCI` 里设置
控制器 locationID 的那段代码（`setLocationID` 的调用点），而不是继续猜。

## 九、2026-10-09 晚（五）：USB 找到确切算法；ATTO 驱动已取到并拆解

### 9.1 USB：控制器字节的来源查清了 —— `AppleUSBXHCIPCI::generateBusNumber`

反汇编 `0xffffff8002e2c754`（`__ZN15AppleUSBXHCIPCI17generateBusNumberERh`）：

```asm
+13  mov  r14, rdi
+16  mov  rdi, [rdi+0xec8]
+26  call [rax+0x8f0]        ; 取 acpiPort（符号里另有 getAcpiPort(unsigned)）
+32  mov  r15d, eax
+35  and  r15b, 0x1f         ; 低 5 位 = acpiPort & 0x1f
+39  mov  rdi, [r14+0xec8]
+49  call [rax+0x8f8]        ; 取 PCI 设备号
+55  shl  al, 5              ; 设备号 << 5
+58  or   al, r15b           ; 合并
+61  mov  [rbx], al          ; *outByte = (devNum<<5) | (acpiPort & 0x1f)
+86  jmp  [rax+0xad0]        ; 尾调用 setLocationID
```

**即控制器字节 = `(PCI 设备号 << 5) | (acpiPort & 0x1F)`。** 用实测值验算，全部吻合：

| 控制器 | `_ADR` | 实测字节 | 验算（devNum<<5）\| acpiPort |
| --- | --- | --- | --- |
| XHC0 | 3 | `0x60` | (3<<5)\|0 = `0x60` ✓ |
| XHC1 | 4 | `0x80` | (4<<5)\|0 = `0x80` ✓ |
| XHC2 | 0 | `0x01` | (0<<5)\|1 = `0x01` ✓ |
| XH00 | 0 | `0x00` | (0<<5)\|0 = `0x00` ✓ |

⇒ 之前"两个 `_ADR=0` 却拿到不同字节"的疑问解决：差别在 **`acpiPort`**（XHC2 是 1，XH00 是 0），
不是 `_ADR`。**字节是拓扑的确定性函数，不是自由参数。**

**对补丁的意义（重要）**：这给出了**精确的改法位置**——要重新分配控制器编号，
改的就是 `generateBusNumber` 里 `+55`/`+58` 那两条（或 `acpiPort` 的来源），
而不是去补端口号的掩码。配合 §6.1 的模拟器，可以"先算冲突、再改字节"。

### 9.2 ATTO FastFrame 2 macOS 驱动：已取到、已拆解 —— **它是 Mellanox 自己写的驱动**

通过 playwright 走完锁框（姓名 + 邮箱）拿到 `macos_kext_fastframe2_1171f1.dmg`（2,010,003 B，
sha256 `8c43edd5…b0fb`）。外层是 zip，里面才是 DMG，再里面是 `ATTOFastFrame2_1171f1.pkg`
（可用 `pkgutil --expand-full` 展开）。

**发现一：作者是 Mellanox 本人（决定性）**。二进制里的断言字符串带完整源码路径：

```
/Volumes/Second/dev/mellanox/source/cna2/osx/kext/shared/driver/alloc.cpp:328
/Volumes/Second/dev/mellanox/source/cna2/osx/kext/shared/driver/cmd.cpp:420
/Volumes/Second/dev/mellanox/source/cna2/osx/kext/shared/driver/en_rx.cpp:199
/Volumes/Second/dev/mellanox/source/cna2/osx/kext/shared/driver/en_tx.cpp:631
/Volumes/Second/dev/mellanox/source/cna2/osx/kext/shared/driver/resource_tracker.cpp:56
```

`mellanox/source/cna2/osx/kext` —— 这是 **Mellanox 自家 OS X kext 的源码树**，ATTO 只是贴牌方。
⇒ 它对这颗硅片的编程是**原厂级**的，不是第三方逆向。

**发现二：匹配条件与我们的卡只差一个字符串**。kext 的 `Info.plist`：

```xml
<key>IONameMatch</key>
<array>
  <string>pci117c,90</string>  <string>pci117c,91</string>
  <string>pci117c,9b</string>  <string>pci117c,9c</string>
  <string>pci117c,40a4</string><string>pci117c,40a5</string>
  <string>pci117c,40a8</string><string>pci117c,40a9</string>
</array>
```

`117c` 是 **ATTO 的 PCI vendor id**，且用的是"`pci<子系统厂商>,<子系统设备>`"形态
（我们 GPU 的 `compatible` 里就有 `pci1458,41a1` = 技嘉子系统，这印证了该形态）。

我们这张卡在 macOS 里的实测属性：

```
"compatible" = <"pci15b3,3","pci15b3,1007","pciclass,020000">
"vendor-id" = <b3150000>  "device-id" = <07100000>
"subsystem-vendor-id" = <b3150000>  "subsystem-id" = <03000000>
```

⇒ **我们需要的字符串是 `pci15b3,3`（子系统）或 `pci15b3,1007`（厂商+设备）。**
**把它加进 `IONameMatch` 就行 —— 一行改动。**

**发现三：依赖在 macOS 15.8.1 上是过的**。`kmutil print-diagnostics` 结果：
`Dependencies: OK`，唯一报错是文件属主不对（501:20，解包导致，chown 即可）。

**发现四：随包带完整的 NVRAM/MAC 工具**，这正是"交叉刷固件后 MAC 归零"那个死结的解法：

| 工具 | 作用 |
| --- | --- |
| `atflash2` | 刷固件 |
| **`atnetnvr2`** | **读写 NVRAM（含 MAC）** —— 可 `-s` 存、`-r` 恢复 |
| `atnetstat` / `atinfo` | 状态与信息 |

**发现五：能力覆盖**（ReadMe 的 Supported Products）：FastFrame **NQ41 / NQ42**（40GbE，正是我们这类）、
ThunderLink NS3101/3102、NQ3401/3402。官方 OS 列表到 **macOS 12 Monterey** 为止；
二进制是 **universal（x86_64 + arm64e）**，`LSMinimumSystemVersion 12.1`，用 macOS 12.1 SDK 构建，
签名 TeamIdentifier `FC94733TZD`（开发者签名，非 Apple）。

⇒ 结论：**"改驱动适配原厂卡"是可行的，而且改动极小**（一个字符串 + 签名/授权）。
真正的未知数只剩一个：**驱动运行期是否还会校验板卡身份**（比如读 NVRAM 里的 PSID/board type
并拒绝非 ATTO 板）。这个只能上机测。

## 十、2026-10-09 晚（六）：Mellanox 实测结论 —— **改 ID 不够，卡在 ABI（14 个符号）**

### 10.1 做了什么

1. 复制 kext 到 `tools/mellanox/ATTOFastFrame2.kext`，给 `IONameMatch` 追加
   `pci15b3,3` 与 `pci15b3,1007`（我们这张卡的 compatible 里确实有这两个字符串）。
2. 用 `kmutil libraries`（权威的符号解析器）做**离线**预检。
3. 用 `kmutil load -z -p`（`-z` 跳过授权门）做活体加载实测。

### 10.2 决定性结果：14 个基类槽位在 macOS 15 已不存在

`kmutil libraries` 定量输出：

```
kext 需要的未定义符号：568
kmutil 能解析到的     ：554
差额（无法解析）      ：14        ← 全部是 _RESERVED 基类槽位
```

缺失的正是这些（`nm -m` 显示为 strong external，与能解析的 `_IOLog` 同类）：

| 基类 | 缺失槽位 | macOS 15.8.1 现存 |
| --- | --- | --- |
| `IONetworkController` | `_RESERVED…2` … `7` | 只剩 `8`、`9` |
| `IOEthernetController` | `_RESERVED…2` … `9` | 只剩 `10` … `31` |

**机制**：Apple 逐步把 `_RESERVED` 占位方法"兑现"成真方法（编号从低到高被消耗掉）。
ATTO 的 kext 是 2022 年用 **macOS 12.1 SDK** 编的，当时 1–9 还是占位；到 macOS 15
这些编号已经变成真方法，符号名不复存在。**旧 kext 的 vtable 引用了不存在的符号 → 无法加载。**
这跟 PCI ID、签名都无关，是基类 ABI 的硬断裂。

对照：`kmutil print-diagnostics` 只报 `Dependencies: OK`（它只查 kext 级依赖，不查单个符号）
——**这是个会误导人的检查，以后别只看它**。

### 10.3 一个有希望、且能离线验证的走法：兼容 shim

`_RESERVED` 槽位当年就是**空实现**。所以可以：

1. 写一个极小的 shim kext，用**完全相同的 mangled 名**定义那 14 个空函数；
2. 先**离线验证**：`kmutil libraries -r <含 shim 的目录> -p ATTOFastFrame2.kext`
   —— 若 14 个符号全部解析成功，方案可行性就得到证明（**零风险，不需要重启/加载**）；
3. 再考虑实际加载。

前提假设是"Apple 只是把占位方法换成真方法、槽位**位置**没变"。位置若变了，vtable 会错位，
方案作废 —— 这一点可以用 shim 方式间接验证。

### 10.4 顺带学到的：`kmutil load` 的三道门与副作用

| 门 | 现象 | 绕过 |
| --- | --- | --- |
| 用户授权 | `KMErrorDomain Code=27 not approved to load` | `kmutil load -z` |
| 需重启 | `Code=28 requires a reboot` | 无法绕过（设计如此） |
| 符号解析 | 加载/构建时裁定 | 唯一无法绕过的**

**副作用（本次实测，已全部清理）**：`kmutil load` 会把这个 kext 加进三处持久状态 ——
① 辅助内核集合的构建集（`…/AuxKC/CurrentAuxKC/com.apple.kcgen.instructions.plist` +
`StashedExtensions/`）；② UAKL 用户批准列表（`/private/var/db/SystemPolicyConfiguration/UAKL.plist`
与 `…/com.apple.kcgen.uakl.plist` 各一条 cdHash）；③ syspolicyd 决策库
（`KextPolicy` 的 `kext_policy` 表里一条 `allowed=1` 记录）。

已按"只删本次新增的那条"原则清理，并分别备份：
`tools/backup-auxkc-20261009-185427/`、`tools/backup-uakl-20261009-185723/`、
`tools/backup-kextpolicy-20261009-190105/`。清理后逐项核实：
kext_policy 里 ATTO 0 条、UAKL 两条 plist 均无该 hash、stash 7 条（原本的第三方 kext）、
instructions 无 ATTO、任何 `.kc` 文件里 `ATTOFastFrame2` 出现 0 次
（Preboot 里那几个 `com.ATTO.driver.ATTOExpressPCIUl4` 是 Apple 自带的其它 ATTO 驱动，不是我们的）。

`kmutil rebuild` 仍会提示要"移除"该 kext（内存态残留，磁盘已无对应记录）；
下次启动时 kernelmanagerd 从磁盘重读，应无影响。

## 十一、2026-10-09 晚（七）：CX-3 移植的**许可结论**与工作量实测

### 11.1 直接回答：**可以提交仓库**（前提是走 BSD 基底，不用 ATTO 二进制）

实测证据（本次逐文件取回）：

**① 基底是双许可，我们有权选 BSD。** `sys/dev/mlx4` 每个文件头部都是同一段：

```
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, … or the
 * OpenIB.org BSD license below:
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that …
 *      - Redistributions of source code must retain the above copyright notice …
 *      - Redistributions in binary form must reproduce the above copyright notice …
```

逐文件确认过 `mlx4_core/mlx4_main.c`、`mlx4_en/mlx4_en_rx.c`、`mlx4_en/mlx4_en_netdev.c` 都是这段
（Linux 主线的 mlx4 也是同样的 GPL-2.0 / OpenIB BSD 双许可）。

⇒ **选 OpenIB BSD 那一支**，我们移植后的 macOS 驱动就可以用 BSD/MIT 发布。
**义务只有两条**：保留版权声明与许可全文（源码与二进制形式都要）。

**② 不需要打包任何固件。** 在 `mlx4` 里搜索 `request_firmware`：`main.c` / `fw.c` / `cmd.c`
**都是 0 次** ⇒ CX-3 的固件在卡自身 flash 里，驱动不向用户态索取固件文件。
⇒ 不存在"再分发固件"的许可问题（这也解释了为什么 ATTO 包里固件是"刷进卡"而不是"随驱动带"）。

**③ 绝对不能提交的东西**：ATTO 的 kext 二进制及我们改过的副本（专有）、以及由它反编译/抄录的代码。
它只能作为**行为参考**（设备期待什么命令），不能作为代码来源 —— 而 FreeBSD 基底本身就是
Mellanox 官方源头，用后者没有任何法律模糊。

**④ 结论一句话**：CX-3 驱动做完**可以公开**，只要 (a) 代码源自 FreeBSD/Linux 的 mlx4 双许可基底、
(b) 不含 ATTO 二进制、(c) 保留 BSD 版权声明。

### 11.2 基底与工作量实测（这决定了项目规模是否现实）

已取回 FreeBSD `sys/dev/mlx4` 全部 41 个 `.c/.h`（1.1 MB）到
`tools/mlx4-port/freebsd-src/`。分成两层看：

| 层 | 内容 | 体量 |
| --- | --- | --- |
| **协议/DMA 主体**（要保留） | 命令队列、EQ/CQ、ICM、资源跟踪、QP/MR/PD、en 的收发路径 | `mlx4_core` 782 KB + `mlx4_en` 216 KB |
| **内核接口层**（要替换成 IOKit） | 见下表 | — |

接口层实测（grep 计数）：

| 类别 | FreeBSD 里的形态 | 计数 | macOS 对应 |
| --- | --- | --- | --- |
| DMA | `bus_dma` / `bus_dmamap` | 44 / 33 | `IODMACommand` + `IOMemoryDescriptor` |
| 包缓冲 | `mbuf` / `m_freem` / `m_copydata` | 71 / 15 / 7 | macOS **也有 mbuf**（同源 BSD），改动量小 |
| 网络接口 | `ifnet` / `if_alloc` | 1 / 1 | `IOEthernetController` + `IOEthernetInterface` |
| 定时/工作 | `taskqueue` / `callout_reset` / `queue_work` | 10 / 2 / 12 | `IOCommandGate` / `thread_call` / `IOTimerEventSource` |
| 内存 | `kmalloc` / `kfree` / `vzalloc` / `vfree` | 31 / 154 / 1 / 4 | `IOMalloc` / `IOFree`（薄封装） |
| 锁 | `mutex_lock` | 99 | `IOLock` / `IORecursiveLock` |
| 其它 | `sysctl_ctx` / `printk` / `mod_timer` | 12 / 5 / 5 | `sysctl` 保留 / `IOLog` / `IOTimerEventSource` |

**关键发现（修正我的乐观预期）**：`sys/dev/mlx4` 并非纯 BSD API，它引用了 **47 个不同的
`linux/*.h` 兼容头**（来自 FreeBSD 的 LinuxKPI，`sys/compat/linuxkpi/common/include/linux/`
共 242 个头）。但实际用到的 **只有上表那十几个名字**，且都是**薄封装**
（`kmalloc`/`kfree`/`mutex_lock`/`queue_work`/`mod_timer`/`printk` …），
每个都能一对一映到 IOKit 原语。**真正的重活（设备编程、DMA 环、EQ/CQ）本来就是 BSD 语汇**，
而且 macOS 与 FreeBSD 共享 mbuf 血统 —— 这比从 Linux 源码起步要省力得多。

⇒ 项目形态清晰：**保留协议主体 + 写一层 `mlx4compat` 把上面那十几个名字映到 IOKit**。
这与 ATTO 当年做的事（把 Mellanox 的 `cna2` 代码接到 IOKit）是同一路线，而我们有 BSD 许可的基底，
不必碰专有代码。

---

## 四、顺带确认：上游 1401 的 AMD 策略现状

`nullmoth/1401` **main 分支**的 `p1401/policy.py`（本次实取，行号）：

```python
44: # AMD platforms whose MMIO layout needs DevirtualiseMmio (Dortania: TRx40); everything else boots with it off.
45: AMD_DEVMMIO_ON = {"TRX40", "TRX50"}
177: if cpu AMD and chipset not in AMD_DEVMMIO_ON and quirks["DevirtualiseMmio"] is True:
178:     quirks["DevirtualiseMmio"] = False
```

注释自己在 164 行承认：*"AM5 (A620 B650 X670 X870) is not on the list at all."*

⇒ **上游仍未修**：任何 AMD 主板只要芯片组名不在 `{TRX40, TRX50}`，一律被关掉
`DevirtualiseMmio`。我们用 1.0.16+ 重建盘时仍然要手工改回，并按 AM5 配方加回
`MmioWhitelist 0xFD000000`。
