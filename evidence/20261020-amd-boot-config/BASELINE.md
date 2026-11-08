# AMD 适配 Wave 0 基线（2026-10-20）

采样窗口：2026-10-20 12:39–12:51 CST。机器：Ryzen 7 9700X / 48 GB / macOS 15.8.1 (24H32) / OpenCore。
12:39–12:51 的基线采样阶段只做只读采样、卷外备份、离线恢复演练；该阶段**未写 ESP、未改 BIOS、
未重启、未加载/卸载第三方 kext**。

> **2026-10-21 04:00 更新**：R1 **PASS**——baseline `EFI/` 写回后已在 03:55:25 CST
> 从内置 ESP 成功启动；EFI 436/436 与 baseline 0 差异。R6 也已由用户 BIOS 目视确认并通过。
> **M0 通过（R1+R6）**；R2/R10 仍开放。
> 2026-10-21 04:03 另开了 S 单文件窗口：只把 UTBMap `Info.plist` 从 `f9b484e8…`
> 改为 `83f47223…`；06:35 启动报 prelink `Invalid Parameter`，**S runtime FAIL**；
> 06:39 已单文件回滚到 `f9b484e8…`，config/OpenCore/BOOT 未变。
> 06:43 又部署了 XML 修复版 S2 `998c7fbb…`（5,665 B），只改 UTBMap 一个文件；
> **2026-10-21 07:55:46 启动 runtime PASS**：日志出现 `Prelinked injection UTBMap.kext ... Success`
> 和 `UTBMap.kext v1.1`，XH00 7 口、其他控制器 4/4/1；待 100 插拔/30min 音频压力验收。
> 详见 `RESTORE-DRILL.md`。本文件其余内容是 12:39–12:51 的基线快照。

## 1. 结论先行

1. **实际启动源**：内置 NVMe `disk0`（ZHITAI TiPro9000 2TB）的 ESP `disk0s1`，
   GPT type `C12A7328-F81F-11D2-BA4B-00A0C93EC93B`，分区 UUID `6D351E57-…2F8C`，
   引导文件 `EFI/OC/OpenCore.efi`。不依赖卷名判断。
2. **当前 bootloader**：OpenCore `DBG-107-2026-10-14`（872,448 B），配置 56,063 B；
   本次启动日志与 `kern.boottime` 对齐。
3. **备份**：`private-backups/esp-disk0s1-20261020-1146/`，EFI 436 文件 + ESP 根目录 335 文件，
   逐文件 SHA256 771/771 通过；另有整分区 UDRO 镜像（sha256 `b4ba67b0…182e7a`）。
4. **三位一体不一致**：仓库 `efi/` 是 2026-10-07 的旧快照；实际 ESP 已被 10-10…10-19 多个会话改过；
   运行态 boot-args 与实际 ESP 完全一致，但运行态还加载了 5 个不在 ESP 的 AuxKC 驱动。
5. **IOMMU 口径**：2026-10-21 用户 BIOS 目视确认 **Enabled**；10-08 的 “Disabled” 是解码/快照误差。
   IOMMU Enabled 满足 AppleVTD 的前提之一，但不等于 AppleVTD 已可用。
6. **阻塞项**：R1、R6 均已 PASS/关闭，M0 通过；R2 DBG-107/schema、R10 ESP 日志占用仍开放。

## 2. 实际启动源（证据）

### 2.1 磁盘 / GPT / 卷

| 项 | 值 | 证据 |
|---|---|---|
| 物理盘 | `disk0`，ZHITAI TiPro9000 2TB，internal NVMe，GPT | `diskutil list` / `system_profiler SPNVMeDataType` |
| ESP | `disk0s1`，209,715,200 B，offset 1,048,576 B | `diskutil info disk0s1` |
| GPT type | `C12A7328-F81F-11D2-BA4B-00A0C93EC93B` | IORegistry `Content Hint` |
| 分区 UUID | `6D351E57-EE46-4CE7-AACC-0BDA08532F8C` | `diskutil info` / IORegistry |
| FAT 卷 UUID | `451CAD3F-44A7-328F-8D07-9402D4781951` | `diskutil info` |
| 卷标 | `OPENCORE`（仅标签，判据不用它） | `diskutil info` |
| GPT 属性 | `0x8000000000000000`（不自动挂载） | IORegistry |
| macOS | APFS container `disk1`，`disk0s5` 1000 GB，`Macintosh HD` | `diskutil list` |

