# AMD 平台 macOS 适配项目规划（初步）

> 状态：2026-10-19 初步规划；2026-10-20 Batch 0 已执行（见 §6.2）。本文件本身不授权新的执行；每个工作流由 owner 会话按 §3 的派发规则执行。
> 本文件是项目级 spec/roadmap。每个工作流真正启动时，从 §5 抽出该流的 session brief，再按 `superpowers:writing-plans` 生成任务级计划。

**Goal:** 把当前 AMD Hackintosh 的适配工作拆成边界清晰、可独立验收的会话工作流；先补齐日常可用性，再处理平台能力与长期研究，避免多个会话同时改 EFI 导致不可归因的故障。

**Architecture:** 分三层：A = Apple 内核/框架层，B = I/O 与总线层，C = 设备驱动层；D 是依赖上述能力的验收功能，不单独立项。每个工作流一个 owner session；跨工作流的 boot 配置由 `amd-boot-config` 统一冻结、备份和发布。

**Tech Stack:** OpenCore、AMD_Vanilla 内核补丁、Lilu/VirtualSMC/AppleALC/WhateverGreen、AirportMtk、NullMoth NVRM/NVMTL/NVAccel、Cx3Mlx4、AVAMDVi、macOS 15.8.1 (24H32)、Ryzen 7 9700X。

**Spec / Context:** `docs/device-driver-feasibility.md`、`docs/install-runbook.md`、`docs/todo-open-items.md`、`outputs/20261017-audio-current/`、`outputs/20261013-applevtd-amdvi/`、`~/Documents/Github/AirportMtk/docs/architecture/upstream-port-plan.md`、`~/Documents/usb-port-limit-macos/verification/`。

---

## 0. 结论先行

- **真正“Apple 侧没有 AMD 后端”的**：AppleHV（CPU 虚拟化）、AppleVTD（IOMMU）、XCPM/P-state 电源管理、SMC 传感器/风扇、NVRAM 写入语义、Zen5 定时器标定。
- **有第三方/自研驱动但尚未完成的**：USB 15 端口上限、MT7922 WiFi 数据面/HE/AWDL、MT7922 蓝牙、RTX 5060 的 VideoToolbox、Mellanox CX3 稳定性、音频断音。
- **已有会话在做的**：AppleVTD（`01a13455…`）、USB（`01a123d5…`）、MT7922 WiFi（`01a123d6…`）、NVIDIA 安装/音频/PR（`01a11ccc…`、`01a14a75…`、`01a14a4b…`）。这些会话是 owner，不另起同名流。
- **已拍板（2026-10-19）**：G0 日常优先，研究线保留但不阻塞；G1 研究超过 15 口限制的最优解，不预设全 17/15；核显（C8）列为 Deferred，后续再做。
- **不建议同时开超过 3–4 个会改 boot 配置的会话**；所有 EFI 写入必须走 `amd-boot-config` 的冻结窗口。
- **本会话只做协调/派发**：Wave 0、USB 研究、WiFi/NVIDIA/音频实现都由对应 owner 会话执行，不在本会话里包办。
- **现在只做规划**。其余决策门（G2–G9）按 §6 的触发时机再定；确认后再按波次启动会话。

---

## 1. 当前基线（2026-10-20）

> 这是规划用快照，不是交付验收。Wave 0 必须重新冻结“仓库快照 / 实际 ESP / 本次运行态”三者，三者不一致时以实际运行态为准。

| 项 | 当前证据 | 备注 |
| --- | --- | --- |
| 硬件 | Ryzen 7 9700X（8C/16T）、48 GB、MacPro7,1、RTX 5060 8 GB、2 TB NVMe、RTL8125、MT7922、CX3 | 双系统，U 盘/Windows 可作恢复入口 |
| OS | macOS 15.8.1（24H32），裸机 | `kern.hv_vmm_present=0` |
| 启动源/ESP | 内置 NVMe `disk0` 的 ESP `disk0s1`；OpenCore `DBG-107-2026-10-14`；config 56,063 B（`3dad2596…`） | 仓库 `efi/` 是 10-07 旧快照，不能覆盖 ESP；详见 `outputs/20261020-amd-boot-config/BASELINE.md` |
| BIOS | SVM Enabled；IOMMU Enabled；SR-IOV Disabled；XHCI Hand-off 无此选项；Secure Boot/CSM/Fast Boot Disabled | 2026-10-21 用户目视确认，未改值 |
| CPU 虚拟化 | `kern.hv_support=0`；`machdep.cpu.vendor=AuthenticAMD`；`machdep.cpu.features` 无 VMX | AppleHV 无 AMD 后端 |
| IOMMU | `ioreg` 无 AppleVTD 实例；10-20 基线看到 `1022:14d9` IOMMU 在 `00:00.2` 正常枚举；BIOS 目视 **Enabled** | 10-08 的 “Disabled” 是解码误差；以 10-21 目视为准 |
| CPU 电源 | `machdep.xcpm.mode=0`；`hw.cpufrequency=5.14 GHz`；`pmset -g therm` 无 CPU power status | 无 XCPM/P-state；未装 SMCAMDProcessor |
| 定时器 | 音频项目实测 100/500/1000 µs 等待实际中位为请求的 7.93–7.99 倍；Zen5 12-bit FID/FSB 处理可疑 | timer 补丁已部署但未单独验收 |
| WiFi | AirportMtk 0.1.0（`d4c5c5a5…`）；en1 active `192.0.2.130`，5GHz 160MHz；PHY a/b/g/n/ac/ax | 关联/4-way PASS：msg4 `dv=2/sec=1`，Rsn `akms=2`，RxBA agreed=2，RxAgg ampdu=515；短吞吐 75/37 Mbps；扫描 UI 仍 0（airportd 空缓存） |
| 蓝牙 | `State: Off`、`Address: NULL`、`Firmware v0 c0`；BT 在 USB Port 16 | 被 15 端口上限挡住；SimpleMtkBtFw 属实验性 |
| USB | S2 稀疏 map 已生效：XH00 **7 个对象**（硬件 2/3/7/8/15/16/17，位置 1..7），其他控制器 4/4/1；蓝牙已 On | 2 条内核补丁仍关闭；待探针观察旧 USB 补丁症状 |
| GPU | NullMoth 2.3.7 已加载；Metal 3、4K@160 Hz 正常 | `h264/hevc_videotoolbox` 编码失败 `-12908`；`videotoolbox_vld` 解码初始化失败 |
| CX3 | Cx3Mlx4 0.44.36 已加载；mailbox/DMA 已通 | `attachInterface` 冻屏未解决 |
| 音频 | AppleALC + NMHDA；内置输入/输出/HDMI 可用 | 曾有断音；关闭 USB 端口补丁后改善；timer 补丁未单独验收 |
| NVRAM | `DisableVariableWrite` 规避写 NVRAM；早期 panic 无报告/无 unified log | 仓库 10-14 决定不做接管 |
| 已正常 | RTL8125 en0、NVMe TRIM=Yes、USB 键鼠、4K 显示 | 不单独立项，只做回归 |

### 1.1 已有会话（owner 映射）

