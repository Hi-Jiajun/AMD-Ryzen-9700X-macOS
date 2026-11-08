# A3/A4 可行方案对比

结论先行：

- 原版 `SMCAMDProcessor 0.7.2f1`：**No-Go**，不能直接用于当前 Zen 5。
- 自研“只读 SMC 传感器插件”：**Conditional Go**，先做温度最小原型，不做 P-state 写入。
- 维持“macOS 不参与 P-state，风扇由 BIOS 智能曲线控制”：**Go**，作为当前稳定默认档。

## 1. 方案矩阵

| 方案 | Zen 5 兼容性 | 传感器覆盖 | 是否只读 | 成熟度 | 风险 | 结论 |
| --- | --- | --- | --- | --- | --- | --- |
| 原版 SMCAMDProcessor | 不支持，源码只认 17h/19h | CPU 封装温度、能耗；频率走用户客户端；无独立电压键 | 否；依赖的 AMD kext 会写 MSR/P-state | 一般；2024 后无功能提交 | 高 | No-Go |
| 自研只读 SMC 插件 | 可针对 1Ah/44h | 可做 Tctl/Tdie/Tccd、APERF/MPERF 频率、RAPL；风扇需另接 SuperIO | 设计上可以 | 需自建并长期测试 | 中 | Conditional Go |
| 维持现状 | 已工作 | 无 CPU/SMC 实时传感器 | 是 | 已稳定运行 | 低 | Go |

## 2. 原版 SMCAMDProcessor

### 2.1 版本与维护状态

2026-10-21 通过 GitHub API 核实：

```text
repo:        trulyspinach/SMCAMDProcessor
license:     BSD-3-Clause
default:     master
last commit: 029ef3a97d89c44c07c63a6345a7c87f69ba90f8
commit date: 2024-09-16T10:31:40Z
release:     0.7.2f1 / 名称 0.7.2
published:   2024-03-31T05:29:08Z
open issues: 48（查询时）
```

发布包包含：

- `AMDRyzenCPUPowerManagement.kext`：电源管理、传感器采样、SuperIO/fan 功能；
- `SMCAMDProcessor.kext`：把采样结果注册成 VirtualSMC 键；
- `AMD Power Gadget.app`：用户态图表/P-state/fan GUI。

### 2.2 为什么不能直接用

上游源码的 CPU family 开关是：

```cpp
//Only 17h Family are supported offically by now.
// EDIT: add check bypass for 19h Zen 3 processors
cpuSupportedByCurrentVersion = (cpuFamily == 0x17 || cpuFamily == 0x19)? 1 : 0;
```

来源：

