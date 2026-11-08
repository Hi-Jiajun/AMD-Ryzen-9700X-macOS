# AppleVTD 是开源的 —— 源码级证据与本机版本匹配

这条把上一轮的"逆向"降级成"读源码 + 定版本"：**Apple 公开了 `apple-oss-distributions/IOPCIFamily`，
仓库里直接有 `AppleVTD.cpp` / `AppleVTD.h` / `dmar.h` / `balloc.c` / `rballoc.c` /
`IOPCIMessagedInterruptController.cpp`。**

---

## 1. 本机这一版对应哪个 tag（已用二进制指纹判定）

判定方法：Apple 的断言展开成 `leaq "断言文本" @%s:%d ; leaq "AppleVTD.cpp" ; movl $行号, %edx ; callq panic`，
**行号是编译进二进制的立即数**，可与源码逐一对上：

| 冷路径（本机二进制） | 断言文本 | 二进制行号 | tag `681.140.3` | tag `726.100.6` |
| --- | --- | --- | --- | --- |
| `AppleVTD::space_create(...).cold.1` @0x28b4a66 | `"!lvl2page"` | `0x41d` = **1053** | **1053 ✓** | 1047 ✗ |
| `AppleVTD::newContextPage(...).cold.1` @0x28b4b56 | `"!context_page"` | `0x9e7` = **2535** | **2535 ✓** | 2529 ✗ |

→ **本机 /System/Library/Extensions/IOPCIFamily.kext（macOS 15.8.1 / 24H32）对应 tag `IOPCIFamily-681.140.3`。**

已下载到 `refs/`（tag 记在 `refs/SOURCE-TAG.txt`），sha256：

```
8ba954e0670557968d889b29bb66ce67c5d62376beac90bf7dd9a739bb8f8bf0  AppleVTD.cpp   (77071 B)
0f8f96873e9535d987d1ca72ecfa7a63e14f616169fb52113bb03b2b076ce3d4  AppleVTD.h     (14685 B)
e312b6fd5f52b99dda7439fc858cbd2f5527890597b1c6404714ccf07890834e  dmar.h          (4711 B)
```

旁证：`kmutil showloaded` 显示 IOPCIFamily 装载于 `0xffffff8002886000`、大小 `0x35000`——与上一轮从
KC 里量出的 `__TEXT` 完全一致，所以地图里"基址 + 偏移"的算法就地成立。

---

## 2. 结构体（源码级，含 Apple 自己标注的偏移）

### 2.1 `vtd_registers_t`（`AppleVTD.h:46`）—— VT-d 寄存器文件

| off | 字段 | off | 字段 |
| --- | --- | --- | --- |
| 00 | version | 48 | res3[2] |
| 08 | capability | 58 | advanced_fault |
| 10 | extended_capability | 64/68/6c/70/78 | protected memory 相关 |
| 18 | global_command | 80/88/90 | invalidation_queue_head / _tail / _address |
| 1c | global_status | 9c | invalidation_completion_status |
| 20 | root_entry_table | a0/a4/a8/ac | invalidation_completion_event control/data/addr/upper |
| 28 | context_command | b8 | interrupt_remapping_table |
| 34 | fault_status | 38/3c/40/44 | fault_event control/data/addr/upper |

### 2.2 其它类型

* `vtd_iotlb_registers_t{ address; command; }`（`AppleVTD.h:84`）、`vtd_fault_registers_t{ fault_low; fault_high; }`（`:89`）
  —— 这两块的**基址是从 CAP/ECAP 动态算出来的**（`AppleVTD.cpp:154-158`：`offset = (extended_capability >> 4) & ~0x3ff` 等），
  所以上一轮在二进制里看到的 `0x100/0x108` 是运行期值，不是硬编码。
* `root_entry_t{ context_entry_ptr; resv; }`、`context_entry_t{ address_space_root; context_entry; }`（`:100`/`:106`）
* `qi_descriptor_t{ command; address; }`（`:112`）、`ir_descriptor_t{ data; source; }`（`:118`）
* `vtd_unit_t`（`:178`）：`dmar / regs / iotlb / faults / qi_map / qi_table / qi_table_stamps / root / msi_address /
  qi_address / qi_stamp_address / ir_address / qi_tail / qi_mask / qi_stamp / qi_stalled_stamp / msi_data /
  num_fault / hwrounding / rounding / domains` + 8 个位域 `global ig caching translating selective qi intmapper x2apic_mode`
  → 与上一轮从二进制反推的 `unit+0x08 = regs`、`unit+0x10 = iotlb`、`+0x38 = root`、`+0x48 = ir_address(0x90 寄存器)` 完全一致。