| 会话 | 标题/职责 | 在计划中的角色 |
| --- | --- | --- |
| `01a13455-ee1f-73b2-a501-793fd020dc51` | AppleVTD 支持 AMD IOMMU（路线 1b） | A2 owner，继续使用，不新建 |
| `01a123d5-8642-7810-9a09-8d4f19d4f12a` | USB 15 端口上限补丁 | B1 owner，继续使用，不新建 |
| `01a123d6-0859-7163-8086-ef2a01220941` | MT7922 Wi-Fi：HE 开机可用 + 运行时开关面 | C1 owner，继续使用，不新建 |
| `01a11ccc-49a1-7ff2-8ba9-3698f271045f` | NVIDIA 驱动安装（2.3.x） | C3 的安装/打包子线 |
| `01a14a75-0606-7a40-8574-5b52fa2a26bc` | HDMI/DP 音频修复 | C3 的 HDMI 音频子线 |
| `01a14a4b-a3cc-7b61-ae18-331017050bc8` | NVIDIA Chrome 加速 PR #263 | C3 的上游/PR 子线 |
| `01a1482b-452f-7700-bf2b-11cb9beb1e2a` | VNC / USB / 显卡驱动验收 | C5 的关联验收会话，不直接当音频 owner |
| `01a15c2a-ed65-7f52-a04a-d2b103a4de9b` | CX3 冻屏复现与 60 探针取证 | C4 owner，继续使用，不新建 |
| `01a153f1-711c-7e72-a33e-aac4728694dc` | 本会话 | 程序协调/规划；不承担实现，避免既当裁判又当运动员 |

---

## 2. 分类法

### A. Apple 内核/框架层（没有 AMD 后端，或语义不完整）

| ID | 工作项 | 现状 | 影响 |
| --- | --- | --- | --- |
| A1 | AppleHV / Hypervisor.framework（Intel VMX → AMD-V/SVM） | 无后端；`hv_support=0` | 本地 VM、Docker Desktop、UTM(vz)、Lima(vz)、QEMU-HVF 全部不可用 |
| A2 | AppleVTD / IOMMU（Intel VT-d → AMD-Vi） | IOMMU BIOS=Enabled；已有 AVAMDVi 研究；AppleVTD 未装成；无真实消费者 | 无 DMA 隔离；SR-IOV 另受 BIOS Disabled 限制 |
| A3 | CPU 电源管理（XCPM/P-state/C-state/CPPC） | `xcpm.mode=0`；报告频率固定 5.14 GHz | 无原生调频调压；功耗/温度偏高 |
| A4 | SMC 传感器/风扇 | 只有 VirtualSMC 假 SMC；无 SMCAMDProcessor | 无 CPU 温度/功耗/风扇传感器；风扇只能 BIOS 控制 |
| A5 | 定时器/TSC 标定（Zen5 FID/FSB/LAPIC） | 实测 7.93–7.99 倍等待上界；timer 补丁未单独验收 | 音频断音、调度抖动、USB deadline 异常 |
| A6 | NVRAM 写入/早期启动可观测性 | `DisableVariableWrite`；早期 panic 无报告、无 unified log | 变量持久化不可靠；早期故障只能靠屏幕/复位猜 |
| A7 | 启动链/内核补丁维护（AMD_Vanilla、ProvideCurrentCpuInfo、CPUID、KC patches） | 版本绑定；每次 macOS 更新需重推 | 更新即可能失效；多会话并行改 EFI 风险高 |
| A8 | 安全/平台完整性（SIP/AMFI/FileVault/Apple Pay/DRM） | SIP 自定义；Kext Signing/FS Protections/NVRAM Protections 关闭；FileVault Off | 安全姿态与原生 Mac 不同；Apple Pay/T2 相关不可用 |

### B. I/O 与总线层

| ID | 工作项 | 现状 | 影响 |
| --- | --- | --- | --- |
| B1 | USB 15 端口上限 / locationID / port map | **S2 + MT7922 remote-wake 禁用 runtime PASS** | 控制器恢复风暴的触发路径已关闭 |
| B2 | PCIe 枚举 / ReBAR / MMIO / IOMMU | Above 4G + ReBAR 已开；IOMMU Enabled；SR-IOV Disabled | 影响 GPU BAR、直通、AppleVTD 前提；SR-IOV 不可用 |

### C. 设备驱动层

| ID | 工作项 | 现状 | 影响 |
| --- | --- | --- | --- |
| C1 | MT7922 WiFi（AirportMtk） | **关联/4-way/data PASS**；扫描 UI 0、RLM/AWDL 未完成 | 可用但吞吐仍低；airportd 缓存问题独立 |
| C2 | MT7922 蓝牙（SimpleMtkBtFw/BlueToolFixup） | **remote-wake 禁用已生效并 runtime PASS** | 蓝牙保持在线，remote wake 已关闭 |
| C3 | RTX 5060（NVRM/NVMTL/NVAccel/NVHDA） | Metal 3/显示正常；VideoToolbox 编解码失败 | 无硬件视频编解码；剪辑/转码/部分播放走软件 |
| C4 | Mellanox CX3（Cx3Mlx4） | **0.44.37 已加载并 probe PASS**；call/phase 取证链已自测 | 等待用户暂停工作后做下一次受控复现 |
| C5 | 音频（AppleALC/NMHDA/CoreAudio 调度） | 可用但曾有断音；timer/USB 交互 | 音频体验不稳定 |
| C6 | RTL8125 有线 | 工作正常 | 只做回归 |
| C7 | NVMe/TRIM | 工作正常 | 只做回归 |
| C8 | Ryzen 核显 | macOS 无 Raphael 核显驱动；IORegistry 无 1002 设备 | 不可用；**后续再做（Deferred）**，当前不进入日常波次 |

### D. 依赖功能（不单独立项，作为验收场景）

| ID | 功能 | 依赖 |
| --- | --- | --- |
| D1 | 本地 VM / Docker / UTM / Lima | A1 |
| D2 | PCI 直通 / SR-IOV | A2 + B2 + BIOS；SR-IOV 当前 BIOS=Disabled，只能先做传统 PCI 直通评估 |
| D3 | AirDrop / Handoff / Continuity / Universal Control / Sidecar / 连续互通相机 | C1 + C2 + C3（HEVC 编码） |
| D4 | 硬件视频编解码 | C3 |
| D5 | 睡眠/唤醒 | A3 + A6 + B1 |
| D6 | iServices / Apple Pay / DRM | A8 + SMBIOS；Apple Pay 需 T2/Secure Enclave，基本不可达 |

### 2.1 明确不做 / 后续再做

- Apple Pay / Secure Enclave / T2 相关：硬件不存在，不作为可实现目标。
- Thunderbolt / USB4、SATA：本机没有对应硬件。
- 把修改过的 Apple 二进制分发或公开；AppleVTD 路线只做本机自用。
- 通用化到其他 AMD 平台；本计划只针对这台 Ryzen 7 9700X 机器。

**后续再做（Deferred）：**

- **Ryzen 核显（C8）**：macOS 没有 Raphael 后端；后续评估自研/移植驱动、显示输出、硬件编解码和功耗收益，先做 Go/No-Go，不进入当前波次。

---

## 3. 工作流与会话分配

> 新会话名是**建议名**，尚未创建。已有会话优先继续，避免重复立项。

