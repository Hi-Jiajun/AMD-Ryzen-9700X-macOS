# AMD-Vi 移植对照表（路线 ①b 的"下半场"）

左列证据 = `refs/AppleVTD.cpp`（tag `IOPCIFamily-681.140.3`，与本机二进制行号指纹一致）；
右列证据 = `refs/linux-amd_iommu_types.h` / `refs/linux-amd-iommu.c`（Linux `drivers/iommu/amd/`）。
**右列常量属"参考实现"，写码前仍需与 AMD IOMMU 规范（PUB 48882）逐项核对**；左列是本机这一版的事实。

---

## 1. 寄存器组对照

| 用途 | VT-d（Apple `vtd_registers_t` 偏移） | AMD-Vi（MMIO 偏移，Linux 定义） |
| --- | --- | --- |
| 设备/上下文表根 | `root_entry_table` **0x20** | `MMIO_DEV_TABLE_OFFSET` **0x0000** |
| 命令缓冲 / 失效队列 | `context_command` **0x28** + QI：`invalidation_queue_head/tail/address` **0x80/0x88/0x90** | `MMIO_CMD_BUF_OFFSET` **0x0008**，head **0x2000** / tail **0x2008**（8192 B / 512 项） |
| 故障/事件 | `fault_status` **0x34**、`fault_event_*` **0x38-0x44**、`advanced_fault` **0x58** | `MMIO_EVT_BUF_OFFSET` **0x0010**，head **0x2010** / tail **0x2018** |
| 全局命令/状态 | `global_command` **0x18** / `global_status` **0x1c** | `MMIO_CONTROL_OFFSET` **0x0018** / `MMIO_STATUS_OFFSET` **0x2020** |
| 能力 | `capability` 0x08 / `extended_capability` 0x10 | `MMIO_CAP_HDR_OFFSET` 0x00 / `MMIO_EXT_FEATURES` **0x0030** |
| IOTLB 失效 | `vtd_iotlb_registers_t{address,command}`（**由 ECAP 动态算基址**，`AppleVTD.cpp:154-158`） | 无独立寄存器：改用命令 `CMD_INV_IOTLB_PAGES` |
| 中断重映射表 | `interrupt_remapping_table` **0xb8** | 无单独寄存器：`DTE` 的 IR 位 + `EXT_FEATURES` |

> ⚠️ **实现要点（源码级）**：`AppleVTD.cpp:151` 只映射 **0x1000 字节**（`ml_io_map(dmar->Address, 0x1000)`）。
> AMD 的 STATUS/head/tail 在 **0x2000-0x2048**，落在这一页之外 → 接管时必须自己重新映射（≥0x2100），
> 或把 `unit->regs` 指到我们自己的映射上。

## 2. 失效/命令对照

AMD 命令是 16 字节、opcode 位于 `data[1]` 的 bit28-31（Linux `CMD_SET_TYPE`）；VT-d 走 GCMD 位 + IOTLB 寄存器。

| Apple 侧函数（源码行） | VT-d 做的事 | AMD-Vi 替换 |
| --- | --- | --- |
| `AppleVTD::contextInvalidate(uint16_t)` `:2259` | 写 `CCMD` / QI，失效 domain 的上下文缓存 | `CMD_INV_DEV_ENTRY` = **0x02**（参数 = DeviceID） |
| IOTLB 失效（散落在 `space_alloc`/`space_free`/`spaceUnmapMemory` 路径） | 写 `iotlb->command` | `CMD_INV_IOTLB_PAGES` = **0x04**；页表失效用 `CMD_INV_IOMMU_PAGES` = **0x03**（含 S/PDE/GN 位） |
| `AppleVTD::interruptInvalidate(uint16,uint16)` `:2341` | 失效 IRTE 区间 | `CMD_INV_IRT` = **0x05**（第一版可不做：IR 整体旁路） |
| 等待失效完成（`panic("qfull")`/`("context qi")`/`("interrupt qi")` 周边） | 轮询 QI 完成 | `CMD_COMPL_WAIT` = **0x01** + `CMD_COMPL_WAIT_STORE_MASK`(0x01) + 轮询 store 地址 |
| 单元停用 `unit_quiesce` | `global_command = 0` + 等 status 位清 | `CONTROL` 清 `IOMMU_EN`(bit0)，等 `STATUS` 运行位清 |
| 单元使能 `unit_enable` | QIE→RTADDR→CCMD→IRTA→IRE | 写 `DEV_TABLE_BASE`/`CMD_BUF_BASE`/`EVT_BUF_BASE` → `CONTROL` 置 `IOMMU_EN`(0)、`CMDBUF_EN`(12)、`EVT_LOG_EN`(2) → 等 `STATUS` |
| 全清（可选） | — | `CMD_INV_ALL` = **0x08** |

## 3. 表结构对照（结构差异最大）

> 先记一条 Apple 侧的事实（`AppleVTD.cpp:2579-2623`）：**AMD 厂商（0x1002）设备共用一个持久 mapper space**
> （`fAMDSpace`），非 AMD 设备才每设备一个；粒度是 **space**，不是 PCI 设备。我们的 `av_space` 按 space 建模，
> 但要照 Apple 的语义做 space 生命周期（AMD 空间不随设备 deactivate 销毁）。

* VT-d：`root_entry_t`（8 项/页）→ `context_entry_t`，**两层树**；Apple 用 `domains` / `contextwidth` / `treebits`
  三个量描述（日志 `domains %d, contextwidth %lld, treebits %d, round %d`），建表在 `newContextPage()` `:2500`、
  绑定在 `deviceMapperActivate()` `:2551`。
* AMD-Vi：**扁平 DTE 表**，每个 DeviceID 32 字节；Linux 位定义：`V` = bit0（`DEV_ENTRY_VALID`）、
  `TV` = bit1（`DEV_ENTRY_TRANSLATION`）、`Mode` = bits **9-11**（`DEV_ENTRY_MODE_SHIFT=9`、`MASK=0x07`）、
  页表根指针紧跟其后（mask 参考 `PM_ADDR_MASK 0x000ffffffffff000`）。
* 影响：`newContextPage` 不再需要；`deviceMapperActivate` 改成"按 DeviceID 写 DTE"；
  `space_create` 的树参数只需保留页表级数。

## 4. 页表对照

* VT-d：Apple 用 `lvl0page` / `lvl2page` + `address_space_root`，表项写入集中在 `vtd_space_set()`（`:1053` 断言）与
  `space_alloc`/`space_free` 路径。
* AMD-Vi：页表级数由 DTE 的 Mode 位决定——Linux 侧枚举是 `PAGE_MODE_NONE(0x00)` …
  `PAGE_MODE_4_LEVEL(0x04)` … `PAGE_MODE_7_LEVEL(0x07)`（`linux-amd_iommu_types.h:326-333`），
  地址位 `PM_ADDR_MASK = 0x000ffffffffff000`（`:338`）；DTE 里 Mode 占 bits 9-11
  （`DEV_ENTRY_MODE_SHIFT=9`、`DEV_ENTRY_MODE_MASK=0x07`，`:231-232`）。
  **页表项（PTE）位编码已补齐**（`refs/linux-v6.6-amd_iommu_types.h`，Linux 当前 master 已把该文件移出，故取 v6.6）：

| 位 | 名字 | 含义 |
| --- | --- | --- |
| bit0 | `IOMMU_PTE_PR` | **present** |
| bit9-11 | `PM_PTE_LEVEL(pte) = (pte>>9)&0x7` / `IOMMU_PTE_MODE` | 叶/非叶与页级（0x0=4KB 叶…；与 DTE 的 `PAGE_MODE_*` 同域） |
| bit52-12 | `IOMMU_PAGE_MASK = ((1<<52)-1) & ~0xfff` | 下一级表/页的物理地址 |
| bit59 | `IOMMU_PTE_U` | 用户可访问 |
| bit60 | `IOMMU_PTE_FC` | force-coherent |
| bit61/62 | `IOMMU_PTE_IR` / `_IW` | 读/写许可（对应 `IOMMU_PROT_IR=1`、`IOMMU_PROT_IW=2`） |

  Apple 侧对应物：`vtd_space_set()` 写 `lvl0page/lvl2page` 时用的 VT-d 表项位（含 snoop-control 等），
  换成上表即可，分配器不动。
