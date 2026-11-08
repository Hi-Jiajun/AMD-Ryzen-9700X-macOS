# OpenCore 1.0.7 → 1.0.9 变更对当前 config 的影响清单与判定

> 编制日期：2026-10-23 CST  
> 任务暗号：`OCDELTA-1079-LUNA`  
> 目标平台：AMD Ryzen 7 9700X / 铭瑄 MS-Terminator B850BKB WIFI / SMBIOS MacPro7,1  
> 系统版本：macOS Sequoia 15.8.1 (24H32, Darwin 24.6.0)  
> 当前运行引导器：OpenCore `DBG-107-2026-10-14`（sha256: `7754f6d66b23ad2410e1a6ef9ee63b7d527ce2dd8e8dbf58bca9c657f87273c8`，872,448 B）  
> 当前配置：`EFI/OC/config.plist`（56,573 B，sha256: `bb95cd36c24f89bcfff5bc4f51d414ad169f43f486d86f9f629bcf20fd222812`）  
> 源码与变更基准：`outputs/20261017-audio-current/oc-tools-src/`（commit `81ecf1a7e75c318dfc859ddabe5b91f4af757231`，2026-10-10）

---

## 1. Changelog 全量变更提取与影响面总表

依据本地材料 `outputs/20261017-audio-current/oc-tools-src/Changelog.md` 提取 1.0.7、1.0.8 以及 1.0.8 之后到当前源码树顶部（master HEAD，版本标记为 1.0.9）的全部变更。

> **特别说明（查证事实）**：  
> 在本地 `Changelog.md` 中，第 3 行为 `#### v1.0.8`，**未在本地材料中找到独立命名为 `#### v1.0.9` 的章节标题**。  
> 本地源码树 `Include/Acidanthera/Library/OcMainLib.h` 第 33 行明确定义 `#define OPEN_CORE_VERSION  "1.0.9"`，`Utilities/ocvalidate/ocvalidate.c` 第 86 行亦提示兼容 OpenCore 1.0.9，且 `Docs/Differences/Differences.tex` 第 157 行记录了 `Reference Manual (1.0.8 → 1.0.9)` 的版本递增。这表明当前主线开发分支处于 1.0.9 开发周期，`Changelog.md` 中 `#### v1.0.8` 章节汇集了 1.0.7 正式版发布后至 1.0.8/1.0.9 主线的全部累计变更；1.0.8 之后的最新代码变更以 git commit 形式存在（commit `81ecf1a`）。

| 版本 | Changelog.md 行号 / 原文条目 | 涉及模块 / 影响面 | 对本机当前环境影响判定 |
|---|---|---|---|
| **v1.0.8** | **第 4 行**：`- Fixed macrecovery utility on Windows, thx @pzhlkj6612` | `Utilities/macrecovery`（Windows 恢复镜像下载工具） | **完全无关**（离线 Python 辅助工具，不参与引导运行时） |
| **v1.0.8** | **第 5 行**：`- Added launch helper for macrecovery utility on Windows, thx @aayushprsingh` | `Utilities/macrecovery`（Windows 批处理启动脚本 `macrecovery.bat`） | **完全无关**（离线工具脚本） |
| **v1.0.8** | **第 6 行**：`- Added option to hide verbose output from any driver, thx @ilikesn0w` | `UEFI > Drivers` schema 字典字段与驱动加载器 | **核心相关**（启动日志 4 条 `No schema for HideVerbose` 根因所在，详见 §2.1） |
| **v1.0.8** | **第 7 行**：`- Re-enable Secure Boot after DMG loading, thx @albert-mueller` | `OcAppleDiskImageLib` / Apple Secure Boot 状态维护 | **完全无关**（当前 `SecureBootModel` 为 `Disabled`，详见 §2.4） |
| **v1.0.8** | **第 8 行**：`- Fixed uninitialized L3 cache value on CPUs without L3 cache, thx @bugprogrammer` | `OcCpuLib`（无 L3 缓存 CPU 的 SMBIOS/CPU 属性探测） | **完全无关**（本机 CPU 为 AMD Ryzen 7 9700X，具备 32MB L3 缓存，非无 L3 处理器） |
| **v1.0.8** | **第 9 行**：`- Added grub4dos detection to OpenLegacyBoot, thx @bugprogrammer` | `OpenLegacyBoot.efi`（传统 BIOS / grub4dos 引导检测） | **完全无关**（纯 UEFI 环境，不使用 `OpenLegacyBoot.efi`） |
| **v1.0.8** | **第 10 行**：`- Added OpticalDrive icon to OpenCanopy, thx @ilikesn0w` | `OpenCanopy`（图形选单光驱/DVD 图标资源） | **完全无关**（图形选单正常渲染，本机无光驱，不触发该图标显示） |
| **v1.0.8** | **第 11 行**：`- Fixed unresolved ATAPI device paths on NVMe Macs during hibernation, thx @rctphone` | `OcBootManagementLib`（NVMe 设备休眠唤醒 ATAPI 设备路径解析） | **完全无关**（台式机环境，`HibernateMode` 设为 `None`，且无 ATAPI 设备） |
| **v1.0.7** | **第 14 行**：`- Improved \`XhciPortLimit\` compatibility with macOS Tahoe, thx @laobamac` | `Kernel > Quirks > XhciPortLimit`（macOS 26 Tahoe 端口限制解除补丁） | **完全无关**（本机 `XhciPortLimit = false`，且当前系统为 macOS 15.8.1，详见 §2.2） |
| **v1.0.7** | **第 15 行**：`- Updated builtin firmware versions for SMBIOS and the rest` | `PlatformInfo`（内置 SMBIOS 固件版本表） | **完全无关**（本机使用固定 `MacPro7,1` SMBIOS 注入） |
| **v1.0.7** | **第 16 行**：`- Migrated to edk2-stable202511` | OpenCore 编译底座（EDK2 基础协议库与编译器支持） | **间接相关**（属于编译器与底座代码更新，无配置语法变更） |
| **1.0.8 之后至 tree 顶部 (1.0.9)** | **本地材料未设独立 Changelog 条目**；Git commit `81ecf1a`（2026-10-10）：`Add AppleLinkState protocol header` | `Include/Apple/Protocol/AppleLinkState.h`（协议头文件） | **完全无关**（仅引入协议接口定义，无任何运行时消费代码） |