| ID | 工作流 | 类型 | 优先级 | Owner session | 依赖 | 当前状态 |
| --- | --- | --- | --- | --- | --- | --- |
| P0 | 程序协调与基线 | 协调 | P0 | 本会话 `01a153f1…` | — | 本文件 |
| A7/B2 | boot 配置、备份、补丁注册表、更新回归 | 平台 | P0 | **`amd-boot-config`（新）** | — | Batch 0 完成（R1+R6 通过；R2/R10 开放） |
| B1 | USB 15 端口上限 / port map | **REPAIR RUNTIME PASS（29m）** | P1 | `01a123d5…`（已有） | A7 | `kUSBRemoteWakeOverride=No`；HS11/recovery/MCHOSE 掉线均为 0 |
| C1 | MT7922 WiFi / AirportMtk | 用户接管 | P1 | 用户 + 旧线程 `01a123d6…` | — | 关联/4-way/data PASS；扫描 UI/RLM/AWDL 由用户续 |
| C3 | RTX 5060 驱动 + HDMI 音频 + VideoToolbox | 实现 | P1 | **`amd-nvidia-5060`（umbrella）** | A7 | 2.3.7 已加载；VideoToolbox 失败 |
| C5 | 音频断音 / CoreAudio 调度 | 诊断 | P1 | **`amd-audio`（新）** | A5、B1 | timer/USB 交互待收敛 |
| A5/A6 | 定时器/TSC + NVRAM + 早期启动日志 | 平台 | P2 | **`amd-timing-nvram`（新）** | A7 | timer 补丁未单独验收 |
| A3/A4 | CPU 电源管理 + SMC 传感器/风扇 | 研究 | P2 | **`amd-power-smc`（新）** | A7 | 未开始 |
| C2 | MT7922 蓝牙 | 验收 | P2 | **`amd-bt-mt7922`** | B1 | **runtime PASS**（基础音频/重连）；重启/睡眠待验 |
| A2 | AppleVTD / AMD-Vi | 研究 | P2 | `01a13455…`（已有） | B2、BIOS | 路线 1b，稳定透传档 |
| C4 | CX3 驱动 / attachInterface 冻屏 | **OWNER REVIEW** | P2 | `01a15c2a…`（已有） | A7 | 0.44.37 已加载；0.44.38 候选待 owner 独立审查；新 watcher 待接管 |
| A1 | AppleHV / AMD-V 后端 | 研究 | P3 | **`amd-hv-cpu`（新）** | A7 | 未开始；工作量大 |
| A8 | 安全姿态 / SIP / FileVault / Apple Pay / DRM | 评估 | P3 | **`amd-security-posture`（新，可选）** | A7 | 现状可用，非 AMD 专属 |
| C8 | Ryzen 核显 | Deferred | — | **`amd-igpu`（未来，未创建）** | — | macOS 无驱动；后续做 Go/No-Go，不进入 Wave 1–4 |

**并发上限建议：** 同一时间最多 3–4 个活跃实现会话；其中最多 1 个持有 EFI 写权限（`amd-boot-config`）。其余会话只产出候选补丁/诊断，不直接写 ESP。

**工作量量级（相对）：** S = 几天；M = 1–2 周；L = 3–6 周；XL = 2 个月以上。Wave 1 的 USB/NVIDIA/audio 为 M–L，WiFi 为 XL；Wave 2 的 timing 为 M、power/SMC 为 L、BT 为 S–M；Wave 3 的 AppleVTD/CX3 为 L–XL，AppleHV 为 XL+；security 为 S；iGPU 为 Deferred，不计入当前估算。

### 3.1 隔离/协作模型

**角色：**

| 角色 | 谁 | 职责 | 不做什么 |
| --- | --- | --- | --- |
| 程序协调 | 本会话 | 维护本计划、优先级、依赖图、跨流决策；审阅证据；在波次边界做集成判断 | 不写驱动、不改 EFI、不替 owner 做实现 |
| 工作流 owner | 每个工作流一个会话 | 拥有该流的仓库/目录、基线、实现、证据、handoff | 不碰其他 owner 的目录；不直接写 ESP |
| EFI 单点写者 | `amd-boot-config` | 唯一允许写 ESP 的会话；应用候选、维护补丁注册表、冻结窗口、恢复演练 | 不实现设备驱动 |
| 集成/验收 | 程序协调 + 对应 owner | 波次边界验收、回归、更新兼容性 | 不在未验收时宣布“完成” |

**隔离边界：**

- **代码隔离**：`AirportMtk`、`usb-port-limit-macos`、`nvidia-macos-driver`、`tools/cx3-mlx4` 等各自归 owner；跨目录只读。
- **产物隔离**：每个流只写自己的 `outputs/<date>-<workstream>/`；共享结论写 `docs/`，但先经协调会话确认。
- **EFI 隔离**：除 `amd-boot-config` 外，任何会话不得直接写 ESP；候选以“补丁 + 目标文件 + 哈希 + apply/rollback + 测试计划”交付。
- **实验隔离**：一次重启只改一个变量；需要重启的流先在协调会话登记冻结窗口，避免和 WiFi/NVIDIA/USB 的改动叠加。
- **上下文隔离**：每个 owner 只读本计划 §5 的对应 brief + 自己的 handoff；不继承其他流的现场假设。

**协作契约：**

- **单一事实源**：本文件负责 scope/优先级/依赖；owner 的 handoff 负责该流的最新状态。
- **状态格式**：`状态 / 本次证据 / 阻塞 / 下一步 / 需要协调`，每次阶段结论必须带可复现命令或哈希。
- **候选格式**：候选补丁必须带目标文件、应用前后哈希、命中位置/冲突数、回滚命令、测试计划。
- **依赖协议**：owner 声明 blocking edges；协调会话更新波次 frontier；blocker 未完成不启动下游 ticket。
- **跨流请求**：需要别的流配合时，发消息给协调会话，由它排冻结窗口/优先级，不直接改别人的目录或 EFI。
- **升级路径**：启动失败/panic/EFI 损坏 → 立即通知 `amd-boot-config` + 协调会话，先回滚再分析；不在坏状态上继续叠加变量。

### 3.2 派发批次

| 批次 | 会话 | 启动条件 | 备注 |
| --- | --- | --- | --- |
| Batch 0 | `amd-boot-config`（新） | 已执行 | 备份 771/771、离线恢复 436/436、R1 写回+重启、R6 BIOS 目视均通过；R2/R10 开放 |
| Batch 1 | `amd-usb-ports`（已有 `01a123d5…`） | Batch 0 完成 | S2 已部署；重启后做 7 口/蓝牙 + 探针观察，不做人工压测 |
| Batch 1 | `amd-wifi-mt7922`（已有 `01a123d6…`） | Batch 0 条件通过（只读/离线可开工） | 数据面/HE/RA；AWDL 后置 |
| Batch 1 | `amd-nvidia-5060`（umbrella） | Batch 0 条件通过（只读/离线可开工） | 复用已有安装/HDMI/PR 子线；VideoToolbox 新子线 |
| Batch 2 | `amd-audio`（新） | USB + timing 基线有结论 | 断音归因；不先于 USB/timer |
| Batch 2 | `amd-timing-nvram`（新） | Batch 1 有稳定组合 | timer 独立 A/B + NVRAM/早期日志 |
| Batch 2 | `amd-power-smc`（新） | Batch 0 完成 | 先测量，再决定是否调频 |
| Batch 2 | `amd-bt-mt7922`（新） | USB Port 16 可枚举 | 否则只做离线固件/协议准备 |
| Batch 3 | `amd-vtd-iommu`（已有 `01a13455…`） | Batch 0 完成 | 已有流继续；不新建 |
| Batch 3 | `amd-cx3-mlx4`（新/续） | Batch 0 完成 | 冻屏复现优先 |
| Batch 3 | `amd-hv-cpu`（新） | G6 确认有 VM/Docker 需求 | 先做 Go/No-Go |
| Batch 4 | `amd-security-posture`（可选） | M4 前 | 非 AMD 专属 |
| 后续 | `amd-igpu`（未来） | Wave 4 后或你点名 | 只做 Go/No-Go，不进入日常波次 |

**派发规则：** 每个批次最多 3–4 个活跃 owner；Batch 0 必须先于任何会改 EFI 的批次。派发时由协调会话给每个 owner 一份 session brief（§5 对应小节 + handoff 路径 + 验收 + 回滚），owner 不得自行扩大 scope。