* 可以**原样保留**的是 Apple 的 IOVA **分配器**：`balloc.c` / `rballoc.c`（红黑树 + 位图），
  只把"写表项"那一步换成 AMD 编码。

## 5. 故障路径对照

* VT-d：`fault_status` 0x34 + `fault_event_*` + `vtd_fault_registers_t{fault_low,fault_high}`，
  由 `AppleVTD::handleFault()` `:1859` + `unit_faults()` 处理，日志格式 `vtd[%d] fault: device %d:%d:%d reason 0x%x %c:0x%llx`。
* AMD-Vi：**事件日志**（`EVT_BUF_BASE` + head/tail），事件项 16 字节。
* 第一版策略：**轮询事件日志**（不开中断），把 AMD 事件字段翻译进 Apple 原有的日志格式，便于对照取证。

## 6. 保留不动（上半场）

`AppleVTD::install/init` 骨架、`space_alloc/space_free/space_alloc_fixed/checkFree` 的分配逻辑、
`balloc.c`/`rballoc.c`、`iovmMapMemory/iovmUnmapMemory/mapToPhysicalAddress/iovmInsert`、
`AppleVTDDeviceMapper::*`、`addMemoryRange/reserveRanges`、`handleInterrupt/timer/callPlatformFunction`，
以及 `IOPCIConfigurator` 的设备→domain 绑定（`IOPCIConfigurator.cpp`）。

## 7. 接管顺序（与注入文档 §5 一致）

1. 重映射寄存器窗口（≥0x2100）+ `unit_init` 的 `regs/iotlb/faults` 指向改造后的结构；
2. `unit_quiesce` / `unit_enable`（CONTROL/STATUS）；
3. 命令缓冲 + `CMD_COMPL_WAIT`（先把失效通路打通）；
4. DTE 表（`deviceMapperActivate` → 写 DTE；`contextInvalidate` → `CMD_INV_DEV_ENTRY`）；
5. I/O 页表（`vtd_space_set` 等 → AMD 编码）；
6. 事件日志（轮询）+ 故障日志翻译；
7. IR 整体跳过（合成 DMAR 里不声明 `ACPI_DMAR_INTR_REMAP`）。

---

## 8. 实测出来的对象布局（stage 2 直接依赖，写错会伤设备）

这三个偏移决定"给哪个 DeviceID 写 DTE"，写错等于**给错误的设备打开地址翻译**，所以逐条列证据：

| 对象/字段 | 偏移 | 证据（本机二进制反汇编，`refs/AppleVTD.h` 字段顺序对照） |
| --- | --- | --- |
| `AppleVTDDeviceMapper::fIsAMD` | `0xEC` | `forDevice`: `cmpl $0x1002,%r15d`（AMD 厂商 ID）紧接 `sete 0xec(%rbx)` |
| `AppleVTDDeviceMapper::fSourceID` | `0xE8` | `deviceMapperActivate`: `movl %eax,0xe8(%rbx)`（值 = `bus<<8\|dev<<3\|func`，源码 2563），随后两处 `movl 0xe8(%rbx),%edx` 读回算 `idx` |
| `AppleVTDDeviceMapper::fSpace` | `0xE0` | **5 个函数独立读同一偏移**并把它当 space 传出去：`deviceMapperActivate`（`movq %r14,0xe0` / `cmpq $0,0xe0`）、`iovmMapMemory`、`iovmUnmapMemory`、`mapToPhysicalAddress`、`iovmInsert`（都是 `movq 0xe0(%rdi),%rsi`） |
| `vtd_space_t::domain` | `0x18` | `space_create`: `movl %ebx,0x18(%r13)` 写入后两次 `movl 0x18(%r13),%esi` 读回（对应源码里紧随的两次 `vtd_bitmap_*` 调用）；同函数 `vsize=0x10` / `rsize=0x14` 与源码顺序一致 |

运行期交叉验证：stage 1 的探针会把 `mapper` / `space` 前 0x40–0x50 字节按字打印出来
（`av_dump_words`），上机后可用它核对上面每个偏移的实际取值（例如 `mapper+0xE0` 应是指向 `vtd_space_t` 的内核指针、
`mapper+0xE8` 应是 `0x0000xxxx` 形状的 `bus:dev.func`）。

## 9. 命令/DTE 编码审计（2026-10-14，全部对照 `refs/` 里的 Linux 参考逐条核对）

写这段是因为 stage 2 **一碰硬件就有可能是不可回退的**：命令编码错不会崩在明面上，
而是让 IOMMU 报"非法命令"事件后**停止消费命令缓冲**（之后所有失效都静默丢失），
或者让 DTE 看起来完全正常但设备一个 DMA 都做不了。全部结论都带参考行号。

### 9.1 已修（都是硬件致命级，且以前在主机上一测就能发现）

| # | 问题 | 依据 | 修法 |
| --- | --- | --- | --- |
| 1 | **所有命令的操作码写进了错误的 dword**。AMD 命令是 16 字节 = 4 个 dword，操作码必须在 `data[1]` bits 28-31：`#define CMD_SET_TYPE(cmd,t) ((cmd)->data[1] \|= ((t) << 28))`（`refs/linux-amd-iommu.c:50`）。旧代码的 `av_cmd_submit(u, d0, d1)` 把 `d1` 写进字节 8-15，于是操作码落到 `data[2]` | `refs/linux-amd-iommu.c:50`、构建器 `:1278/:1292/:1338/:1355` | 编码器改为**显式吃 4 个 dword**（`av_amdvi_bits.h` 的 `av_cmd_*`），`av_cmd_submit` 也改签名为 `const uint32_t d[4]` |
| 2 | **COMPLETION_WAIT 完全没有操作码**（旧 `av_cmd_wait` 只填了 store 地址与数据）。操作码 0 是保留值 → 非法命令 → 后续失效静默丢失 | `refs/linux-amd-iommu.c:1278 build_completion_wait()` | `av_cmd_completion_wait()` 填 `data[1] = addr_hi \| (0x01 << 28)`；等待循环加 `pause` + 编译器屏障（store 由 IOMMU 完成，读不能被提出循环） |
| 3 | **INVALIDATE_IOMMU_PAGES / INVALIDATE_IOTLB_PAGES 的 id 与地址两半写反**：Linux 把 id 放 `data[1]`（IOMMU_PAGES 是 domid、IOTLB 是 DeviceID，见 `data[0]`），地址放 `data[2]/[3]`；旧代码把地址放在 `data[0]/[1]`、把 id 放在 `data[2]` | `refs/linux-amd-iommu.c:1338`、`:1355` | 按参考逐字段重写 |
| 4 | **整段失效缺 S（size）位**：`build_inv_address()` 对"整段"返回 `CMD_INV_IOMMU_ALL_PAGES_ADDRESS \| CMD_INV_IOMMU_PAGES_SIZE_MASK`，S 位是地址字段的一部分。旧代码用 `0x7fff_ffff_ffff_f000`（S=0）→ 命令含义变成"只失效 0x7fff_ffff_ffff_f000 这一个 4 KB 页"，整段刷新静默失效 | `refs/linux-amd-iommu.c:1325`、`refs/linux-amd_iommu_types.h:204-211` | 新增 `AV_INV_ALL_PAGES_ADDR`（含 S 位） |
| 5 | **DTE 没有置 IR/IW**（读/写许可）。参考里 `amd_iommu_set_dte_v1()` 把 `DTE_FLAG_TV \| mode \| root \| DTE_FLAG_IR \| DTE_FLAG_IW` 写进 `data[0]` | `refs/linux-amd-iommu.c:2146` | `av_dte_encode()` 补 `AV_DTE_IR \| AV_DTE_IW` |

> 这几条现在**都锁进主机测试**了（`impl/tests/av_ivrs_test.cpp`，`bash impl/tests/run.sh`）：
> 逐条断言操作码落在 `data[1]` bits 28-31、地址落在 `data[2]/[3]`、S 位在、DTE 有 IR/IW。
> 旧测试里那条 `DTE encode: addr|mode<<9|V|TV` 正是把 bug 当成预期，已一并改正。