---

## 2. 逐条评估对本机当前 Config 的实质影响

针对当前正在运行的 `config.plist` 关键配置项进行逐项深度比对：

### 2.1 UEFI/Drivers（4 条驱动配置与 HideVerbose 告警）
- **当前配置现场**：
  `UEFI > Drivers` 包含 4 个字典条目：
  1. `HfsPlus.efi` (`Enabled=true`, `HideVerbose=false`, `LoadEarly=false`, `Arguments=""`, `Comment=""`)
  2. `OpenRuntime.efi` (`Enabled=true`, `HideVerbose=false`, `LoadEarly=false`, `Arguments=""`, `Comment=""`)
  3. `OpenCanopy.efi` (`Enabled=true`, `HideVerbose=false`, `LoadEarly=false`, `Arguments=""`, `Comment=""`)
  4. `ResetNvramEntry.efi` (`Enabled=true`, `HideVerbose=false`, `LoadEarly=false`, `Arguments=""`, `Comment=""`)
- **1.0.7 现状**：
  OpenCore 1.0.7 的驱动描述结构体 `OC_UEFI_DRIVER_ENTRY` 仅定义 5 个键：`Arguments`、`Comment`、`Enabled`、`LoadEarly`、`Path`。
  当 1.0.7 解析到键名 `HideVerbose` 时，在上下文 `<Drivers>` 的 schema 映射表中未找到该定义，触发反序列化警告：
  `OCS: No schema for HideVerbose at 3 index, context <Drivers>!`（共报 4 次）。
  由于 OpenCore 对未知键采用"跳过并忽略"的容错策略，`Got 4 drivers` 表明 4 个驱动均正常加载运行，未产生任何功能中断。
- **升到 1.0.9 的变化**：
  1.0.8（及后续 1.0.9）在 `OC_UEFI_DRIVER_ENTRY_FIELDS`（`Include/Acidanthera/Library/OcConfigurationLib.h:624`）中正式合入了 `HideVerbose` 字段。
  升级至 1.0.9 后，引导器能原生识别这 4 个字典项中的 `HideVerbose`，4 条 `OCS: No schema for HideVerbose` 警告将被彻底消除。
  同时，由于配置中的值均为 `false`，各驱动仍维持默认的详细输出行为，**实际运行逻辑与 1.0.7 完全等价**。

### 2.2 Kernel > Quirks > XhciPortLimit
- **当前配置现场**：
  `Kernel > Quirks > XhciPortLimit = false`。
- **Changelog 对应项**：第 14 行 `- Improved \`XhciPortLimit\` compatibility with macOS Tahoe, thx @laobamac`。
- **影响判定**：**完全无影响**。
  - 本机采用 `UTBMap.kext` + `USBToolBox.kext` 进行物理端口映射，在 ACPI 和 kext 层面精确定位 16 个逻辑端口（或受控于原生 15 口限制以内），从未开启 `XhciPortLimit`。
  - `XhciPortLimit` 自 macOS 11.3 起即极不稳定，黑苹果长期规范坚决保持 `false`。
  - 该补丁改进针对的是后续大版本 macOS Tahoe（macOS 26），对当前系统（macOS 15.8.1 / Darwin 24.6.0）毫无影响。