---

## 4. 波次与依赖

```text
Wave 0  基线
  amd-boot-config ──> 冻结仓库/ESP/运行态，备份，补丁注册表，恢复演练

Wave 1  日常可用性
  amd-usb-ports ──> amd-bt-mt7922（Port 16 解锁后）
  amd-wifi-mt7922 ───> D3（AWDL/AirDrop，数据面达标后）
  amd-nvidia-5060 ──> D4（VideoToolbox）

Wave 2  平台稳定性
  amd-timing-nvram ──> amd-audio
  amd-power-smc
  amd-bt-mt7922（依赖 Wave 1 的 USB）

Wave 3  平台能力 / 研究
  amd-vtd-iommu ──> D2（PCI 直通/SR-IOV）
  amd-cx3-mlx4
  amd-hv-cpu ──> D1（VM/Docker）

Wave 4  集成验收
  D3 / D4 / D5 / D6
  安全姿态与 macOS 更新回归
```

**启动规则：**

1. Wave 0 未完成前，不启动任何会改 boot 配置的实现会话。
2. Wave 1 先做“对日常影响最大、且不依赖其他研究”的三条：USB、WiFi、NVIDIA。
3. `amd-audio` 在 `amd-usb-ports` 和 `amd-timing-nvram` 都有稳定基线后再开始，否则无法归因。
4. `amd-bt-mt7922` 必须等 USB Port 16 能被枚举；否则只能做离线固件/协议准备。
5. `amd-hv-cpu` 放到最后：先确认是否真有 VM/Docker 需求；没有就只保留研究文档，不投入实现。

---

## 5. 每个工作流的交付与验收

### 5.1 `amd-boot-config`（P0）

**Scope:** ESP/config.plist/Kexts/内核补丁/备份/恢复演练/更新回归/BIOS 关键项只读核对与变更申请；不实现设备驱动。

**Deliverables:**
- 一份“实际启动源”证据：磁盘/GPT/卷/`EFI/OC` 路径，不依赖卷名。
- 全量 EFI 备份 + 逐文件 SHA256 + 卷外副本。
- 补丁注册表：每条 Kernel/Booter patch 的用途、版本限制、命中位置、回滚值。
- 冻结窗口协议：谁在何时可以写 ESP，其他会话如何提交候选。
- macOS 小版本更新检查清单（timer/USB/AMD_Vanilla/AMFIPass）。
- BIOS 关键项快照：SVM、IOMMU、SR-IOV、Above 4G、ReBAR、XHCI Hand-off；与仓库 10-08 快照做差异表。

**Acceptance:**
- 从备份恢复一次，机器可正常启动（恢复演练）。
- 仓库快照、ESP 文件、运行态 `nvram boot-args`/`kmutil showloaded` 三者有明确差异表。
- 任何候选补丁都有“应用前/后哈希 + 反汇编命中 + 冲突数”记录。
- IOMMU 口径冲突（10-08 快照 Disabled vs AppleVTD 会话 Enabled）已解释并以实际 BIOS 为准。

**当前状态（2026-10-20）：** 备份 771/771、离线恢复 436/436 通过；真机恢复待批；M0 条件通过。详见 §6.2。

### 5.2 `amd-usb-ports`（B1，研究→实现，已有 `01a123d5…`）

**Scope:** 研究并实现“超过 macOS 每控制器 15 口限制”的最优解；不预设“全 17 口”或“稳定 15 口”。范围包括 AppleUSBHostPort/AppleUSBXHCI 的检查、locationID 4-bit 编码、port-count、控制器基址、ACPI port 来源、UTBMap 重排、控制器拆分/重映射，以及与音频/USB 时序的交互；不改蓝牙协议栈。

**当前基线（2026-10-20）：** XHC0 硬件 17 口，但 2 条 USB 15→17 补丁 `Enabled=false`，实际只建 15 口，HS11/HS12 不存在。研究从“15 口可用”开始，不要假设 17 口已开。

**研究结论（2026-10-20）：** XH00 的 17 个对象里只有 **7 个接到实际连接器**（硬件 `{2,3,7,8,15,16,17}`）。推荐 **S（稀疏 map）**：`port-count` 仍写 17，map 只声明这 7 个，`XhciPortLimit=false`，两条自定义补丁继续关闭；预期 7 个对象、逻辑位置 1..7、零内核补丁、零位置溢出。P（创建 17 个对象）只作为冻结窗口里的受控实验，原因是 16/17 会越过 4-bit 端口域，且 16 号下挂 Hub 会与根 17 冲突。

**S 部署状态（2026-10-21）：** S1 `83f47223…` 是 binary plist，OpenCore prelink 只认 XML，日志报 `Invalid Parameter`，已回滚 baseline。S2 改为 XML `998c7fbb…`（与 10-17 已实测候选逐字节一致），06:43 写回；07:55 重启后 OpenCore 日志为 `Prelinked injection UTBMap.kext ... - Success` / `UTBMap.kext v1.1`。首次启动时 XH00 7 个对象、位置 1..7，其他控制器 4/4/1，蓝牙 On，短时验收通过。

**S2 回归（2026-10-21 10:10 boot）：** CX3 `netupstop(2)` 冻屏并重启后，XH00/HS11（硬件 port 16，逻辑 `0x00600000`）从 10:10:52 起持续出现 `createDevice(0xe00002bc)`、命令超时、`0x00000040` 和控制器 recovery；截至 10:22 已计数 160 次创建设备失败/160 次 recovery。Bluetooth 当时为 `State: Off / Address: NULL`，USB 树无 `Wireless_Device`；同控制器 HS10 上的 MCHOSE 键盘与 HS12 上的麦克风在 recovery 时一起被断电并短暂移出 HID Event System。

**冷启动复验（2026-10-21 10:25:54 boot）：** 用户关机并拔掉交流电 30 秒，BIOS 设置未改。上电后 Bluetooth 恢复 `On`、Address `02:1A:7D:DA:71:2A`、Firmware `v9735 c9236`，`Wireless_Device@0x00600000` 与 MCHOSE HID service 在线。到 10:33:33（跨过上一轮持续故障起点），HS11 create failure 0、controller recovery 0、MCHOSE terminate 0。判定为“冷启动后临时 PASS”；普通重启与睡眠/唤醒仍未验收。证据在 `outputs/20261021-freeze-keyboard/post-cold-boot-20261021-102554/`。

**Remote-wake 修复（2026-10-21 10:50 部署，11:23 验收）：** 根据 Linux `btusb` 对 Ryzen + MT7922 的 quirk，新增 codeless `MT7922USBWakePolicy.kext`，精确匹配 `locationID=0x00600000` 并设置 `kUSBRemoteWakeOverride=false`。重启后 MT7922 节点实测 `No`，Bluetooth On、MCHOSE 在线；到 boot 后 29 分钟，HS11 create failure、XH00 recovery、MCHOSE terminate 均为 0。证据：`outputs/20261021-mt7922-wake-policy/VERIFY-PASS-20261021-112305.txt`。

**研究顺序（先研究，后实现）：**

1. 复盘现有补丁与冲突模拟器，确认 15 口限制到底卡在哪几层。
2. 枚举候选路径，逐条做离线冲突模拟、稳定性/可维护性/成本评分。
3. 选出最优解与第二选择；先做非关键端口、只读或可秒回滚的验证。
4. 只有评分、回滚和探针观察都通过，才进入 EFI 写入。