### 9.2 有意为之、不是 bug

- **`data[1]` 里的域 id 用 0**（`AV_V1_DOMAIN`）：参考会把真实 domid 写进 DTE `data[1]` 的 `DTE_DOMID_MASK`（`:2146`），
  并按 domid 过滤 `INVALIDATE_IOMMU_PAGES`。v1 这里给每个设备各自的页表根、靠**按 DeviceID 的 IOTLB 失效**拿精确性，
  所以统一用一个平坦域；`av_inv_iommu_pages()` 因此显式刷**域 0** 的整段范围，
  而不是把上层传进来的 DeviceID 当 domid 用（否则会刷一个不存在的域、等于什么都没刷）。
  三处必须保持一致：DTE 写 0、`av_dte_set` 注释、`av_inv_iommu_pages`。
- **MMIO 窗口按 `0x2100` 映射**：Apple 只映射 `0x1000`（`refs/AppleVTD.cpp:151`），AMD 的
  `STATUS`(0x2020)/head/tail 在 0x2000 以上，所以必须自己映射更大窗口（`AV_REG_WINDOW_SIZE`）。

### 9.3 与 Apple 侧对照后的两条结论

- **`unit_init()` 会把 AMD MMIO 当 VT-d 用**：它 `ml_io_map(dmar->Address, 0x1000)` 后按
  VT-d 语义解读 `capability`/`extended_capability`（`refs/AppleVTD.cpp:151-175`），并据此算出
  `iotlb`/`faults` 的偏移与 `num_fault`/`domains`。这就是 `av_unit_fixup()` 存在的原因，
  也是**绝不能把 Apple 的 `initHardware` 放过去**的原因。
- **`unit_quiesce()` 里有无超时自旋**：它写 `global_command = 0` 后
  `while (((1UL<<26)|(1<<31)|(1<<25)) & global_status) {}`（`refs/AppleVTD.cpp:261-272`）。
  AMD 在该偏移不是 GLSTS，位随机 → 一旦命中就是死循环。`unit_enable`/`unit_quiesce` 已在 20 个钩子里，
  且 stage 2 必须"钩子全装齐才允许跑"——见 `av_hooks_complete()` 的互锁与 `stage1/STATUS.md`。

### 9.4 启用/关闭时序与事件日志（同样逐条对照 `refs/linux-amd-init.c`）

| # | 问题 | 依据 | 修法 |
| --- | --- | --- | --- |
| 6 | **失效超时从没设过**：CONTROL bits 5-7 一直是 0（`CTRL_INV_TO_NONE`）= 无超时；卡住的失效会让 IOMMU 永久卡死且我们察觉不到。Linux 明确设 1 秒 | `refs/linux-amd-init.c:2784`、档位表 `:183-190` | 新增 `av_ctrl_enable_value()`，把超时钉成 1 s |
| 7 | **多设了 `CONTROL_COMWAIT_EN`**。它只管 COMPLETION_WAIT 的**中断**，与我们轮询的"完成存储"无关（`:1278`）；Linux 从不设它（init.c/iommu.c 里 COMWAIT 零命中）。而 MSI 地址寄存器（`MMIO_MSI_ADDR_LO/HI_OFFSET`）我们根本不写，一旦固件留了值就可能朝旧向量发中断 | `refs/linux-amd_iommu_types.h:159` | 启用掩码里显式清掉 |
| 8 | **quiesce 顺序**：我们一次写清三个位；Linux 是分开写且有序——先命令缓冲、再事件日志、最后才关 IOMMU 本体 | `refs/linux-amd-init.c:430-455 iommu_disable()` | 改成同样顺序分开写 |
| 9 | **MMIO 宽度**：CONTROL 是 64 位寄存器（Linux `readq/writeq`，`:402`），而 head/tail 是 32 位（Linux `writel`，`:787-788`、`:889`）——我们两处都写反了（CONTROL 用 32 位 RMW、TAIL 用 64 位访问） | 同上 | 宽度对齐参考 |
| 10 | **事件日志溢出后不再重启**：溢出时 IOMMU 会**停止记录**（STATUS 的 EventLogRun=BIT(3) 清空），溢出位要"写 1 清零"，然后靠切换 log/int-enable 位重启。`av_evt_poll()` 只排空环形缓冲、从不看 STATUS → 第一次溢出之后**所有 fault 静默丢失**，而 v1 的 fault 通道就是轮询 | `refs/linux-amd-init.c:730-758`、掩码 `refs/linux-amd_iommu_types.h:118-121` | 按同一序列加 `av_evtlog_stopped()` 判定 + 重启；Linux 只在 run 位为空时重启（溢出位有但仍在跑则不重启），这点也照做 |

**核对无误、未改动的部分**（同样值得记下来）：`av_events.cpp` 的字段布局与
`refs/linux-amd-iommu.c:954-960` **逐位一致**——type 取 `w1` bits 28-31、DeviceID 取 `w0` bits 0-15、
pasid 取 `w0` bits 16-19 与 `w1` bits 0-15、flags 取 `w1` bits 16-27、address 取 `w3<<32|w2`；
`av_dte_encode` 的页表根掩码与 Linux `IOMMU_PAGE_MASK`（`(1<<52)-1 & ~0xfff`）一致。
这些现在都有主机测试断言（`bash impl/tests/run.sh`，63+ 条，0 failure）。

### 9.5 页表层号约定与页池（核对无误）

与 `refs/linux-v6.6-io_pgtable.c` 的 `__get_next_pte()` 逐条对齐：根表按 level 3 索引；
**level-L 表里的表项携带 L**（`:258 __npte = PM_LEVEL_PDE(level, ...)`，`level` 是"包含该表项的表的层号"）；
叶子在 level 0（`PM_LEVEL_ENC(0) == 0`）；双方都拒绝 level skipping（`:265 if (pte_level != level) return NULL`）。
`av_pool` 用 `IOMallocContiguous` 交出真物理地址并按页切分 ✔（IOMMU 只吃物理地址）。

## 10. stage 2 的**结构性缺口**：我们替换掉 initHardware 后，Apple 的软件状态没人初始化

这一节是 2026-10-14 审计的**最重要**发现，它改的是计划本身，所以单独列。

### 10.1 事实链（全部带行号）

1. `AppleVTD::initHardware()` 里除了碰硬件，还负责**一整套软件状态**的初始化：
   - `fDisabled / fIsSystem / free_mask = kFreeQElems - 1`（`refs/AppleVTD.cpp:1421-1424`，`kFreeQElems=256` 见 `refs/AppleVTD.h:292`）
   - **`fTreeBits`**（`:1430,1439`，从 `unit->regs->capability` 推出来）
   - `fContextWidth`（`:1433-1441`）
   - **`fDomainSize = 512`**，再与 `unit->domains` 取小（`:1444-1447`）
   - `fMaxRoundSize`（`:1455-1458`）
   - **`fHWLock = VTHWLOCKALLOC()`**（`:1464`；`VTHWLOCKALLOC == IOLockAlloc`，`refs/AppleVTD.h:36`）
   - **`fDomainBitmap = vtd_bitmap_alloc(fDomainSize); vtd_bitmap_bitset(..., 0)`**（`:1466-1467`）
   - **`fSpace = space_create(kVPages, kBPagesLog2, kRPages)`** + `reserveRanges(fSpace, true)`（`:1468-1478`）
   - 之后才是 context page / context entry / unit_enable 等硬件侧动作。
2. 我们**整段替换**了 `AppleVTD::initHardware`（`av_hooks` 里它由 `av_entry.cpp::probeInitHardware` 接管，
   stage 1 直接 `return true`、stage 2 走 `av_stage2_unit_bringup`）——所以上面这些**一条都不会被设置**。
