# AppleVTD 逆向地图（第一交付物 · 供审核）

环境：macOS 15.8.1 (24H32) / Darwin 24.6.0 / x86_64，iMacPro1,1，AMD Ryzen 7 9700X + MAXSUN B850BKB。
样本：`/System/Library/KernelCollections/BootKernelExtensions.kc`（66,125,824 B，2026-09-23 10:33）。
本文所有地址都是**本机这一版二进制的实测地址**，换 macOS 版本会变。
全程只读：未改 ESP / config.plist / boot-args / 任何系统文件。

> **2026-10-13 追加：Apple 公开了 IOPCIFamily 源码，本文的"逆向"已有源码权威版本。**
> 本机 kext 对应 tag **`IOPCIFamily-681.140.3`**（用二进制里的断言行号 1053 / 2535 逐一对上），
> 源码已取到 `refs/`。请配合读 `appleVTD-source-notes.md`（结构的字段级真相、二进制符号↔源码行号）。
> 本文保留其价值：**二进制地址、调用链、以及"哪些函数要 route"的定位**。

---

## 0. 一句话结论

AppleVTD **不是独立 kext**，而是 `com.apple.iokit.IOPCIFamily` 里的一个 TU（源文件名 `AppleVTD.cpp`）。
它的"上半场"（ACPI 取值、IOMapper 契约、space 调度、设备→domain 绑定）和"下半场"（VT-d 寄存器、root/context 表、
IOTLB 失效、fault、MSI 重映射）**在同一个 Mach-O 里**，所以路线 ①b 的落点就是这一个 kext 的 3 个函数簇，
而改它的机器码在本机**已有验证过的通道**（见 §6）。

---

## 1. 可复现的取样步骤

```bash
KC=/System/Library/KernelCollections/BootKernelExtensions.kc
# 1) IOPCIFamily 的 fileset 条目：fileoff 41443328 (0x2786000)，长度 282624（下一条目 IOReportFamily 在 41725952）
dd if="$KC" of=/tmp/iopcifamily.entry.macho bs=4096 skip=10118 count=69
# 2) 该条目自带 LC_SYMTAB，但指向 KC 共享 __LINKEDIT（symoff 55757664 / nsyms 1583 / stroff 56898832 / strsize 8650493）
/usr/bin/python3 scripts/extract_kext_symbols.py "$KC" 55757664 1583 56898832 8650493 \
    0xffffff8002886000 0xffffff80028c4000 raw/iopcifamily.symbols.txt     # -> 1027 个符号
# 3) 反汇编要连 LINKEDIT 一起切，并把 KC 绝对 fileoff 改成切片相对偏移
dd if="$KC" of=/tmp/iopcifamily.slice.macho bs=4096 skip=10118 count=6026
/usr/bin/python3 scripts/fixup_kc_slice.py /tmp/iopcifamily.slice.macho 41443328 /tmp/iopcifamily.fixed.macho
xcrun llvm-objdump -d --start-address=0xffffff800289d000 --stop-address=0xffffff80028b4e14 \
    /tmp/iopcifamily.fixed.macho > disasm/applevtd.text.txt                 # 26648 行，带符号名
/usr/bin/python3 scripts/split_functions.py disasm/applevtd.text.txt disasm/fn   # 381 个函数各自成文件
```

节布局（vmaddr / 大小）：`__TEXT` 0xffffff8002886000 / 0x35000，其中 `__text` 0xffffff80028874c0 / 0x2d954、
`__cstring` 0xffffff80028b4e14 / 0x5bfc；`__DATA` 0xffffff80028bb000 / 0x8000；`__DATA_CONST` 0xffffff80028c3000 / 0x1000。

---

## 2. 关键符号清单（AppleVTD / AppleVTDDeviceMapper，共 123 个）

完整表见 `raw/applevtd.symbols.txt`。以下是决定架构的那些（地址为函数入口，括号内为到下一个符号的距离≈函数长度）：

