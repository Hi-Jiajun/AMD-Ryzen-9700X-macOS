# USB 端口映射：本机现状 + USBToolBox 操作手册

最后核对：2026-10-08 22:2x　依据：盘上 `UTBMap.kext` 实测解析 + USBToolBox 官方 README

## 1. 本机现状：Mac 端那次"USB map"只记录了当时插着的设备

盘上 `E:\EFI\OC\Kexts\UTBMap.kext\Contents\Info.plist` 实测：

```
XHC0          端口数 2   AppleUSB20XHCIPort(connector 0) / AppleUSB30XHCIPort(connector 9, 注释 "Xiaomi 15S Pro")
XHC1          端口数 2   AppleUSB20XHCIPort(connector 0) / AppleUSB30XHCIPort(connector 3, 注释 "DataTraveler 3.0")
XHC2          端口数 1   AppleUSB20XHCIPort(connector 0, 注释 "MCHOSE G87 V2")
pci1022,43fc  端口数 2   AppleUSB20XHCIPort(connector 0, 注释 "HECATE G2000") / AppleUSB30XHCIPort(connector 3)
```

那些注释就是你当时插着的设备（手机 / U 盘 / 键盘 / 音箱）。**它不是"发现全部端口"，只是把当场看到的记下来**。
而这块板子光 BIOS 里就有 13 个端口开关：`USB2_1..4`、`USB3_1_LAN`、`USB3_2_LAN`、`USB3_3`、`USB3_4`、
`TYPE-C`、`F_USBC`、`F_USB3_1`、`F_USB3_2`、`F_USB2_1_1..F_USB2_2_2`。

### ⛔ 而且现在配置是**错的**：`UTBDefault.kext` 与 `UTBMap.kext` 同时启用

```json
Kernel > Add:
   6  USBToolBox.kext   Enabled=True
   7  UTBDefault.kext   Enabled=True     ← 冲突
  10  UTBMap.kext       Enabled=True     ← 冲突
```

USBToolBox 官方 README 第 8 条明确写着：

> *If building a map that uses the USBToolBox kext, make sure to grab the latest release of the kext too.*
> ***Make sure to remove `UTBDefault.kext`, if you have it.***

两个 map kext 会往同一批 XHCI 提供者注入端口表，结果不可预期。实测 `UTBDefault.kext` 里
`XHC1` 的端口数是 **0**（一个空壳）。这本身就可能是"USB 时好时坏"的来源之一。

## 2. 工具与版本（已下载并校验，放在 `tools/usbtoolbox/`）

| 文件 | 大小 | SHA-256（前 16 位） | 说明 |
| --- | --- | --- | --- |
| `USBToolBox-tool-0.2-Windows.zip` | 11,836,574 B | `b528378e4d383b1a` | 工具本体（v0.2，含 `dist/Windows.exe`） |
| `USBToolBox-kext-1.2.0-RELEASE.zip` | 60,756 B | `c315a3a5acfd496d` | 配套 kext v1.2.0 |

**好消息**：盘上现有的 `USBToolBox.kext` 二进制与上游 1.2.0 **逐字节相同**
（都是 39,360 B，sha256 `c5c8dc205a079ee3…`），所以 kext 不用换，只换 map。

来源：`github.com/USBToolBox/tool`（tool:v0.2）、`github.com/USBToolBox/kext`（kext:1.2.0）。

## 3. 操作步骤（Windows 侧，这是官方推荐路径）

> 前置：USB 2.0 设备一个、USB 3.0 设备一个（U 盘即可）、USB-C 设备或 USB-C 转 A 转接头一个。
> 官方原文：Windows 10/11 64 位才有完整功能（伴随端口绑定 + 端口类型猜测）。

1. 解压 `USBToolBox-tool-0.2-Windows.zip`，**管理员**运行 `dist\Windows.exe`
   （若杀软拦截，用 `Windows.exe` 而不是带自解压壳的那个）。
2. （可选）按 `C` 进设置：把 **Bind Companions 改成 Disabled**，这样能单独决定
   "USB3 端口上那个 USB2 伴随端口"是否禁用，不被绑在一起。
