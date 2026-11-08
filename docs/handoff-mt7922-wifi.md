# 交接：RZ616 / MT7922 的 macOS 驱动（WiFi + 蓝牙）

> 新会话从这里开始。蓝牙部分**依赖 USB 端口补丁**（见 `docs/handoff-usb-portlimit.md`）；
> WiFi 可以独立推进。完整证据见 `docs/device-driver-feasibility.md` §2、§5、§7、§8。

## 1. 硬件事实（实测）

| 项 | 值 |
| --- | --- |
| WiFi | `14c3:0616` = MediaTek **MT7922**（RZ616），subsystem `0x0616` |
| 位置 | PCI `0000:06:00.0`，链路 **5.0 GT/s ×1**，挂在 **PCIE4 插槽**（bus `0000:00:02.1`）后面 |
| 形态 | 单通道 **M.2 模块** ⇒ **可换**（需物理确认是标准 E-key 座还是焊死） |
| 蓝牙 | USB `0e8d:0616`（MediaTek Wireless_Device，类 `0xE0`），**在 XH00 的 Port 16** |

## 2. 两条路的现状（都已查清）

### 路线 A：SimpleMtkWlan（现成项目，但天花板是"能扫描"）

`laobamac/SimpleMtkWlan`，已 clone 到 `tools/research/mtkwlan/`。

| 项 | 实况 |
| --- | --- |
| 作者自述（issue #2 原文） | *"At present, **only the firmware download and WiFi scanning** of MT7922/MT7921 network card are realized … I am currently a senior high school student, so there is no expected arrival time for the project."* |
| 仓库 | **总共 2 个提交**，最后 **2026-06-27**，无 release/tag |
| 已知 bug | issue #3：*"WiFi scans OK but connection fails / crashes"* |
| 设备匹配 | `IOPCIPrimaryMatch = 0x796114c3 0x060814c3 0x792214c3` —— **不含我们的 `0x061614c3`** |
| 依赖框架 | `com.apple.iokit.IONetworkingFamily` ⇒ **以太网风格**（itlwm 那条路），**不是** `IO80211Family` |
| 固件 | 在仓库内（`include/FwBinary.cpp`，12 MB；含 `mwx-mt7922_ram_code_1`） |

### 路线 B：走 `IO80211Family`（能拿 AirPort 特性，但要自己写驱动）

Intel 已被验证可行 —— **AWDL 不是"只有 Broadcom 硬件能做"**，而是"需要走 `IO80211Family` 的驱动 +
OCLP root patch"。证据：

- EliteMacx86 指南原文：AirportItlwm *"has several advantages (such as iServices, Location services and
  **partial AirDrop**)"*，并要 *"spoof the Intel WiFi device to make it see as a Broadcom WiFi device to
  allow the root patching"*（OCLP 只看 `IOName`）。
- InsanelyMac 专帖：「Experimental Fork of OCLP 3.0.0 Nightly – **modern Wi-Fi, AWDL** and AppleHDA
  Fully Working under Tahoe」。
- 反例（说明还不稳）：itlwm issue #1062 同配置下 `ifconfig awdl0` 仍**不存在** ⇒ AirDrop/个人热点/连续互通相机不可用。

| 项目 | 框架 | 能拿 AirPort 特性 |
| --- | --- | --- |
| itlwm | `IOEthernetController` | ❌ |
| AirportItlwm | **`IO80211Family`** | ✅（Intel 靠它） |
| SimpleMtkWlan | `IONetworkingFamily` | ❌ |

## 3. 为什么必须 OCLP root patch（本机实测证据）

macOS 15.8.1 上查：

```
/System/Library/Extensions/IO80211Family.kext/Contents/
    Info.plist  _CodeSignature  version.plist      ← 只剩这些
    PlugIns/                                        ← 目录在，但 0 个条目！
CFBundleVersion = 1200.13.1
```

逐个确认**全部缺失**：`AirPortBrcmNIC`、`AirPortBrcm4360`、`Apple80211`、`IO80211FamilyLegacy`。
Wi-Fi 驱动的新家在 DriverExtensions：

```
/System/Library/DriverExtensions/com.apple.DriverKit-AppleBCMWLAN.dext   ← Broadcom 专用，DriverKit
```