| 地址 | 符号 | 角色 |
| --- | --- | --- |
| `0x289d5f0` | `AppleVTD::install(IOWorkLoop*, uint, IOService*, OSData const*, IOPCIMessagedInterruptController*)` | 唯一入口，由 `IOPCIPlatformInitialize()` 调用 |
| `0x289d73a` | `AppleVTD::init(IOWorkLoop*, OSData const*)` | 解析 DMAR `OSData`、建 `vtd_unit_t` 数组 |
| `0x289deaa` | `AppleVTD::installInterrupts()` | 故障/中断源装配 |
| `0x289defe` | `unit_interrupts_enable(vtd_unit_t*)` *[static]* | 单单元中断使能 |
| `0x289df68` | `AppleVTD::adjustDevice(IOService*)` | 设备→mapper 绑定调整（"mapper activate(%s, 0x%04x)"） |
| `0x289e380` | `AppleVTDDeviceMapper::forDevice(IOService*, uint)` | 取/建每设备 mapper（= 每设备 domain） |
| `0x289e588` | `AppleVTD::removeDevice(IOService*)` | 解绑 |
| `0x289e624` | `AppleVTD::deviceMapperActivate(AppleVTDDeviceMapper*, uint)` | 写上下文项（`did ena qi/ir/trans`） |
| `0x289e88a` | `AppleVTD::relocateDevice(IOService*, bool)` | 设备迁移 |
| `0x289e916` | `AppleVTD::free()` | 释放 |
| `0x289e9ec` | `AppleVTD::space_destroy(vtd_space*)` | IOVA space 销毁 |
| `0x289ebe8` | `AppleVTD::checkFree(vtd_space*, uint)` | 空间校验 |
| `0x289ed19` | `vtd_bitmap_count(...)` *[static]* | 位图统计 |
| `0x289eda8` | `AppleVTD::space_create(uint,uint,uint)` | IOVA space 创建（"domains %d, contextwidth %lld, treebits %d, round %d"） |
| `0x289f3b6` | `vtd_ballocator_init(vtd_space*, uint)` *[static]* | 块分配器 |
| `0x289f494` | `AppleVTD::space_alloc(...)` | IOVA 分配 |
| `0x289fa75` | `vtd_rballoc_fixed(...)` *[static]* | 固定区间分配 |
| `0x289fb6f` | `vtd_space_fault(vtd_space*, uint, uint)` *[static]* | 空间故障处理 |
| `0x289fcbf` | `vtd_space_set(...)` *[static]* | **写页表项（"lvl2 %p[%x] = %p"）** |
| `0x289fda6` | `AppleVTD::space_free(vtd_space*, uint, uint)` | 释放 |
| `0x289ffc8` | `vtd_rbfree(...)` *[static]* | 区间释放 |
| `0x28a010c` | `AppleVTD::space_alloc_fixed(...)` | 固定分配 |
| `0x28a0306` | `AppleVTD::addMemoryRange(uint64, uint64)` | **brief 里那条日志的出处**（`[%s()] AppleVTD is not yet installed as gSystem`） |
| `0x28a039c` | `____ZN8AppleVTD14addMemoryRangeEyy_block_invoke` | 其 block |
| `0x28a04d6` | `AppleVTD::reserveRanges(vtd_space*, bool)` | `ACPI_DMAR_TYPE_RESERVED_MEMORY` 处理 |
| `0x28a06f8` | `AppleVTD::initHardware(IOService*)` | **映射寄存器、读 CAP/ECAP、使能单元** |
| `0x28a1036` | `AppleVTD::handleInterrupt(IOInterruptEventSource*, int)` | 中断处理 |
| `0x28a10b4` | `AppleVTD::handleFault(IOInterruptEventSource*, int)` | 故障处理 |
| `0x28a10f4` | `AppleVTD::timer(OSObject*, IOTimerEventSource*)` | 轮询定时器 |
| `0x28a1158` | `unit_enable(vtd_unit_t*, uint)` *[static]* | **写 GCMD/RTADDR/CCMD/IRTA + 轮询 GSTS** |
| `0x28a13e0` | `AppleVTD::contextInvalidate(uint16)` | 上下文缓存失效（GCMD/CCMD 路径） |
| `0x28a16a2` | `AppleVTD::interruptInvalidate(uint16, uint16)` | 中断缓存失效 |
| `0x28a188d` | `unit_quiesce(vtd_unit_t*)` *[static]* | 单元停用 |
| `0x28a1c76` | `unit_faults(vtd_unit_t*, bool)` *[static]* | 故障寄存器读写（FECTL/FEDATA/FEADDR/记录区） |
| `0x28a1df6` | `AppleVTD::callPlatformFunction(...)` | 平台函数钩子（`IOPlatformQuiesceAction`/`IOPlatformActiveAction`） |
| `0x28a1f50` | `AppleVTD::spaceMapMemory(...)` | IOMapper KPI 实现 |
| `0x28a22c2` | `AppleVTD::spaceInsert(vtd_space*, uint, uint64, uint64, uint64, uint64)` | 插入映射 |
| `0x28a2326` | `AppleVTD::spaceUnmapMemory(...)` | 解除映射 |
| `0x28a2658` | `AppleVTD::spaceMapToPhysicalAddress(vtd_space*, uint64)` | IOVA→PA |
| `0x28a26b2` | `AppleVTD::getPageSize()` | 页大小 |
| `0x28a26be` | `AppleVTD::iovmMapMemory(...)` | IOMapper KPI |
| `0x28a2708` | `AppleVTD::iovmUnmapMemory(...)` | IOMapper KPI |
| `0x28a2722` | `AppleVTD::mapToPhysicalAddress(uint64)` | IOMapper KPI |
| `0x28a2736` | `AppleVTD::iovmInsert(uint,uint64,uint64,uint64,uint64)` | 转发到 `spaceInsert`（读 `this+0x1a8` 当 space） |
| `0x28a2762` | `AppleVTD::newContextPage(uint)` | 建 root/context 页（`"!context_page"`/`"!fRootEntryPage"`） |
| `0x28a28a2`… | `AppleVTDDeviceMapper::*`（含 `initHardware` `0x28a2a6e`、`iovmMapMemory` `0x28a2aaa`、`iovmUnmapMemory` `0x28a2b6e`、`mapToPhysicalAddress` `0x28a2b94`、`iovmInsert` `0x28a2ba8`） | **每设备一 mapper = 每设备一 domain** |
| `0x28a3f25` | `__GLOBAL__sub_I_AppleVTD.cpp` | TU 初始化 |
| `0x28b4a66`+ | `space_create/initHardware/contextInvalidate/interruptInvalidate/spaceUnmapMemory/newContextPage` 的 `.cold.*` | 冷路径（断言失败等） |