3. 主菜单按 `D` = **Discover Ports**，等列表填充出来。
4. **把 USB 设备逐个插进每一个物理端口**，每插一个等列表里出现该设备、再拔下来插下一个。
   * Windows 下插 USB 3 端口只需插一次（伴随检测会工作）。
   * **USB-C 端口要插两次**（正反各插一次）。
5. 按 `S` = **Select Ports** 检查/调整每个端口的类型。常用类型：

| 类型 | 值 |
| --- | --- |
| USB 2 Type-A | `0` |
| USB 3 Type-A | `3` |
| USB-C（带开关 / 正反不同） | `9` |
| USB-C（不带开关 / 正反相同） | `10` |
| 内置（如蓝牙、摄像头，禁止外部访问） | `255` |

6. 批量设类型用 `T:` 语法，例如 `T:3,4,5,6:3`（把 3/4/5/6 设成 USB3 Type-A）、
   `T:17:255`（把 17 设成内置）。给端口起名用 `C:`，例如 `C:22:Rear-Top-USB3`。
   —— 这些名字会写进生成文件的注释里，以后在 Hackintool 里一眼能对上是哪个口。
7. 把**用不到**的端口关掉（直接输入端口号列表，如 `1,2,11,12`），便于控制在 macOS 的
   15 端口上限之内。