- [`AMDRyzenCPUPowerManagement.cpp` L273-L279](https://github.com/trulyspinach/SMCAMDProcessor/blob/master/AMDRyzenCPUPowerManagement/AMDRyzenCPUPowerManagement.cpp#L273-L279)

本机是 `family=26=0x1A`、`model=68=0x44`，不在支持集内。

上游 issue #190（2025-10-06，仍 open，0 条评论）标题为
“Weird frequency readings on Zen5 processor”，正文明确写：

> P.S. I realize that this CPU isn't yet supported but the numbers were very funny.

并报告 Zen 5 频率被读成 8 GHz 以上。来源：

- <https://github.com/trulyspinach/SMCAMDProcessor/issues/190>

因此不能把“kext 能加载”或“出现了数值”当成兼容。

### 2.3 并非只读

`SMCAMDProcessor.kext` 本身主要是注册 `TC*` 温度和 `PC*` 能耗键，但它的必需依赖
`AMDRyzenCPUPowerManagement.kext` 不是只读：

- 初始化向 `MSR_HWCR` 写位；
- 向 `MSR_PSTATE_CTL` 写 P-state；
- `pmAMDRyzen.c` 会修改 P-state 定义；
- SuperIO 路径提供 fan override。

代表性源码：

- [`AMDRyzenCPUPowerManagement.cpp` L123-L167](https://github.com/trulyspinach/SMCAMDProcessor/blob/master/AMDRyzenCPUPowerManagement/AMDRyzenCPUPowerManagement.cpp#L123-L167)
- [`pmAMDRyzen.c` L106-L124](https://github.com/trulyspinach/SMCAMDProcessor/blob/master/AMDRyzenCPUPowerManagement/pmAMDRyzen.c#L106-L124)

`-amdpnopchk` 只是解除 P-state/fan 控制的 root 权限检查，不是只读开关。

更危险的是：源码没有在 `cpuFamily==0x1A` 时立刻 `return false`。它继续读取
RAPL MSR，失败路径直接 `panic`，之后仍执行 `pmRyzen_init()` 和写 MSR 的定时初始化。

### 2.4 传感器覆盖也不是全目标

`SMCAMDProcessor` 当前实际注册：

- `TC0E/TC0F/TC0J/TC0P/TC0T/TC0p`：封装温度；
- `PCPR/PSTR`：封装能量。

源码：

- [`SMCAMDProcessor.cpp` L17-L30](https://github.com/trulyspinach/SMCAMDProcessor/blob/master/SMCAMDProcessor/SMCAMDProcessor.cpp#L17-L30)

它不注册独立 CPU 电压键。频率由 `AMDRyzenCPUPowerManagement` 的用户客户端提供给
Power Gadget/CLI，不是通用的 SMC 频率键。

风扇支持在 `AMDRyzenCPUPowerManagement/SuperIO` 内，只明确识别
NCT6681/NCT6683 和部分 NCT67xx；MAXSUN B850BKB WIFI 的实际 SuperIO 型号尚未确认，
不能假定原版风扇路径可用。

### 2.5 判定

原版方案是 No-Go：

- Zen 5 明确未支持；
- 有错误频率的公开实例；
- 没有 unsupported-family fail-safe；
- 包含 P-state、fan 写路径；
- 会与当前稳定的 NVIDIA/USB/WiFi/音频组合叠加一个高风险变量。

## 3. 自研只读 SMC 传感器插件

### 3.1 推荐目标

只解决“看得到”，不接管“调得了”：

1. CPU 封装温度；
2. 实时核心频率；
3. CPU 封装功耗/能量；
4. 若实际 SuperIO 被上游支持，再复用 `SMCSuperIO` 读 fan/voltage；
5. 不写 P-state、不写 HWCR、不接管风扇曲线。

### 3.2 技术依据

Linux `k10temp` 已把 Family 17h/18h/19h/1Ah 放在同一套 Zen 温度接口：

```text
ZEN_REPORTED_TEMP_CTRL_BASE = 0x00059800
labels: Tctl, Tdie, Tccd1..Tccd16
```

并列出 Family 1Ah 的多个 PCI device ID。来源：

- [`drivers/hwmon/k10temp.c`](https://github.com/torvalds/linux/blob/master/drivers/hwmon/k10temp.c)

AMD 公开 PPR 也把 9700X 对应到 Family 1Ah Model 44h：

- <https://docs.amd.com/v/u/en-US/57896>

频率可以走 `APERF`/`MPERF`（MSR `0xE7`/`0xE8`），上游原版已经提供该方法，但必须
重新验证 Family 1Ah 的 P0、溢出和每核/每 CCD 拓扑。

风扇/电压的较低风险路线是 VirtualSMC 官方 `SMCSuperIO`：当前源码明确支持
NCT6683D/NCT6686D/NCT6687D；驱动底层有 16 个 tach 寄存器，但当前 668x 设备类
实际只映射 5 路 fan 和 23 路 voltage。6687D 还存在“隔离读取只得到部分风扇”的
公开缺陷。它不是当前已加载的 kext，且本机 SuperIO 型号未知。

### 3.3 分阶段工作与验收

阶段 0：静态可行性

- 以 AMD PPR 57896 和 Linux `k10temp.c` 核对 Family 1Ah 的 SMN 路径；
- 确认是否需要 DF F3 PCI 设备或直接 SMN；
- 从 Windows/HWiNFO 或一次受控的只读 `SMCSuperIO` 探测确认实际 SuperIO；
- 不写 ESP。

阶段 1：温度最小原型

- 一个只读 VirtualSMC 插件，只导出 `TC0P` 或等价封装温度；
- 空闲 + 轻载（不做满载压测）各采样几次，读数应有合理变化；
- 无 panic，重启 3 次可复现。

阶段 2：频率与功耗

- 用 APERF/MPERF 计算每核频率；
- 用 PPR 核对 RAPL 单位/能量寄存器；
- 与 BIOS 温度、Linux/Windows 或外部功耗计做交叉校验。

阶段 3：风扇/电压

- 先加载官方 `SMCSuperIO` 做只读识别，不做 fan override；
- 只有实际 chip ID 和通道映射确认后才扩展自定义代码；
- 不使用原版 `AMDRyzenCPUPowerManagement` 的 P-state/fan 控制。

粗略工作量：

| 范围 | 量级 |
| --- | --- |
| 封装温度只读原型 | 3–5 天 |
| 温度 + 频率 + 封装能耗 | 1–2 周 |
| 加 SuperIO fan/voltage、睡眠回归、长期稳定性 | 2–4 周 |

以上是相对估算，不包含未知的 MAXSUN SuperIO 兼容问题。

### 3.4 风险

- 仍是内核代码；错误 MSR/SMN/IO 端口访问可能导致 panic。
- 只读也有并发和 CPU 迁移问题，必须用 rendezvous 或锁定当前 CPU。
- SMC 键注册错误会让传感器应用崩溃或显示错误数据。
- 当前系统已有 USB/WiFi/NVIDIA/音频多个变量，新 kext 必须单独 A/B。

### 3.5 判定

Conditional Go：

- 若需要“看温度/频率/功耗”，值得做；
- 只批准分阶段最小原型，不批准原版 SMCAMDProcessor 整包；
- 任何 P-state/fan override 另立决策，不混入传感器原型。

## 4. 维持现状

当前状态是：

- macOS 不参与 P-state；
- 风扇由 BIOS Smart Fan 控制；
- 无 CPU 温度/频率/电压可见；
- 没有新增内核写 MSR 或 I/O 端口控制的风险。

BIOS IFR 已确认：

- CPU/Pump/Sys1/Sys2 四路 fan；
- 每路 5 点温度/占空比曲线；
- CPU 温度源默认 `CPU Temp`；
- 可选 SYS/MEM/CPU/PCH 温度源。

因此对于“日常稳定优先”，现状是可接受的。它的问题是可观测性，不是当前已证明的
散热失效。

判定：Go，作为默认稳定档。

## 5. 建议

1. **不安装原版 SMCAMDProcessor。**
2. **也不让当前 macOS 接管 P-state。**
3. 如果用户只想要风扇转速，先评估官方 `SMCSuperIO` 的只读识别，这是比整包
   SMCAMDProcessor 更窄的变量；但加载 kext 仍需单独批准。
4. 如果目标升级为“CPU 温度 + 实时频率 + 封装功耗”，则开一个只读自定义 SMC
   插件的最小原型；先温度，后频率/功耗，不碰 P-state。
5. 在拿到可控温度读数之前，不尝试自定义 P-state/CPPC/C-state。

## 6. 参考

- SMCAMDProcessor 仓库：
  <https://github.com/trulyspinach/SMCAMDProcessor>
- 0.7.2f1 release：
  <https://github.com/trulyspinach/SMCAMDProcessor/releases/tag/0.7.2f1>
- Zen 5 未支持/错误频率公开问题：
  <https://github.com/trulyspinach/SMCAMDProcessor/issues/190>
- AMD Family 1Ah Model 44h PPR：
  <https://docs.amd.com/v/u/en-US/57896>
- Linux k10temp：
  <https://github.com/torvalds/linux/blob/master/drivers/hwmon/k10temp.c>
- Linux nct6683：
  <https://github.com/torvalds/linux/blob/master/drivers/hwmon/nct6683.c>
- VirtualSMC SMCSuperIO：
  <https://github.com/acidanthera/VirtualSMC/tree/master/Sensors/SMCSuperIO>
- NCT6687D 风扇顺序读取缺陷：
  <https://github.com/acidanthera/bugtracker/issues/2545>
