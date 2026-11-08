# BIOS 分析：哪些是**实时**读出来的，哪些是**静态** ROM 里的

最后核对：2026-10-08 22:1x

## 1. 两条完全不同的数据来源（要分清）

| 来源 | 是什么 | 拿到的东西 |
| --- | --- | --- |
| **你下载的 ROM** `MSTerminatorB850BKBWIFI_E2.4D_062926.ROM`（33,554,432 B，sha256 `ef1e972a…f92aa`） | **静态**固件镜像 | 「设置名 ↔ 变量偏移 ↔ 可选值」的**菜单定义**（IFR）。**不含你的任何设置值** |
| **运行中固件的 NVRAM 变量**（管理员 `GetFirmwareEnvironmentVariableEx` 实时读取） | **实时** | 你的**当前设置值**（原始字节） |

两者相乘才能得到「设置名 = 当前值」。所以关键是：**你机器里装的 ROM 和上面这个 ROM 是不是同一个版本**。

## 2. 为什么可以说「配上了」——两条独立交叉验证

1. **版本一致**：机器报 `SMBIOSBIOSVersion = E2.4D`、`ReleaseDate = 2026/6/29`；下载包里的 ROM 文件名
   是 `…_E2.4D_062926.ROM`；铭瑄官网该型号最新一版也是 `E2.4D (beta) 2026/06/29`，文件名
   `MSTerminatorB850BKBWIFI24.zip`（正是你下载的那个）。三者同一版本。
2. **尺寸逐字节对上**：IFR 里声明的变量尺寸，与实时读到的字节数**完全相等**：

   | 变量 | IFR 声明 | 实时读到 | 一致 |
   | --- | --- | --- | --- |
   | `Setup`（`EC87D643-…`） | `0x132` = 306 | 306 B | ✅ |
   | `AmdSetupRPL`（`3A997502-…`） | `0x7E0` = 2016 | 2016 B | ✅ |
   | `AodSetupRpl`（`5ED15DC0-…`） | `0x320` = 800 | 800 B | ✅ |

   如果装的是别的 BIOS 版本，偏移/尺寸几乎不可能同时吻合。

## 3. 逐项：哪些是实时的

### 3.1 实时（从运行中的固件读出原始字节）

```
Setup              EC87D643-EBA4-4BB5-A1E5-3F3E36B20DA9    306 B
AmdSetupRPL        3A997502-647A-4C82-998E-52EF9486A247   2016 B
AmdSetupSTXKRK     3A997502-647A-4C82-998E-52EF9486A247   2245 B
AodSetupRpl        5ED15DC0-EDEF-4161-9151-6014C4CC630C    800 B
AodSetupRplLast    5ED15DC0-EDEF-4161-9151-6014C4CC630C    800 B
AodCoreInfo        5ED15DC0-EDEF-4161-9151-6014C4CC630C    798 B
AodCoreInfoTemp    5ED15DC0-EDEF-4161-9151-6014C4CC630C    798 B
AMD_PBS_SETUP      A339D746-F678-49B3-9FC7-54CE0F9DF226    256 B
AutoDetectData     EC87D643-EBA4-4BB5-A1E5-3F3E36B20DA9     63 B
SystemRamInfo      EC87D643-EBA4-4BB5-A1E5-3F3E36B20DA9      4 B
SecureBoot/SetupMode/VendorKeys/BootOrder/Timeout/…（115 个变量全表见 nvram\all-variables.txt）
```

`SystemRamInfo` = `00 C0 00 00` = **49152** → 与 2×24 GB = **48 GB** 内存总量吻合（这是个独立的正确性旁证）。

### 3.2 ⛔ **不实时、拿不到**：超频页（"SKOC"）的保存档

IFR 里超频页的存储是：

```
VarStore Guid: A3A3B874-7636-4182-BA1E-F52C584494E3, VarStoreId: 0x1F, Size: 0x2015, Name: "VARSTORE_SKOC_SETTINGS_NAME"
```

**实测：实时枚举出的 115 个固件变量里，没有任何一个用这个 GUID。** 直接按名字读也全是
`ERROR_ENVVAR_NOT_FOUND (203)`。结论：**这块 BIOS 的"SKOC"超频页在 NVRAM 里没有对应的保存变量**
（即该页的设置没有作为一组配置被持久化）。

所以：**"你的 BIOS 超频/内存设置"这件事，我拿不到一份完整的、可回灌的配置**——
它不存在于固件变量里，也就不可能被提取或注入到新版 BIOS。

### 3.3 与实测不一致的几项（**不要采信**，需要复核）

