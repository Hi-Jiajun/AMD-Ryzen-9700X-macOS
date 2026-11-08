# 合成 DMAR 入口 + 注入机制（设计稿 · 供审核）

配套：`appleVTD-reverse-map.md`（函数/寄存器证据）。本文所有结论都有本机反汇编依据，标「待验」的除外。

---

## 1. AppleVTD 是怎么被喂进来的（实测）

`IOPCIPlatformInitialize()`（IOPCIFamily，0x28b069a）里的顺序是：

```
callq 0xffffff8000ad5eb0                ; 返回一个 IOService/registry 对象        ← 待验（是谁）
  ->getProperty("ACPI Tables")          ; vtable +0x2e8，字符串 0x28ba7f8
  ->getObject("DMAR")                   ; vtable +0x208，字符串 0x28ba804
PE_parse_boot_argn("intcpumax"/"forceintcpu")
new IOPCIMessagedInterruptController(0xb8) -> init(0x190, 0x70) -> reserveVectors(...)
AppleVTD::install(workLoop, vec, proxy, dmar, msic)      ; 0x28b08c3
```

`AppleVTD::init(workLoop, OSData*)` 再把它当 DMAR 表解析（`unit_init(acpi_dmar_hardware_unit*)`，
日志 `dmar %p Address %llx, Flags %x`）。**所以 AppleVTD 只认一个东西：名为 `DMAR` 的 OSData。**

本机现状：IORegistry 里没有 `"ACPI Tables"` 属性（`ioreg -l -w0 | grep '"ACPI Tables"'` 空），
所以现在 `IOPCIPlatformInitialize` 取 DMAR 必然失败 → `IOMapper::gSystem` 为空 → 才有 brief 里那条日志。

## 2. 两条注入路线

> **2026-10-13 更新：A 已实现**，见 `impl/src/av_entry.cpp`——挂钩 `AppleVTD::install`，
> 在调用现场从 `ACPI Tables` 取 IVRS、现场合成 DMAR 并替换 `OSData*` 参数；插件里已编入
> （`build-plugin.sh` → `compiled 7/7`）。是否生效取决于 stage 1 的时序探针结果，失败则回落到 A′。

| | A. 插件早期读 IVRS 后**运行时合成** DMAR（**新首选**，已实现） | A′. OpenCore `ACPI > Add` 注入静态合成 DMAR（**兜底**） | B. 插件直接改 `"ACPI Tables"` 字典 |
| --- | --- | --- | --- |
| 做法 | 从 `getPlatform()->getProperty("ACPI Tables")` 取 **IVRS** → 解析出单元 MMIO 基址 → 合成 DMAR → 交给 AppleVTD | 把预先构造的 DMAR 放进 ESP→OC/ACPI，用 `ACPI > Add` 加载 | 直接在该字典里塞 `DMAR` 键 |
| 优势 | 地址真实、不硬编码、不碰固件层、回滚干净 | 社区验证过（CaseySJ `SSDT-DMAR.aml`） | 不碰固件层 |
| 风险 | 依赖时序（必须早于 AppleACPIPlatform 的调用） | 需硬编码 MMIO 基址；改 ESP | 依赖时序；可能与 ACPI expert 自身写入冲突 |

（下表为原 A/B 对比，保留作背景）

| | A. OpenCore `ACPI > Add` 注入合成 DMAR | B. 插件早期直接挂 `"ACPI Tables"` 属性 |
| --- | --- | --- |
| 做法 | 把一张我们自己构造的 DMAR 表（.aml/.bin）放进 ESP→OC/ACPI，用 `ACPI > Add` 加载 | 我们的 kext 在启动早期向 §1 那个节点 `setProperty("ACPI Tables", …)` |
| 社区先例 | **CaseySJ 的 `SSDT-DMAR.aml`**：Intel/AMD Hackintosh 上就是靠注入 DMAR 让 AppleVTD 起来（与本机 config 里那三条 CaseySJ 补丁同一知识库） | 无公开先例 |
| 风险 | 改 ESP（可在 U 盘配置上做），ACPI 层多一张表 | 要先确定 §1 的节点是谁；属性可能被 ACPI expert 后来的写入覆盖 |
| 回滚 | 删 `ACPI > Add` 那一条 | 删 `Kernel > Add` 那一条 |

结论：走 A。B 作为 A 失败时的备选（且 B 完全不碰固件层，风险更低）。

## 3. 合成 DMAR 要满足什么（按解析器读的字段倒推）

