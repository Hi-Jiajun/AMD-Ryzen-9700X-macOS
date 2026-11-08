# ConnectX-3 Pro 的 macOS 驱动适配：初步可行性分析

日期：2026-10-08　参考：知乎《MacStudio 40G网卡Mellanox ConnectX-3踩坑》+
FreeBSD/Linux mlx4 源码实测体量 + `b-ostrov/MelonDMA` 先例

## 0. 结论先行

**技术上可行，但成本远高于换一张卡；而这两种情况的和解点是「把它当研究项目、分阶段做，而不是当解决方案」。**

| 判断 | 内容 |
| --- | --- |
| 机制上能不能做 | **能。** 已有先例 `MelonDMA` 用 PCIDriverKit 在 macOS 上接管 Mellanox HCA（不需要 Apple 的驱动） |
| 为什么知乎那篇是死局 | 作者在 **Apple Silicon** 上，kext 路径被架构堵死，只剩 DriverKit 一条；而且他找的是「现成驱动」，本来就不存在 |
| 我们这台的关键差异 | **Intel + hackintosh**：kext 和 DriverKit **两条路都开着**，而且 SIP 已经是宽松态 |
| 工作量 | 需要搬运/重写约 **3 万行**硬件驱动代码；参照先例，单人全力做 **3–6 个月**量级 |
| 性价比 | 同代替换品 **ConnectX-4 Lx（~100 元，原生即插即用）**，或 Intel X520-DA2（~100–200 元，`AppleEthernetIXGBE` 原生） |
| 建议 | 除非目标是「为这块 85 元的卡写个驱动」这件事本身，否则**换卡**。真想动手，按 §6 的 M1 先花 1–3 天脱敏风险 |

## 1. 先厘清：知乎那篇的结论对，但对我们不完全适用

那篇的排查过程完全正确 —— `0x15b3:0x1007` = MT27520 [ConnectX-3 Pro]，macOS 只带
`AppleEthernetMLX5`（覆盖 ConnectX-4/4Lx/5/6），CX-2/3 属于 mlx4 世代，从来没进过支持列表。
他试的三条路（原生驱动 / 硬上老 kext / ATTO 马甲）也确实全死。

但结论里有一句只对 Apple Silicon 成立：

> macOS 从 Big Sur 开始强推 DriverKit，老的 kext 在新系统上要么被拦、要么只能恢复模式关 SIP 强装，
> 而且 Apple Silicon 上 kext 基本属于历史遗留物

**在 Intel 机器上 kext 路径仍然活着** —— 证据就是我们自己那份 EFI 里的 `RTL812xLucy.kext`
（IONetworkController 系）在 macOS 15 上正常工作，IntelMausi / LucyRTL8125 这类都是这么活的。
所以我们的可选路径**比那篇作者多一条**：

| 路径 | Apple Silicon（知乎作者） | Intel + hackintosh（我们） |
| --- | --- | --- |
| 借用现成 kext | 死 | 死（本就没有 CX-3 的 kext） |
| 自己写 IOKit kext（IONetworkController） | 死（架构） | **可行** |
| 自己写 DriverKit DEXT（PCIDriverKit + NetworkingDriverKit） | 可行 | **可行** |

## 2. 已经有了可行性证明：MelonDMA

`b-ostrov/MelonDMA`（Apache-2.0）—— 用 **PCIDriverKit** 为 Mellanox ConnectX 写的 macOS DEXT：

| 项 | 值 |
| --- | --- |
| 做什么 | 用 DEXT 接管 ConnectX HCA，实现 RoCEv2 RDMA（verbs），**替代** Apple 的 `AppleEthernetMLX5` |
| PCI 匹配 | `0x101515b3` = **ConnectX-4 Lx（mlx5）**，不是 CX-3 |
| 体量 | 69 个源文件、**约 4.2 万行**（含 userspace verbs 兼容层） |
| 最大单文件 | `MlxPCIDriver.cpp` 约 6,100 行；`MlxUserClient.cpp` 约 3,300 行 |
| 时间 | 2026-08-23 建库 → 2026-09-19 最后推送（约 4 周，单人全力） |
| 权限现状 | **Apple 尚未批准 PCI/UserClient entitlement**，所以目前必须 **SIP 关闭**或 `systemextensionsctl developer on` |
| 已通过的 | 在 ConnectX-4 Lx + ADT-Link PCIe 转接卡上，P0–P3 门禁全过（SEND/RECV、WRITE、READ、RC atomics、多 QP） |

**这条先例的价值**：它证明了三件事对我们成立 ——