| 解码结果 | 实测/旁证 | 判断 |
| --- | --- | --- |
| `AmdSetupRPL +0x117 Memory Target Speed = 3200` | 内存实测 **6000 MT/s**（DIMM 标称 6400） | 该字段没反映生效频率（EXPO 生效在别处），**不可用** |
| `Setup +0x0B1 Integrated Graphics = Disabled` | `1002:13C0`（Raphael 核显）仍作为 PnP 设备存在，Status=`Unknown` | **存疑**，未证实 |
| `Setup +0x0AC Core Performance Boost = Disabled` | `Win32_Processor.CurrentClockSpeed` 自身不可靠 | **存疑**，未证实 |
| `Setup +0x0D9 SATA Mode = AHCI` | 本机没有 SATA 盘（控制器只有 stornvme/spaceport） | 无法用观测验证 |
| `Setup +0x0A8 IOMMU = Disabled` | HV 存在导致 `SLAT/VMMonitor` 读数失真 | 未证实 |

### 3.4 与实测**一致**的（三个独立来源互相印证）

| 解码结果 | 独立证据 |
| --- | --- |
| `Setup +0x06D Above 4G Decoding = Enabled` | `nvidia-smi` BAR1 = 8192 MiB |
| `Setup +0x06E Resize BAR Support = Enabled` | 同上（8 GB 窗口不可能在 4 GB 以下） |
| `Setup +0x125 Launch CSM = Disabled` | `BiosFirmwareType = Uefi`，纯 UEFI 启动 |
| `Setup +0x04D SVM Mode = Enabled` | `VirtualizationFirmwareEnabled = True` |

## 4. 想要"可信的完整快照"，正道是这两条

1. **BIOS 里自己存一份**：进 BIOS → 找 `OC Profile / Save Profile to USB` 之类，把当前超频配置存到
   U 盘；或者干脆 F9/F10 前后对着拍照/记录。这是唯一"官方支持"的配置导出。
2. **差分法（最可靠，我可以带你做）**：现在的 `Setup` / `AmdSetupRPL` 已经存好基线；你进 BIOS
   **只改一项**（比如把 EXPO 从当前的关/开切换一次），保存重启后我再读一次，两份字节一 diff，
   **立刻就能知道这一项到底存在哪个字节、取值怎么编码**。这样我们能把所有你在意的项一条条钉死，
   而不是靠猜 IFR 选项名。需要重启 N 次，但每次 30 秒。

## 5. 工具链（已就绪，都在仓库里）

| 工具 | 版本 | 用途 | 位置 |
| --- | --- | --- | --- |
| UEFIExtract | A75 | 拆 ROM → 找 Setup/CBS 模块 | `~/.codex/tmp/…/uefi-tools/uefiextract/` |
| IFRExtractor-RS | 1.6.1 | ROM → 人读的 IFR（设置名/偏移/选项） | `~/.codex/tmp/…/uefi-tools/ifr/` |
| `parse_ifr.py` | — | IFR 文本 → JSON（VarStore + 804 个设置项） | `docs/bios/scripts/` |
| `decode_bios.py` | — | NVRAM 字节 + IFR → 人读的当前值 | `docs/bios/scripts/` |
| `dump_varstores.ps1` / `read_more.ps1` / `esp_inspect.ps1` | — | 提权读变量 / 检查 ESP | `docs/bios/scripts/`、`~/.codex/tmp/…/` |

## 6. 2026-10-08 23:0x 定稿：BIOS 逐项结论（是否需要改）

数据来源：**运行中固件**的 `Setup` 变量（`0xF101`，306 B）+ 标准 UEFI 变量
`SecureBoot`/`SetupMode`/`VendorKeys`，配 IFR 的选项表解码。**不是**从 ROM 抄的默认值。