DMAR 是 ACPI 表：36 字节头 + 若干子结构。AppleVTD 的解析路径只读下面这些（都有字符串/指令依据）：

| 字段/子结构 | 解析器读它做什么 | 依据 |
| --- | --- | --- |
| DMAR 头（Host Address Width / Flags） | 只打日志 | `DMAR Width %x, Flags %x`（0x28b857d） |
| DRHD（hardware unit）条目的 Register Base Address | **成为 `vtd_unit_t+0x08` 的 MMIO 基址** | `dmar %p Address %llx, Flags %x`（0x28b8a5b） |
| DRHD 的 Segment/Device Scope 列表 | 建立 `scope type %d, bus %d, path %d,%d`（设备→单元归属） | 0x28b8b33 / 0x28b8b4a |
| RMRR（Reserved Memory Region） | 保留内存，映射时绕过 | `ACPI_DMAR_TYPE_RESERVED_MEMORY 0x%llx, 0x%llx`（0x28b87d6） |
| ATSR / SATC / 其它 | 未在字符串里出现，判断为不读 | — |

**关键点**：AppleVTD 会把 DRHD 的地址当"寄存器块基址"用，而 AMD 的 IOMMU MMIO 寄存器布局与 VT-d **完全不同**。
所以合成 DMAR 里填 AMD 单元的 MMIO 基址是**可达成的**（能让 Apple 的匹配/初始化跑起来），
但紧接着 `initHardware`→`unit_enable` 就会按 VT-d 偏移去写这块内存 —— **必须同时把 §5 的那批函数接管掉**，
否则就是在往 AMD IOMMU 的寄存器里写错误的位（有把平台写挂的可能）。这也是"只注入 DMAR 不接管硬件层"不可行的原因。

**字段级要求现在有源码依据**（`refs/dmar.h`，Apple 自己的 DMAR 定义）：

```
ACPI_TABLE_DMAR { ACPI_TABLE_HEADER Header /*36B*/; u8 Width; u8 Flags; u8 Reserved[10]; }
   Flags: ACPI_DMAR_INTR_REMAP (1)     ; 置位 = 声明中断重映射可用（第一版不置）
ACPI_DMAR_HARDWARE_UNIT { ACPI_DMAR_HEADER Header{u16 Type;u16 Length;}; u8 Flags; u8 Reserved;
                          u16 Segment; u64 Address; }
   Type=0 (HARDWARE_UNIT)；Flags: ACPI_DMAR_INCLUDE_ALL (1)
ACPI_DMAR_DEVICE_SCOPE { u8 EntryType; u8 Length; u16 Reserved; u8 EnumerationId; u8 Bus; }
   + 若干 ACPI_DMAR_PCI_PATH{u8 Device; u8 Function;}
   EntryType: ENDPOINT=1 / BRIDGE=2 / IOAPIC=3 / HPET=4
ACPI_DMAR_RESERVED_MEMORY { Header; u16 Reserved; u16 Segment; u64 BaseAddress; u64 EndAddress; }  // Type=1
```

第一版合成 DMAR 的内容（草案）：

```
DMAR 头：HostAddressWidth=物理位宽（本机 48 或 52，待读 CPUID）、Flags=0
DRHD #0：RegisterBase = AMD IOMMU MMIO 基址（从 IVRS 或 PCI 配置空间 BAR 取，待验）
         Segment = 0、Flags = INCLUDE_ALL
DeviceScope：可选，先用 INCLUDE_ALL，不逐设备列
RMRR：0 条（先不加）
```

另外源码给了一个额外杠杆：**`IOPlatformMapperPresent`**（`AppleVTD.cpp:1421`）——
platform expert 上没有这个属性时 AppleVTD 自认 `fDisabled`。用它可以做"注入 DMAR 但先不启用"的过渡态
（比 `-amdvtdoff` 更靠上游），也可以用来反证"是不是这条路径在起作用"。

## 4. 代码注入机制（已选定：Lilu 插件 + 内存补丁）

* 载体：一个自研 kext（`OSBundleLibraries: as.vit9696.Lilu >= 1.6.8`），OpenCore `Kernel > Add` 注入。
* 本机 Lilu 1.7.3 已加载，可用 API（`nm` 实测导出）：`KernelPatcher::routeFunction`(0x…3da0)、
  `routeFunctionLong`、`routeMultiple`、`applyLookupPatch`、`findPattern`、`solveSymbol`、`waitOnKext`。
