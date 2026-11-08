# 磁盘分区方案 + 两张网卡的 macOS 支持情况

日期：2026-10-08　依据：本机实测（PowerShell / 分区表）+ 上游仓库与论坛检索

## 1. 先更正一个我之前的错误读数

我 10-07 那次读到的「C 盘剩 254 GB」**是错的**。用三种方式复核后：

```
Get-Volume          : Size 1906.6 GB, Free 1055.7 GB
Get-PSDrive C       : Used  851.0 GB, Free 1055.7 GB
Win32_LogicalDisk   : Size 1906.6 GB, Free 1055.7 GB
```

**C 盘实际有 1055.7 GB 空闲**，分出 300 GB 完全够用。

## 2. 但真正的障碍不是空间，是「一个字节的空闲分区都没有」

```
Size              : 2,048,408,248,320 B (1907.73 GB)
Allocated         : 2,048,407,199,744 B   ← 几乎等于整盘
LargestFreeExtent : 1,048,576 B (0.001 GB) ← 未分配空间 = 1 MB
```

分区表当前状态：

| # | 类型 | 起始 | 大小 | 说明 |
| --- | --- | --- | --- | --- |
| 1 | EFI System (ESP) | 0.000 GB | 200 MB | 分区类型 `{c12a7328-…}` |
| 2 | Microsoft Reserved (MSR) | 0.201 GB | 16 MB | Windows 保留 |
| 3 | Basic (C:) | 0.212 GB | 1906.63 GB | NTFS |
| 4 | **Recovery** | **1906.84 GB** | 907 MB | 分区类型 `{de94bba4-…}` |

**所以「分区前必须先压缩 C:」**，不是空间不够，是整块盘已经被四个分区占满。

## 3. 压缩 C: 的可操作清单

压缩会在 **C: 与 Recovery 之间**留出未分配空间 —— 位置正好，不用移动任何分区。

Windows 压缩的上限由「最后一个不可移动文件的位置」决定。这台机器上两个典型障碍：

| 文件 | 大小 | 处理 |
| --- | --- | --- |
| `C:\hiberfil.sys` | **19.07 GB** | `powercfg /h off`（关休眠，可逆） |
| `C:\pagefile.sys` | **60.00 GB** | 临时设成「无分页文件」或挪到别的盘，重启 |

步骤：

1. 先直接试：`磁盘管理` → 右键 C: → **压缩卷** → 看它给出的「可用压缩空间」有多大
2. 若 ≥ 307200 MB（300 GB）→ 直接压 300 GB，跳到 §4
3. 若不够 → 以管理员运行 `powercfg /h off`，把分页文件设为无（或挪走），**重启**，再压缩
   注意：关休眠 + 去掉分页文件会让你暂时失去「休眠」和崩溃转储，装完可以恢复
4. 压缩完成 → 得到约 300 GB 未分配空间（在 C: 之后、Recovery 之前）

> 这一步**完全可逆**：装完测试若不满意，删掉 macOS 分区、右键 C: → 扩展卷即可把空间还回去。

## 4. 分区与「要不要单独给 macOS 一个 EFI 分区」

**结论：测试阶段一个 EFI 都不用动；不要建第二个 ESP。**

理由与依据：

1. **1401 的设计就是把 OpenCore 放在 U 盘上**（README 原话：*OpenCore on that stick is what starts
   macOS*）。也就是说测试阶段 macOS 的启动链路是 `固件 → U 盘 EFI → OpenCore → macOS`，
   **内部 ESP 完全不参与**。零风险。
2. 等测试通过、你想脱离 U 盘启动时，**共用现有那 200 MB ESP**即可：Windows 在 `\EFI\Microsoft`，
   OpenCore 在 `\EFI\OC`，同盘共存是标准做法。剩余空间够：ESP 只用约 37 MB，
   而我们的 EFI 只有 8.3 MB。
3. **不要在同一个盘上建第二个 ESP。** GPT 规范上允许多个 ESP，但实践中有坑：部分固件只认第一个 ESP，
   或者会多出一个分不清的启动项（两个都叫 Windows Boot Manager 的样子），启动顺序会变得难以排查；
   而且你什么也换不来。网上的「第二个 ESP」做法基本出现在**双硬盘**场景（每盘一个），不是单盘。