* `vtd_space`（`:295`） = 上一轮的 `vtd_space_t`；分配器在 `balloc.c` / `rballoc.c`（红黑树）。
* 常量：`kMaxUnits 8`（`:170`）、`kIRCount 512`（`:171`）、`kMaxMemRegions 64`（`:173`）。

### 2.3 `dmar.h` —— 合成 DMAR 必须满足的结构

`ACPI_TABLE_DMAR` 就是"36 字节头 + Width + Flags + 10 保留"；`ACPI_DMAR_HARDWARE_UNIT` =
`{Header(u16 type,u16 len); u8 Flags; u8 Reserved; u16 Segment; u64 Address;}`，
`ACPI_DMAR_INCLUDE_ALL (1)`；`ACPI_DMAR_RESERVED_MEMORY = {…; u64 Base; u64 End;}`；
device scope = `{u8 EntryType; u8 Length; u16 Reserved; u8 EnumerationId; u8 Bus;}` + `pci_path{Device,Function}` 列表。
→ 合成表的字段级要求已完全确定，见 `synthetic-dmar-and-injection.md`。

---

## 3. 二进制符号 ↔ 源码（`refs/AppleVTD.cpp` 行号）

| 二进制地址 | 符号 | 源码位置 |
| --- | --- | --- |
| — | `unit_init(ACPI_DMAR_HARDWARE_UNIT*)`（上一轮只见到它的 kalloc_type_view） | **`:133`** |
| `0x289d73a` | `AppleVTD::init(IOWorkLoop*, const OSData*)` | `:797` |
| `0x289d5f0` | `AppleVTD::install(IOWorkLoop*, uint32_t, IOService*, const OSData*, IOPCIMessagedInterruptController*)` | `:621` |
| `0x289df68` | `AppleVTD::adjustDevice(IOService*)` | `:664` |
| `0x289e588` / `0x289e88a` | `removeDevice` / `relocateDevice` | `:760` / `:779` |
| `0x289e916` | `free()` | `:850` |
| `0x289e9ec` | `space_destroy` | `:872` |
| `0x289eda8` | `space_create` | `:946` |
| `0x289f494` / `0x289fda6` / `0x28a010c` | `space_alloc` / `space_free` / `space_alloc_fixed` | `:1100` / `:1231` / `:1283` |
| `0x28a0306` / `0x28a04d6` | `addMemoryRange` / `reserveRanges` | `:1322` / `:1360` |
| `0x28a06f8` | **`initHardware`** | **`:1407`** |
| `0x28a1036` / `0x28a10b4` / `0x28a10f4` | `handleInterrupt` / `handleFault` / `timer` | `:1842` / `:1859` / `:1870` |
| `0x28a1df6` | `callPlatformFunction` | `:1884` |
| `0x28a1f50` | `spaceMapMemory` | `:1921` |
| `0x28a2326` | `spaceUnmapMemory` | `:2074` |
| `0x289ebe8` | `checkFree` | `:2215` |
| `0x28a13e0` | **`contextInvalidate(uint16_t)`** | **`:2259`** |
| `0x28a16a2` | **`interruptInvalidate`** | **`:2341`** |
| `0x28a2658` / `0x28a22c2` / `0x28a26b2` | `spaceMapToPhysicalAddress` / `spaceInsert` / `getPageSize` | `:2398` / `:2422` / `:2453` |
| `0x28a26be` … `0x28a2736` | `iovmMapMemory` / `iovmUnmapMemory` / `mapToPhysicalAddress` / `iovmInsert` | `:2459` / `:2475` / `:2483` / `:2489` |
| `0x28a2762` | `newContextPage` | `:2500` |
| `0x289e624` | `deviceMapperActivate` | `:2551` |

---

## 4. 两个只有源码才看得见的开关

1. **`IOPlatformMapperPresent`**（`AppleVTD.cpp:1421`）：
   `fDisabled = (!IOService::getPlatform()->getProperty(kIOPlatformMapperPresentKey));`
   → platform expert 上**没有**这个属性，AppleVTD 就直接把自己禁用；释放时还会 `removeProperty`（`:647`）。
   这是我们手上最直接的"额外闸门/开关"（也让合成 DMAR 之外多了一条注入杠杆）。
2. **`pci-dev-mapper`**（`AppleVTD.cpp:691`）：boot-arg，按 PCI 地址强制指定 mapper；调试每设备 domain 时可用。

## 5. DMAR 在哪里被取出来 + **谁在什么时候调它**（已解）