* **不落盘**：本机 boot KC 文件里搜不到任何注入 kext（Lilu/VirtualSMC 皆无），但运行期类都活着
  → 注入是 OpenCore 在**内存里**完成的，磁盘的 KC 不动。
* 取地址的两种办法（按可靠性排序）：
  1. `KernelPatcher::solveSymbol("__ZN8AppleVTD12initHardwareEP9IOService", …)`——本机 symtab 里这些符号
     `n_type=0x1e`（N_SECT|N_PEXT，私有外部符号），**是否可被 solveSymbol 解析待实测**；
  2. 兜底：拿 `com.apple.iokit.IOPCIFamily` 的 `kmod_info->address` 当基址，加固定偏移
     （本版基址 = `__TEXT` 的 0xffffff8002886000；已算好：`install` = base+0x175f0、`init` = base+0x1773a、
     `initHardware` = base+0x1a6f8、`unit_enable` = base+0x1b158、`contextInvalidate` = base+0x1b3e0、
     `iovmInsert` = base+0x1c736、`newContextPage` = base+0x1c762、`AppleVTDDeviceMapper::forDevice` = base+0x18380）。
     这条路对**锁定 15.8.1** 完全够用，且不依赖符号导出。
* 时序风险：`routeFunction` 必须在 `AppleVTD::start`/`initHardware` 之前完成。Lilu 插件在 `kern_start`
  阶段做 patch（早于 IOKit 匹配），理论上够早，但**必须实测**；不够早就退回"重建 boot KC"（成本高，见地图 §6 的手段）。

## 5. 接管清单与顺序（第一版实现顺序）

按"能启动 → 能建表 → 能映射 → 能失效 → 能报故障"推进，每一步都保留 `-amdvtdoff` 开关：

1. `unit_init` 路径：让 `vtd_unit_t` 指向 AMD 单元的寄存器块，并填好我们自己的附加字段。
2. `unit_enable` / `unit_quiesce`：改用 AMD `CONTROL(0x18)`、`STATUS(0x2020)`。
3. 表基址：AMD `DEVICE_TABLE_BASE(0x0000)`、`CMD_BUF_BASE(0x0008)`、`EVENT_LOG_BASE(0x0010)`、`EXCL_BASE(0x0020)`、`EXT_FEATURE(0x0030)`。
4. 失效：`COMPLETION_WAIT(0x01)`、`INVALIDATE_DEVTAB_ENTRY(0x02)`、`INVALIDATE_IOMMU_PAGES(0x03)`、`INVALIDATE_IOTLB_PAGES(0x04)`——命令码/位布局**写码前逐条对 AMD 规范与 Linux `drivers/iommu/amd/`**。
5. DTE 表：把 Apple 的 root/context 两层树换成**扁平 DTE 数组**，每设备 32 字节，`DTE[0].V/TV/Mode/PageTableRoot`。
6. I/O 页表：AMD 4 级（保留 Apple 的分配/水位逻辑，只换表项编码）。
7. Fault：轮询事件日志（先不做中断），把 `vtd[…] fault:` 格式按 AMD 的 event 结构翻译。
8. 中断重映射（VT-d IR）**第一版整体旁路**：合成 DMAR 里不给 IR 能力，让 Apple 走 `x2APIC` 检查的"不支持 IR"分支；
   代价是 MSI 仍走物理投递（对"让 macOS 用上 AMD IOMMU"这件事不影响）。

> AMD 侧常量在本文件里只是**待核对的参考值**；实现时以 AMD IOMMU 规范（PUB 48882）与 Linux `drivers/iommu/amd/`
> 为准逐项核对。Apple 侧的偏移/字段/位都是本机反汇编直接读出来的，可逐条复核。

## 6. 安全与回滚

* 配置改动**只在 U 盘 OC 配置**上做；内置 ESP 保持不动；稳定后再合并（合并前后比 `sha256`，因为并行的 WiFi 会话在写同一个 config.plist）。
* 每次只改一个变量；每轮改动前记录 config.plist / OC/ACPI / OC/Kexts 的 sha256 与文件清单，写清回滚动作。
* `-amdvtdoff` 总开关：插件读 boot-arg 决定是否接管；出问题改一行 boot-arg 即回到原状，不用删 kext。
* 硬停线：同一改动连续 2 次启动失败，或需要动引导之外的东西（重装 / 关 SSV / 改分区）→ 停下来回报。
* 已知会给别人带来干扰的点：OpenCore 把日志写在自己启动卷的根目录（U 盘启动 → 日志落 U 盘），可据此判断本轮到底走的哪个配置。