3. 但**上半场真的会读它们**。`AppleVTD::space_create()`（`refs/AppleVTD.cpp:946` 起）：
   - `:960 treebits = fTreeBits;` → `:964 vtassert(treebits > 12)`，
     再用它算整棵表的几何：`:983 treebits -= 12`、`:995-1005 while (bit < treebits) {...}`、`:1030 vtassert(level < arrayCount(bf->tables))`。
     `fTreeBits` 若为 0，几何直接退化（release 下 `treebits -= 12` 还会下溢）。
   - `:977 domain = vtd_bitmap_count(fDomainBitmap, true, 0, fDomainSize);`
     `:981 if (domain == fDomainSize) break;` → `fDomainSize == 0` 时**必然 break 并返回 NULL**，
     而 `fDomainBitmap` 若为 NULL，中间那两步还会解引用空指针。
   - `:976/:979 VTHWLOCK(fHWLock)` → `fHWLock` 为 NULL 时是锁空指针。
4. 而我们的 `probe_space_create` 是**转发**给 Apple 原函数的（`impl/src/av_hooks.cpp:177-195`：
   `space = orig_space_create(...)`，之后才附加我们的 AMD 页表根）。也就是说 Apple 这段代码**真的会跑**，
   不是死代码。
   （这也符合目标里"保留 AppleVTD 上半场"的要求：IOVA 分配器 balloc/rballoc 就是它。）

**结论：按现在的设计，stage 2 一进制 `space_create` 就会失败（或空指针崩）——不是"性能问题"，是走不通。**

### 10.2 两条修法（这条要你拍板）

**A. 我们的替身把软件状态也一起初始化**（照抄 Apple 的初始化清单）

- 需要 `IOLockAlloc()`（简单）、`fTreeBits=48`/`fContextWidth` 定成 4 级页表对应值（简单）、`fDomainSize=512`（简单）、
  `free_mask=255`、`fDisabled=fIsSystem` 按 `IOPlatformMapperPresent` 定（简单）。
- **卡在 `fDomainBitmap`**：`vtd_bitmap_alloc` / `vtd_bitmap_bitset` 在 `refs/AppleVTD.h` 里没有定义、
  在符号表里也不存在（整个 IOPCIFamily 只有 `vtd_bitmap_count` 是真符号），说明它们是别的源文件里的
  static inline，`vtd_bitmap_t` 也只有前置声明。要走 A 就得**反汇编把 `vtd_bitmap_t` 的布局扒出来**
  （`disasm/` 里有 `vtd_bitmap_count` 的切片可以起点），或者补路由 `vtd_bitmap_count` 拿它做判据 → 大约 1–2 轮活。
- 还要**主动调用** Apple 的 `space_create` + `reserveRanges`（偏移已在 `raw/applevtd.symbols.txt`：
  `space_create`=`0x018da8`、`reserveRanges`=`0x…8a04d6`），把它们当成"库函数"用。

**B. 给 Apple 一个假的 VT-d 寄存器文件，让它自己的 initHardware 照常跑完**（我倾向这条）

- 做法：`av_units_fixup()` 重建 unit 时，除了现有字段，把 `unit->regs` 指向**我们自己映射的一页 scratch**，
  在里面按 VT-d 的形状**合成** CAP/ECAP：让 `(0x100 << fContextWidth) & capability` 恰好在 4 级页表对应的宽度上成立
  （于是 `fTreeBits` 得到我们想要的 48）、`unit->domains = 512`、`rounding` 给一个能让 `:1461 !fMaxRoundSize` 不返回 false 的值。
  然后**不再整段替换 `initHardware`**，而是照旧只钩那 20 个硬件函数（`unit_enable`/`unit_quiesce`/
  `unit_interrupts_enable`/`unit_faults`/`newContextPage`/…，已经是现状）。
- 好处：`fDomainBitmap`、`fHWLock`、`fSpace`、`reserveRanges`、上下文页、域位图——**全部由 Apple 自己的代码完成**，
  我们一行都不用抄，也就绕开了 A 里那个"扒 bitmap 布局"的活。而且 Apple 的 MMIO **写**全部落在 scratch RAM 上
  （对硬件零影响），真正的 AMD 编程仍只在我们的钩子里发生。
- 需要先验证/决定的三件事（都要证据，不能猜）：
  1. `rounding` 给什么值最安全（它决定 `fMaxRoundSize`；我们的页表只做 4K 叶子，
     `av_pgtable.cpp` 只建 4K 叶）。要确认上半场里除了已被我们钩住的 map 路径之外，还有谁读 `fMaxRoundSize`；
  2. `initHardware` 后半段往它的 context page 里写的那些 context entry 是否有副作用（目前看只写自己的缓冲）；
  3. 有没有**没被钩住**的函数会去读 `unit->regs` 指向的真 MMIO——已知 `handleInterrupt`/`timer`/`unit_faults` 都已在钩子表里。

### 10.3 决定：走 B（2026-10-14），三点前置已逐条验证通过并已实现

选 B 而不是 A，理由是证据而不是偏好：B 不需要抄 Apple 的软件初始化，也就不需要反汇编 `vtd_bitmap_t`；
而且它更贴合目标里"保留上半场、只换硬件操作"的措辞。10.2 里列的三点待验证，全部查清：

1. **`fMaxRoundSize` 只被两处读**：`refs/AppleVTD.cpp:1178`（`space_alloc`）与 `:1243`（`space_free`），
   都是 `if (hwalign > fMaxRoundSize) hwalign = fMaxRoundSize;`——**只是把对齐钳小**，
   所以给一个偏保守的值（12 = 4 KB，与"只建 4K 叶子"一致）是安全的。
   顺带查出一个关键细节：`:1455` 那次赋值**被 `unit->selective` 门控**，
   所以只把 `rounding` 置非零还不够——**`selective` 必须为 1**，否则 `fMaxRoundSize` 仍是 0，
   而 `:1463 if (!fMaxRoundSize) return (false);` 会让整个 initHardware 直接返回失败。
   （现有 `av_unit_fixup` 恰恰把 `F_SELECTIVE` 清掉了，这就是"Apple 必然失败"的直接原因，已改。）
2. **`initHardware` 后半段只写自己的主机内存**：context entry / root entry / `fIRTable` 全在它自己
   `IOBufferMemoryDescriptor` 的映射里（`:1520-1590`），另外只做两件对 unit 的写入：`unit->root` 和
   `unit->ir_address` 指针字段。`mapInterrupts` 又依赖 `unit->intmapper`，而我们把它清成 0 → 整段 IR 路径跳过。
3. **没有任何"没被钩住"的函数会碰 `unit->regs`**：全文件 `regs->` 的访问点只落在
   `unit_init`（`:154-190`，纯读，且在我们 fixup 之前就已经跑完）以及
   `unit_faults`(:233) / `unit_quiesce`(:261) / `unit_interrupts_enable`(:275) / `unit_enable`(:297)
   ——**后四个全在 20 条钩子表里**。也就是说 Apple 对"寄存器"的读写要么落在我们的 scratch 页，
   要么根本不会执行。`:362` 之后全文件再无 `regs->` 访问。

**已实现**（都在 `impl/src/`）：

- `av_unit_fixup.cpp`：新增 `av_make_fake_regs()`——分配一页 scratch（`AV_REG_WINDOW_SIZE`），
  写入合成的 VT-d 寄存器文件：CAP 置 `bit 10`（等价 `kAddressWidth48`，令 Apple 的宽度循环选中它、
  `fTreeBits = 30 + 9*2 = 48`，正好是 4 级 AMD 页表的深度）与 `bits 48-53 = 12`（4 KB）；
  ECAP 置 `bit 1`（QI，避免被当成无用 unit）。`av_fix_one_unit()` 改为把 `unit->regs` 指向这个 scratch 页
  （不再指向真 MMIO——真 MMIO 由我们自己的 `av_unit_attach` 单独映射），
  `rounding/hwrounding = 12`，并把 `F_SELECTIVE` 置 1。
- `av_entry.cpp::probeInitHardware()`：stage 2 的顺序改为**先**做我们的 AMD bring-up，
  **再**转发给 Apple 的 `initHardware` 让它的软件半场跑完；bring-up 失败就**不转发**
  （AppleVTD 保持今天这样禁用，fail-safe）。

