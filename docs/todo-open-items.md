# 未决事项登记（NVIDIA 驱动 / EFI 运维）

更新时间：2026-10-14 20:0x（本轮会话）

## 1. 反汇编 AMFI，产出 `amfi=` 的位表（用户点名要，标记为待办）

- **目标**：给出"`amfi=<位掩码>` 每一位对应 AMFI 的哪项检查"的表，而不是社区传闻值。
- **已具备的材料**：
  - 抽出来的 AMFI 二进制：`outputs/20261013-amfipass-re/extracted/AppleMobileFileIntegrity.macho`
    （注意：它是**切片导出**，LC_SEGMENT_64 里的 fileoff 仍指向原 BootKC 偏移，`otool`/`strings`
    会报 "truncated or malformed object"，需要先按段把偏移修成自洽的）。
  - 同一目录下的逆向工具：`scripts/relocs.py`、`scripts/rip_refs.py`（RIP 相对引用扫描，可用于找 xref）、
    `scripts/analyze.py`；BootKC 符号表 `raw/kc_nm.txt`。
- **方法（预计半天）**：
  1. 修偏移 → 得到可被 `otool -tV`/capstone 正常反汇编的副本；
  2. 定位 cstring `"amfi"` 与 `"amfi_get_out_of_my_way"` 的 VM 地址，用 `rip_refs.py` 找引用它的代码；
  3. 读 `PE_parse_boot_argn("amfi", …)` 之后的位测试（`test`/`and`/`bt`）分支，把"位 → 后续调用/字符串"对上；
  4. 交叉核对：AMFI 与 ASP（AppleSystemPolicy）里各有一份读 `amfi_get_out_of_my_way` 的代码
     （ASP 侧符号 `Config::isAMFIGetOutOfMyWayEnabled`）。
- **产出**：一张表（位值 / 影响哪项检查 / 证据地址），落到本目录 `amfi-bitmap.md`；查不出来的位如实标注。
- **现状**：未开始。本机 `amfi=0x80` 的有效性是**实测**（四个 kext + 用户态 bundle 都能用），不是推理。

## 2. `AirportMtkTail`（NVRAM 里的 MT7922 调试尾巴，1394 字节）

- 状态：**先不动**。用户决定等 WiFi 那条会话做完后由他们处理。
- 备选处置：把 `AirportMtkTail` 加进两侧 config 的 `NVRAM > Delete`（每次开机由 OpenCore 清掉）。

## 3. `DisableVariableWrite` —— **已结案：保持 `true`**（2026-10-14）

- 用户决定：不改成 `false`，不做"接管 macOS 侧写 NVRAM"的三条实验/改造（见 §5）。
- 依据（保留备查）：NullMoth 安装器源码注释写明该类机器（Ryzen 7000/8000/9000 AM5）在关掉它的情况下
  "安装 19 分钟就冻死"；173/175 台 Ryzen EFI 因此一直让 macOS 经被重写的内存映射往固件写 NVRAM，
  其中一块 AM5 板子重启冻死并返修。两侧现均为 `Booter > Quirks > DisableVariableWrite = true`。
- 注意：以后每次安装/更新驱动时，安装器都会把它设为 `true`（对 Zen 4/5 台式机强制），属预期行为。

## 4. 已结案（本轮及以上几轮）