**Deliverables:**
- 机制报告：15 口限制到底卡在哪几层（数量检查、locationID 宽度、端口号分配、hub 半字节、控制器基址），每层给出符号/偏移/证据。
- 候选路径清单，至少覆盖：
  - 放宽 `AppleUSBHostPort::setPortLocation` / `AppleUSBXHCI::createPorts` 的检查与掩码；
  - 压缩/重排 UTBMap，把 16/17 映射到逻辑 ≤15 的端口号；
  - 改 ACPI `_UPC`/port 号，从源头避免 >15；
  - 拆分/重映射控制器基址（XHC0/XHC1/XHC2 的 locationID 划分）；
  - 社区已有方案（corpnewt/USBMap issue #63 等）的适用性复核。
- 评分矩阵与推荐：稳定性、无 locationID 冲突、音频/USB 时序、macOS 更新可维护性、实现成本；给出最优解和第二选择。
- 每版 macOS 可复用的补丁校验器/冲突模拟器（输入候选补丁，输出命中位置/冲突数）。
- 选定方案的实现与探针观察/重启记录（不做人工 100 次插拔/30 分钟音频压测）。

**Acceptance:**
- 至少 2 条可行路径的对比和评分，明确“最优解”及理由；不以“全 17”或“15 口”作为预设答案。
- 选定方案下：XHC0 目标端口全部创建且无 locationID 冲突；或明确记录为何某条路径不可行。
- 探针在正常使用窗口内未发现与旧 USB 补丁相同的症状：`hardwareException`、`CommandRing::executeCommand`、`createDevice: failed`、`powerOn: deadline`、HS11/port16 错误或控制器恢复风暴；不做人工 100 次插拔/30 分钟音频压测。
- 不引入新的 panic/复位；回滚只需恢复 UTBMap/config 的单个文件。

### 5.3 `amd-wifi-mt7922`（C1，已有 `01a123d6…`）

**Scope:** AirportMtk 数据面（TXWI、STA/BSS records、RX reorder）、HE/RA、运行时开关；AWDL 单独排期。

**当前状态（2026-10-21 09:18）：** AKM/dv 一致性修复已生效（build `d4c5c5a5…`）：`en1` active `192.0.2.130`，system_profiler Connected / Channel 36 160MHz；Eapol2 最后为 msg4 `dv=2/sec=1/kdlen=0`；`AirportMtkRsn akms=0x2`；`RxBa agreed=2`、`RxAgg ampdu=515`；ping 网关 6.3ms，短吞吐 75/37 Mbps。`networksetup -getairportnetwork` 仍报未关联、`cwscan` 仍 0，是 airportd 空缓存/UI 报告问题，数据面已可用。`BssRlm sent=0` 因开关默认 off；RLM/RA/AWDL 后续再评估。

**线程状态（2026-10-21 09:20）：** owner 线程 `01a123d6…` 的**用户 UI turn 仍可执行**；失败的是 `send_message_to_thread` 的**自动投递 turn**。投递内容会留在信箱里，用户随后发“继续”可把它带进正常 turn 执行。**该 WiFi 线由用户自己继续旧线程，协调会话不再投递/接管**；需要程序化连续驱动时才新建会话。

**历史根因（2026-10-21 09:07）：** `AirportMtkRsn` 曾为 `akms=0x8 supp_akms=0xa`，msg2 却写 RSN IE **AKM 2**、发 `dv=3`；AP 按 AKM 2 期望 `dv=2`，静默丢弃。修复后 `akms=0x2`、msg4 `dv=2`，4-way 已通过。

**Deliverables:**
- 以 Linux 7.2.11 为蓝本的差异表和逐项移植记录。
- 吞吐/延迟/丢包基线：Linux 对照 vs macOS 当前 vs 目标。
- HE 开机与运行时开关的稳定性矩阵。

**Acceptance:**
- 上行/下行达到 Linux 对照的 90%（当前记录：Linux 上行 842 Mbps；macOS 下行 113 / 上行 33）。
- 连续 60 分钟 iperf/大文件传输无断流、无 panic。
- AWDL/AirDrop 作为独立里程碑，不在数据面达标前承诺。

### 5.4 `amd-nvidia-5060`（C3 umbrella）

**Scope:** NVRM/NVMTL/NVAccel/NVHDA 的安装、Metal/显示、HDMI/DP 音频、VideoToolbox、上游 PR；不改 macOS 系统二进制。

**Sub-tracks / existing sessions:**
- 安装/打包：`01a11ccc…`
- HDMI/DP 音频：`01a14a75…`
- Chrome/上游 PR：`01a14a4b…`
- VideoToolbox：新子线（可由 umbrella 会话直接做）

**Acceptance:**
- `h264_videotoolbox`/`hevc_videotoolbox` 在 `-allow_sw 0` 下编码成功；`videotoolbox_vld` 解码初始化成功。
- 4K@160 Hz、Metal 3、HDMI 音频、睡眠/唤醒回归通过。
- 驱动版本/补丁可回滚；`config.plist` 哈希不变或有明确差异。

### 5.5 `amd-audio`（C5）

**Scope:** CoreAudio 调度、AppleALC/NMHDA、USB 端口补丁与 timer 的交互；不改 WiFi/GPU 驱动。

**当前状态（2026-10-21）：** 用户在 timer + USB S2 组合下做了短音频检查，**没有断音**；按 `amd-audio` 回归 PASS 处理。不做强制 60 分钟/30 分钟压测，保留日常观察和短回归。

**Deliverables:**
- 断音复现脚本与最小判据（512/4096 帧、60 分钟）。
- timer 补丁、USB 补丁、NVIDIA HDMI 音频三者的 A/B 矩阵。
- 可日常使用的稳定组合（不是“自建播放器能跑”）。

**Acceptance:**
- 日常短检/正常使用无可听断音；观察窗口内 HALS 无 `kIOReturnIsoTooOld`（不做强制长压测）。
- 512 帧缓冲下 `mach_wait_until` 中位 < 1.1× 请求值。
- 回滚组合有记录，且不依赖重新编译。

### 5.6 `amd-timing-nvram`（A5/A6）

**Scope:** Zen5 TSC/FID/FSB/LAPIC 标定、timer 补丁、NVRAM 写入语义、早期启动 panic 可观测性。

**当前状态（2026-10-21 09:35）：** timer 补丁已命中（`_tsc_init` 的 `busFCvtn2t` = 25 MHz 换算常量）；100/500/1000µs 三轮中位 1.027/1.007/1.004×，历史 7.933/7.982/7.987× 未复现。当前是复合启动（timer + USB S2 + NVIDIA），只能算组合 PASS；timer-only A/B 仍缺。NVRAM 继续 `DisableVariableWrite=true`，14 变量/795B 稳定；早期 panic 落盘通道仍未解决。未写 EFI/未重启。

**Deliverables:**
- 本机 timer 标定的可复现测量（不只看绝对值，记录倍率/上界）。
- timer 补丁的独立 A/B 验收（排除 USB/音频同时变化）。
- NVRAM 写入风险矩阵；panic 日志落盘方案或“明确不做”的结论。

**Acceptance:**
- 100/500/1000 µs 等待中位与上界都有记录；补丁前后可对照。
- 早期启动故障至少有一个可落盘的证据通道（或明确记录不可达原因）。
- 所有补丁带 Darwin 版本限制和回滚说明。

### 5.7 `amd-power-smc`（A3/A4）

**Scope:** CPU P-state/C-state/CPPC、SMC 温度/风扇/电压键；不碰 GPU 电源。