私有类型（kalloc_type 站点，来自 `__DATA_CONST`）：`site.vtd_unit_t`(0x28b85da)、`site.vtd_space_t`(0x28b85fc)、
`unit_init(acpi_dmar_hardware_unit*)::kalloc_type_view_143/225`(0x28c3540/0x28c3580)、
`AppleVTD::initHardware(IOService*)::kalloc_type_view_1508`(0x28c3a20)。
→ **`unit_init(acpi_dmar_hardware_unit*)` 是 DMAR 解析器**（被内联，符号表里只剩它的 kalloc_type_view）。

---

## 3. 调用链（上半场）

```
IOPCIPlatformInitialize()                      @ 0x28b069a   （boot 期，IOPCIFamily 内）
  x = <外部函数>()                              @ callq 0xffffff8000ad5eb0   ← 待确认（见 §7 未决项 1）
  tbl = x->getProperty("ACPI Tables")           @ vtable+0x2e8, 字符串 0x28ba7f8
  dmar = tbl->getObject("DMAR")                 @ vtable+0x208, 字符串 0x28ba804
  PE_parse_boot_argn("intcpumax"/"forceintcpu")  @ 0x28ba809 / 0x28ba813
  msic = new IOPCIMessagedInterruptController(0xb8) ; msic->init(0x190, 0x70) ; reserveVectors(...)
  AppleVTD::install(workLoop, vec, proxy, dmar, msic)   @ 0x28b08c3 -> 0x289d5f0
        └─ AppleVTD::init(workLoop, dmar)        @ 0x289d73a
              └─ unit_init(acpi_dmar_hardware_unit*)   （内联；"dmar %p Address %llx, Flags %x"
                                                          "DMAR Width %x, Flags %x"
                                                          "scope type %d, bus %d, path %d,%d"
                                                          "cap 0x%llx extcap 0x%llx glob %d round %d cache sel %d mode %d…"）
  … unit 数组挂在 this+0x100（stride 8，以 NULL 结尾；见 initHardware 的遍历）
AppleVTD::initHardware(IOService*)             @ 0x28a06f8   ← 真正碰硬件
  └─ unit_enable()/unit_quiesce()/unit_faults()/unit_interrupts_enable()
AppleVTDDeviceMapper::forDevice(dev, idx)      @ 0x289e380   ← 设备匹配期
  └─ AppleVTD::deviceMapperActivate(mapper, idx)  @ 0x289e624（写上下文项：qi / ir / trans 位）
```

