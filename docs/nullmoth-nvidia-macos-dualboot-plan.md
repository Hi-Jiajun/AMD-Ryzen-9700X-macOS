# 本机双启动安装 macOS 15 + nullmoth NVIDIA 驱动：调整清单

日期：2026-10-07（**当日已按上游更新修订**）
依据：nullmoth/nvidia-macos-driver@a7d2f91（**v1.0.1**）+ nullmoth/1401@2cf5917（**v1.0.7**）README、
docs/HOW-IT-WORKS.md、package/install.sh、p1401/policy.py、p1401/guide.py，以及本机实测（未做任何修改）。

> **修订说明**：本文件第一版基于 driver v1.0.0 / 1401 v1.0.0。当天上游连发 1401 1.0.1→1.0.7 与 driver 1.0.1，
> 其中两条直接命中本机，已在下文标 ✅ 修正：
> ① **`SetupVirtualMap` 在 Ryzen/AM5 上必须为 `False`**（1.0.7 修复，v1.0.0 生成的 EFI 会卡在
> `Couldn't allocate runtime area`）；
> ② **BIOS 里 `Resizable BAR` 必须 `On`**（1.0.1 起的要求）——第一版本文写的是「由 OpenCore quirk 接管、
> BIOS 保持默认」，**那是错的**，本机实测该项现在是 `Disabled`，必须手动打开。

## 0. 结论

**可行，而且你这张卡正是作者唯一实测的那张。** 但有 4 处非默认调整、1 处硬件必须换、2 个未知数。

| 判断 | 内容 |
| --- | --- |
| 最大利好 | `app/Resources/nvidia_gsp_ids.json` 里 `"tested": {2D05}` —— 你的 RTX 5060 就是这个 device id |
| 最大不确定 | 平台是 AMD Zen5，而驱动只在 Intel Mac / OpenCore 上验证过；AMD 需要一整套内核补丁 |
| 必须换硬件 | 板载无线 RZ616（MediaTek MT7922）macOS 无驱动，蓝牙同样无解 |
| 数据风险 | 目前只有一块 NVMe，要装就得动分区；建议加第二块盘 |

## 1. 本机实测事实（2026-10-07）

| 项 | 实测值 | 对 macOS 的含义 |
| --- | --- | --- |
| CPU | AMD Ryzen 7 9700X（8 核 16 线程，Zen5 / Family 1Ah） | 必须打 AMD 内核补丁；`install.sh` 只查 `uname -m = x86_64`，架构这关能过 |
| 主板 | MAXSUN MS-Terminator B850BKB WIFI，AMI BIOS E2.4D (2026-06-29)，UEFI | AM5 / B850；注意 AM5 板载 WiFi 可能触发 PCI 枚举问题 |
| 显卡 | NVIDIA RTX 5060，PCI `10DE:2D05`，驱动 32.0.16.1692，**唯一显示设备** | 驱动实测目标卡；没有第二块显卡可用 |
| 核显 | 未枚举到任何 `VEN_1002` 设备（BIOS 里关着） | 打开了也没用，macOS 无 Zen5 核显驱动 |
| 内存 | 47.7 GB | 够 |
| 有线网卡 | Realtek RTL8125 2.5GbE，PCI `10EC:8125` | ✅ macOS 用 LucyRTL8125Ethernet |
| 无线 | RZ616 Wi-Fi 6E，PCI `14C3:0616`（MediaTek MT7922）+ 蓝牙 | ❌ 无驱动，必须换卡或只用有线 |
| 声卡 | Realtek ALC1220，`HDAUDIO VEN_10EC&DEV_1220` | ✅ AppleALC（layout-id 需实测） |
| 磁盘 | 单块 ZHITAI TiPro9000 2TB NVMe（disk 0，GPT）：ESP 200MB + MSR 16MB + C: 1906GB + 恢复 0.89GB；C: 剩 254GB | 要么压缩 C:，要么加盘 |
| ESP 占用 | 已用 37.1 MB / 剩 158.9 MB | 放 OpenCore（约 30–60 MB）够，不必扩 ESP |
| 安全启动 | 注册表 `UEFISecureBootEnabled = 0`（已关） | 满足驱动要求，**BIOS 不用改这一项** |
| BitLocker | C: `EncryptionMethod` 为空 → 未加密 | 不用挂起加密，省一步 |
| **Resizable BAR** | **`Disabled`** | ⚠️ **必须改**：驱动需要显卡完整 BAR，否则装完驱动屏幕停在启动文字 |
| 扩展卡 | Mellanox ConnectX-3 Pro（占前置 PCIe 4.0 x4 槽） | 与 macOS 无关，但会占掉唯一的前置槽 |
| 主板空位 | 官方规格：**2 个 PCIe 5.0 x4 M.2**（1 前 1 后），当前只用 1 个 | → 还有 1 个 M.2 可插第二块 NVMe |

## 2. 必须做的调整

### 2.1 硬件

1. **无线网卡**：拆掉 RZ616，换一张 macOS 原生支持的 M.2 无线卡（BCM94360NG / BCM943602CS 之类，
   或直接用有线）。这是纯硬件动作，不做就只能有线联网、没有 AirDrop/接力。