- U 盘与内置 ESP 的 `Misc > Debug > Target` 统一为 **71**（日志开关 + 屏显 + Data Hub + 写文件，不写 NVRAM）。
- `-amfipassbeta` **保留**（用户计划后续上 macOS 26/27，届时该参数才会真正起作用）。
- U 盘 `NullMoth/` 安装包补齐为官方 1.15.0 四件套并校验通过；加了 `STICK-NOTES.md`。
- 两侧 `NVRAM > Add` 移除 `SystemAudioVolume`（那条从来没生效）；`NVRAM > Delete` 曾**临时**加入
  `SystemAudioVolume` / `SystemAudioVolumeDB` / `LocationServicesEnabled`（2026-10-14 22:2x，用户要求"先去掉"）。
  ✅ **2026-10-15 已撤掉**（用户："删掉吧"）：内置 ESP `config.plist` 的 Delete 里移除这三条，
  备份 `config.plist.bak-dropdel-20261015-020219`，`plutil -lint` OK，diff 只有这一处；下次开机生效。
  撤除理由（实测）：它们进了 Delete（每次开机被删）却**又出现在 NVRAM 里**
  （`SystemAudioVolume` = 0x41、`SystemAudioVolumeDB` = 0xF0、`LocationServicesEnabled` = 0x01），
  即 macOS 侧的写照样落地 —— 留着只会每次开机重置这两项设置，并额外制造一次"删—写"的事件噪音。
  这也与 §5.1 那条"变量确实变化过、写者未知"相互印证。
  另：U 盘当时未插，那一侧的 Delete 尚未核对/对齐。
- `AirportMtkTail` **保持不进 Delete**（WiFi 会话明确要求：它是跨 panic 唯一存活的现场记录，他们还要拿去复盘；
  要清就等他们收尾时手工清一次）。本会话误加过一次，已撤回。

## 5. "让 macOS 能安全写 NVRAM"（替代 `DisableVariableWrite` 的接管方案）

- 状态：**已关闭（不做）**，2026-10-14 用户决定。下面的三条路保留作记录，将来若重启这个话题直接从这里接。
- 三条候选路（详见本轮答复）：
  1. **先做诊断实验**（最低成本）：`DisableVariableWrite=false`，保持其他不变，观察是否还会冻；
     若冻，再逐个关 `DevirtualiseMmio` / `RebuildAppleMemoryMap` / `SyncRuntimePermissions` 复现，
     把"哪条路径在写坏固件"定位出来。**在定位之前没有可设计补丁的目标**。
  2. **模拟 NVRAM**（架构级"接管"）：`OpenVariableRuntimeDxe.efi`（`LoadEarly=true`，
     与 `OpenRuntime.efi` 排序、`LegacySchema` 填 `*`），变量落到 ESP 的 `NVRAM/nvram.plist`。
     代价：变量不再是真 NVRAM（Windows 侧看不到、双系统不共享）、持久化要配合 picker 里的
     `CTRL+Enter`（或 logout hook）、本路径原本是给"固件 NVRAM 脆弱"的机器准备的，风险要重新评估。
  3. **自己写内核补丁把写"转发"而不是禁用**：即改写 `DisableVariableWrite` 那条 patch 的语义。
     工程量与爆炸半径都大（要动 `AppleEFINVRAM` 的语义），暂不建议。

### 5.1 "写记录器"（用户要求做）

**先有的一条实测（2026-10-14 22:0x，本次启动内）**：

- 从 macOS 改音量（70 → 15 → 31，各观察 30 s），`SystemAudioVolume` / `SystemAudioVolumeDB`
  在 NVRAM 里**完全没变**，整份 NVRAM 的 sha256 也前后一致 → **音量这条写入路径确实被挡住了**
  （与 `DisableVariableWrite = true` 一致）。
- 但同一启动期间，这两个变量**确实变化过一次**（`SystemAudioVolume` 69 → 53；
  `SystemAudioVolumeDB` 0xF3 → 0xE7），**写者未知**：可能是某个并行会话的脚本/整份 NVRAM 回写，
  也可能是某条没覆盖到的 macOS 路径。这条只有靠记录器才能定性。

- 形态：一个 Lilu 插件，挂在 `AppleEFINVRAM` 的写入口上（与 OpenCore `DisableVariableWrite` 同一条路径，
  目标符号可从 OpenCore 源码 `OcAfterBootCompatLib` 公开查到），记录 `(GUID, 变量名, 调用者)` 到内核日志。