设备→domain 的挑选在 `IOPCIConfigurator`（`IOPCIConfigurator.cpp`，"Checking device [i%x]%u:%u:%u"、
"Found a match with …"、`findEntryByAddress`）里，与 AppleVTD 通过 `iommu-parent` / `pci-dev-mapper`
属性（字符串 0x28b855d / 0x28b8534）挂钩。

---

## 4. 硬件模型（这是 ①b 的"下半场"，全部有反汇编依据）

### 4.1 `vtd_unit_t` 关键字段（由 `unit_enable`/`initHardware` 的访问反推）

| 偏移 | 内容 | 证据 |
| --- | --- | --- |
| `+0x08` | **MMIO 寄存器基址（内核虚拟地址）** | `movq 0x8(%rbx),%rax` 后直接用 `0x18(%rax)` 等小偏移读写 |
| `+0x10` | 第二个寄存器窗口（其中 `+0x08` 被写 `0x9003000000000000`） | `0x28a1206: movq 0x10(%rbx),%rax; movq %rcx,0x8(%rax)` |
| `+0x38` | root/context 表基址 → 写入寄存器 `0x20`(RTADDR) | `0x28a11c6: movq 0x38(%rbx),%rcx; movq %rcx,0x20(%rax)` |
| `+0x48` | 中断重映射表地址 → 写入寄存器 `0x90` | `0x28a123c: movq 0x48(%rbx),%rcx; movq %rcx,0x90(%rax)` |
| `+0x58` | IR 相关指针（`testb $0x40, 0x84(%rbx)` 分支里用） | 0x28a12b5 |
| `+0x60` | 计数器，使能时清零 | `0x28a1226: movl $0x0, 0x60(%rbx)` |
| `+0x68` | 使能时写入入参（中断向量） | `0x28a1247: movl %r14d, 0x68(%rbx)` |
| `+0x80` | 32 位量，与 `0x200` 比较后取小 | `0x28a0826` 起 |
| `+0x84` | 能力/标志字节（bit3、bit4、bit6 被分别测试） | `testb $0x8/0x10/0x40, 0x84(%rbx)` |

### 4.2 实测到的寄存器偏移（VT-d 寄存器文件）

`unit_enable` 的使能序列（0x28a11b9 起）：

```
reg 0x20 <- unit+0x38              ; Root Table Address
GCMD(0x18) <- 0x4000_0000          ; bit30 = queued-invalidation enable
poll GSTS(0x1c) bit30 set
CCMD(0x28) <- 0xA000_0000_0000_0000; 上下文缓存失效命令；poll 直到 bit63 清零
reg 0x90   <- unit+0x48            ; IRTA
GCMD(0x18) <- 0x0400_0000          ; bit26 = interrupt-remapping enable
poll GSTS(0x1c) bit26 set
reg 0x88   <- 0
```

其它在 AppleVTD 代码里出现的偏移（`data` 段外的），按出现次数：
`0x08`(CAP, 读)、`0x10`(ECAP, 读)、`0x18`(GCMD, 写)、`0x1c`(GSTS, 轮询)、`0x20`(RTADDR)、`0x28`(CCMD)、
`0x34`(FSTS)、`0x38`/`0x3c`/`0x40`(故障事件控制/数据/地址；`unit_faults` 里成组使用)、`0x100`/`0x108`/`0x118`(IOTLB 失效)。