失败模式与回滚：这条路径只在 `-amdvtamd` 且 20 条钩子全装齐时才可达（`av_hooks_complete()` 互锁），
U 盘上现在没有 `-amdvtamd`；`-amdvtdoff` 仍是总开关。

## 11. 钩子边界复核（B 让 Apple 的软件半场活起来之后，2026-10-14）

改成 B 之后 Apple 的软件半场会真跑，所以边界必须重新回答两个问题。

### 11.1 还有没有"没被钩住"的硬件路径？——没有

全文件 `unit->regs->` 的访问点只有：`unit_init`（`:154-190`，**纯读**，发生在 `AppleVTD::init` 里、
也就是我们 fixup **之前**，读的是 Apple 自己那 4 KB 映射）以及
`unit_faults`(:233) / `unit_quiesce`(:261) / `unit_interrupts_enable`(:275) / `unit_enable`(:297)。
**后四个全在 20 条钩子表里**，且 `:362` 之后全文件再无 `regs->` 访问。
结论：不存在"绕过钩子去写真 IOMMU"的路径；Apple 的寄存器读写要么落在 scratch 页，要么根本不执行。
（对 `interruptInvalidate` / `handleInterrupt` / `handleFault` / `timer` 也成立——它们要么已被钩住，
要么只读已经被置零的 VT-d-only 指针字段。）

### 11.2 被钩住的函数会不会破坏 Apple 的簿记？——不会

- **映射路径是"先转发、再追加"**：`probe_space_insert` / `probe_space_unmap` 先调 Apple 原函数
  （保留 balloc/rballoc 记账与它自己那份 VT-d 形态的表），**然后**才把同一段 range 编进我们的 AMD 页表；
  `probe_space_destroy` / `probe_space_free` 也都转发。
- **IOVA 分配器保持 Apple 的**：`space_alloc` / `space_alloc_fixed` / `reserveRanges` 都没钩，
  照原样跑。它们最终调用的 `vtd_space_set`（全文件唯一调用点是 `space_alloc:1201`）只写
  Apple 自己的 `bf->tables[...]`（用 `wireRange` 钉住的自己的内存），**对硬件零影响**；
  也就是说 Apple 会同时维护一份"VT-d 形状的表"，那是无害且自洽的（`fTreeBits=48` 已保证几何一致），
  真正被 AMD IOMMU 走的是我们的页表 + DTE。
- `newContextPage` 全文件唯一调用点是 `deviceMapperActivate:2571`（**返回值被忽略**），
  而我们的 `probe_new_ctx_page` 是转发（`AV_FWD` 本身就把原函数返回值带回来）——不是"丢了返回值"。

### 11.3 顺带修掉的一处不对称（`impl/src/av_hooks.cpp`）

原来无论 Apple 的 `spaceInsert` 成功与否，我们都会去编 AMD 页表。现在**只在 `rc == 0` 时**才追加映射
（Apple 因为 IOVA 耗尽/参数错误而拒绝时，我们不该让设备仍能 DMA 到它认为空闲的地址）。
**unmap 故意不这样门控**：清掉一条陈旧的 AMD 表项，永远比留着它安全——这个不对称是有意的，已写进注释。

### 11.4 复核后确认"不是漏洞"的一处

`probe_space_free` 只转发、不做 AMD 侧动作，看起来像"释放了却没收表"。核对调用点：
`space_free` 全文件唯一调用点是 `:2245`，位于 Apple 的 **unmap 路径**里，而那条路径正是
`probe_space_unmap` 已经清掉我们 AMD 表项的地方。所以这里没有洞，不需要额外动作。

## 12. 已知限制与回滚语义（写清楚，免得第一次上机误判）

### 12.1 页表池与 space 槽位：会**大声**退化，不会静默出错

- `av_stage2_unit_bringup()` 建 `av_pool_init(&g_pt_pool, 64)`（64 页 = 256 KB）与 `AV_MAX_SPACES 32`。
- 池只往外发页、**从不回收**（`av_stage2_space_drop` 只是把槽位标空）。也就是说
  Apple 建/销 space 的循环会永久消耗页：约 15–30 个 space 之后，`alloc_page` 返回 0，
  `av_pt_map4k` 返回 -1，我们会打 `stage2: spaceInsert AMD mapping failed`。
- 失败模式是**安全且可诊断的**：没有映射 ⇒ 设备 DMA 走到未映射 IOVA ⇒ 触发 IO_PAGE_FAULT 事件，
  被我们的轮询打印成 `vtd[%d] fault: ...`；**不会**变成"映射到错误的内存"。
- 第一次上机如果看到这两条日志（`mapping failed` / `root allocation failed`），
  先把 `g_pt_pool` 的页数调大（`av_stage2.cpp` 里 `av_pool_init(&g_pt_pool, 64)`），
  而不是怀疑寄存器/编码。

### 12.2 `-amdvtdoff` 的语义（已静态核对，目标要求的"总开关"成立）

- 插件构建：`impl/src/av_lilu.cpp:103` 把它登记成 Lilu 的 `bootargOff`。Lilu 的 `shouldLoad()`
  因此直接返回错误，`kern_start` **不会**调用 `pluginStart()` ⇒ 一个钩子都不装、一行 AVLOG 都不打。
- 独立构建：`av_plugin.cpp:av_disabled()` 用 `PE_parse_boot_argn("-amdvtdoff", ...)` 自查 ✔。
- **副作用（判读要注意）**：被 `-amdvtdoff` 关掉时，Lilu 的 `PRODUCT_NAME::probe()` 返回 nullptr，
  连它那个同名 service 节点都不会出现在 IORegistry 里。所以"ioreg 里什么都没有"在这一种情况下是
  **预期行为**，不代表没注入 —— `stage1/analyze-log.sh` 现在会读内核 boot-args 把这种情况单独报成
  `[SKIP] ... disabled ON PURPOSE`，而不是误报成 FAIL。

## 13. 又两处真缺陷（2026-10-14 第二轮只读审计的产出，已修）

### 13.1 映射描述符算术：把"页内偏移"减到了物理侧（会映射到**错误的页**）

Apple 自己的算术是**两侧都按页粒度、字节偏移只作用在 IOVA 侧**：

```c
// refs/AppleVTD.cpp:2432-2435  AppleVTD::spaceInsert
	addr = atop_64(mapAddress);
	offset = atop_64(mapAddress + byteOffset) - addr;
	phys = atop_64(physicalAddress);
	pageCount = atop_64(physicalAddress + length) - phys;
// refs/AppleVTD.cpp:2100-2101  AppleVTD::spaceUnmapMemory
	addr = atop_64(mapAddress);
	pages = atop_64(round_page_64(mapAddress + mapLength)) - addr;
```

（`atop_64` 是"地址→页号"，于是 `atop(a)+atop(a+o)-atop(a) == atop(a+o)`，物理侧始终从 `atop(pa)` 起。）
而我们原来的 `probe_space_insert` 把 `delta = map_addr & 0xfff` **从物理基址里减掉**了：
只要 `map_addr` 不是页对齐，IOVA 页就会映到**比正确物理页低一页**的地方——
这是"映射到错误内存"，不是"少映射一点"。`probe_space_unmap` 有同样的偏移问题，
并且少了 Apple 那边的"结束页向上取整"。

修法：把这套算术抽成纯函数 `av_insert_span()` / `av_unmap_span()`（`impl/src/av_amdvi_bits.h`），
两个钩子改为调用它，并**用主机测试把语义钉住**（7 条断言，含"非对齐 map_addr 不得动物理基址"）。

### 13.2 `av_unit_enable()` 不是幂等的（重复使能会清掉已排队的命令）

Apple 不止一处使能单元：`refs/AppleVTD.cpp:1656`（initHardware）与 **`:1897`（`callPlatformFunction` 路径）**。
原来的 `av_unit_enable()` 每次都会把 `CMD_HEAD/CMD_TAIL/EVT_HEAD/EVT_TAIL` 清零——
也就是说，在后来的某次重新使能时，**已经排进命令缓冲、但还没被 IOMMU 消费的命令会被静默丢掉**
（失效命令被丢掉 = TLB 里留着陈旧表项）。

