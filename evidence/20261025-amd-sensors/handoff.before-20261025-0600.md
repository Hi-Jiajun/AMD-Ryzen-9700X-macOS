# amd-power-smc handoff

更新时间：2026-10-21 09:45 CST  
状态：第一轮只读基线完成；原版 SMCAMDProcessor 判定 No-Go；自研只读传感器原型为
Conditional Go；当前稳定默认档为“macOS 不管 P-state + BIOS 风扇”。

## 本次证据

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

## 当前阻塞/未决

- MAXSUN B850BKB WIFI 的实际 SuperIO 型号未知。BIOS IFR 证明有四路风扇，但
  macOS 用户态无法执行 SuperIO I/O；需要 Windows/HWiNFO、板厂资料，或经批准的
  一次只读 `SMCSuperIO` 识别实验。
- 用户尚未决定是否值得为温度/频率可见性投入自研 kext。
- `powermetrics` 未取得 root 结果；不能把它列为已排除的通道。
- 睡眠/唤醒未测，且本轮明确不作为验收项。

## 下一步（需用户批准后才做）

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