### 2.2 为什么确定走的是这块 ESP

- OpenCore 日志 `opencore-2026-10-20-034624.txt`：
  `OC: Booter path - EFI\OC\OpenCore.efi`、`OC: Absolute booter path - EFI\OC\OpenCore.efi`、
  `OC: Loaded configuration of 56063 bytes`、`OC: OpenCore DBG-107-2026-10-14`、
  `Boot timestamp - 2026.10.20 03:46:24`。
- 03:46:24 UTC = 11:46:24 CST；`kern.boottime` = **11:46:52 CST**，相差 28 秒。
- 该日志位于 `disk0s1` 根目录；OpenCore 把日志写在自己启动时所在的卷。
- 外置 `disk2`（`MTKCAP`）是 Linux/Alpine 盘（`efi/boot/bootx64.efi` + `grub.cfg`），
  `BOOTED.txt` 停在 2026-10-18 17:31 UTC，不是本次 macOS 启动源。

### 2.3 当前关键哈希

| 文件 | sha256 | 大小 |
|---|---|---|
| `EFI/OC/config.plist` | `3dad2596c1b3457c956e0c1308a560a9ca1cd9cc9c1cfd705963ace3f57e4292` | 56,063 B |
| `EFI/OC/OpenCore.efi` | `7754f6d66b23ad2410e1a6ef9ee63b7d527ce2dd8e8dbf58bca9c657f87273c8` | 872,448 B |
| `EFI/BOOT/BOOTx64.efi` | `d3a4854c82f183e2a73976e69490ce334bbaf8055c8efaa78f4d4626fddb10f3` | 53,248 B |

## 3. 仓库 / ESP / 运行态三位一体

| 维度 | 仓库 `efi/`（2026-10-07） | 实际 ESP（2026-10-20） | 运行态 |
|---|---|---|---|
| `config.plist` | `05919f3b…d4dce` / 42,980 B | `3dad2596…e4292` / 56,063 B | 日志载入 56,063 B |
| `OpenCore.efi` | `a3cfa9ac…c00ae` / 630,784 B（REL-109） | `7754f6d6…273c8` / 872,448 B（DBG-107） | 日志报 DBG-107 |
| `BOOTx64.efi` | `19fa90b9…f6474` / 24,576 B | `d3a4854c…b10f3` / 53,248 B | — |
| Kernel/Patch | 27 | 30（USB 2 条 disabled，timer 1 条 enabled） | USB 补丁未生效：XHC0 只建 15 口 |
| Kernel/Add | 10 | 22（18 enabled） | ESP 内 enabled 的基本已加载；另有 5 个 AuxKC 驱动 |
| boot-args | 106 字符，旧 NVIDIA 基线 | 389 字符，含 MTK/CX3/音频/显示开关 | 与 ESP **逐字符一致**（389/389） |
| SMBIOS | `iMacPro1,1` | `MacPro7,1` | `MacPro7,1` |
| LauncherOption | `Disabled` | `Full` | — |
| Debug.Target | 67 | 71 | 有 `opencore-*.txt` 落盘 |
| USB | `UTBDefault.kext` | `UTBMap.kext` + `XHC2 bus-number=2` | XHC0 `port-count=0x11`，实际建 15 口（HS11/HS12 缺失） |

运行态额外加载（不在 ESP `Kernel > Add`）：
`NVRM` / `NVAccel` / `NVRMFB` / `NVRMAGDC`（2.3.7）与 `Cx3Mlx4`（0.44.36）——
这些走 `/Library/Extensions` + AuxKC，改 ESP 不会替换它们。

**结论：仓库 `efi/` 只能当历史参考，绝不能用来覆盖 ESP。**

## 4. 备份与恢复演练

路径：`private-backups/esp-disk0s1-20261020-1146/`

### 4.1 只读读取方法（可复现）

- `disk0s1` 当时未挂载；普通用户 `diskutil mount readOnly disk0s1` 失败（ESP 由 root 拥有）。
- 改用 `hdiutil create -srcdevice /dev/disk0s1 -format UDRO` 读取整分区（409,600 扇区），
  再 `hdiutil attach -readonly -nobrowse` 挂载镜像；全程不写源分区。
- `diskutil verifyVolume disk0s1` 的 `fsck_msdos -n` 只读检查：exit 0，
  1,035 files / 46,698 KiB free；FAT 的 “dirty” 标志保持原样，未做修复。