2. **第二块 NVMe（强烈建议）**：主板还有一个空 M.2。加一块 1TB 给 macOS 独占 → Windows 的 ESP、
   分区表、恢复分区完全不动，风险最低。
   备选：不买盘，压缩 C: 出 ~150–200 GB 做 APFS，共用现有 ESP（空间够）。代价是 macOS/Windows
   更新会争 ESP，且分区操作直接碰到你的系统盘。

### 2.2 BIOS（以 1401 v1.0.7 的机器专属步骤为准）

| 设置 | 值 | 备注 |
| --- | --- | --- |
| **Above 4G Decoding** | **On** | must；驱动要把显卡内存映射到 4 GB 以上 |
| **Resizable BAR / Re-Size BAR** | **On** | ⚠️ **must**，你机器现在是 `Disabled`；1.0.1 起 1401 明确要求，不改则「装完驱动后屏幕停在启动文字」。*有些板子要先开 Above 4G 才出现这一项* |
| CSM | Off | 已确认是 UEFI 启动，保持 |
| Secure Boot | Off | 已是 0，不用动 |
| Fast Boot / 快速启动 | Off | 避免跳过 USB 初始化，认不到 U 盘 |
| XHCI Hand-off（有 EHCI 也一起） | On | 让 macOS 接管 USB |
| IOMMU | Off | AMD IOMMU 是 macOS 早期启动挂起的常见原因 |
| 核显 | 保持关闭 | macOS 用不上 |

注：`Re-Size BAR Support` 与 OpenCore 的 `ResizeGpuBars` 是**两层**——BIOS 打开后显卡才会暴露大 BAR，
OpenCore 的 quirk 再决定给 macOS 看到多大。两层都要对。

### 2.3 OpenCore（这是工作量最大的一块）

驱动 README §4 要求的项：

```
NVRAM > Add > 7C436110-AB2A-4BBB-A880-FE41995C9F82
  boot-args            = nvfb=1 nvaccel=1 nvfbheads=4 -nvkmsnosmooth amfi_get_out_of_my_way=0x1 amfi=0x80
  csr-active-config    = <430A0000>     # SIP 部分关闭：允许未签名 kext + 不受限文件系统
  （以上每个 key 同步加进 NVRAM > Delete）
Misc > Security > SecureBootModel   = Disabled
UEFI  > Quirks  > ResizeGpuBars     = 13          # 8GB BAR1
Booter > Quirks > ResizeAppleGpuBars = -1         # macOS 看到完整 BAR
Kernel > Block  > com.apple.iokit.IONDRVSupport, Strategy=Exclude
```

AMD 平台额外要加的（这套是 Intel 平台上没有的）：

| 项 | 值 | 为什么 |
| --- | --- | --- |
| `Kernel > Quirks > ProvideCurrentCpuInfo` | **True** | AMD_Vanilla 补丁集强制要求，不开直接不启动 |
| AMD_Vanilla 内核补丁 | 按 macOS 15 那一组导入 | 见 https://github.com/AMD-OSX/AMD_Vanilla（仍在维护，支持到 Sequoia 15 / Tahoe 26） |
| core count 补丁 | 4 处 `algrey - Force cpuid_cores_per_package` 的 Replace 改成 `BA 08 0000 00` | 8 物理核 = `08`，填错会起不来 |
| **PAT 补丁选型** | 必须用 **Algrey 原版**（默认那个） | 官方 README 明确：Shaneee 版会让 **NVIDIA 显卡起不来** |
| `Kernel > Emulate > DummyPowerManagement` | True | AMD 没有 XCPM |
| `AppleMCEReporterDisabler.kext` | 加上 | AMD 平台常规项 |
| `Kernel > Patch > Fix PCI bus enumeration` | 视情况启用（默认关） | AM5 板载 WiFi + USB4/雷电 同时开启时 macOS 会枚举不全；你正好是 AM5 + 板载 WiFi |
| ✅ `Booter > Quirks > SetupVirtualMap` | **False** | **1.0.7 新增**：Ryzen/AM5 板上必须关。硬件嗅探器认不出 AM5 芯片组（只报裸 `"AMD"`），1.0.0 会留着 `True` → 开机 `EB.MM.AKM Err(0xE)` / `STOP 0x16`「Couldn't allocate runtime area」 |
| `Misc > Debug` | `Target=67`（0x43）+ `AppleDebug` + `ApplePanic` | **1.0.7 新增**：把 OpenCore 启动日志与 macOS panic 写到 U 盘，失败后能回 Windows 上报 |

kext 清单（除 AMD 补丁外都是常规）：

Lilu、VirtualSMC(+SMCProcessor/SMCSuperIO)、AppleALC、LucyRTL8125Ethernet、USBToolBox+UTBMap（USB 端口映射，
Sequoia 必须）、AppleMCEReporterDisabler。

不装：WhateverGreen（README 要求去掉 NVIDIA 相关补丁与 `agdpmod=pikera`；本机只有这张 N 卡，用不上）、
任何 `nv_disable=1`。