安装时在磁盘工具里怎么处理那 300 GB：

- 最省事：**Windows 这边压缩完就停手，不建分区**，让 macOS 磁盘工具在未分配空间上建 APFS；
- 更常见的 Dortania 做法：在 Windows 里把这块未分配空间建成一个简单卷（NTFS/exFAT 都行，甚至不格式化），
  然后在 macOS 磁盘工具的「显示所有设备」里**选中这个分区抹成 APFS**（GUID 分区图）。

两种都能用。第二种更不容易选错盘，我倾向第二种。

## 5. Windows 恢复分区是什么，会不会挡路

**它叫 WinRE（Windows 恢复环境）分区**，类型 `{de94bba4-06d1-4d40-a16a-bfd50179d6ac}`，
里面装的是 `WinRE.wim`。用途：

- 设置 → 系统 → 恢复 → **「重置此电脑」**
- 开机失败时的**自动修复**
- Shift + 重启 进入的**高级启动**菜单
- 部分 Windows 更新/回滚流程需要它

**它不挡你的路。** 关键在于它的位置：它在**盘尾**（偏移 1906.84 GB），而压缩 C: 产生的是
它在**前面**的未分配空间。所以：

| 你想做的事 | 会不会被恢复分区挡住 |
| --- | --- |
| 压缩 C: 拿 300 GB | ❌ 不挡（未分配空间出现在 C: 之后、恢复分区之前） |
| 在未分配空间建新分区 | ❌ 不挡 |
| 以后把 300 GB 还给 C:（扩展卷） | ❌ 不挡（新空间与 C: 相邻，恢复分区仍在更后面） |
| 用「恢复分区之后」的空间扩展 C: | ✅ 会挡 —— 但这不是我们要做的事 |

**所以：不用删、不用移，只有 907 MB，留着。** 删它反而弄坏「重置此电脑」，而且要 `diskpart` 的
`delete partition override` 才能删，得不偿失。

## 6. RZ616（MediaTek MT7922）在 macOS 有驱动吗

### WiFi：没有，而且短期内不会有

多方一致结论：

- AMD OS X 论坛：*"there is no one looking to create a kext that works with the Mediatek WiFi card"*
- EliteMacx86：*"its also not supported. This is why you couldn't find any kexts"*
- Frame.work 论坛：*"RZ616/MT7922 has never even…"*

技术上的根因：Apple **确实**在 macOS 里带了 MT7922/MT7923 的**蓝牙固件**，也在某个 DEXT 里出现过
MT7932 的 **WiFi** 引用 —— 但**没有 MT7922 的 WiFi 驱动**。论坛里的调侃是「Apple 要么写错了名字，
要么只是给板载蓝牙模块用的」。结论：**WiFi 这条路走不通**，换卡或用有线。

> 我们这份 EFI 已经把 RZ616 Wi-Fi（`14C3-0616`）和它的蓝牙一起禁用了，这是对的，别去解开。

### 蓝牙：**有一个真实项目**（但很新）

`laobamac/SimpleMtkBtFw`（GPL-3.0）—— 专门做 MediaTek USB 蓝牙固件装载：

| 项 | 值 |
| --- | --- |
| 作者 | laobamac（**就是 1401 用的那个 AMD_Vanilla fork 的同一位作者**） |
| 覆盖 | MT7921 / MT7922 / MT7922A |
| 状态 | 11 stars，2026-05-29 创建并当天最后推送，有 1.0.0 release（源码 + RELEASE.zip） |
| 加载顺序 | `Lilu.kext` → `SimpleMtkBtFw.kext` → `SimpleMtkBtPatcher.kext` |
| 限制 | 只能 OpenCore 注入；不能再注入别的蓝牙固件加载器（会抢同一个 USB 设备）；实验性 |

**我们的设备恰好能匹配。** 我把它的 `Info.plist` 拉下来核对了匹配条件：