1. PCIDriverKit 能在 macOS 上**完整接管一块 Mellanox HCA**（BAR、MSI-X、doorbell、固件页都搞定了）；
2. 不需要 Apple 的驱动做底座，也不需要 Apple 批准就能**开发/自用**（SIP 放宽即可，我们本来就是）；
3. 硬件层代码量是**万行量级**，不是一个周末的项目。

但它**不能直接改出 CX-3 支持**：`15b3:1007`（mlx4）与 `15b3:1015`（mlx5）是**完全不同的硬件接口** ——
命令接口、初始化流程、EQ/CQ/QP 布局、固件页格式都不一样。MelonDMA 里 `ConnectX-3`、`mlx4`、`1007`
出现次数都是 **0**。

## 3. 真正要搬运的东西有多大（实测）

移植源有两个，**许可差别很关键**：

| 来源 | 组成 | 体量 | 许可 |
| --- | --- | --- | --- |
| **FreeBSD** `sys/dev/mlx4` | mlx4_core 23,566 行 + mlx4_en 6,507 行 + mlx4_ib 13,188 行 | 以太网只需 **≈30,000 行** | **GPL-2.0 或 OpenIB.org BSD 双许可（可选 BSD）** |
| Linux `drivers/net/ethernet/mellanox/mlx4` | 39 个文件 | ≈34,666 行 | **仅 GPL-2.0** |

我逐个头文件确认了 FreeBSD 那版的双许可头：

> This software is available to you under a choice of one of two licenses. You may choose to be licensed
> under the terms of the GNU General Public License (GPL) Version 2 … or the **OpenIB.org BSD license** below

**所以走 FreeBSD 那版、选 BSD 分支，是许可上最干净的路**（往 macOS 驱动里搬，不必背上 GPL 传染）。
mlx4_ib（13,188 行）纯做以太网用不到，可以整块砍掉。

体量最大的几个文件（也就是最花时间的部分）：

```
FreeBSD mlx4_core/mlx4_resource_tracker.c   135,375 B   ← QP/CQ/MTT 资源与 ICM 分配
FreeBSD mlx4_core/mlx4_main.c               118,221 B   ← PCI 探测、初始化、命令接口
FreeBSD mlx4_core/mlx4_fw.c                 100,891 B   ← 固件/端口/设备能力
FreeBSD mlx4_core/mlx4_cmd.c                 89,552 B   ← 命令队列（mailbox）
FreeBSD mlx4_core/mlx4_port.c                56,571 B   ← 端口状态机、MAC/VLAN
FreeBSD mlx4_en/mlx4_en_netdev.c             86,772 B   ← 数据面胶水层
```

## 4. 移植的难点在哪（不是"翻译代码"那么简单）

这 3 万行不是能逐行翻译的 —— **操作系统抽象层必须重写**，这部分是真正的工作量：

| 要替换的内核设施 | Linux/FreeBSD | macOS 侧对应 | 难度 |
| --- | --- | --- | --- |
| 网络设备抽象 | `struct net_device` / `ifnet` + `mbuf`/`sk_buff` | kext 走 `IONetworkController`/`IONetworkInterface`，或 dext 走 `IOUserNetworkEthernet` + `IOUserNetworkPacket` | 中高（两套 API 都要按队列模型重写收发） |
| 一致性 DMA 内存 | `dma_alloc_coherent` | `IOMemoryDescriptor` + `IODMACommand`（dext 侧是 `IOBufferMemoryDescriptor`） | 中 |
| **ICM**（InfiniHost Context Memory） | 大块物理连续内存 + MTT 映射 | 需要自己管 DMA 映射与 IOMMU（`DisableIoMapper` 之类的取舍） | **高**（CX-3 特有的坑） |
| 中断 | MSI-X + `request_irq` | kext: `IOInterruptEventSource`；dext: `IOInterruptDispatchSource` | 中（MelonDMA 已验证 MSI-X 可行） |
| 锁/工作队列/定时器 | `spinlock_t`/`workqueue`/`timer_list` | `IOSimpleLock`/`IOWorkLoop`/`IOTimerEventSource`（dext 侧是 `OSAction` + dispatch queue，**语义差别最大**） | 高 |
| 固件加载 | `request_firmware()` | kext 直接读文件 / dext 由宿主 app 提供 | 低（CX-3 固件在卡上 flash，通常不需要外部 blob） |

另外三个 CX-3 特有的注意点：

1. **VPI 卡需要端口类型是 Ethernet**。你那张是 `ConnectX-3 PRO VPI (MT04103)` —— VPI 可跑 IB 也可跑
   以太网，需要固件里端口类型设成 Ethernet（Linux 侧靠 `mlxconfig` / `connectx_port_config`）。
   驱动做完还得确认这一项。