**当前状态（2026-10-21 09:45）：** `machdep.xcpm.mode=0`、vectors/ratio changes 均为 0；`hw.cpufrequency*` 都是 5.14GHz 静态占位值；65 个 SMC 键里真实温度/风扇/电压/功耗/频率传感器为 0（`FNum=0`）。没有 SMCAMDProcessor/SMCProcessor/SMCSuperIO。原版 SMCAMDProcessor 对 Zen5（Family 1Ah）No-Go；自研只读 SMC 插件 Conditional Go（温度原型约 3–5 天，完整监控 1–2 周）；维持现状 + BIOS Smart Fan 是当前最低风险 Go。

**Deliverables:**
- 现状测量：空闲/负载频率、功耗、温度、风扇（能从 BIOS/外部工具拿到的先记录）。
- 可行方案评估：SMCAMDProcessor/自定义 SMC kext/接受固定 P-state。
- 若实现：传感器与调频的最小可用集 + 回滚。

**Acceptance:**
- 至少能看到 CPU 温度/频率；或明确记录“当前平台不可达”的证据。
- 空闲功耗/温度有前后对比；不引入新的 panic/睡眠问题。
- 若把睡眠/唤醒纳入本轮，需有 10 次循环记录；否则明确写“不在本轮范围”。

### 5.8 `amd-bt-mt7922`（C2）

**Scope:** SimpleMtkBtFw/BlueToolFixup/SimpleMtkBtPatcher；依赖 USB Port 16 可枚举。

**Deliverables:**
- 控制器枚举证据（`ioreg`/`system_profiler` 的 Address/Firmware/State）。
- 固件装载与 HCI 初始化日志。
- 与 USB 端口补丁的依赖矩阵；失败时回滚到“无蓝牙”稳定态。

**Acceptance:**
- 蓝牙控制器 Address 非 NULL、State On、可配对键鼠/耳机。
- 重启/睡眠后仍可用；不影响 WiFi 与 USB。

**当前状态（2026-10-21 09:30，runtime PASS / 本轮范围）：** 本次启动 09:16:51；
XH00 的 HS11（硬件 16）在 `locationID=0x00600000` 枚举，控制器 `State: On`、
Address `02:1A:7D:DA:71:2A`。AirPods Pro 已连接，HFP/AVRCP/A2DP/AACP/GATT/ACL
服务可用，并同时成为默认输入和默认输出。本地 Aiff 的播放/暂停/恢复状态为
`true → false → true`；09:30:07 主动断开、09:30:34 重连后 A2DP 再次流式工作。
该窗口无 panic、无蓝牙控制器 reset，USB 四个硬失败签名计数均为 0。
重启/睡眠持久性仍待后续验收。证据见 `docs/handoff-bt-mt7922.md` 与
`outputs/20261021-bt-mt7922/RUNTIME-ACCEPTANCE.md`。

### 5.9 `amd-vtd-iommu`（A2，已有 `01a13455…`）

**Scope:** AppleVTD 上层 + AMD-Vi 后端；不碰 CPU 虚拟化。

**Deliverables:**
- 路线 1b 的阶段性证据：DTE/页表/失效/故障可见性。
- 真实消费者清单；没有消费者时明确“研究性”定位。
- 安全回滚：U 盘启动、ESP 零改动、boot-args 分级。

**Acceptance:**
- AppleVTD 实例存在且 `gSystem` 生效，设备 DMA 正常，无 panic。
- 或明确判定“当前不可行/无消费者”，保留证据并停止投入。

### 5.10 `amd-cx3-mlx4`（C4）

**Scope:** CX3 驱动、attachInterface 冻屏、mailbox/DMA/网络路径；不改其他驱动。

**复现结果（2026-10-21 10:06）：** 用户放行后，最小 `netupstop(2)` 复现整机冻屏。`cx3freeze` 最后一条驱动日志为 10:06:53.366 的 `queryPort` 成功与 MAC 暂存；`cx3watch` 收到 `pre selector 46 netupstop 2`，没有对应 post，也没有后续 `step/hook`。此前 reset、queryfw、fwload、querydevcap、m2dprofile、m2dbringup、queryPort 均成功。结论：卡点落在 selector 46 的 attach 入口附近，mailbox/ICM 不是本轮阻塞点。

**下一次前置条件：** 必须先恢复 USB 干净基线（冷启动后确认 Bluetooth On、`Wireless_Device` 存在、HS11 错误为 0，MCHOSE HID 不间歇消失）。没有新的代码路径或更细 breadcrumb 时，不再运行 `netupstop(2)`。

**0.44.37 取证增强（2026-10-21 11:25）：** USB 基线已连续为 0 错误。上一轮 `cx3freeze` 没有任何 `enter selector 46` 或 netup marker，因此新增 `cx3ClientSelector` / `cx3ClientPhase`，由 100 ms watcher 同时推送 `call / phase / step / hook`。阶段可区分 sweep、transaction lock 等待/获得、dispatch 进入/返回和释放。已构建 PASS、安装到 `/Library/Extensions/Cx3Mlx4.kext`，binary `952e9515…85cf5`，`kmutil load rc=28` 等待重启；0.44.36 完整 bundle 已备份。

**重启后自测（2026-10-21 11:29）：** `0.44.37` 已加载，只读 `cx3ctl probe` PASS；registry 写出 `cx3ClientSelector=0`、`cx3ClientPhase=8`。同时发现旧 watcher 的 `ioreg -n … -d1 -w0` 在 24H6 返回 0 字节并把缺失值伪装成 0；已修为 `ioreg -r -n … -d1 -l -w0`，并增加 `read=ok/read=EMPTY`，commit `e546439`。修正后 watcher 实测 `read=ok call=0 phase=8 step=0 hook=0`，USB 基线仍全绿。

**第二次最小复现（2026-10-21 11:31）：** USB 基线全绿后执行最小 `netupstop(2)`。prefix 全部成功；11:32:07 发出 `pre netupstop 2 attach` / `pre selector 46 netupstop 2`，60 侧 11:32:08.922 记录 Mac DEAD。60 最后 watcher 样本为 11:32:04 `read=ok call=24 phase=8 step=0 hook=0`，这是旧 shell watcher 落后约 3 秒；`cx3freeze` 最后驱动行仍是 selector 24 后 `user client closed`，未出现 `enter selector 46`。selector 46 入口后的 phase/hook 仍未知。

**0.44.38 审查候选（2026-10-21 11:39）：** 已构建但**未安装**：`outputs/20261021-cx3-attach-context/build/Cx3Mlx4.kext`，binary `efeae121…`。方案为把 `attachInterface` 移到独立 `thread_call` worker，user-client 有界等待 10 秒，`stop()` 增加 `thread_call_cancel_wait/free`。新增常驻 10 ms IOKit sampler：`tools/cx3-mlx4/tools/cx3-phase-watch.c`、`netup-watch.sh`；构建使用 `/usr/bin/clang -isysroot "$(xcrun --sdk macosx --show-sdk-path)"`。均未提交，已交 C4 owner 独立审查。

**Deliverables:**
- 冻屏最小复现与调用栈/日志。
- 修复或规避方案；1 小时压力测试。

**Acceptance:**
- 1 小时 mailbox/网络压力无冻屏；重启/睡眠后仍可用。
- 回滚到“不加载 CX3”的稳定态只需移除 kext/恢复 config。

### 5.11 `amd-hv-cpu`（A1）

**Scope:** AppleHV 的 AMD-V 后端可行性；先做决策，不默认写代码。

**Deliverables:**
- 消费者清单（Docker/UTM/Lima/自研 VM）与收益评估。
- XNU `hv_support`/VMX 依赖清单；AMD-V 后端工作量拆解。
- Go/No-Go 建议：研究文档或正式立项。

**Acceptance:**
- 明确回答“值不值得做”；若 Go，给出分阶段原型（hv_vm_create → 最小 vCPU → Linux guest）。
- 若 No-Go，记录原因和替代方案（外部 Proxmox/双系统）。