`IOPCIPlatformInitialize()` 的符号在二进制里（`0x28b069a`），但**这个 TU 不在 Apple 公布的仓库里**
（仓库树里没有 `IOPCIFamily.cpp`；`IOPCIBridge.cpp` 也没有该函数）。所以"取 `ACPI Tables` → `DMAR`"
这一段仍只有反汇编证据。不过语义已明确：对象要能 `getProperty("ACPI Tables")`，
而 `AppleVTD.cpp` 用的是 `IOService::getPlatform()`，即**platform expert**。
→ 合成 DMAR 的落点 = 让 platform expert 的 ACPI 表字典里出现 `DMAR`（OpenCore `ACPI > Add` 正是干这个的，
社区先例 CaseySJ `SSDT-DMAR.aml`）。运行时可以用我们的插件把 `getPlatform()->getProperty("ACPI Tables")`
打出来自证（下一轮 U 盘验证时做）。

**调用点已定位（2026-10-13 追加）**：用 `scripts/find_callsite_in_kc.py` 扫整份 boot KC 的 `E8 rel32`，
`IOPCIPlatformInitialize` 只有一个直接调用者：

```
direct E8 calls to 0xffffff80028b069a: 1
  from 0xffffff80014093b7  in com.apple.driver.AppleACPIPlatform
```

（自检：同一脚本对 `AppleVTD::install` 报出唯一调用者 `0xffffff80028b08c3` / IOPCIFamily，与反汇编一致。）

**含义（对就位时机）**：

- AppleVTD 的 `install` 链是在 **AppleACPIPlatform 启动期**被触发的，不是 IOPCIFamily 自己的 `_start`；
- 所以我们的补丁只要**早于 AppleACPIPlatform 的 `IOService::start`** 即可。OpenCore 把 kext 注进 boot KC 后，
  各 kext 的 `_start` 在 IOService 匹配之前执行，理论上有窗口——但**必须实测**（stage 1 探针：
  在插件 `_start` 与钩子里各打一条日志，看先后与是否命中）；
- 「ACPI Tables/DMAR」的那个 platform expert 就是 **AppleACPIPlatform**，所以合成 DMAR 走 OC `ACPI > Add`
  正是打到它嘴边。

## 6. 对路线的影响（一句话）

①b 从"二进制级替换"升级成**源码级移植**：照 `AppleVTD.cpp` 的结构写一份 AMD-Vi 版的"下半场"
（`vtd_registers_t` → AMD IOMMU 寄存器组；`root_entry_t`/`context_entry_t` 两层树 → 扁平 DTE；
`qi_descriptor_t` → AMD 命令缓冲；`vtd_fault_registers_t` → AMD 事件日志），
再用 Lilu 插件把这些函数体在内存里接管。上层（`install/init/space_*/iovm*/deviceMapper*`）**原样保留**。

---

## 7. 本机 IOMMU 与"ACPI Tables"归属（2026-10-13 实测，合成 DMAR 的关键参数）

### 7.1 AMD IOMMU 设备就在 IORegistry 里，但 **MMIO 被 macOS 藏了**

```
IORegistry:  PCI0@0 / IOMA@0,2      class-code 0x080600   pci1022,14d9   pcidebug "0:0:2"
             acpi-path = "IOACPIPlane:/_SB/PCI0@0/IOMA@2"      （ACPI 名 IOMA = I/O Memory Management Unit）
             IOInterruptControllers = ("IOPCIMessagedInterruptController")
             IOInterruptSpecifiers  = (<0a00000000000100>)      ← IOPCIFamily 已经给它配了 MSI
             "reg" = <00020000...0000>   ← 只有 devfn，**没有 BAR**
             没有 "assigned-addresses" / 没有 IODeviceMemory
```

→ **DeviceID = 0x0002**（bus0/dev0/fun2）；而 **IOMMU 的 MMIO 基址拿不到**，必须从 **IVRS** 里读
（AMD 平台由 IVRS 描述单元寄存器块，不保证出现在设备 `_CRS`/BAR 里）。这条直接决定合成 DMAR 的
`ACPI_DMAR_HARDWARE_UNIT.Address` 从哪来。

### 7.2 `"ACPI Tables"` 就一个来源

扫整份 boot KC：`ACPI Tables` 只出现 3 次 ——

```
com.apple.driver.AppleACPIPlatform   19927040  (2 处)
com.apple.iokit.IOPCIFamily          41443328  (1 处，即 IOPCIPlatformInitialize 的读取点)
```

→ 属性由 **AppleACPIPlatform** 提供（它是 platform expert，所以 `IOService::getPlatform()->getProperty("ACPI Tables")` 成立）；
它**不序列化进 IORegistry**，所以 `ioreg` 看不到（这也解释了上一轮为什么 grep 不到）。
顺便解释了为什么我们的插件**可以在内核里读它**：和 IOPCIFamily 走同一条路。