| 项 | IFR 位置 | 实测值 | 目标 | 结论 |
| --- | --- | --- | --- | --- |
| Secure Boot | 标准变量 | **0 = Disabled** | Off | ✅ 已对 |
| Fast Boot | `Setup` +0x03 | **Disabled** | Off（推荐） | ✅ 已对 |
| Launch CSM | `Setup` +0x125 | **Disabled** | Off | ✅ 已对 |
| Above 4G Decoding | `Setup` +0x6D | **Enabled** | On | ✅ 已对 |
| Resize BAR Support | `Setup` +0x6E | **Enabled** | On（推荐） | ✅ 已对 |
| Above 4GB MMIO Limit | `Setup` +0x69 | 40bit (1TB) | — | ✅ 无要求 |
| IOMMU | `Setup` +0xA8 | **Disabled** | Off 更省事 | ✅ 已对 |
| SVM Mode | `Setup` +0x4D | Enabled | — | ✅ 无所谓，不必动 |
| SATA Mode | `Setup` +0xD9 | AHCI | AHCI | ✅ 已对（本机无 SATA 盘） |
| NVMe RAID mode | `Setup` +0xD7 | Disabled | Off | ✅ 已对 |
| Global C-state Control | `Setup` +0xA7 | Disabled | — | ✅ 无要求，**别动** |
| Core Performance Boost | `Setup` +0xAC | 解码 Disabled | — | ⚠️ **解码不可信**（CPU 实测能到 5140 MHz）→ 别按它改 |
| Integrated Graphics | `Setup` +0xB1 | 解码 Disabled | — | ⚠️ **解码存疑**（`1002:13C0` 设备仍在）→ 别按它改 |
| **XHCI Hand-off** | `UsbSupport`（`0x13`）+0x2B | **读不到**（该变量不暴露） | On（推荐） | ⚠️ **需要你进 BIOS 目视确认一次** |

### 结论：**不需要做任何更改。**

1401 / Dortania 要求的必需项（Secure Boot、CSM、Above 4G、Resizable BAR）**全部已经是对的**，
而且这次不是照 IFR 猜的，是**从实时变量解出来**的，还有独立旁证：

| 解码结果 | 独立旁证 |
| --- | --- |
| `Above 4G Decoding = Enabled` + `Resize BAR Support = Enabled` | `nvidia-smi` BAR1 = **8192 MiB**（RTX 5060 默认 256 MB；8 GB 窗口不可能在 4 GB 以下） |
| `Launch CSM = Disabled` | `BiosFirmwareType = Uefi`，纯 UEFI 启动 |
| `SVM Mode = Enabled` | `VirtualizationFirmwareEnabled = True` |

### 唯一要你亲眼确认的一项：XHCI Hand-off

它存在（`QuestionId 0x23A`，在 `UsbSupport` 变量里），但这个变量不是标准 UEFI 变量、读不到，
所以只能进 BIOS 目视。位置通常在 **Advanced → USB Configuration**，标为 `XHCI Hand-off`，
目标值 **Enabled**（这是给"没有 XHCI hand-off 支持的 OS"（macOS 就是）用的兼容开关；
Dortania 列为推荐项）。**如果已经是 Enabled 或找不到，就不用管。**

#### 补充（2026-10-08 23:5x）：查清了它为什么在 BIOS 里看不到，以及"不用管"的完整依据

1. **它是被条件隐藏的**。IFR 里 `XHCI Hand-off` 被包在：

   ```
   SuppressIf
     EqIdVal QuestionId: 0x237, Value: 0x0     # 0x237 = "USB Support"
     EqIdVal QuestionId: 0x2DF, Value: 0x0     # 0x2DF = 内部项（只作条件、无界面）
     Or
   End
   ```

   即：**当 `USB Support = Disabled` 或 `0x2DF = 0` 时，BIOS 就不显示这一项**。
   另有一层 `GrayOutIf (0x2AA == 1)`。

2. **默认值就是我们要的 Enabled**。IFR 里两个默认档都写着：

   ```
   OneOfOption Option: "Enabled"  Value: 1
   OneOfOption Option: "Disabled" Value: 0
   Default DefaultId: 0x0 Value: 1
   Default DefaultId: 0x1 Value: 1
   ```

   同一页的 `USB Support` 默认也是 `Enabled`（`Enabled Value: 1, Default, MfgDefault`）。

3. **这个 VarStore 在 NVRAM 里根本不存在** —— 两种独立方法一致：
   ① 直接按名读 `UsbSupport`（GUID `EC87D643-…`）返回 `ERROR_ENVVAR_NOT_FOUND (203)`；
   ② 枚举全部 115 个固件变量，含 `Usb` 的只有 `BootFromUSB`（另有用途）。

   ⇒ **USB 配置页的所有项都跑在固件默认值上，而默认值全是 Enabled。**

4. **旁证**：USB 在 Windows 下工作正常，macOS 也已成功引导并可用（USBToolBox 能枚举出
   4 个控制器 17 个端口）。

**结论：不需要做任何事。** 如果将来真的要改，先在 BIOS 里把 `USB Support` 设为 `Enabled`
（它本来默认就是），`XHCI Hand-off` 才会显示出来。

顺带：`Resize BAR Support` 的 IFR 帮助文本里铭瑄自己写着
*"To enable Resize BAR Support to fully harness GPU memory, please navigate to Boot section and disable CSM"*
—— 与我们现在的状态（CSM 已关 + ReBAR 已开）完全吻合。