### 5.12 `amd-security-posture`（A8，可选）

**Scope:** SIP/AMFI/FileVault/Apple Pay/DRM 的现状与风险；非 AMD 专属。

**Deliverables:**
- 安全姿态清单与风险分级。
- FileVault 可行性/风险；Apple Pay/T2 的不可达结论。

**Acceptance:**
- 不改变当前可用性的前提下，给出“保持现状/收紧/放弃”的建议。

### 5.13 `amd-igpu`（C8，Deferred，后续再做）

**Scope:** Ryzen Raphael 核显（1002:13C0）在 macOS 下的驱动可行性；不进入当前 Wave 1–4。

**Deliverables:**
- 现状证据：IORegistry 无 `pci1002` 设备；macOS 无现成 Raphael 驱动。
- 可行路径评估：自研 kext / 移植 Linux amdgpu / 仅做显示输出 / 仅做硬件编解码，分别列出工作量与收益。
- 收益清单：显示输出、硬件 H.264/HEVC、功耗、与 RTX 5060 的分工。
- Go/No-Go 文档；若 Go，再单独立项并给分阶段原型。

**Acceptance:**
- 形成一份有证据的 Go/No-Go 结论；不把“没有现成驱动”直接等同于“永远不做”。
- 若 Go，给出最小原型范围（枚举/寄存器访问/显示或编解码之一）；若 No-Go，记录原因。

**Trigger:** Wave 4 完成后，或你明确点名“现在做核显”。在此之前不创建会话、不投入实现。

---

## 6. 决策门（需要你拍板）

| ID | 问题 | 选项 | 建议 | 触发时机 |
| --- | --- | --- | --- | --- |
| G0 | 项目目标优先“日常可用”还是“平台完整性/研究”？ | 日常优先 / 研究优先 / 双线 | **已拍板（2026-10-19）：日常优先；研究线保留但不阻塞** | 已确认 |
| G1 | USB 如何突破 15 口限制、什么是最优解？ | S 稀疏 map / P 17 对象 / 稳定 15 | **研究完成：S 稀疏 map 最优；P 17 对象仅受控实验；建议 S** | Wave 1（`amd-usb-ports`） |
| G2 | WiFi 继续重写 AirportMtk，还是换 Broadcom/Intel 卡？ | 继续重写 / 换卡 / 双轨 | 先继续重写；若 4 周内数据面无 2× 提升，再评估换卡 | Wave 1 结束 |
| G3 | 蓝牙修 Port 16，还是用 USB 蓝牙 dongle？ | 修端口 / dongle / 暂不做 | 先修端口；dongle 作为低成本兜底 | B1 有结论后 |
| G4 | VideoToolbox 等上游，还是自己补？ | 等上游 / 自己实现 / 接受软件编解码 | 先确认上游路线；无路线再评估自研 | Wave 1 |
| G5 | AppleVTD 继续投入，还是停在研究档？ | 继续 / 停 / 找消费者 | 没有真实消费者就停在研究档 | A2 下一里程碑 |
| G6 | AppleHV 是否立项？ | 立项 / 仅研究 / 放弃 | 先确认 VM/Docker 真实需求；无需求仅研究 | Wave 3 |
| G7 | CPU 电源管理投入，还是接受固定 P-state？ | 投入 / 接受 / 只做传感器 | 先做传感器与测量，再决定是否调频 | Wave 2 |
| G8 | CX3 继续修，还是换网卡？ | 继续 / 换卡 / 停 | 先拿 1 小时复现；无稳定路径再换 | Wave 3 |
| G9 | iGPU（C8）是否立项？ | Go / No-Go / 继续观察 | 先做可行性与收益评估，不预设 | Wave 4 后或你点名 |
| G10 | USB 最终交付选 S 还是 P？ | S 稀疏 7 / P 17 对象 / 双轨 | **默认 S**；P 只在冻结窗口做单变量实验 | 生成候选前 |

### 6.1 已拍板（2026-10-19）

| 决策 | 结果 | 影响 |
| --- | --- | --- |
| G0 | 日常优先；研究线保留但不阻塞 | Wave 1 先做 USB/WiFi/NVIDIA/音频；AppleVTD/AppleHV/CX3 放到后续波次 |
| G1 | 研究完成：S（稀疏 map，7 个真实连接器）最优；P（17 对象）只做受控实验 | USB 会话基于 10-20 基线生成 S 候选；P 不进入默认交付 |

**G1 最优解判定标准（按优先级）：** 稳定性 > 无 locationID 冲突 > 音频/USB 时序不受影响 > macOS 更新可维护性 > 实现成本。任何方案必须通过探针观察（旧 USB 补丁症状为 0）；不做人工 100 次插拔/30 分钟音频压测。

**G1 研究结果（2026-10-20）：** XH00 的 17 个对象只有 **7 个接了实际连接器**（硬件 `{2,3,7,8,15,16,17}`）。S 方案用这 7 个、`port-count=17`、补丁全关，逻辑位置压在 1..7；P 方案创建 17 对象，但 16/17 越过 4-bit 端口域，16 号下挂 Hub 会与根 17 冲突，且每版 macOS 要重推字节。默认交付取 S。

### 6.2 Batch 0 结论（2026-10-20）

| 项 | 结果 | 对计划的影响 |
| --- | --- | --- |
| 实际启动源 | 内置 NVMe `disk0` 的 ESP `disk0s1`；OpenCore `DBG-107-2026-10-14`；`config.plist` 56,063 B | 以后以这份 ESP 为准，不再用仓库 `efi/` 旧快照 |
| 备份 | 771 文件；`MANIFEST.sha256` 771/771 OK；Kexts 64/64 OK；整分区 UDRO 镜像 | 已具备离线回滚材料 |
| 离线恢复 | 临时 FAT32 镜像写入预演 436/436 OK；R1 已把 baseline EFI 写回实时 ESP，436/436 校验通过、0 删除 | 写回链路可信；2026-10-21 重启验收通过 |
| 三位一体 | 仓库 `efi/`（10-07）≠ 实际 ESP（10-20）≠ 运行态（有 5 个 AuxKC 驱动不在 ESP） | 仓库快照不能覆盖 ESP；运行态需单独记录 |
| USB | S2 稀疏 map 生效：XH00 7 个真实对象（位置 1..7），其他控制器 4/4/1；蓝牙 On | 内核补丁仍关闭；S 方案已通过启动/拓扑验收 |
| IOMMU | `1022:14d9` 在 `00:00.2` 正常枚举；BIOS 目视 **Enabled**（2026-10-21） | A2/B2 可按 Enabled 准备；AppleVTD 前提满足 |
| BIOS | IOMMU Enabled；SR-IOV Disabled；XHCI Hand-off 无此选项；Secure Boot/CSM/Fast Boot Disabled | R6 已关闭；SR-IOV 关闭影响 D2 的 VF 路线 |
| 补丁 | Kernel/Patch 30 条：27 条基础 + 2 条 USB（关）+ 1 条 timer（开，仅 Darwin 24.6.0） | USB owner 注意补丁是关的；更新前先校验 |
| 阻塞 | R2 DBG-107 与 1.0.9 config schema 不匹配；R10 ESP 已用 79% | **M0 启动/回滚 + BIOS 目视均通过；R2/R10 仍开放** |

**M0 状态：通过。** 备份、离线恢复、R1 写回、重启验收和 R6 BIOS 目视均通过；R2/R10 仍是后续风险。Batch 1 可以在冻结窗口下进入 EFI 写实验，但仍须遵守“一次一变量 + 先备份”。

