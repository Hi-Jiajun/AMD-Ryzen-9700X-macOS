# AMD CPU 电源管理与 SMC 传感器只读基线

采样窗口：2026-10-21 09:41:51–09:45:02 CST  
主机：Ryzen 7 9700X / 48 GB / macOS 15.8.1（24H32）/ MacPro7,1  
范围：A3/A4 的第一轮只读测量与 SMC 键枚举

本轮没有改 EFI/BIOS/config，没有重启，没有加载/卸载驱动，也没有做 CPU 压测。
09:45 的 `top` 读数为 18.26% user + 13.10% sys、68.62% idle，负载均值
6.41/5.29/6.00；这是存在后台任务的轻载快照，不应称为严格空闲。

## 1. 结论

1. macOS 侧没有启用 XCPM：`machdep.xcpm.mode=0`，
   `vectors_loaded_count=0`，`ratio_changes_total=0`。
2. `hw.cpufrequency`、`hw.cpufrequency_min`、`hw.cpufrequency_max`
   三者都报告 5,140,000,000 Hz，是静态占位值，不是实时频率。
3. `system_profiler` 同时报告 845 MHz，也是已知的错误展示值；不能把二者任一当成
   当前核心频率。
4. `pmset -g therm` 明确报告没有 CPU power status。
5. VirtualSMC 1.3.9 正在运行，但没有加载 `SMCProcessor`、`SMCSuperIO` 或
   `SMCAMDProcessor`。当前 65 个 SMC 键中，真实温度/风扇/电压/功耗/频率读数均为 0。
6. 物理 CPU 和主板当然有温度、风扇、电压采样电路；当前缺的是 macOS/SMC
   *导出通道*，不是硬件传感器本身。
7. 本轮没有证明 CPU 被锁在 5.14 GHz。可证明的是 macOS 不参与 P-state 管理，
   且 macOS 没有可信的实时频率读数；固件侧仍可能自行调频。

## 2. CPU / XCPM 现状

命令：

```sh
sysctl -n machdep.xcpm.mode
sysctl -n hw.cpufrequency hw.cpufrequency_min hw.cpufrequency_max \
  hw.busfrequency machdep.tsc.frequency hw.model hw.ncpu \
  machdep.cpu.brand_string machdep.cpu.vendor \
  machdep.cpu.family machdep.cpu.model machdep.cpu.stepping
sysctl -a | rg '^(machdep\.xcpm|machdep\.cpu\.thermal|hw\.cpufrequency|machdep\.tsc\.frequency|hw\.busfrequency)'
```

关键结果：

```text
machdep.xcpm.mode                         0
hw.cpufrequency                    5140000000
hw.cpufrequency_min                5140000000
hw.cpufrequency_max                5140000000
hw.busfrequency                    3424000000
machdep.tsc.frequency              5140336049
hw.model                           MacPro7,1
hw.ncpu                            16
machdep.cpu.brand_string           AMD Ryzen 7 9700X 8-Core Processor
machdep.cpu.vendor                 AuthenticAMD
machdep.cpu.family                 26
machdep.cpu.model                  68
machdep.cpu.stepping               0
```

Family `26 = 0x1A`、model `68 = 0x44`，与 Zen 5 Granite Ridge
Family 1Ah Model 44h 一致。

同一输出中的 XCPM 状态还包括：

```text
machdep.xcpm.vectors_loaded_count          0
machdep.xcpm.ratio_changes_total           0
machdep.xcpm.deep_idle_count               0
machdep.xcpm.hard_plimit_max_100mhz_ratio  0
machdep.xcpm.soft_plimit_max_100mhz_ratio  0
machdep.xcpm.cpu_thermal_level             0
machdep.cpu.thermal.sensor                 0
machdep.cpu.thermal.package_thermal_intr   0
machdep.cpu.thermal.core_power_limits      0
```

这些 Intel 风格计数/能力位为零，只能说明当前 macOS 路径不可用；不能据此推断
物理 CPU 没有热传感器。Intel 风格的 `X86PlatformPlugin` 和 `X86PlatformShim`
仍被加载，但在 AMD 上没有产生可用的 P-state/传感器结果。

## 3. pmset 与受限读数

`pmset -g` 关键项：

```text
hibernatemode        0
powernap             0
sleep                0 (sleep prevented by ChatGPT)
disksleep            0
displaysleep         0
```

`pmset -g therm`：