- **第一阶段（零风险，推荐起点）**：保持 `DisableVariableWrite = true`，插件只"记录后转发"——
  下游仍是 OpenCore 的"返回成功但不写"。这样**不用关掉拦截就能拿到"谁在写哪个变量"的完整清单**。
- 第二阶段（可选）：`DisableVariableWrite = false` + 插件按策略放行/拦截（例如"记录后拦截"或按 GUID/变量白名单），
  以便验证"哪些写其实无害"。这一步会恢复"macOS 真写固件"的风险窗口，只在受控的短窗口内做。
- 可摘除：插件放 ESP 的 `Kernel > Add`（或 `/Library/Extensions` + AuxKC），删掉即恢复原状。

#### 5.1.1 "选择性放行"（用户想要的形态，2026-10-14 提出）

- 目标：不是"全拦/全放"，而是**记录 + 按策略决定**——放行 macOS 推送的动态值（如 `fmm-*`、`efi-boot-device`、
  音量），继续拦住危险项，从而"和原生 Mac 接近一致"。
- 实现形态：`DisableVariableWrite = false`（关掉 OpenCore 的全拦），由插件**独占写入口**做决策
  （默认拦 + 白名单放行）。原因：保留 OpenCore 的 patch 时，下游是"返回成功但不写"的实现，
  "放行"就得绕开它；让插件独占更干净、也更好回退。
- 硬约束（务必记住）：
  1. 风险来自"**写这个动作**"本身（固件 runtime + 被重写的映射），与"写哪个变量"基本无关 →
     白名单只能缩小暴露面与频率，**不能消除**风险，必须当成有风险的实验，且要一键回退；
  2. 需要复用原始写路径（关掉 quirk 后即固件 runtime 的 `SetVariable`），不要自己重写一遍写入逻辑；
  3. 放行之后这些变量会像原生 Mac 一样随时间更新 —— 这正是"和原生一致"的来源，前提是接受那次写的风险。
- 建议白名单：`fmm-*`、`SystemAudioVolume(+DB)`、`prev-lang:kbd` 一类；**绝不放行** `boot-args`、
  `csr-active-config`、`BootOrder` / `Boot####` 这类与启动/安全直接相关的。

#### 5.1.2 记录器插件：已构建并装入 ESP（2026-10-14 22:34）

- 代码/构建：`outputs/20261014-nvram-write-recorder/impl/`（`src/nvrwlog.cpp`、`src/kmod-info-plugin.c`、
  `Info.plist`、`build-plugin.sh`；`3rdparty/` 软链到 AVAMDVi 那份 Lilu/MacKernelSDK）。构建：`bash build-plugin.sh`。
- 产物：`build/NVRWLog.kext`（Mach-O kext bundle x86_64；`NVRWLog_kern_start` 与 `_kmod_info` 都在；
  未定义符号只有 KPI 的 `IOLog`/`PE_parse_boot_argn`/`proc_selfname`/`proc_selfpid`/`snprintf`/`strncmp` 和 Lilu 的 KernelPatcher）。
- 已装：`/Volumes/OPENCORE/EFI/OC/Kexts/NVRWLog.kext` + `Kernel > Add` 最后一条（Enabled=true，
  排在 Lilu 之后）；config 备份 `config.plist.bak-nvrwlog-20261014-223423`。
- 钩子**默认不装**：加 `-nvrwhook` 才 route `AppleEFIRuntime::setVariable`。日志用 `IOLog`
  （本机实测 Lilu 的 SYSLOG 不进统一日志），读法 `log show … | grep nvrwlog`。
- 两阶段：① 先重启验证"注入+加载"（日志应出现 `loaded, hook NOT armed`）；② 再加 `-nvrwhook` 重启开钩子。
- 挂点/地址：符号 `_ZN15AppleEFIRuntime11setVariableEPKsPK8EFI_GUIDjjPKv`（在 **BootKC** 里）；
  解析先试 `solveSymbol(KernelID)`，失败退回 `kmod` 里的 `com.apple.kernel` 基址 + 链接期地址
  `0xffffff80018d0568`（两者都会打日志，另有首字节与路由后读回校验）。