### 2.3 Kernel > Patch（2 条 USB 15 口上限补丁）
- **当前配置现场**：
  `Kernel > Patch` 中的条目 #28（`setPortLocation`）与条目 #29（`AppleUSBXHCI::createPorts`）状态均为 `Enabled = false`。
- **影响判定**：**完全无影响**。
  - 无论引导器是 1.0.7 还是 1.0.9，禁用的补丁均不会被内核补丁引擎加载或执行，不存在行为差异。

### 2.4 Kernel > Patch（AMD_Vanilla 内核补丁 30 条）
- **当前配置现场**：
  共 30 条补丁（26 条启用，4 条禁用），覆盖 CPUID 模拟、核心数注入（`Force cpuid_cores_per_package`）、`_commpage_populate`、`_cpuid_set_info` GenuineIntel 绕过、IOPCIFamily 10-bit tag 规避（#18 / #19）、AM5 拓扑热插拔（#20）、非单调时间 panic 规避（#21 / #22）及 LAPIC 标定（#30）。
- **影响判定**：**完全无影响**。
  - 1.0.7 → 1.0.9 期间，OpenCore 的内核打补丁引擎（`OcAppleKernelLib`）对 x86_64 Mach-O 内核符号解析、Base 查找、Mask/Replace 机制未发生任何语法或执行逻辑变更。
  - 补丁的命中率与生效状态仅取决于当前 macOS XNU 内核（Darwin 24.6.0）的二进制字节排列（当前状态为 17 条命中、9 条按版本门控正常跳过、0 条失效），与引导器版本无关。

### 2.5 Kernel > Add（AMFIPass 及其他 kext）
- **当前配置现场**：
  `Kernel > Add` 共注入 19 个启用的 kext，包含 `Lilu.kext` (v1.7.3)、`AMFIPass.kext` (v1.4.1)、`AppleALC.kext` (v1.9.9)、`RestrictEvents.kext` 等。
- **影响判定**：**完全无影响**。
  - OpenCore 1.0.7 至 1.0.9 期间，kext 注入协议、预链接缓存修补机制均未变更，`AMFIPass` 的加载时机与符号劫持过程完全一致。

---

## 3. 升级到 1.0.9 后的综合行为结论表

| 维度 | 升级到 1.0.9 后行为表现 | 是否需要重新验收 | 验收重点 / 说明 |
|---|---|---|---|
| **引导日志 (opencore-*.txt)** | **改变**：4 条 `OCS: No schema for HideVerbose` 消失，全日志 `OCS:` 计数归零 | 是 | 启动后读取 ESP 根目录最新日志，核实 `OCS:` 告警数为 0 |
| **驱动加载 (UEFI Drivers)** | **不变**：HfsPlus、OpenRuntime、OpenCanopy、ResetNvramEntry 正常加载 | 是 | 验证 `Got 4 drivers`，图形选单正常出现，NVRAM 重置功能正常 |
| **选单交互 (OpenCanopy)** | **不变**：选单主题、图标布局、快捷键、倒计时保持一致 | 否 | 仅新增光驱图标，本机无光驱，界面完全一致 |
| **内核补丁 (Kernel Patch)** | **不变**：30 条补丁执行结果保持一致（17 命中、9 跳过、0 失效） | 是（安全抽检） | 运行 `patchreport.py` 核验 `replace count`，确认无 B 档静默失效 |
| **USB 拓扑与控制器** | **不变**：`XhciPortLimit` 保持关闭，UTBMap 16 口拓扑保持有效 | 是 | 确认键盘、鼠标、蓝牙 HCI、音频各端口枚举正常 |
| **电源与休眠管理** | **不变**：睡眠/唤醒策略不变，`HibernateMode = None` | 否 | 变更涉及的 ATAPI 休眠路径与本机无关 |
| **macrecovery 工具** | **不变**：不影响本机运行时 | 否 | 仅 Windows 辅助脚本更新，与 macOS 引导完全无关 |

---

## 4. 关键决策回答：仅为消除 4 条 HideVerbose 告警，升级 Bootloader 是否为合理代价？

### 明确结论：**否（不合理，严禁仅为了消除该告警而盲目升级引导器）**

### 核心理由：
1. **告警本质为无害的向下兼容噪音**：
   `OCS: No schema for HideVerbose at 3 index, context <Drivers>!` 是 OpenCore 反序列化器（`OcSerializeLib`）在严格模式下的信息性告警。它的语义仅仅是"1.0.7 不支持在 driver 级别静音输出"。处理策略是安全跳过未知键，并不阻断后续流程。运行时日志明确记录 `Got 4 drivers`，四个驱动均 100% 正常执行。