2. **CX-3 没有 GSP**，这点反而比 CX-4/5 简单（不需要 host 侧 boot 固件的复杂流程）。
3. **40G 在 macOS 侧没有太多参考**：MelonDMA 那台的实测是 40G 链路上跑 RDMA；以太网 40G 的
   队列/中断调优要自己摸。

## 5. 两条技术路线对比

### 路线 A：IOKit kext（IONetworkController）

- **优点**：Intel 上成熟，hackintosh 社区最熟（LucyRTL8125、IntelMausi 都是这个模式）；
  不需要 Apple 的任何 entitlement；我们的 SIP 已经是 `430A0000`，装未签名 kext 本来就允许。
- **缺点**：架构上是被淘汰的路，Apple 后续版本可能继续收紧；调试体验差（panic 就重启）。
- **适合**：先把"能通"做出来，验证硬件层移植正确性。

### 路线 B：DriverKit DEXT（PCIDriverKit + NetworkingDriverKit）

- **优点**：Apple 官方推荐的现代路径；**有 MelonDMA 这个真实先例可抄脚手架**（PCI 接管、MSI-X、
  doorbell、固件页）；崩溃不带走内核，调试友好。
- **缺点**：需要 **Apple 开发者账号（付费）** 才能申请 entitlement；未获批准前只能
  SIP 关闭 / `systemextensionsctl developer on` 自用（MelonDMA 就是这个状态）；
  `IOUserNetworkEthernet` 的队列模型有大量未文档化的坑（AQC111Driver 作者专门写了一段"教训"，
  包括 queue 必须显式 enable、`setMulticastAddresses` 不实现则 mDNS/IPv6 静默失效、dext 必须叫
  `.dext`、dispatch source 必须在 Cancel 完成块里释放等等）。
- **适合**：长期路线的正解，但前期门槛更高。

**我的建议：先 A 后 B** —— 用 kext 快速验证 mlx4 硬件层移植是否正确（能 ping 通即证明），
再把数据面搬到 DEXT 做长期维护。避免一上来就同时踩两套坑。

## 6. 如果要真的动手：分阶段，先做最便宜的脱敏

| 里程碑 | 内容 | 预估 | 价值 |
| --- | --- | --- | --- |
| **M0** | 装上 macOS（正在进行） | — | 一切的前提；没有 macOS 连编译都做不了 |
| **M1** | 写一个最小 PCIDriverKit dext，匹配 `15b3:1007`，映射 BAR0 并读出 device ID / 固件版本 | **1–3 天** | **最关键的脱敏**：证明这台 AMD hackintosh 上 DEXT 能绑定并访问这块卡。做不到就没有后面 |
| M2 | 移植 mlx4_core 的 reset + INIT_HCA + ICM，让 `QUERY_DEV_CAP` 能返回 | 2–4 周 | 打通与硬件的命令通道 |
| M3 | EQ/CQ/QP bring-up，把端口切成 Ethernet，链路 up | 3–6 周 | 最难的一段（资源追踪 + 中断） |
| M4 | 接 `IOUserNetworkEthernet`（或 IONetworkController），单队列收发，ping 通 | 3–6 周 | **第一个"能用"的版本** |
| M5 | 多队列、校验和卸载、jumbo、性能调优 | 长期 | 从"能通"到"能用" |

合计 **3–6 个月**（单人、业余时间、有 MelonDMA 当脚手架）。M1 只需 1–3 天，是决定要不要继续的
分水岭 —— 而且它只在 macOS 装好之后才能做。

## 7. 替代方案（同样 10GbE，零开发）

| 方案 | 价格 | macOS 支持 | 说明 |
| --- | --- | --- | --- |
| 板载 **Realtek RTL8125** 2.5G | **0**（已有） | 原生 kext（我们 EFI 里已装） | 2.5G，够大多数场景 |
| **ConnectX-4 Lx**（MCX4121A-ACAT） | ~85–150 元 | macOS 13+ **原生** `AppleEthernetMLX5`，即插即用 | 25G 卡，10G 稳跑（kittenlabs 实测 10.3 Gb/s 走雷电） |
| **Intel X520-DA2**（82599ES） | ~100–200 元 | macOS **原生** `AppleEthernetIXGBE` | 双口 10G SFP+，hackintosh 社区最常见 |
| 自己写 CX-3 驱动 | 3–6 个月 + 一张 85 元的卡 | 取决于你 | 如果目标是"做出驱动"本身，那它是唯一选项 |

**一句话**：这张 CX-3 Pro 在 macOS 上真正的归宿是 Linux/Windows 服务器（知乎作者最后也这么处理）；
想在 Mac 侧要 10G，花 ~100 元换 CX-4 Lx 是最优解。**但如果你要的就是"给这块卡写个 macOS 驱动"
这件事本身（比如为了 DriverKit / IOKit 的工程练手），M1 值得花那 1–3 天先探一下。**