## 6. 自签链 + 仿 AMFIPass 插件（可选实验）

- 状态：**用户已要求做**（2026-10-14 晚）。起因：想让"我们自己签名的 kext"也具备 OCLP 那种"被 CoreTrust
  认可的链"。**明确预期：这是整洁/研究性质的收益，不会减少现有放行**（XNU/SIP 那层与 AMFI/amfid 那层照旧，
  `csr-active-config` 与 `amfi=0x80` 一个都不能省；报告已证 AMFIPass 不可替代 `amfi=0x80`）。
- 建议步骤（分阶段，先验证再扩展）：
  1. 造一条自有链：自签 CA + 叶证书（脚本化，产物放 `experiments/amfipass-clone/certs/`）；
  2. 用叶证书给**一个**不重要的 kext 签名（例如 `NVAccel`）→ 重建 AuxKC → 验证能加载、`codesign -dvvv` 链正确；
  3. 写一个仿 AMFIPass 的 Lilu 插件：route 报告里那两个 CoreTrust 符号
     （`_X509PolicySetFlagsForRoots`、`_X509ChainGetAppleRootUsingKeyIdentifier`），把"被当作 Apple 根"的证书
     换成本项目自有的根；插件需要现有放宽（SIP 位 / `amfi=0x80`）才能自举加载 —— 这也是它"不是替代品"的证据；
  4. 四个 kext 全部签名 + 插件上线，做一次"把 `amfi=0x80` 收窄"的受控实验（可选，有回退）。
- 允许从现有 AMFIPass 借的部分：符号名与调用方式（报告已给出）；不复制其二进制。
- 真到那天的**首选路线**：直接沿用 **AMFIPass + Dortania 链**（用 Dortania 证书签自己的 kext + 跑 AMFIPass），
  而不是自建一条信任根——成本最低、已被大量机器验证，且本机 AMFIPass 已在位。
  自建根只在"不想依赖 OCLP 信任根"或研究/教学时才有价值。
- 已有先例（本项目里已经在用 OCLP 式手艺）：ESP 注入的
  `IO80211FamilyLegacy.kext`（Apple 的 `com.apple.iokit.IO80211FamilyLegacy` 1200.12.2b1 旧版本）
  与 `IOSkywalkFamily.kext`（同为 Apple 的 ID，但签名为 **adhoc**，说明副本被改/降级过），
  配合 config 里 `Kernel > Block` 的 "Allow IOSkywalk Downgrade"。二者现在都加载着——靠的是
  "OpenCore 注入 + SIP 位"，不是 CoreTrust 那条路。
- 可行形态：自签 CA + 叶证书 → `codesign` 给四个 kext 签名 → 写一个仿 AMFIPass 的 Lilu 插件
  （route 报告里那两个 CoreTrust 符号：`_X509PolicySetFlagsForRoots`、
  `_X509ChainGetAppleRootUsingKeyIdentifier`），把"被当作 Apple 根"的证书换成我们自己的根。
  材料齐全（逆向报告 + 那两个符号名），预估一个小项目。
- 必须说清：这**只解决"链"这一层**；XNU 的 kext 判定/信任缓存、AMFI/amfid 那两层的"非 Apple 签发"
  事实不变，`csr-active-config` 与 `amfi=0x80` 一个都不能少。真"被系统认可"只有 Apple Developer ID
  （$99/年 + 公证 + 用户授权，即本机 `kext_policy` 表那套机制），对这类驱动不现实。

## 7. OpenCore DEBUG 构建（后备手段，用户已表态"重建不是问题"，2026-10-14）

