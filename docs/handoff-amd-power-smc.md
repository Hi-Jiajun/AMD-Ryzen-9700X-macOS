# amd-power-smc handoff

更新时间：2026-10-25 CST  
状态：基线与离线温度解码完成；原版 SMCAMDProcessor 仍为 No-Go；Linux 自带 k10temp 为下一阶段硬件对照候选，尚未加载。macOS 实时读温通道与传感器 kext 尚未实现；维持固件管理 CPU、BIOS 控风扇。

## 2026-10-25 增量（本节优先于下方历史基线）

- 用户已要求按计划继续，当前推进只读传感器方向；硬件加载仍需单变量窗口，不与现有 STA/CX3 实验叠加。
- 本机仍为 AuthenticAMD / Family 1Ah / Model 44h；root `powermetrics --samplers smc -n 1 -i 1000` 退出 0，但只有 `Invalid key detected!` 和空 SMC sensors 段，没有提供真实 CPU 温度。旧基线中“未取得 root 结果”的阻塞已解除。
- 最新 MTKCAP 只枚举 NVMe hwmon；SuperIO 两个端口仍因 PCI Bus 占用跳过，型号未知，不能继续承诺“再启动一次就能拿到 chip ID”。
- 现有 modloop 自带匹配 `6.6.134-0-lts` 的 k10temp 模块，支持本机枚举的 `1022:14e3`。Linux stable 6.6.134 对 Family 1Ah 有 Tctl 路径但没有该 Family 的 CCD 枚举；下一轮优先取得真实 Tctl 对照。
- 已固定 Linux 源码，实现纯离线 JSON 温度解码原型，8 项测试通过，含全部 2048 个温度编码和失败拒绝。合成温度不是本机温度；尚无硬件读取或 macOS kext。
- 新发现：SMN read 本身需要写地址选择寄存器。因此只读传感器应指不修改电源/风扇设置，不能称为零硬件写入。macOS SMN 访问归属与跨消费者同步需先解决。
- 详细依据、工具、硬件阶段输入/输出/通过条件：`outputs/20261025-amd-sensors/README.md`。本轮未改 EFI/BIOS/USB 启动包，未加载驱动。

## 2026-10-21 历史基线

- 机器：Ryzen 7 9700X、Family `0x1A`、Model `0x44`、48 GB、
  macOS 15.8.1（24H32）、MacPro7,1。
- 启动时间：`2026-10-21 09:16:51 CST`。
- `machdep.xcpm.mode=0`、`machdep.xcpm.vectors_loaded_count=0`、
  `machdep.xcpm.ratio_changes_total=0`。
- `hw.cpufrequency` / min / max 全部 `5140000000`，是静态值。
- `system_profiler` 的 `845 MHz` 也是静态/派生展示值。
- `pmset -g therm`：`No CPU power status has been recorded`。
- `kmutil showloaded`：AppleSMC 3.1.9、VirtualSMC 1.3.9；没有
  `SMCProcessor`、`SMCSuperIO`、`SMCAMDProcessor`、`AMDRyzenCPUPowerManagement`。
- IORegistry：AppleSMC 存在；`IOPlatformSensor`/`IOPlatformStateSensor`/
  `AppleACPIThermalZone`/`AppleACPIEC`/`AppleSMBusController` 均为 0 实例。
- SMC `#KEY=65`；`T*/TC*/TG*`、`F0Ac`、`V*`、`PCPR/PSTR` 均为 0。
- `FNum=0`、`FAC0=0`，均不是实时风扇转速。
- `powermetrics` 两次均未取得数据：非 root 拒绝；`sudo -n` 要求密码。

完整记录：

- `outputs/20261021-amd-power-smc/BASELINE.md`
- `outputs/20261021-amd-power-smc/SMC-KEYS.md`
- `outputs/20261021-amd-power-smc/OPTIONS.md`
- `outputs/20261021-amd-power-smc/smcdump.c`

## 结论

1. 当前没有真实 CPU/SMC 传感器数据。物理传感器存在，但 macOS 没有导出通道。
2. 没有证据证明 CPU 锁在 5.14 GHz；只能证明 macOS 不参与 P-state，且没有可信
   实时频率读数。固件侧是否调频仍不可观测。
3. 原版 `SMCAMDProcessor` 不能直接装：
   - 源码只支持 17h/19h，本机为 1Ah；
   - 上游 issue #190 已报告 Zen 5 未支持且频率读数大于 8 GHz；
   - 依赖的 `AMDRyzenCPUPowerManagement` 不是只读，会写 HWCR/P-state；
   - 没有 unsupported-family 的 fail-safe，且存在 RAPL 读取失败直接 panic 的路径。
4. 后续若投资，建议只做“只读传感器插件”，先温度，再频率/功耗；
   不把 P-state、CPPC 或 fan override 混入第一阶段。

## 2026-10-21 历史阻塞（更新见顶部增量）

- MAXSUN B850BKB WIFI 的实际 SuperIO 型号未知。BIOS IFR 证明有四路风扇，但
  macOS 用户态无法执行 SuperIO I/O；需要 Windows/HWiNFO、板厂资料，或经批准的
  一次只读 `SMCSuperIO` 识别实验。
- 用户尚未决定是否值得为温度/频率可见性投入自研 kext。
- `powermetrics` 未取得 root 结果；不能把它列为已排除的通道。
- 睡眠/唤醒未测，且本轮明确不作为验收项。

## 原分阶段路线（以顶部增量和新阶段设计为准）

最低风险顺序：

1. 若只需风扇 RPM：单独评估官方 `SMCSuperIO` 只读识别；先确认 chip ID，
   不做 fan override。
2. 若需要 CPU 温度：根据 AMD PPR 57896 与 Linux `k10temp.c` 的 Family 1Ah
   温度路径做最小只读原型。
3. 温度原型稳定后，再评估 APERF/MPERF 实时频率与 RAPL 封装功耗。
4. P-state/CPPC/C-state 控制继续 Deferred。

任何阶段若涉及加载 kext 或改 ESP，必须先由 `amd-boot-config` 开冻结窗口，
记录文件哈希、回滚方法和单变量 A/B；不得与其他 WiFi/USB/NVIDIA/音频变更叠加。

## 不在本轮范围

- NVRAM 写入、早期启动日志。
- IOMMU/AppleVTD。
- GPU 电源/GPU 传感器。
- CPU 满载压测。
- 睡眠/唤醒循环。
- 修改 EFI、BIOS 或加载/卸载任何驱动。
