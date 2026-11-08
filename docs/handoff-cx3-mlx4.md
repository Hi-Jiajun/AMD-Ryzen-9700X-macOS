# 交接：Mellanox ConnectX-3 Pro 的 macOS kext（mlx4 移植）

> 新会话从这里开始。目标：给 `15b3:1007` 写一个能在本机 macOS 15.8.1 上工作的以太网 kext。
> 完整证据链见 `docs/device-driver-feasibility.md` §3、§5、§9、§11；本文件是可直接开工的摘要。

## 1. 硬件事实（实测）

| 项 | 值 |
| --- | --- |
| 卡 | `15b3:1007` = MT27520 Family **[ConnectX-3 Pro]**，subsystem `15b3:0003` |
| BAR | BAR0 1 MiB / BAR2 8 MiB / ROM 1 MiB |
| macOS 里的属性 | `compatible = "pci15b3,3","pci15b3,1007","pciclass,020000"` |
| 当前驱动 | **无**（macOS 原生 `AppleEthernetMLX5.dext` 只覆盖 ConnectX-4/5/6） |
| ACPI 路径 | `\_SB.PCI0.GPP7.UP00.DP40.EP00` |

## 2. 移植基底（已就位）

`tools/mlx4-port/freebsd-src/` —— FreeBSD `sys/dev/mlx4` 全部 41 个 `.c/.h`（1.1 MB）。

- **许可：GPL-2.0 / OpenIB BSD 双许可，逐文件已核实**。选 **OpenIB BSD** 那一支 ⇒ 成品可用 BSD/MIT 发布。
  义务：源码与二进制形式都保留版权声明与许可全文。
- **不需要外部固件**：`request_firmware` 在 `main.c`/`fw.c`/`cmd.c` 里**都是 0 次**（固件在卡自身 flash）。
- 结构：`mlx4_core/`（命令队列、EQ/CQ、ICM、资源跟踪、QP/MR/PD，782 KB）+ `mlx4_en/`（收发路径，216 KB）。

## 3. 要替换的内核接口层（实测计数）

| 类别 | FreeBSD 形态 | 计数 | macOS 对应 |
| --- | --- | --- | --- |
| DMA | `bus_dma` / `bus_dmamap` | 44 / 33 | `IODMACommand` + `IOMemoryDescriptor` |
| 包缓冲 | `mbuf` / `m_freem` / `m_copydata` | 71 / 15 / 7 | macOS **也有 mbuf**（同源 BSD） |
| 网络接口 | `ifnet` / `if_alloc` | 1 / 1 | `IOEthernetController` + `IOEthernetInterface` |
| 定时/工作 | `taskqueue` / `callout_reset` / `queue_work` | 10 / 2 / 12 | `IOCommandGate` / `thread_call` / `IOTimerEventSource` |
| 内存 | `kmalloc` / `kfree` / `vzalloc` / `vfree` | 31 / 154 / 1 / 4 | `IOMalloc` / `IOFree` |
| 锁 | `mutex_lock` | 99 | `IOLock` / `IORecursiveLock` |
| 其它 | `sysctl_ctx` / `printk` / `mod_timer` | 12 / 5 / 5 | `sysctl` 保留 / `IOLog` / `IOTimerEventSource` |

⚠️ 它引用了 47 个不同的 `linux/*.h` 兼容头（FreeBSD LinuxKPI），但**实际只用上表这十几个名字，且都是薄封装**。
所以项目形态 = **保留协议主体 + 写一层 `mlx4compat` 做名字映射**。

## 4. 本机已具备的构建能力（不用 Xcode）

| 工具 | 用途 |
| --- | --- |
| `tools/research/build-mtkwlan.sh` | 无 Xcode 构建 kext 的完整示例（实测 54/54 通过并链接成功） |
| `tools/research/assemble-kext.sh` | 组装 `.kext` bundle + `kmutil print-diagnostics` 校验 |
| `tools/research/mksdk/` | `acidanthera/MacKernelSDK`（kext 头文件集） |
| 关键前提 | CLT 的 SDK **自带 `Kernel.framework`**；缺 Xcode 也能编 |

**三个必踩的坑（已解决，直接照抄）**：
1. 必须 `-DKERNEL -D__KERNEL__` —— 否则 `PCIDriverKit/IOPCIDevice.h` 走 DriverKit 分支报 `OSClassLoadInformation` 未知。
2. 带 `-nostdinc` 时**所有 `.c` 也要按 C++ 编**（`-x c++`）—— `mtk80211/compat.h` 那类头没有 `__cplusplus` 保护却包含 IOKit 的 C++ 头。
3. `Info.plist` 里的 Xcode 占位符（`$(PRODUCT_BUNDLE_IDENTIFIER)`、`$(MODULE_VERSION)`…）必须手工替换，否则 `CFBundleVersion` 非法、kext 加载不了。

## 5. ATTO 驱动：能借鉴什么、不能碰什么