8. 按 `K` = **Build Kext** → 在 `dist\` 里生成 **`UTBMap.kext`**。
9. 替换进 EFI（**这一步可以由我来做**）：
   * `EFI/OC/Kexts/` 里放新的 `UTBMap.kext`；
   * **删掉 / 禁用 `UTBDefault.kext`**（官方要求，见上）；
   * `USBToolBox.kext` 必须在 `UTBMap.kext` **之前**加载（你现在的顺序 6 → 10 是对的）；
   * `config.plist` → `Kernel > Quirks > XhciPortLimit` 必须 **Disabled**（实测你已经是 `False` ✅）；
   * 如果你有 `SSDT-RHUB.aml` 之类改 USB ACPI 的表，要停用它 —— 你**没有**，只有
     `SSDT-USB-Reset.aml` / `SSDT-USBX.aml`（这两个与之无关，保持启用）。
10. 重启验证：进 macOS 后用 Hackintool → USB 标签页，把 U 盘逐个插一遍，看端口是否都在、
    速率是否正确（USB2 设备验 USB2、USB3 设备验 USB3、USB-C 验正反两面）。

## 4. 本机的控制器清单（映射时对照用）

来自 1401 的 Windows 侧报告（`Report.json → USB Controllers`），4 个控制器：

| PCI ID | ACPI 路径 | 说明 |
| --- | --- | --- |
| `1022-15B6` | `\_SB.PCI0.GP17.XHC0` | AMD USB 3.10 |
| `1022-15B7` | `\_SB.PCI0.GP17.XHC1` | AMD USB 3.10 |
| `1022-15B8` | `\_SB.PCI0.GP19.XHC2` | AMD USB 2.0 |
| `1022-43FC` | `\_SB.PCI0.GPP7.UP00.DP60.XH00` | AMD USB 3.20（CPU 直连，5030 系列 Promontory 之外的 Prom21 通道） |

## 5. 谁做什么

| 步骤 | 谁 |
| --- | --- |
| 下载/校验工具、解压 | ✅ 已完成（`tools/usbtoolbox/`） |
| 跑工具、逐个插拔端口、定类型 | ✅ 已完成（用户 2026-10-08 22:47 生成） |
| 把生成的 `UTBMap.kext` 装进 EFI、禁用 `UTBDefault.kext`、改 config、跑 ocvalidate | ✅ 已完成（见 §6） |

## 6. 2026-10-08 22:50 已落地：新 map 装好，冲突的 UTBDefault 已关

### 生成结果的核对（生成文件 vs 意图，逐口核对通过）

| 控制器 | IONameMatch | 端口数 | 说明 |
| --- | --- | --- | --- |
| `XH00`（`1022:43FC`，USB 3.20） | XH00 | 7 | HS01/HS02=Type-A、HS03=Type-A、**HS04=Internal(255)**、HS05=Type-A、SS01/SS02=Type-A |
| `XHC0`（3.10） | XHC0 | 4 | HS01=Type-A、**HS02=USB-C(9)**、SS01=Type-A、**SS02=USB-C(9)** |
| `XHC1`（3.10） | XHC1 | 4 | 全 Type-A |
| `XHC2`（`1022:15B8`，USB 2.0） | XHC2 | 1 | HS01=Type-A |

* `HS04 = Internal(255)` = `Wireless_Device` = **RZ616（MediaTek MT7922）蓝牙**（Windows 设备树实证：
  `USB\VID_0E8D&PID_0616&MI_00`）。✓
* `SS02/HS02 = USB-C(9)` = 小米 15S Pro 实测连过的那个 Type-C（`USB 3.1 Gen 2` = 10 Gbps）。
  **双重独立印证**：① USBToolBox 从 Windows 设备树读到 ACPI 名并归到 `XHC0`；
  ② Mac 端早先那张残缺表也把 `Xiaomi 15S Pro` 标在 `XHC0`、`UsbConnector=9`（USB-C）。两者一致。✓
* 每个控制器端口数 7/4/4/1，全部 ≤ macOS 的 15 端口上限 ✓
* 换了匹配方式：旧表用 `pci1022,43fc`，新表用 ACPI 名 `XH00` —— 与报告里的
  `\_SB.PCI0.GPP7.UP00.DP60.XH00` 一致 ✓

### 已做的改动（两处 EFI 同步，全部可回滚）

| 位置 | 改动 | 备份 |
| --- | --- | --- |
| 启动盘 `D:\EFI\OC\Kexts\UTBMap.kext` | 换成 USBToolBox 生成的表（`Info.plist` 5,653 B / sha `0cd3a353…`） | `D:\EFI\OC\Kexts\_backup-20261008-224904\UTBMap.kext.old-macside` |
| 启动盘 `D:\EFI\OC\config.plist` | **`UTBDefault.kext` Enabled True → False** | `config.plist.bak4-20261008-224907`（= Mac 端那版 69,786 B） |
| 内置 ESP `\EFI\OC\` | 同样两份一起同步（sha 与上表一致） | `\EFI\OC\_backup-20261008-224929\`（含 `config.plist.esp-old`、`UTBMap.kext.esp-old`） |

`ocvalidate`（1.0.9）：**No issues found**。断言校验通过：
`UTBDefault=False`、`UTBMap=True`、加载顺序 `USBToolBox(6) → UTBMap(10)`、`XhciPortLimit=False`，
以及历史补丁全在（`DevirtualiseMmio=True`、`MmioWhitelist=[0xFD000000]`、`AppleDebug=True`、
`ApplePanic=True`、`Target=67`、`PickerMode=External`+OpenCanopy、`prev-lang:kbd=zh-Hans:252`）。
ESP 的 `EFI\Microsoft\*`（`bootmgfw.efi` / `BCD` / `EFI\Boot\BOOTx64.efi`）**原样未动**。

### ⚠️ 一个待你确认的点：第二个 Type-C 口现在是**关着**的

USBToolBox 自己也把 USB 3.20 控制器的 **Port 1 / Port 6** 猜成 `Type C - with switch`
（互为伴随），但它们在列表里是 `[ ]` 未勾选 → **等于从映射里剔除**。磁盘侧解码出的 BIOS
`Setup` 变量里同时存在 `TYPE-C` 与 `F_USBC` 两个端口开关，说明这块板子有两个 Type-C
（一个后置、一个前面板插针）。**如果那个口你要用，需要勾上 10,15 并设 `T:10:9` `T:15:9`
后重新 Build Kext。** 不用就保持现状（还能省端口名额）。

### 另外记录：盘符与 config 的变迁（避免以后再踩）

* 这台机器插拔设备后**盘符会重排**：1401 启动盘从 `E:` 变成了 `D:`，原来占 `D:` 的 Ventoy
  exFAT 盘已被拔掉。脚本一律按**卷标 + 卷 GUID**认盘，别硬编码盘符。
* 启动盘上的 `config.plist` 被 **Mac 端 1401 改写过**（69,786 B，FAT32 时间戳为 UTC，
  显示成 `10/7 18:08` 实为 `10/8 02:08` 本地；旧版被它备份为 `oldConfig.plist`）。
  改前必读当前文件、别拿早先的 `.bakN` 当现状。