修法：只在**首次**使能（`u->enabled == false`）时复位这四个 head/tail；重复调用只重写基址寄存器与 CONTROL。

## 14. 缺陷三：失效命令是"发了就不管"的（`av_cmd_wait` 从来没被调用）

**发现过程**：审 `av_space.cpp` 时看到 `av_space_map4k`/`unmap4k` 会"失效所有共享设备"，
于是去查有没有等失效完成——结果 `av_cmd_wait()`（就是我前面修好操作码的那个 COMPLETION_WAIT）
**全项目零个调用点**，是死代码。也就是说：控制器是**异步**消费命令缓冲的，而我们发完失效就返回。

**为什么这是安全问题**：unmap 的方向最危险——`spaceUnmapMemory` 返回后，上层立刻可以释放并复用那片内存，
而设备的 IOTLB 里可能还留着旧表项 ⇒ 设备继续往**已经属于别人的内存**里 DMA。
DTE 写/清同理：`av_dte_set` 返回时新表项未必生效（"bound" 日志会撒谎），
`av_dte_clear` 返回时设备可能还在走我们即将遗忘的那棵表。
Linux 的做法一直是配套的：每次 flush 都先排失效命令、再排一个 COMPLETION_WAIT 并等它
（`refs/linux-amd-iommu.c:1278 build_completion_wait()` + `iommu_completion_wait`）。

**修法**（`impl/src/av_backend.*`、`av_space.*`、`av_stage2.cpp`）：

1. 每个 unit 多一页 **DMA 可见的信号页**（`sem_mem`/`sem_pa`），并新增
   `av_unit_wait_completion(idx)`——每次都发一个**新的**序号当哨兵（避免把上一轮的旧值当成本轮完成）。
2. `av_dte_set()` / `av_dte_clear()`：写完表项、发 `INVALIDATE_DEV_TAB_ENTRY` 之后**等它完成**才返回
   （每设备一次、频率低，不心疼）。
3. **失效从"每 4KB 一页"里挪出来**：`av_space_map4k`/`unmap4k` 现在只动页表，不动失效；
   新增 `av_space_flush(s)` = 失效所有绑定设备 + **等一次**；
   由 `av_stage2_space_map_range`/`unmap_range` **每个 range 调一次**，
   `av_stage2_inv_domain` 的分支也一样。1 MB 映射因此只等 1 次，而不是 256 次（与 Linux 的批量方式一致）。
4. `struct av_unit_ops` 增加 `flush_complete` 回调，好让主机测试能断言"确实等了"。

**测试证据**（`bash impl/tests/run.sh`，0 failure）：mock 里加了 `flush_complete` 计数，断言
①`map4k`/`unmap4k` 单独**不**失效、②`av_space_flush` 先失效 N 个设备再等**一次**、
③共享 space 的失效覆盖全部绑定设备。另外 unmap 的"没这条表项"降级为**提示性日志**而不是失败——
因为我们清的是按页向上取整后的**超集**（见 §13.1），尾页本来就可能没有东西可清；
**只有 flush 失败才算致命**。

## 15. 缺陷四（迄今最严重）：`unit->qi_stamp` 永远不会前进 → Apple 的自旋等它会**死等**

### 15.1 事实链

1. Apple 用一套"延迟释放队列"管理 IOVA 回收：unmap 时把 (addr,size,space) 入队，
   并为每个 unit 记一个戳 `stamp = ++fQIStamp[unitIdx]`（`refs/AppleVTD.cpp:2148-2149`，初值 `:1593 fQIStamp[idx]=0x100`），
   同时把这个戳写进 `unit->qi_table_stamps[idx]`（`:2170`、`:2199`）。
2. 释放门控在 `stampPassed(unit->qi_stamp, 记录的戳)` 上（`checkFree:2233`），
   而 `stampPassed(a,b)` 的定义是 **`(((int32_t)((a)-(b))) >= 0)`**（`:104`）。
3. **有 9 处 `WAIT_QI_FREE` 会自旋等它**，而且是**没有超时**的忙等：

   ```c
   #define WAIT_QI_FREE(unit, idx)                                        \
   	if (!stampPassed((unit)->qi_stamp, (unit)->qi_table_stamps[(idx)])) \
   	{                                                                  \
   		(unit)->qi_stalled_stamp = (unit)->qi_table_stamps[(idx)];      \
   		while (!stampPassed((unit)->qi_stamp, (unit)->qi_table_stamps[(idx)])) {} \
   	}
   ```

   分布：`spaceUnmapMemory` 里两处（`:2158`、`:2194`）、`checkFree` 里七处（`:2283-2315`、`:2365-2373`）。
   两条路径我们都够得着：`spaceUnmapMemory` 是我们**转发**的（§11）；`checkFree` 被 `space_destroy`（`:886`）
   与 10 ms 的 `timer`（`:1875`）调用，而 `timer` 我们也转发。
4. **而 `unit->qi_stamp` 在我们的配置里永远停在 0**。它的写入者只有两个：
   - Apple 自己 `unit_enable()` 的 `:331 unit->qi_stamp = qi_stamp` —— **这个函数被我们替换掉了**；
   - **VT-d 硬件的回写**：initHardware 把 `qi_stamp_address` 指到 `&unit->qi_stamp` 本身（`:1616`），
     等 QI 完成后由**芯片**把戳写回这块主机内存 —— **AMD 的 IOMMU 没有这个机制**。
5. 后果：`stampPassed(0, 0x101)` = `(int32_t)(0-0x101) < 0` = false ⇒
   **第一次用某个 QI 槽位没事（记录戳为 0，`stampPassed(0,0)` 成立），第二次开始就是死等**。
   同时 `checkFree` 永远释放不了 → IOVA 只出不回。
   现象级别：**系统在正常 unmap/destroy 时卡死**（不是崩，是挂住）。
6. 也不能简单地把 Apple 的 `unit_enable` 放过去让它自己写：那个函数里有**在 VT-d 状态寄存器上的无超时自旋**
   （`:312 while (!((1UL<<30) & global_status))`、`:317`、`:271`），而在我们的方案里这些寄存器落在
   **scratch 页**上、永远不会回应 ⇒ 换一种死等。

### 15.2 修法：由我们"回放"这个硬件回显

VT-d 那句回写的**语义**是"我排的失效已经完成了"。而这件事**我们已经能证明**——
`av_unit_wait_completion()` 就是靠 COMPLETION_WAIT 确认命令队列排空的（§14）。所以：

1. `av_unit_qi_seed(unit, stamp)`：在我们的 `unit_enable` 钩子里补上 Apple `:331` 的**主机内存那一半**
   （Apple 把戳作为第二个参数传进来了，我们手里就有），不碰任何寄存器。
2. `av_units_qi_echo()`：在**每次 COMPLETION_WAIT 成功之后**调用，把 `unit->qi_stamp` 抬到
   Apple 已记录戳里的**最大值**（`av_qi_stamp_advance()`，纯函数、有单测）。
   于是 `stampPassed` 为真、Apple 的 `WAIT_QI_FREE` 立刻通过、`checkFree` 也能正常回收 IOVA——
   而且**这个"完成"是真的**，不是为了让循环退出而撒谎。

实现落在 `av_unit_fixup.cpp`（它本来就持有 unit 结构体的偏移；新增 `U_QI_MASK 0x64`/`U_QI_STAMP 0x68`，
并记住第一个可用 unit），钩子在 `av_hooks.cpp` 的 `probe_unit_enable`，回显挂在 `av_backend.cpp` 的
`av_unit_wait_completion()` 里（一处覆盖所有路径）。

**测试**（`bash impl/tests/run.sh`，10 条新断言全过）：`stampPassed` 的相等/落后/超前/回绕边界，
以及"抬到最大、绝不倒退、容忍空数组/空区间"。

## 16. 缺陷五：命令/事件环的 head/tail 是**字节偏移**，我们写的是槽位下标

### 16.1 事实

AMD 的 `CmdBufHead/Tail` 与 `EvtLogHead/Tail` 都是**环形缓冲里的字节偏移**，每次前进一个 16 字节条目、
按整环大小取模。Linux 两处都写得很直白：