**它的出身（已确证）**：二进制里的断言字符串带源码路径
`/Volumes/Second/dev/mellanox/source/cna2/osx/kext/shared/driver/{alloc,cmd,en_rx,en_tx,resource_tracker}.cpp`
⇒ **ATTO 的 kext 就是 Mellanox 自家 `cna2` 代码**，ATTO 只是贴牌方。

| 项 | 结论 |
| --- | --- |
| 可否参考它的**行为**（设备期待什么命令、IOKit 怎么接） | ✅ 可以 |
| 可否把它的**二进制放进任何仓库** | ❌ **绝对不行**（专有软件） |
| 可否抄袭/反编译它的代码 | ❌ 不行 |
| 它的匹配条件 | `IONameMatch` 用的是 `pci117c,90` 这类（ATTO 的 subsystem id） |

**我们需要的字符串**（本机实测 `compatible`）= `pci15b3,3` 或 `pci15b3,1007`。
已改好的副本在 `tools/mellanox/ATTOFastFrame2.kext`（仅供本地参考，勿提交）。

**它的能力覆盖**（ReadMe）：FastFrame NQ41/NQ42（40GbE，即我们这类）、ThunderLink NS3101/3102、NQ3401/3402；
随包带 `atflash2`（刷固件）、`atnetnvr2`（读写 NVRAM/MAC —— **这是"刷固件后 MAC 归零"的解法**）、`atnetstat`、`atinfo`。

## 6. 为什么"改 ID 直接用 ATTO kext"行不通（已实测，别再试）

用 `kmutil libraries`（权威符号解析器）定量：

```
kext 需要未定义符号：568
能解析到          ：554
差额（无法解析）  ：14   ← 全部是基类 _RESERVED 槽位
```

| 基类 | 缺失槽位 | macOS 15.8.1 只剩 |
| --- | --- | --- |
| `IONetworkController` | `_RESERVED…2` ～ `7` | `8`、`9` |
| `IOEthernetController` | `_RESERVED…2` ～ `9` | `10` ～ `31` |

机制：Apple 逐步把 `_RESERVED` 占位方法"兑现"成真方法（编号从低到高被消耗）。该 kext 是 2022 年用
**macOS 12.1 SDK** 编的，当时 1–9 还是占位 ⇒ 到 15 符号名已不存在 ⇒ vtable 引用失效、无法加载。
**与 PCI ID、签名都无关。**

（可选实验：写 14 个**同 mangled 名**的空函数 shim —— 因为那些槽位当年就是空实现 ——
用 `kmutil libraries -r <含 shim 的目录> -p <kext>` **离线**验证是否解析成功。命中即证明可行，零风险。）

## 7. 里程碑（来自 `docs/cx3pro-driver-feasibility.md`，结合现状修正）

| 里程碑 | 内容 | 预估 |
| --- | --- | --- |
| **M1** | 最小 kext：匹配 `15b3:1007`，映射 BAR0，读出 device id / 固件版本 | **1–3 天**（分水岭：证明能绑定并访问这块卡） |
| M2 | 移植 `mlx4_core` 的 reset + INIT_HCA + ICM，让 `QUERY_DEV_CAP` 返回 | 2–4 周 |
| M3 | EQ/CQ/QP bring-up，端口切 Ethernet，链路 up | 3–6 周（最难：资源跟踪 + 中断） |
| M4 | 接 `IOEthernetController`，单队列收发，**ping 通** | 3–6 周（第一个"能用"版本） |
| M5 | 多队列 / 校验和卸载 / jumbo / 调优 | 长期 |

路线选择（建议**先 A 后 B**）：**A. kext** 快速验证硬件层移植是否正确；**B. PCIDriverKit dext** 做长期正解
（dext 的坑更多：队列要显式 enable、`setMulticastAddresses` 不实现会静默失效 mDNS/IPv6、必须叫 `.dext` 等）。

## 8. 许可与发布（已定）

- 代码源自 FreeBSD/Linux 的 mlx4 **双许可基底** ⇒ 选 **OpenIB BSD** ⇒ **可以公开到仓库**。
- **不得包含**：ATTO 的 kext 二进制及其改本、由它反编译/抄录的代码。
- 保留 BSD 版权声明与许可全文（源码 + 二进制形式）。

## 9. 本会话踩过的坑（省时间）

- `kmutil print-diagnostics` **只查 kext 级依赖、不查单个符号**，会报 `Dependencies: OK` 骗人 —— 用 `kmutil libraries`。
- `kmutil load` 有三道门：用户授权（`-z` 可跳）、**需要重启**（设计如此，绕不过）、符号解析（唯一的硬墙）。
- ⚠️ `kmutil load` 会写**三处持久状态**（`AuxKC` 的 `instructions.plist` + `StashedExtensions`、
  两个 `UAKL.plist`、`KextPolicy` 数据库的 `kext_policy`）。清理时**只删自己新增的那条**，
  别用 `kmutil clear-staging`（会连带清掉 NullMoth/HighPoint）。备份见
  `tools/backup-auxkc-*`、`tools/backup-uakl-*`、`tools/backup-kextpolicy-*`。