⇒ **Sequoia 起 Apple 把 Wi-Fi 从 IO80211Family 的 kext 插件整体迁到 DriverKit，且只做了 Broadcom。**
所以要跑 IO80211 系的第三方驱动，必须把**旧版 IO80211Family（含插件）+ 降级后的 IOSkywalkFamily**
装回**签名的只读系统卷** ⇒ 只能靠 OCLP 式 root patch（并放宽 SIP）。伪装成 Broadcom 是**另一件事**
（让 OCLP 愿意打补丁），两者别混。

## 4. 参考样板：AirportItlwm（已 clone 到 `tools/research/itlwm/`）

| 项 | 值 |
| --- | --- |
| 声明依赖 | `com.apple.iokit.IO80211Family: 1200.12.2b1`（本机是 `1200.13.1`）+ `IOSkywalkFamily` |
| 私有头 | `include/Airport/` 下 28 个头：`IO80211Controller.h`、`IO80211ControllerV2.h`、`IO80211SkywalkInterface.h`、`apple80211_var.h`、`apple80211_ioctl.h`、`Apple80211.h` … |
| **AWDL** | `AirportItlwm/AirportAWDL.cpp` **只有 58 行** ⇒ AWDL 主要靠框架，驱动侧只需少量配合（**降低了 MT7922 版在 AWDL 上的难度**） |
| 许可 | GPLv2（若复用其私有头需接受其约束） |

## 5. 本机已具备的构建能力（不用 Xcode，已实测通过）

用 `tools/research/build-mtkwlan.sh` 实测：**54/54 源文件编译通过并链接成功**
（`Mach-O 64-bit kext bundle x86_64`，`KEXTBUNDLE`，1851 符号）。产物与设备 ID 扩展版在 `/tmp/mtkwlan-build/`
（会因 /tmp 清理丢失，脚本可重建）。

**三个必踩的坑（已解决，直接照抄）**：
1. 必须 `-DKERNEL -D__KERNEL__`（否则 `PCIDriverKit/IOPCIDevice.h` 走 DriverKit 分支报错）。
2. `-nostdinc` 下**所有 `.c` 也要按 C++ 编**（`-x c++`）—— `mtk80211/compat.h` 无 `__cplusplus` 保护却包含 IOKit C++ 头。
3. `include/` 需要 `mtk80211/linux` 在搜索路径里（提供 `types.h`）；要 `-include SimpleMtkWlan/PrivateSPI.pch`。

另外 `kmutil print-diagnostics` 只查 kext 级依赖、**不查单个符号**（会报 `Dependencies: OK` 骗人）——
查符号用 `kmutil libraries -p <kext>`。

## 6. 蓝牙（与 WiFi 一起做，但前置是 USB 补丁）

| 项 | 值 |
| --- | --- |
| 项目 | `laobamac/SimpleMtkBtFw`（GPL-3.0），做 MediaTek **USB 蓝牙固件装载** |
| 覆盖 | MT7921 / MT7922 / MT7922A |
| 匹配条件 | `IOProviderClass = IOUSBHostDevice`、`bDeviceClass = 0xE0`、`bDeviceSubClass = 1`、`bDeviceProtocol = 1`、`idVendor = 0x0E8D` |
| 我们的设备 | `0e8d:0616` —— **VID 符合，类不符合（见下方更正）** |
| 加载顺序 | `Lilu.kext` → `SimpleMtkBtFw.kext` → `SimpleMtkBtPatcher.kext` |
| 限制 | 只能 OpenCore 注入；不能再注入别的蓝牙固件加载器（会抢同一个 USB 设备）；实验性 |

⚠️ **前置条件**：蓝牙在 **XH00 的 Port 16**，被 15 端口上限挡在门外 —— **必须先解决端口限制**
（见 `docs/handoff-usb-portlimit.md`），否则连枚举都到不了。
`system_profiler` 里那行 `BCM_4350C2 / Address: NULL / Firmware v0 c0` 是 macOS **找不到真硬件时的兜底占位**，不是识别到了什么。

### 6.1 更正（2026-10-11，已实测证伪上表的"类都符合"）

**USB 端口补丁已经生效**，设备现在**确实枚举到了**（`Wireless_Device`，三个接口
`BT_ACL_If@0` / `BT_SCO_If@1` / `BT_ISO_If@2` 都在），所以"连枚举都到不了"这一条已经过去。

但上表"VID 与类都符合"是**错的**，原因是把两个层级的字段当成了同一个：