| 项 | 结果 |
|---|---|
| 文件级 | `EFI/` 436 文件（69,117,595 B）+ `root/` 335 文件（84,207,950 B） |
| 逐文件清单 | `MANIFEST.sha256` 771 行；`shasum -c` **771/771 OK** |
| Kext | `KEXTS.sha256` 64 文件；**64/64 OK** |
| 权限/大小 | `TREE.txt` 771 行（FAT 无 POSIX 权限，备份统一 700） |
| 整分区镜像 | `esp-disk0s1-20261020-1240-UDRO.dmg`，178,430,242 B，sha256 `b4ba67b024b8ea3b4fe4122ed4cf95336961f016246affe9d7cbbda5e0182e7a` |
| 独立复核 | 12:40、12:43、12:47 三次从 `/dev/disk0s1` 取镜像，EFI 436 文件 **0 差异** |
| 实时直比 | 12:49 用 `sudo mount -t msdos -o ro /dev/disk0s1 /tmp/esp-live` 只读挂载实时 ESP，与备份逐文件比对 **436/436、0 差异**；随后卸载 |
| 写入预演 | 12:50 把备份 `EFI/` 写进一个 200 MB 临时 FAT32 镜像，逐文件校验 **436/436 OK**；未碰真实 ESP |
| 镜像复核 | 重新挂载备份镜像，436/436 文件哈希一致、关键文件一致 |

ESP 容量：总 200,706 KiB，已用 154,008 KiB（77%），剩 46,698 KiB。`root/` 里有 **321 个
`opencore-*.txt`，共 80.2 MiB**；`Debug.Target=71` 每次启动都写 256 KiB 日志，
长期有填满 ESP 的风险（约 180 次启动量级）。

**恢复演练状态：离线通过；真机覆盖 + 重启未做**（本会话禁止）。
步骤见 `RESTORE.md`，需要用户批准窗口与救援 U 盘。

## 5. 补丁注册表摘要

完整表见 `evidence/07-patch-registry.md`。

- **Kernel/Patch 30 条**：27 条 AMD_Vanilla/PAT/调度基础补丁；2 条 USB 15→17（当前 `Enabled=false`）；
  1 条 9700X LAPIC 25 MHz 标定（`Enabled=true`，`MinKernel=MaxKernel=24.6.0`，`Limit=51`）。
- **Booter/Patch 1 条**：`macOS → hacOS`，`Enabled=false`，不影响启动。
- **USB 校验器**：`tools/research/usbpatch-check.py` 对 24H32 KC：
  - `AppleUSBHostPort::setPortLocation` 命中 1 处 @ `+54`；
  - `AppleUSBXHCI::createPorts` 命中 1 处 @ `+1344`；
  - locationID 冲突 0；`VERDICT PASS`；
  - 但两条在 config 里是 disabled，所以运行态只建 15 口。
- 其余 Kernel/Patch **没有通用校验器**，本次未重新验证命中；表里保留 Base/Min–Max/Find/Replace 供后续补。
- 关键 Booter Quirks：`DevirtualiseMmio=true`、`DisableVariableWrite=true`、
  `RebuildAppleMemoryMap=true`、`SetupVirtualMap=false`、`ProvideCustomSlide=true`、
  `SyncRuntimePermissions=true`、`ResizeAppleGpuBars=0`。
- 关键 Kernel Quirks：`ProvideCurrentCpuInfo=true`、`DisableIoMapper=false`、
  `DisableLinkeditJettison=true`、`PanicNoKextDump=true`、`XhciPortLimit=false`。

## 6. BIOS 快照摘要

完整表见 `evidence/10-bios-snapshot.md`。

| 项 | 判断 | 依据 |
|---|---|---|
| SVM | **Enabled** | CPUID `80000001.ecx bit2=1`，`8000000A` 可用 |
| IOMMU | **Enabled** | 2026-10-21 用户 BIOS 目视 + PCI `1022:14d9` @ `00:00.2` |
| Above 4G | **Enabled** | BAR1 8 GiB @ `0xFA00000000` |
| ReBAR | **Enabled** | `bar1-mb=8192`，`nvrm-bar1-placed=1073741824000` |
| SR-IOV | **Disabled** | 2026-10-21 用户 BIOS 目视 |
| XHCI Hand-off | **不适用** | 2026-10-21 用户 BIOS 目视：BIOS 没有该选项 |
| Secure Boot / CSM / Fast Boot | **Disabled** | 2026-10-21 用户 BIOS 目视 |