### 7.3 由此得到的更优入口（比静态 `ACPI > Add` 更好）

插件在早期自己取 `getProperty("ACPI Tables")` → 读出 **IVRS** → 解析出单元 MMIO 基址 →
**运行时合成 DMAR** 再喂给 AppleVTD（或直接交给 `install()`）。好处：

* 地址是真实的、随固件走，不需要在 config.plist 里硬编码；
* 完全不碰固件层 ACPI 表集合（比 OC `ACPI > Add` 更少副作用，回滚更干净）；
* 顺带成了"我们确实走通了这条链"的自证。

代价与前置：插件必须早于 AppleACPIPlatform 触发 `IOPCIPlatformInitialize`（时序仍是 stage 1 要实测的硬未知），
且该属性此刻可读。**若时序不成立**，退回 CaseySJ 派的静态 `ACPI > Add`（社区已验证该路径能被 AppleVTD 吃到）。

---

## 8. 重要副作用：Apple 的 AppleVTD 里本来就有 AMD 分支

读源码时撞到一处必须知道的事实——**AppleVTD 对"AMD 设备"有专门处理**（跟平台是不是 AMD 无关，看的是**设备厂商 ID**）：

```
AppleVTD.cpp:2651   isAMD = (0x1002 == (vendorProduct & 0xffff));      // PCI vendor = AMD
AppleVTD.cpp:2658   mapper->fIsAMD = isAMD;
AppleVTD.cpp:2579   if (mapper->fIsAMD) mapper->fSpace = mapper->fVTD->fAMDSpace;   // 共享一个 space
AppleVTD.cpp:2593   if (mapper->fIsAMD && !mapper->fVTD->fAMDSpace) fAMDSpace = space;
AppleVTD.cpp:2622   // The AMD mapper space is persistent
AppleVTD.cpp:2623   if ((kDeviceMapperDeactivate & options) && !(mapper->fIsAMD)) { space_destroy(...) }
AppleVTD.cpp:2660   mapper->fAllFunctions = (0x91201b4b|0x91231b4b|0x91281b4b|...)   // 部分 AMD SATA/AHCI 设备要多功能映射
```

含义（对本项目）：

* Apple 的粒度是**每个 mapper space 一个 domain**；AMD 厂商设备**共用一个持久 space**，非 AMD 设备各用各的。
  → 我们"每设备一个 domain"的实现按 **space** 建表是对的（`av_space` 正是这个粒度），不需要改设计，但**不能假设每个 PCI 设备都有独立 domain**。
* `fAMDSpace` 是跨 mapper 复用的：**空间生命周期与设备生命周期解耦**（设备 deactivate 时不像非 AMD 设备那样销毁 space）。
  我们的 DTE/页表回收逻辑必须照这个语义写，否则会在设备热插拔/暂停时把共享表拆掉。
* 这也解释了本机某些 AMD 侧设备（`1022:…`）为何会走"共享 space"分支——stage 1 的探针日志里 `forDevice/mapperActivate` 的
  `mapper->fIsAMD` 差异会直接看得到。

---

## 9. 第二个隐藏闸门：`IOPlatformMapperPresent`（**入口必须补上**）

`AppleVTD::initHardware()` 的第一件事就是自我禁用检查：

```
AppleVTD.cpp:1421   fDisabled = (!IOService::getPlatform()->getProperty(kIOPlatformMapperPresentKey));
AppleVTD.cpp:647    （free 时）IOService::getPlatform()->removeProperty(kIOPlatformMapperPresentKey);
```

**这个属性是谁发的？** 扫整份 boot KC，`IOPlatformMapperPresent` 只出现 3 次，归属：

| 归属 | 说明 |
| --- | --- |
| `com.apple.driver.AppleACPIPlatform` | **发布者**（与 `"ACPI Tables"` 同一路径：固件有 DMAR 才会发） |
| `com.apple.iokit.IOPCIFamily` | 消费/清理者（即 AppleVTD） |
| `com.apple.kernel` | 内核侧引用（`IOMapper` 体系） |

含义：**在 AMD 平台上即使我们塞进了合成 DMAR，AppleVTD 仍会在 `initHardware` 里把自己禁用**——
因为没人发这个属性。所以入口（`impl/src/av_entry.cpp`）在调用原 `install` 之前**主动 setProperty**
把这个属性置上（已实现，见 `av_enable_platform_mapper()`，并会打印原有值）。

这条也顺手解释了 brief 里那句日志的完整因果：
`AppleVTD is not yet installed as gSystem` ← 没有 DMAR → 没进 `install` → `gSystem` 空；
而即便进了 `install`，没有 `IOPlatformMapperPresent` 也照样 `fDisabled`。