```text
Note: No thermal warning level has been recorded
Note: No performance warning level has been recorded
Note: No CPU power status has been recorded
```

本轮没有睡眠/唤醒测试，不能从 `pmset` 推断 A3/A4 对睡眠的影响。

`powermetrics` 尝试：

```text
$ /usr/bin/powermetrics --samplers smc -n 1 -i 1000
powermetrics must be invoked as the superuser

$ sudo -n /usr/bin/powermetrics --samplers smc -n 1 -i 1000
sudo: a password is required
```

因此本轮没有取得 `powermetrics` 结果，也没有要求用户提供密码。

`system_profiler SPHardwareDataType` 的关键输出：

```text
Processor Name: AMD Ryzen 7 9700X 8-Core Processor
Processor Speed: 845 MHz
```

845 MHz 与 sysctl 的 5.14 GHz 都是静态/派生显示，不互相校验真实频率。

## 4. 内核与 SMC 运行态

命令：

```sh
kmutil showloaded | rg -i 'VirtualSMC|AppleSMC|SMCAMD|AMDRyzen|SMCProcessor|SMCSuperIO|X86Platform|Lilu'
ioreg -c AppleSMC -r -l -w0
for c in IOPlatformSensor IOPlatformStateSensor AppleACPIThermalZone AppleACPIEC AppleSMBusController; do
  ioreg -c "$c" -r -l -w0 2>/dev/null | wc -l
done
```

加载结果：

```text
com.apple.driver.AppleSMC                  3.1.9
as.vit9696.Lilu                            1.7.3
as.vit9696.VirtualSMC                      1.3.9
com.apple.driver.X86PlatformPlugin         1.0.0
com.apple.driver.X86PlatformShim           1.0.0
```

未发现：

```text
SMCAMDProcessor
AMDRyzenCPUPowerManagement
SMCProcessor
SMCSuperIO
```

IORegistry：

```text
AppleSMC             1 instance, MMIO, APP0001, AppleSMCClient
IOPlatformSensor     0 instances
IOPlatformStateSensor 0 instances
AppleACPIThermalZone 0 instances
AppleACPIEC          0 instances
AppleSMBusController 0 instances
```

所以当前只有 VirtualSMC 的 SMC 用户客户端，没有 Apple 平台传感器对象，也没有
ACPI thermal zone、EC 或 SMBus 传感器通道。

## 5. 其他可读通道

- CPU 占用、内存、磁盘、网络等软件计数器可用。
- NVMe 控制器报告 `NVMe SMART Capable = Yes`，但本轮没有安装 `smartctl`，
  因而没有读取 NVMe 温度。它不是 CPU/主板温度。
- GPU 传感器不在本任务范围，本轮未重新评估。
- 主板 BIOS IFR 证明固件有 4 路风扇（CPU/Pump/Sys1/Sys2）、5 点风扇曲线和
  CPU/MEM/SYS/PCH 温度源；这些是 BIOS 控制面，不是 macOS 已导出读数。

## 6. 本基线能回答与不能回答

能回答：

- macOS 当前没有 XCPM 调频状态；
- 当前 SMC 没有 CPU 温度/风扇转速/电压/功耗/实时频率；
- VirtualSMC 的工作实例存在，但缺少传感器插件；
- `hw.cpufrequency=5.14 GHz` 和 System Profiler 的 845 MHz 均不是实时频率。

不能回答：

- CPU 的真实瞬时/平均频率；
- CPU 当前温度、封装功耗、Vcore；
- 四路风扇的 RPM 或实际占空比；
- CPU 在此期间是否由固件内部降频；
- 睡眠/唤醒是否会被未来的 P-state kext 影响。

## 7. 证据与工具

- SMC 键枚举工具源码：
  `outputs/20261021-amd-power-smc/smcdump.c`
  sha256 `5bc1106b34151946668fd8b818145fc98113e3c3a89af2df3507aad5acef9d11`
- 编译产物：
  `outputs/20261021-amd-power-smc/smcdump`
  sha256 `86d52dd0bd83c3bf5a50eb108c5f8cc7efe67dd4a7164e193309ae8e75ed10be`
- 该工具只打开 `AppleSMC` 并执行
  `READ_INDEX`/`READ_KEYINFO`/`READ_BYTES`，不执行 SMC 写命令。
- 启动时间：`Wed Oct 21 09:16:51 2026 CST`。
- 证据类型：本机 sysctl/pmset/kmutil/IORegistry 只读输出、一次编译执行的 SMC 枚举。