| 层级 | 我们的实际值 | `SimpleMtkBtFw` 的 Generic 匹配要求 |
| --- | --- | --- |
| **设备级**（`IOUSBHostDevice`，personality 的 `IOProviderClass`） | `bDeviceClass=0xEF`, sub=2, proto=1 | `bDeviceClass=0xE0`, sub=1, proto=1 |
| **接口级**（`IOUSBHostInterface`） | `bInterfaceClass=0xE0`, sub=1, proto=1 ✓ | — |

`0xEF/2/1` 是标准 IAD 复合设备。`0e8d` 名下只有两条 personality：
`0x0608`（VID/PID 对，我们是 `0x0616`）和 Generic（要求**设备级** `0xE0`）——
**两条都不匹配 ⇒ kext 不会 attach，固件不会下载。**

对照 Linux `btusb.c:648`，通用规则用的是 `USB_VENDOR_AND_INTERFACE_INFO(0x0e8d, 0xe0, 0x01, 0x01)`，
即 vendor + **接口类**，与我们设备形态一致。所以可行做法是加一条只按 `idVendor=0x0e8d / idProduct=0x0616`
匹配设备级对象的 personality（对 IAD 设备而言没有设备类可依赖）。

完整证据链（含 41 条 personality 逐条比对、运行时 devId 判定链、patcher 的风险评估）见
`outputs/20261010-mt7922-wifi/bt-recon-20261011.md`。要点：

- 运行时流程 `BtMtk.cpp:101` 的 `switch (devId)` 只接受 `0x7922`/`0x7961`；RZ616 是 MT7922 硅片，
  预期读回 `0x7922`（**推断，未实测**）；固件 `BT_RAM_CODE_MT7922_1_1_hdr.bin` 已内嵌在 kext 里。
- 架构上没问题：`SimpleMtkBtFw.cpp` 下载完固件后 `close()` 设备并改名
  `"Bluetooth USB Host Controller"`，把设备交回苹果的 `IOBluetoothFamily`（README 也是这么说的）。
- **`SimpleMtkBtFw.kext` 本身不依赖 Lilu**，只有 `SimpleMtkBtPatcher.kext` 依赖；
  而 patcher 会用 Lilu 路由 `IOUSBHostDevice::deviceRequest`（本机所有 USB 设备类请求都经过）——
  **建议先只上 Fw.kext，确认可行后再单独决定 patcher。**
- 本机只有 CommandLineTools、没有完整 Xcode，`xcodebuild` 不可用；上游有官方 1.0.0 发布包，
  里面是现成 kext，改 Info.plist 即可，不必编译。

## 7. 用户已定的方向

| 项 | 决定 |
| --- | --- |
| MT7922 | **希望完美适配 + 用上 AirPort 等功能** ⇒ 对应路线 B（写 IO80211 系驱动） |
| 蓝牙 | **等 USB 补丁做好后，与 MT7922 WiFi 会话一起开发** |
| 可选替代 | 换 **Intel AX210/AX200** 走 `AirportItlwm`（成熟、有大量样板）；或换 **Broadcom**（原生、免 root patch） |

## 8. 需要一并评估的共存问题

OClP root patch 会**修改系统卷**（并放宽 SIP 的一部分）。本机已装 **NullMoth NVIDIA 驱动**
（4 个 kext 在 `/Library/Extensions`，走 `amfi_get_out_of_my_way` 等 boot-args，当前 SIP 已放宽到 `0xA43`）。
⇒ **必须验证**：OCLP 的 root patch 与现有 NVIDIA 驱动 / 当前 `csr-active-config` 不冲突。
建议先只读评估（OCLP 需要什么 SIP 位、会改哪些路径），再决定是否执行。

## 9. 本会话踩过的坑（省时间）

- `SimpleMtkWlan` 的 `Info.plist` 带 Xcode 占位符（`$(PRODUCT_BUNDLE_IDENTIFIER)`、`$(MODULE_VERSION)`…），
  手工组装 bundle 时必须替换，否则 `CFBundleVersion` 非法、kext 加载不了。
- 未签名的自建 kext 会报 `Bad code signature`（预期），`kmutil print-diagnostics` 会一并提示属主错误（`chown` 到 0:0 即可）。
- ⚠️ `kmutil load` 会写三处持久状态（`AuxKC` 的 `instructions.plist`+`StashedExtensions`、两个 `UAKL.plist`、
  `KextPolicy` 数据库）。清理**只删自己新增的那条**，别用 `kmutil clear-staging`。备份见 `tools/backup-*`。