**IOMMU 冲突已关闭**：2026-10-21 用户 BIOS 目视确认 Enabled；
10-08 的 `Setup+0xA8=Disabled` 是解码/快照误差。不要按旧结论去关 IOMMU。

## 7. 风险 / 阻塞

| ID | 风险 | 影响 | 处理 |
|---|---|---|---|
| R1 | 真机恢复演练：**PASS**（写回 + 重启验收通过） | 无 | 2026-10-21 03:55:25 从内置 ESP 启动；R1 关闭 |
| R2 | bootloader 是 **DBG-107**，config 是 1.0.9 时代（含 `HideVerbose` 字段） | 启动日志已报 4 次 `No schema for HideVerbose`；本机无 `ocvalidate` | 先冻结；是否回退 REL-109 另立决策，不在本会话改 |
| R3 | 仓库 `efi/` 是 10-07 旧快照 | 误覆盖会丢 USB/WiFi/音频/NVIDIA 改动 | 禁止用仓库覆盖 ESP；以本备份为准 |
| R4 | USB 补丁 disabled，但 config 仍保留两条 | 容易误判“已开 17 口”；实际 HS11/HS12 缺失 | USB owner 按 15 口基线继续研究；开关前走冻结窗口 |
| R5 | `SimpleMtkBtFw` enabled 但未加载 | 蓝牙仍不可用 | 属蓝牙会话，不在本基线修 |
| R6 | BIOS 目视：**PASS**（IOMMU Enabled；SR-IOV Disabled；XHCI Hand-off 无此选项；Secure Boot/CSM/Fast Boot 均 Disabled） | 无 | 2026-10-21 用户目视确认；R6 关闭；M0 通过 |
| R7 | 并行会话仍可能写 ESP | 基线漂移、故障不可归因 | 协调会话广播 `FREEZE-WINDOW.md`；本会话不联系其他会话 |
| R8 | 备份含 SMBIOS 身份 | 误提交会泄露序列号/UUID | 备份在 `private-backups/`，已加入 `.git/info/exclude`；不要推送 |
| R9 | boot-args 是多会话叠加态（`-liludbg`、`-smbtcompatdbg`、`cx3debug=1`、大量 `mtk*`） | 不是最小日常配置，可能影响性能/时序归因 | 先冻结；清理需逐项 A/B，另立窗口 |
| R10 | 2026-10-21：ESP 约 80% 使用；323 个日志 / 84,672,512 B（80.8 MiB），每次启动再写 256 KiB | 长期可能填满 ESP，导致写入失败/启动异常 | 另立窗口清理旧日志；不要把日志清理和 EFI 语义变更混在一轮 |

## 8. 冻结状态与下一步

- 冻结协议：`FREEZE-WINDOW.md`；登记表：`WINDOW-REGISTER.md`。
- 当前只允许 `amd-boot-config` 写 ESP；其他会话只产出 `inbox/` 候选。
- 下一步（需协调会话/用户）：
  1. 广播冻结，收回其他会话的 ESP 直写；
  2. R1、R6 已关闭，M0 已通过；
  3. 决定 DBG-107 去留（R2），单独窗口；
  4. 日志清理另立任务（R10）；
  5. USB owner 按 15 口基线继续（R4）；
  6. 更新计划正文的状态（由协调会话做，本会话不改计划）。

## 9. 证据索引

- `evidence/00-host.txt`、`01-disk.txt`、`02-nvram.txt`、`03-kmutil-showloaded.txt`
- `evidence/04-ioreg-boot.txt`、`05-cpuid.txt`、`11-boot-evidence.md`
- `evidence/05-hashes-esp.txt`、`06-hashes-repo.txt`
- `evidence/07-patch-registry.md`、`08-config-diff.md`、`09-kext-runtime.md`
- `evidence/10-bios-snapshot.md`、`evidence/usbpatch-check.json`
- `evidence/12-config-lint.txt`（`plutil -lint` OK；`ocvalidate` 未安装）
- `private-backups/esp-disk0s1-20261020-1146/{MANIFEST.sha256,TREE.txt,VERIFY.txt,KEXTS.sha256}`
- `RESTORE.md`、`FREEZE-WINDOW.md`、`WINDOW-REGISTER.md`