2. **运行语义完全等效**：
   当前配置中 4 个驱动的 `HideVerbose` 均为 `<false/>`。在 1.0.7 下，驱动默认输出 verbose 调试信息；在 1.0.9 下，读取到 `false` 同样是输出 verbose 调试信息。升级后逻辑上没有任何功能增益，行为没有任何改善。
3. **收益仅为清理 4 行日志文本**：
   升级引导器唯一能换来的收益，仅仅是把 debug 日志里的 4 行提示文字消掉，产出收益比极低。
4. **反证表明修改 config 同样不可行**：
   若试图通过从 `config.plist` 中删除这 4 个 `HideVerbose` 键来消除 1.0.7 的告警，则立即会导致 1.0.9 的 `ocvalidate` 报错（`Missing key HideVerbose, context <Drivers>!` ×4）。当前的 `config.plist` 在规范上是严格符合 1.0.9 标准的，不应做倒退性修改。

### 升级伴随的实质性风险：
- **ESP 写入与固件不兼容风险**：
  替换 ESP 分区根基二进制文件（`OpenCore.efi` 与 `BOOTx64.efi`）存在物理引导失败的固有风险。一旦自编译版本、EDK2 子模块或外部依赖发生隐式变更，可能导致机器卡死在 EFI 阶段或直接黑屏重置。由于本机 NVRAM 写入不可持久化，且无硬件串口，排错成本极高。
- **现有验证基线失效风险**：
  当前整机环境（9700X 核心拓扑、LAPIC 标定、NVMe 稳态、CX3 驱动分段、MT7922 蓝牙、S2 跨重启一致性）均在 `DBG-107-2026-10-14` 引导器下完成了严格的闭环实测。替换 bootloader 意味着整机引导底座发生漂移，必须对所有子系统重新执行全量回归验收。

**处置建议**：保持现状，将 R2 维持为"已知无害噪音"结案状态。仅在未来有 macOS 大版本升级需求（如升级到 macOS 26 Tahoe）或其他关键硬件功能必须依赖新版 OpenCore 时，再单独立项、在完备的应急备份窗口下执行引导器升级。

---

## 5. OpenCore 1.0.7 中 UEFI/Drivers Schema 允许键的严谨核对

### 5.1 本地源码依据
通过查阅 OpenCore 源码仓库中各模块演化历史：
- **1.0.7 时期的 `OC_UEFI_DRIVER_ENTRY` 允许键（共 5 个）**：
  1. `Arguments` (`OC_STRING`)
  2. `Comment` (`OC_STRING`)
  3. `Enabled` (`BOOLEAN`)
  4. `LoadEarly` (`BOOLEAN`)
  5. `Path` (`OC_STRING`)
- **1.0.8 引入的变更**：
  依据 `Changelog.md` 第 6 行（`- Added option to hide verbose output from any driver, thx @ilikesn0w`），在 `Include/Acidanthera/Library/OcConfigurationLib.h:624` 以及 `Library/OcConfigurationLib/OcConfigurationLib.c:709` 中新增：
  ```c
  OC_SCHEMA_BOOLEAN_IN ("HideVerbose", OC_UEFI_DRIVER_ENTRY, HideVerbose),
  ```
  此时 `mUefiDriversSchemaEntry` 按 ASCII 字母序递增为 6 个键：
  `Arguments` → `Comment` → `Enabled` → `HideVerbose` → `LoadEarly` → `Path`。

### 5.2 1.0.7 二进制字符串与现场佐证
- **二进制位置**：ESP 备份 `outputs/20261020-amd-boot-config/private-backups/esp-disk0s1-20261020-1146/EFI/OC/OpenCore.efi`（872,448 B，sha256: `7754f6d66b23ad2410e1a6ef9ee63b7d527ce2dd8e8dbf58bca9c657f87273c8`）。
- **字符串匹配实测**：
  - 在该 1.0.7 二进制中检索 `HideVerbose`，字符串命中仅与 `Uefi.Apfs.HideVerbose`（APFS 配置上下文）绑定，在驱动相关的 schema 上下文中完全不存在 `HideVerbose`。
  - 在驱动加载逻辑相关的数据段，仅存在 `Arguments`、`Comment`、`Enabled`、`LoadEarly`、`Path`。
- **运行时报错索引印证**：
  启动日志报 `OCS: No schema for HideVerbose at 3 index, context <Drivers>!`。
  检查 config.plist 中 driver 字典的键序：
  - 索引 0: `<key>Arguments</key>`
  - 索引 1: `<key>Comment</key>`
  - 索引 2: `<key>Enabled</key>`
  - **索引 3: `<key>HideVerbose</key>`**（命中警告）
  - 索引 4: `<key>LoadEarly</key>`
  - 索引 5: `<key>Path</key>`
  0-based 索引 3 与二进制中缺失该键的判定完全吻合，证据确凿。