```c
// 命令环  refs/linux-amd-iommu.c:1417
	next_tail = (iommu->cmd_buf_tail + sizeof(*cmd)) % CMD_BUFFER_SIZE;   // 8192
// 事件环  refs/linux-amd-iommu.c:1043-1052
	head = readl(MMIO_EVT_HEAD); tail = readl(MMIO_EVT_TAIL);
	while (head != tail) {
		iommu_print_event(iommu, iommu->evt_buf + head);   // head 直接当字节偏移用
		head = (head + EVTLOG_ENTRY_SIZE) % amd_iommu_evtlog_size;
		writel(head, MMIO_EVT_HEAD);
	}
```

而我们原来的 `av_cmd_submit()` 是 `slot = cmd_tail % 512`、然后 `cmd_tail += 1` 并把**这个下标**写进
`CMD_TAIL`；`av_evt_poll()` 则拿硬件写回的**字节** tail 去比我们自己的**下标** head。

### 16.2 后果（不是内存安全问题，但是"整机失效"）

写进去的 tail 永远小于一个条目（1…511），硬件按 16 字节粒度一遮，`head == tail` 恒成立 ⇒
**IOMMU 一条命令都不会消费**。于是 DTE 永远不生效、"bound" 是假的、失效全部静默丢失；
事件环那半边则是拿下标比字节，永远对不上、排空逻辑失效。
侥幸的是这一条**会自己喊出来**：`av_dte_set()` 现在会等 COMPLETION_WAIT（§14），
等不到就返回 false，于是会看到 `bind DeviceID ... failed`——而不是一个说不清原因的挂死。

### 16.3 修法

在 `av_amdvi_bits.h` 里把"环单位"固化下来并做成纯函数：
`AV_RING_ENTRY_BYTES 16`、`AV_RING_SLOTS (8192/16 = 512)`、`av_ring_next()`、`av_ring_slot()`；
`av_cmd_submit()` 与 `av_evt_poll()` 两侧都改用它们（写 `CMD_TAIL`/`EVT_HEAD` 的都是字节偏移）。

**测试**：几何（16/512）、前进一格 = 16 字节、`8176 → 0` 的回绕、字节→槽位映射、
以及"走满 512 格回到 0"——全部通过，防止以后有人再把它当数组下标用。

## 17. 第一次碰硬件前的两处加固（2026-10-14，非缺陷、按参考对齐）

### 17.1 使能顺序：先把两个环拉起来，最后才开 IOMMU

Linux 的顺序是 `iommu_enable_command_buffer()` → `iommu_enable_event_buffer()` → … → **最后** `iommu_enable()`
（`refs/linux-amd-init.c:2893-2899`）。我们原来是**一次写**把 `CMDBUF_EN`/`EVT_LOG_EN`/`IOMMU_EN` 一起置上——
也就是理论上有那么一个瞬间，IOMMU 已经开始翻译、而命令缓冲还没配好。现在改成两次写：
先 `CMDBUF_EN|EVT_LOG_EN`（连同 1 秒失效超时），再单独置 `IOMMU_EN`。
（`av_ctrl_enable_value()` 仍然负责 COMWAIT_EN 保持清零那件事，见 §9.4。）

### 17.2 加了一道**活体**健康检查：单元不应答就拒绝继续

`av_stage2_unit_bringup()` 在 `av_unit_enable()` 之后**立刻发一个 COMPLETION_WAIT 并要求它完成**。
不应答就：`av_unit_quiesce()` 收尾、`return false`，于是 `probeInitHardware` **不转发**给 Apple 的
`initHardware`、AppleVTD 保持今天这样禁用、机器照常可用。

为什么值得单列：这是"第一次写真硬件"之前**唯一能拿到活体证据**的关卡。它能区分
"寄存器块不是我们以为的那个"（IVRS 的 MMIO 基址错了、固件把单元按住了、命令环没配对…）
和"一切正常"——把前者从"半初始化后什么都不工作、原因不明"变成一行明确日志 + 安全退出。
换句话说，即使 stage 2 第一次上机失败，我们也能从日志里分清是**没到硬件**还是**到了但没配对**。

### 17.3 顺带核对无误

`av_amdvi.h` 的 MMIO 访问宏（`AV_MMIO_R32/R64/W32/W64`）是 `*(volatile T *)(base + off)` +
写后 `mfence`，对 x86 MMIO 来说正确（volatile 防编译器优化，fence 保证对设备可见的顺序）。

## 18. 两处"最容易致命、又最容易被忽略"的东西：补丁地址与判定器，各自加了验证

### 18.1 27 个补丁地址逐条独立复核（2026-10-14）

`av_offsets.h` 里的偏移是我们**拿来做 kernel patching 的地址**——`routeFunction(base + off, ...)`
直接改写那段指令。差几个字节就是改写**无关的内核代码**，而且几乎无法从现象反推。

复核方式：从 `raw/applevtd.symbols.txt` 独立解析每个符号地址，逐个比对
`symtab_addr - 0xffffff8002886000 == AV_OFF_*`。