> 名称是 VT-d 规范的标准含义；**偏移本身是从本机反汇编直接读出来的**（§1 的文件里可逐条核对）。

### 4.3 表结构与失效语义（Apple 自己的日志字符串，地址见 `raw/cstrings.txt`）

| 字符串地址 | 内容 | 含义 |
| --- | --- | --- |
| 0x28b8835 | `domains %d, contextwidth %lld, treebits %d, round %d` | root/context **两层树**：domain 数、上下文宽度、树位宽 |
| 0x28b86f3 | `tree root 0x%llx` | 根表物理地址 |
| 0x28b86c0 | `tables %p, %p, %p, %p, %p, %p` | 表页分配 |
| 0x28b86ae | `lvl2 %p[%x] = %p` | 二级页表项写入（`vtd_space_set`） |
| 0x28b868e / 0x28b86df / 0x28b88c3 / 0x28b88da / 0x28b88c1 | `"!lvl2page"` / `"!root_page"` / `"!context_page"` / `"!fRootEntryPage"` | 断言点 |
| 0x28b89bf / 0x28b89d3 / 0x28b89b0 | `"context qi"` / `"interrupt qi"` / `"qfull"` | 队列失效（QI）路径 |
| 0x28b8c1e / 0x28b8c37 / 0x28b8c4b / 0x28b8c57 | `did ena qi p 0x%qx v %p` / `did set ir p 0x%qx` / `did ena ir` / `did ena trans` | **上下文项位：QI、IR、TRANSLATION 使能** |
| 0x28b8c02 / 0x28b8c66 | `unit %p enable status 0x%x` / `… quiesce status 0x%x` | GSTS 读回 |
| 0x28b8c83 | `vtd[%d] fault: device %d:%d:%d reason 0x%x %c:0x%llx` | 故障记录 |
| 0x28b88f3 / 0x28b8933 / 0x28b8973 / 0x28b898c / 0x28b894d | `ir p 0x%qx v %p` / `ir[0x%qx] 0x%qx -> 0x%qx` / `ir[0x%x] …` / `"msi irte 0x%qx prior 0x%qx"` / `"IOPCISetMSIInterrupt(%d, %d)"` | 中断重映射表项（IRTE）路径 |
| 0x28b87d6 | `ACPI_DMAR_TYPE_RESERVED_MEMORY 0x%llx, 0x%llx` | DMAR 保留内存条目 |
| 0x28b8737 | `[%s()] AppleVTD is not yet installed as gSystem` | brief 里那条日志 |
| 0x28b8596 / 0x28b8a92 | `x2APIC mode enabled on at least one IRE, but disabled on other(s)!` / `XNU reports x2APIC mode enabled, but IRE does not support it!` | IR 使能前置检查 |

---

## 5. 切割面：要接管的函数清单（路线 ①b 的"下半场"）

| # | 待接管 | 现在的 VT-d 语义 | 换成 AMD-Vi |
| --- | --- | --- | --- |
| 1 | `unit_init`（DMAR 解析，内联） | 解析 ACPI DMAR → `vtd_unit_t`（MMIO 基址、范围、scope） | 解析 IVRS / 或按合成 DMAR 的约定填 `vtd_unit_t`（AMD 单元是 MMIO 0x0000.. 的寄存器块，DTE 表基址另配） |
| 2 | `unit_enable` / `unit_quiesce` | GCMD 位 + GSTS 轮询 + CCMD/RTADDR/IRTA | AMD `CONTROL(0x18)` 的 `IommuEn`/`EventLogEn`/`ComWaitIntEn` 等 + `STATUS(0x2020)` 轮询 |
| 3 | `contextInvalidate` / `interruptInvalidate` | 同步写 CCMD / IOTLB 寄存器 | AMD 命令缓冲提交 `INVALIDATE_DEVTAB_ENTRY` / `INVALIDATE_IOMMU_PAGES` + `COMPLETION_WAIT` |
| 4 | root/context 两层树（`newContextPage`、`domains/contextwidth/treebits`） | VT-d root→context 两层表 | **AMD 是扁平 DTE 表，按 DeviceID 索引**（结构差异最大的一处） |
| 5 | `vtd_space_set` / `space_set` / `lvl0..lvl2` 页表 | VT-d 页表项（含 snoop control 等位） | AMD I/O 页表（4 级、8 字节项，位定义不同） |
| 6 | `unit_faults` / `handleFault` | FSTS/FECTL/FEDATA/FEADDR + 记录区 | AMD **事件日志**（`EVENT_LOG_BASE 0x0010`，head/tail 0x2010/0x2018） |
| 7 | `unit_interrupts_enable` / IRTE 路径 | VT-d 中断重映射 | AMD IR 表（**建议第一版先不做 IR**，只做 DMA 重映射；用 `-amdvtdoff` 之外的开关隔离） |