**不要**给 NVIDIA 卡写 `DeviceProperties`（README 明确）。

### 2.4 安装介质

用同一作者的 **1401**（Windows 侧生成 macOS 安装盘，README 直接指路 https://github.com/nullmoth/1401 ），
或自行用 Dortania 流程 + AMD_Vanilla 配置做 USB。

**注意安装阶段与运行阶段的 BAR 设置是反的**（README §4 明确）：

| 阶段 | ResizeAppleGpuBars | ResizeGpuBars | IONDRVSupport |
| --- | --- | --- | --- |
| 装 macOS 时（跑固件帧缓冲） | `0` | `-1` | 不屏蔽 |
| 装完驱动后 | `-1` | `13` | Exclude |

1401 生成的安装盘就是这个「安装态」配置，其 Mac 端 app 会在装驱动时切成运行态。

## 3. 完整流程

1. 备份 C: 关键数据（或直接上第二块 NVMe，跳过这步的紧张感）。
2. 装第二块 NVMe；BIOS 确认 Above 4G ON、CSM OFF、Fast Boot OFF。
3. 用 1401 在 Windows 上做安装 U 盘（≥16GB）。
4. **改 U 盘 EFI 里的 config.plist**：导入 AMD_Vanilla（Sequoia 组）+ ProvideCurrentCpuInfo + 8 核 core-count + Algrey PAT + AppleMCEReporterDisabler；SMBIOS 设 `iMacPro1,1`（驱动的 1401 也是按这个型号匹配）。
5. 拔掉 Windows 盘（物理隔离最稳）或用 BIOS 引导顺序锁定，从 U 盘装 macOS 到新盘（别让 Disk Utility 碰 Windows 分区）。
6. 首次启动仍然跑固件帧缓冲（低分辨率、慢），正常现象。**如果卡在这里**，U 盘上会有
   `opencore-*.txt` / `panic-*.txt`（1.0.7 默认开启），拿回 Windows 用 1401 的「Scan for logs」上报。
7. 把 1401 Mac app 和驱动包（现在是 **1.0.1**）拷进去 → 运行 → 允许扩展 → 重启 → 二次重启。
8. 验证：`kmutil showloaded --list-only | grep nullmoth`（4 行）、`system_profiler SPDisplaysDataType | head -20`（Metal: supported）。
9. 把 OpenCore 落到 macOS 盘的 ESP，调 BIOS 启动顺序；保留 U 盘做救援（boot picker 里有 `1401: Remove NVIDIA driver`）。

## 4. 风险与已知限制

- **安全姿态**：`amfi=0x80` + `amfi_get_out_of_my_way=0x1` 基本关掉 AMFI，`csr-active-config=430A0000`
  又放宽 SIP —— 这台 macOS 会变成「可以加载任意未签名代码」的环境。只当实验/研究机用。
- **驱动 1.0.0 且未签名**，kext 从 /Library/Extensions 走 Auxiliary Kernel Collection 加载；升级 macOS 小版本后可能失效。
- **无图形加速的安装阶段**：装驱动前只有固件帧缓冲，界面卡、分辨率低。
- **卸载**：driver 1.0.0 的卸载器在有/无其他 kext 的机器上可能卸不干净（内核集合里留着旧驱动），
  1.0.1 修了。**用 1.0.1 的 Mac app 卸载**，别用 1.0.0 留在 `/Library/NullMoth` 的那份。
- **电源管理/睡眠**：AMD 平台 XCPM 不可用，睡眠大概率不正常，别指望笔记本式体验。
- **Windows 侧**：BitLocker 未加密不用挂起；关闭快速启动；Windows 大版本更新可能改 ESP 顺序，装完驱动后留意启动项。
- **许可**：`plugin/`、`kexts/`、`build/`、`package/` 是 PolyForm Noncommercial 1.0.0（不可商用）；
  `translator/` 是 LGPL-3.0。自己用没问题，别往商业项目里搬。

## 5. 两个未知数（要在实机上验证）

1. **Zen5 不在 AMD_Vanilla 文档列出的家族表里**（表里写 17h/19h）。补丁是字节模式匹配，通常照样生效，
   但这是第一个要实测的点；失败就退到社区 Zen5 补充补丁。
2. **驱动本身没在 AMD 平台上验证过**。它的安装脚本只卡 x86_64 架构，kexts 里也没有 CPUID/vendor 检查
   （已 grep 确认），所以理论上与 CPU 厂商无关，但作者只测过 Intel。

已由当天更新消掉的未知数（保留备查）：

- ~~「1401 只在 Intel 那台端到端测过」~~ → 1.0.7 的修复说明与提交注释里成串出现 AMD 用户上报的日志 ID
  （`NM-MH8TV0NW`、`NM-5WSVHVK1`、`NM-EQB69GYK`、`NM-V1W41NGH`、`NM-ZW53KZWZ` 等），说明 AMD 侧已有真实安装量；
  本项目自己的 corpus 回归里 AMD 两台（Threadripper / Zen3）也跑通（见 `1401-audit.md` §4.1）。