**结果：27 条全部相等，零例外**（`AV_OFF_INSTALL/INIT/INIT_HARDWARE/UNIT_ENABLE/UNIT_QUIESCE/
CONTEXT_INVALIDATE/INTERRUPT_INVALIDATE/SPACE_CREATE/VTD_SPACE_SET/SPACE_DESTROY/SPACE_FREE/
SPACE_INSERT/SPACE_UNMAP_MEMORY/SPACE_MAP_TO_PHYS/FOR_DEVICE/DEVICE_MAPPER_ACTIVATE/IOVM_INSERT/
IOVM_MAP_MEMORY/IOVM_UNMAP_MEMORY/MAP_TO_PHYSICAL/NEW_CONTEXT_PAGE/INSTALL_INTERRUPTS/
HANDLE_INTERRUPT/HANDLE_FAULT/TIMER/UNIT_FAULTS/UNIT_INTERRUPTS_ENABLE）。

顺带确认了一件容易误会的事：偏移是**相对于 kext 自身基址**的，运行期基址由内核给的
`kmod` 链表里 `com.apple.iokit.IOPCIFamily` 的 `address` 决定（`av_lilu.cpp::findIOPCIFamily`）；
代码里那个 `0xffffff8002886000` 只是**日志里的期望值**，不参与运算——所以即使哪天基址变了，
也只会看到一行"期望 vs 实际"的日志，不会安静地打错地方。

### 18.2 判定器自己有了回归测试（`stage1/test-analyze-log.sh`）

`analyze-log.sh` 是**唯一**会解读那次昂贵启动的东西，而它已经犯过一次错
（读不到 boot-args 时却断言"没有 `-amdvtprobe`"）。现在它可以被喂夹具：

```bash
AV_FIXTURES=<dir> bash stage1/analyze-log.sh      # 四个输入文件替代 ioreg/log/kmutil
bash stage1/test-analyze-log.sh                   # 8 个用例，逐个断言判定文本
```

8 个用例覆盖了每条分支：`not_usb`（这次启动根本没用 U 盘配置）、`disabled`（`-amdvtdoff` 主动关掉）、
`injected_dead`（注入了但从未初始化）、`loaded_no_cb`（kext 起来了但我们的回调没跑）、
`install_reached`（**PASS：注入窗口成立**）、`partial_seam`、`stage2_refused`、`idle_no_arg`。
**全部按预期定级**，所以下次拿到输出时，判读本身不会再是变量。

## 19. 元凶找到了：三处 `routeFunction` 调用把"成功"当成了"失败"（2026-10-14 修）

**这一条解释了此前所有的"代码一开始真正执行，内核就在起步瞬间复位"**：
从修好 `kmod_info` 那一刻起（10-14 00:57 起四次启动），每次我们都是"挂着钩子却完全没转发"地启动。

### 19.1 契约：`buildWrapper=false` 时，**成功也返回 0**

三份互相独立的证据（都不是推测）：

1. **头文件文档**（我们编译时用的那份，`impl/3rdparty/Lilu/Headers/kern_patcher.hpp:385`，
   来自 acidanthera/Lilu master @ `0515f40`，即 1.7.3 那条线）：
   `@return wrapper pointer or 0 on success`；
2. **上游源码** `Lilu/Sources/kern_patcher.cpp` `routeFunctionInternal()`：只有
   `else if (buildWrapper)` 那一支会 `trampoline = createTrampoline(...)`，函数最后是
   `return trampoline;`；失败那支返回 `EINVAL`（22，非零）。⇒ `buildWrapper=false` 时**永远返回 0**；
3. **canonical 用法**：acidanthera 自家插件一律要 wrapper 并据此判断——
   `RestrictEvents/SoftwareUpdate.cpp:198`、`WhateverGreen/kern_ngfx.cpp:186` 都是
   `routeFunction(from, to, true)` 然后 `if (!orig) 失败`。

**并且：补丁是在产生这个返回值之前就写下去的**（写跳转的代码块跟 `buildWrapper` 无关，
只有"造不造 trampoline"受影响）。所以旧写法下"失败"的真实含义是：
**跳转已经装上，但原函数序言被覆盖、又没有任何东西可以调回原函数**。

### 19.2 两处错误（三处调用点，同一个模式）

| 位置 | 旧写法 | 后果 |
| --- | --- | --- |
| `impl/src/av_entry.cpp:239` / `:250` | `routeFunction(..., false, false, true)` + `if (!orig) 失败` | `install` / `initHardware` 两处钩子"报失败"但**补丁已生效**；`origInstall = nullptr` |
| `impl/src/av_hooks.cpp:481` | 同上（20 条 seam 全一样） | 20 个 `orig_*` 全为 `nullptr`，`av_hooks_ok_count = 0` |

连锁后果（这才是致命的地方）：`hookInstall()` 里 `if (origInstall) origInstall(...)` ——
**AppleVTD::install 的本体一次都没跑**；`probe_init` 返回 `false`、`probe_for_device` 返回 `nullptr`、
`space_create` 返回 `nullptr`…… 也就是"stage 1 号称 log-only、always forwards"这句话，
在旧写法下**物理上不可能成立**：没有 trampoline 就没有"forward"这个选项。

另外 `kernelRoute=false` 也是错的：它跳过 `MachInfo::setKernelWriting(true, ...)`，
而我们的目标在 boot KC 里（IOPCIFamily）⇒ 属于**内核内存**，正是这个参数存在的理由
（`createTrampoline()` 自己是无条件开写保护的，路由这条路只在 `kernelRoute` 为真时才开）。

### 19.3 修复

新增 `impl/src/av_route.h`（唯一调用点，全部注释说明理由）：`buildWrapper=true`、
`kernelRoute=true`、`revertible=true`，并且**同时检查返回值与 `getError()`**
（失败时返回的是 `EINVAL`（真值），所以 `if (!orig)` 单独用也不够）：

```cpp
patcher.clearError();
mach_vm_address_t orig = patcher.routeFunction(addr, impl, true, true, true);
if (!orig || orig == EINVAL || patcher.getError() != KernelPatcher::Error::NoError) -> 0
```

`buildWrapper=true` 还顺带把失败语义变正确：wrapper 是在**写补丁之前**造的，
失败时 `EINVAL` 返回、**一个字节都不会写**，这才让"要么整条 seam 装上、要么什么都不动"成为可能。

**验证**：`bash impl/build-plugin.sh` 14/14 编译通过；`python3 impl/check-kext.py` 通过；
`bash impl/tests/run.sh` 0 failures，并新增一条静态回归守卫——
**除 `av_route.h` 之外任何地方再出现 `routeFunction(` 就 fail**（三处旧调用点因此被钉住）。
部署：U 盘 kext sha256 `6f5caa6d81f5860c9c00449074cf8ed0b7909e1bf9a0b0ef3d257e8d1bb42fdd`
（配置一字未动：`Enabled=true`、`-amdvtprobe`、`LauncherOption=Disabled`）。

### 19.4 残留不确定性（如实标注）

离线无法判定"是写只读内核文本先炸、还是'钩子不转发'先炸"——两者都在同一次修复里被覆盖，
所以不需要分辨也能往前走。但这意味着**下一次启动是这版的第一次检验**：
若仍复位，请记录屏幕最后几行（`-v` 已在 boot-args 里，我们的 `AVAMDVi:` 与 Lilu 的
`patcher: from ...` 都会打到屏幕上），据此可区分"补丁写不进去（页保护）"与"补丁写进去之后 Apple 的路径炸"。

### 19.5 修复后的自查又抓到一个真风险：**长跳板会复制带 `(%rip)` 的序言**（已加固）

`buildWrapper=true` 会把目标函数序言**原样拷进** Lilu 的 trampoline（`createTrampoline()`：
`lilu_os_memcpy(tempDataPtr + opnum, func, off)`，`off = Disassembler::quickInstructionSize(func, min)`），
而 `quickInstructionSize()` 的定义（`Lilu/Sources/kern_disasm.cpp:71`）是**循环累加到 `total >= min`**
——即"覆盖满 min 字节"的边界。`min` 由 Lilu 自己按距离选：`|to - (from+5)|` 塞不进 int32
就用 **14 字节绝对跳**（`LongJump`），否则用 **5 字节近跳**（`SmallJump`）。

用 `disasm/fn/` 对 23 个目标逐一核对序言：

| 复制窗口 | 含 `(%rip)` 相对寻址的目标 |
| --- | --- |
| `min=5`（近跳：拷 5–7 字节） | **无**（全部只是 `push rbp` / `mov rbp,rsp` / `push r14/r15` …） |
| `min=14`（绝对跳：拷 14–20 字节） | `unit_quiesce`@+10、`for_device`@+11、`irq_inv`@+13、`install_interrupts`@+7 |

被复制的 rip 相对指令换了地址就会算错：例如 `unit_quiesce` 的 `leaq 0x2135a(%rip), %r14`（`gIOPCIFlags`）、
`for_device` 的 `leaq 0x208ce(%rip), %rax`（`IOPCIDevice::metaClass`）；
`irq_inv`/`install_interrupts` 那两条还是 **`movq …(%rip), %rax`——即一次解引用**，
在别处执行就是访问错误地址。**而这条 trampoline 正是我们转发时要调的东西**，
一旦 Lilu 选了绝对跳，第一次转发就可能变成内核故障。

**加固（`av_route.h`）**：① 显式改用 `routeFunctionShort()`（近跳或干脆失败，不再让 Lilu 自己选宽跳）；
② 先自己算一遍 `diff = impl - (addr + 5)`，塞不进 int32 就**拒绝路由并打印**
（拒绝 = 一个字节都不写、Apple 的函数原样保留，永远是安全的；坏 trampoline 则不是）。
本机两者都在 boot KC 的同一 ±2GB 段内，所以正常情况走的就是近跳那条；
加固是为了"万一不在"时不会变成一次无声的内核故障。

部署：kext sha256 `cc076cd50aeb2fa1ca3ff35d2a0c107f6e32feabc901774c0d0d02c0431452fc`
（导出表里现在是 `KernelPatcher::routeFunctionShort`，可核对）。

### 19.6 同时被数据否掉的外部假设

外部审查（gemini-4-argon-web）提出的头号机制假设是"**目标函数太短，被 14 字节长跳板砸穿相邻函数**"。
用 `disasm/fn/`（384 份逐函数反汇编 + `_index.tsv` 的字节长度）对 23 个目标逐个核对：
**全部 ≥15 字节**，且**首 14 字节内没有任何 `ret`/`jmp`**（最短的 `iovm_insert` 15 字节、
`iovmUnmapMemory` 26、`space_map_to_phys` 28、`space_insert` 30、`install_interrupts` 26）。
补丁长度上限 14 字节（`FF 25 00 00 00 00` + 8 字节绝对地址；不落在 ±2GB 内才是 5 字节 `E9`），
且 16 字节原子写那条路径写进去的多余 2 字节是**原函数原样内容**（`patch.sourceIt<>()` 先读回原字节）。
⇒ 该假设不成立，不是这次的元凶。