```
SimpleMtkBtFw_MediaTek_Generic:
    IOProviderClass = IOUSBHostDevice
    bDeviceClass    = 224   (0xE0, Wireless Controller)
    bDeviceSubClass = 1
    bDeviceProtocol = 1
    idVendor        = 3725  (0x0E8D = MediaTek)
```

我们主板蓝牙的 USB ID 是 **`0E8D:0616`**（VID 正好 0x0E8D）。它没有把 `0616` 单独列进具体 personality
（列了 `0e8d:0608` 等），但上面这条 **Generic 规则按「厂商 0x0E8D + 无线控制器类」匹配，我们的设备符合**。
运行期固件路径只接受 MT7961/MT7922 内部 ID，我们这颗正是 MT7922。

**建议：装完 macOS 之后再去试它**，当彩蛋而不是前置条件 —— 没有 WiFi 的情况下，蓝牙只能给你
键鼠/耳机，AirDrop、接力这类需要 WiFi+BT 双通道的功能仍然用不了。

## 7. Mellanox ConnectX-3 Pro 在 macOS 有驱动吗

### 没有，而且属于「明确不支持」

| 证据 | 内容 |
| --- | --- |
| NVIDIA 官方论坛答复 | *"At this moment NVIDIA ConnectX Adapters are **not supported** on the Mac OS platform. This hardware has not been tested on a Mac OS system and driver itself is an Apple driver."* |
| macOS 自带的网卡驱动 | `/System/Library/DriverExtensions/` 里是 `AppleEthernetE1000`、`AppleEthernetIXGBE`（Intel 82599）、`AppleEthernetIXL`（Intel X700）、`AppleEthernetMLX5` —— **MLX5 只覆盖 ConnectX-4/5/6**，ConnectX-3 属于 mlx4 世代，从来不在里面 |
| 第三方结论 | kittenlabs（25GbE on macOS 那篇）：*"ConnectX-2 and ConnectX-3 cards are **NOT supported** by the driver. macOS/iOS do not provide drivers for those cards."* |

网上那个「Mellanox ConnectX-3 40GbE using ATTO FastFrame macOS driver」的 MacRumors 长贴，实际内容
**主要是 ConnectX-4/5**（借用 Ventura beta 1 里还是 kext 形态的 `AppleEthernetMLX5.kext`，
配合 `dk.mlx5=0` 屏蔽 dext），跟 ConnectX-3 关系不大；ATTO FastFrame 那条路是给 **ATTO 自家贴牌的卡**用的。

**结论：忘掉这张卡。** 好消息是 macOS 侧不需要它 —— 你的板载 **Realtek RTL8125 2.5G**
（`10EC-8125`）有成熟 kext，我们的 EFI 里已经装好了（`RTL812xLucy.kext`）。
装系统时只要**网线插在 Realtek 那个口上**（不是 Mellanox 那两个 QSFP 口）。

## 8. 修正后的施工顺序

1. [你] BIOS：`Resizable BAR = On`、`Above 4G Decoding = On`、CSM Off、Fast Boot Off、
   XHCI Hand-off On、IOMMU Off
2. [你] 网线插到 Realtek 2.5G 口
3. [你] 压缩 C: 出 ~300 GB（必要时先 `powercfg /h off` + 关分页文件）
4. [你] 把这块未分配空间建成一个简单卷（NTFS 即可），**不要格式化也行**
5. [你] 管理员运行 `tools\1401\1401.exe` 做启动盘（擦 Kingston E:）
6. [我] 核对盘上 EFI 关键项与驱动包哈希
7. [你] 改 BIOS → 从 U 盘启动 → 磁盘工具里把第 4 步那块分区抹成 **APFS / GUID** → 装 macOS 15
8. [你] 装完跑 U 盘上的 `NullMoth\1401-Mac-1.0.2.zip` 里的 1401.app 装 NVIDIA 驱动
9. [我] 验收 `kmutil showloaded | grep nullmoth` 与 `system_profiler SPDisplaysDataType`
10. [可选，之后] 试 `SimpleMtkBtFw` 拿蓝牙；想脱离 U 盘启动就把 `EFI\OC` 拷进共用 ESP