### 6.3 R1 重启验收（2026-10-21）

| 项 | 结果 |
| --- | --- |
| 启动时间 | `kern.boottime` = 2026-10-21 03:55:25 CST；最新日志 `opencore-2026-10-20-195500.txt`（19:55:00 UTC） |
| 启动源 | 内置 NVMe `disk0s1`；日志 `boot-path = .../NVMe(...)/HD(1,GPT,6D351E57-...)/EFI\OC\OpenCore.efi` |
| 配置 | 56,063 B；OpenCore `DBG-107-2026-10-14`；加载配置字节数与日志一致 |
| EFI 校验 | 436/436 与 baseline 0 差异；config/OpenCore/BOOT 哈希全部一致 |
| 运行态 | USB 15 个 USB20 + 9 个 USB30；NVIDIA 2.4.3、CX3 0.44.36 已加载；无 panic 报告 |
| 备注 | 03:54:27 有一条 `shutdown_stall`，属于重启前关闭阶段记录，不是本次启动 panic |

### 6.4 R6 BIOS 目视（2026-10-21）

| 项 | 当前值 | 对计划的影响 |
| --- | --- | --- |
| IOMMU | **Enabled** | AppleVTD/AMD-Vi 的前提满足；保持开启 |
| SR-IOV | **Disabled** | D2 的 SR-IOV VF 路线不可用；只能先评估传统 PCI 直通 |
| XHCI Hand-off | **BIOS 没有该选项** | 不适用；USB 映射按当前控制器行为继续 |
| Secure Boot | **Disabled** | 保持关闭，符合当前 OpenCore/黑苹果配置 |
| CSM | **Disabled** | 纯 UEFI，符合要求 |
| Fast Boot | **Disabled** | 保持关闭，便于故障排查 |

---

## 7. 跨工作流规则

1. **单一 EFI 写者**：只有 `amd-boot-config` 能写 ESP；其他会话产出候选文件/补丁，由它统一应用。
   Wave 0 完成后，已有会话的 ESP 直写权限收回；Wave 0 之前 USB/WiFi 等会话的临时直写必须登记并纳入基线。
2. **一次只改一个变量**：每次重启只允许一个 boot 配置变量变化；记录 `config.plist`/Kexts 的 SHA256。
3. **先备份再动手**：全量 EFI 备份 + 卷外副本 + 恢复演练；U 盘/Windows 恢复入口必须可用。
4. **证据格式**：命令、时间、输出、哈希、before/after；结论写进 `outputs/<date>-<workstream>/`，长期结论进 `docs/`。
5. **不碰别人的目录**：`AirportMtk`、`usb-port-limit-macos`、`nvidia-macos-driver`、`tools/cx3-mlx4` 各自归 owner；跨目录只读。
6. **不重复立项**：已有会话是 owner；新会话启动前先读本文件 §1.1 和对应 handoff。
7. **更新回归**：macOS 小版本更新后，先跑 `amd-boot-config` 的补丁校验器；timer 补丁只允许 Darwin 24.6.0，USB 补丁按 KC 重推。
8. **不分发修改过的 Apple 二进制**：本机自用；上游 PR 只提交自己写的代码。

---

## 8. 里程碑与验收

| 里程碑 | 内容 | 验收 |
| --- | --- | --- |
| M0 基线 | 冻结仓库/ESP/运行态，备份，离线恢复，补丁注册表 | 备份 771/771、离线恢复 436/436、R1 写回+重启、R6 BIOS 目视均通过；R2/R10 开放 |
| M1 日常可用 | USB、WiFi、BT、NVIDIA、音频均有稳定组合 | USB/BT controller storm 修复已 runtime PASS；继续观察睡眠/长期；WiFi 已连通但扫描 UI 仍异常；VideoToolbox 待结论 |
| M2 平台稳定 | timer/NVRAM、CPU 电源/SMC | timer 可对照；温度/频率可见；早期故障有证据通道 |
| M3 平台能力 | AppleVTD、CX3、AppleHV | AppleVTD 有消费者或明确停；CX3 1 小时稳定；AppleHV Go/No-Go |
| M4 集成验收 | AirDrop/Handoff/Sidecar、VM、直通、安全姿态 | 依赖功能逐项通过或明确不可达 |
| M5 后续评估 | iGPU（C8） | 有证据的 Go/No-Go；Go 才单独立项 |

---

## 9. 风险与回滚

| 风险 | 影响 | 缓解 |
| --- | --- | --- |
| 多会话同时改 EFI | 故障不可归因、启动失败 | 单一写者 + 冻结窗口 + 一次一变量 |
| 修改 Apple 二进制/签名 | 启动链/更新失败 | 仅本机、保留原版、AMFIPass/备份、恢复演练 |
| USB 补丁引发音频/USB 时序故障 | 断音、设备掉线 | 先建立音频基线；补丁 A/B；保留 15 口稳定档 |
| timer 补丁只对一个 Darwin 版本有效 | 更新后失效/panic | 版本限制 + 更新后先校验 |
| WiFi 重写范围大 | 长期无稳定结果 | 以 Linux 7.2.11 为蓝本、分 TX/RX 两条、先仪器化 |
| NVIDIA 驱动是第三方 | 更新/兼容性风险 | 固定版本、自动回退、PR 上游 |
| 硬件极限（MT7922 AWDL） | 功能不可达 | 决策门提前判定，不无限投入 |
| iGPU 驱动工作量/收益不确定 | 长期投入无回报 | 先做 Go/No-Go，不进入日常波次 |

---

## 10. 下一步（等你确认，不现在执行）

1. G0 已确认（日常优先，研究线保留）；G1 研究完成：S 稀疏 map（7 个真实连接器）最优，P（17 对象）只做受控实验，默认交付 S。
2. Batch 0 已由 `amd-boot-config` 交回：备份、离线恢复、R1 写回+重启、R6 BIOS 目视均通过；R2/R10 仍开放。
3. 现在可以放行 Batch 1 的 EFI 写实验，但仍须遵守 `amd-boot-config` 的冻结窗口、一次一变量和先备份。
4. S2 首次启动通过拓扑验收；CX3 冻屏后的普通重启出现 HS11 恢复风暴，彻底断交流电 30 秒后恢复。当前 boot 到 10:33:33 为 0 错误，先继续观察普通重启/睡眠；未再复现前不回滚 UTBMap、不继续 CX3 复现。
5. WiFi/NVIDIA 可以继续隔离目录工作；任何重启或 EFI 候选都要向 `amd-boot-config` 申请窗口。
6. iGPU（C8）已列入 Deferred，不进入 Wave 1–4；Wave 4 完成后或你点名时再做 Go/No-Go。
7. C2 已再次恢复：冷启动后控制器 On、Address 有效、`Wireless_Device@0x00600000`；
   下一步验收普通重启和睡眠/唤醒持久性，不重做长音频压测。
8. 任何时候你都可以说“暂停”，本计划不会自行改 EFI/BIOS/驱动。

---

## 11. Session brief 模板（给后续会话）

```text
你是 <工作流名> 的 owner。工作目录：/Users/you/Documents/Hackintosh-MacLocal。

先读：
- docs/plans/2026-10-19-amd-adaptation-program.md 的 §5.<n>
- <该流已有的 handoff/文档/仓库路径>

目标：<一句话，可验收>
非目标：<明确不做什么>
起点：<已有产物/基线/会话 ID>
约束：一次一变量；改 EFI 前先备份；不碰其他会话目录；证据落盘。
第一 actions：只读检查 → 建立基线 → 给出候选方案 → 等我确认后再改。
交付：<文件/补丁/测试记录/结论>
```