**保留不动**：`AppleVTD::install/init` 的骨架、`space_create/alloc/free` 的分配器逻辑（只换它写进表里的位）、
`iovm*` 与 `AppleVTDDeviceMapper::*`（IOMapper KPI 契约）、`handleInterrupt/timer`、`addMemoryRange/reserveRanges`、
`IOPCIConfigurator` 的设备→domain 绑定。

---

## 6. 为什么"改 IOPCIFamily 机器码"在本机是已验证通道

ESP 上的 `config.plist`（不是仓库 `efi/` 里那份过期副本）有 3 条 **Enabled** 的 `Kernel > Patch`，
`Identifier` 全是 `com.apple.iokit.IOPCIFamily`：

```
CaseySJ | probeBusGated | Disable 10 bit tags | 12.0-15.x     21.0.0-24.99.99
CaseySJ | probeBusGated | Disable 10 bit tags | 26.0+         25.0.0-25.99.99
CaseySJ | IOPCIIsHotplugPort | Fix PCI bus enumeration on AM5 22.0.0-25.99.99
```

即：**对 boot KC 里 IOPCIFamily 的机器码做 find/replace，这台机器天天在跑**。所以 §5 的接管不需要
KDK 重建 boot KC，也不需要关 SSV。

---

## 7. 未决项（下一步要验的，不是等用户回答的）

1. ~~`IOPCIPlatformInitialize` 开头那个外部调用是谁~~ → **已定性**：该 TU 未公开，但语义明确 = 从 platform expert
   取 `getProperty("ACPI Tables")` 再取 `"DMAR"`（`AppleVTD.cpp` 用的是 `IOService::getPlatform()`）。
   待运行时由插件打印属性自证。详见 `appleVTD-source-notes.md` §5。
2. ~~`unit+0x10` 那个第二寄存器窗口~~ → **已定性**：是 `vtd_iotlb_registers_t*`（`AppleVTD.h:84`），
   其地址由 ECAP 动态算出（`AppleVTD.cpp:154-158`）。
3. AMD-Vi 侧的寄存器/DTE/命令编码仍要逐项与 AMD IOMMU 规范（PUB 48882）和 Linux `drivers/iommu/amd/` 对齐后再写。
4. 新增：`IOPlatformMapperPresent` 这个"总闸"要不要用（`AppleVTD.cpp:1421`）——见注入文档。

## 8. 附：产物清单

| 文件 | 内容 |
| --- | --- |
| `raw/iopcifamily.entry.macho` | IOPCIFamily fileset 条目（282,624 B） |
| `raw/iopcifamily.symbols.txt` | 该条目地址范围内的 1027 个符号（地址/type/section） |
| `raw/applevtd.symbols.txt` | 其中 AppleVTD/vtd_/unit_ 相关 123 个（已 demangle） |
| `raw/cstrings.txt` | `__cstring` 全表，带地址（1126 条） |
| `disasm/applevtd.text.txt` | AppleVTD 区间反汇编（26,648 行，带符号名） |
| `disasm/fn/` | 按函数拆分的反汇编（381 个）+ `_index.tsv` |
| `scripts/*.py` | 上面三支可复现脚本 |