- 现状事实：本机 OpenCore 是 **RELEASE 1.0.9**（`nvram 4D1FDA02-38C7-4A6A-9CC6-4BCCA8B30102:opencore-version`
  = `REL-109-2026-09-30`；1.0.9 的官方 zip 不存在，上游最新正式版是 1.0.8）。Release 构建照样输出
  INFO 级日志——关键是 `Misc > Debug > DisplayLevel`（现已是 `0x80000042` = ERROR|WARN|INFO）。
- 什么时候需要 DEBUG 构建：① 需要 DEBUG_VERBOSE 级别的引导器内部细节；② 源码级调试（EDK II
  DebugAgent/GDB，Release 里没有该模块）；③ 将来我们自己写 OpenCore 侧的东西（UEFI 驱动/插件/工具）
  时，EDK II 的 DEBUG/ASSERT 才会生效。**与 kext 侧的调试无关**：Wi-Fi/蓝牙/NVIDIA/CX3 的 kext 都在
  macOS 之后加载，调试面是内核日志（IOLog/os_log）+ 各自的 boot-arg 开关；CX3/NVIDIA 走
  `/Library/Extensions`+AuxKC，OpenCore 完全不参与。
- 构建条件（本机已查）：Xcode 26.3 ✓、nasm ✓（/usr/local/bin）、python3 ✓、空间 735 GB ✓；
  **缺 mtoc**（acidanthera 的 cctools 分支，需单独装）与 iasl（部分目标用）。流程：
  `git clone --recurse-submodules`（含 audk）→ `./build_oc.tool` → 产出 `OpenCore-<ver>-DEBUG.zip`。
  预估 20–60 分钟含装 mtoc。
- **使用纪律**：不要用 DEBUG 构建替换内置 ESP 的 1.0.9 Release。整套（OpenCore.efi / BOOTx64.efi /
  Drivers / Tools 必须同构建）放到 **U 盘那份实验 ESP**（先备份它现有的 config 与 kext 清单；那张盘
  同时在给 AMD-Vi 会话当实验场，动手前打招呼），内置 ESP 保持 Release 稳定；用完换回。
- 触发条件：若 §5.1.2 的注入问题在 INFO 级日志下仍定位不到，或我们开始写引导器侧代码，再执行。

### 7.1 本机 OpenCore 的"身份"（2026-10-14 核实）

- 版本串：`REL-109-2026-09-30`（`nvram 4D1FDA02-38C7-4A6A-9CC6-4BCCA8B30102:opencore-version`）→ **Release 构建、
  1.0.9、构建于 2026-09-30**，即官方 1.0.8（2026-09-27 发布）之后 3 天的 **master/开发版**，不是任何官方发布版。
- 哈希与体积：`OpenCore.efi` sha256 前缀 `a3cfa9acc518d61f…`，630,784 B
  （对照：官方 1.0.8 RELEASE = `9ef21f9d846a992c…`，626,688 B）。
- **内置 ESP 与 U 盘 ESP 的 `OpenCore.efi` 逐字节相同**；内置 ESP 上全部文件 mtime 均为 2026-10-07 17:10
  （一次性拷贝），与"装机那天从 U 盘 EFI 树 promote 到内置 ESP"的已知历史吻合。
- 不是 NullMoth 驱动侧的产物：`nullmoth-setup.sh` 只**读** `OpenCore.efi`（诊断用），从不写引导器；
  driver payload（371 MB `nullmoth-nvidia-1.15.0.tar.gz`）与 `1401.app` 里都没有 OpenCore.efi / EFI 树。
  未能判定的：盘制作工具（1401 for Windows，仓库外）或更早版本 App 是否写过；那份工具我们手上没有。
- 结论/纪律：把它当成"上游发布版之后 3 天的开发版"这个已知变量。遇到说不通的行为时，
  先用"官方 1.0.8 RELEASE 或自建的同提交构建"复现一次；自建同时解决"没有 1.0.9 DEBUG 包"的问题。
