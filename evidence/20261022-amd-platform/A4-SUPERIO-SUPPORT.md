# SMCSuperIO 支持范围核实（2026-10-22）

> 2026-10-23 校准：下文“四路风扇可得”仅是 BIOS 通道线索，不是插件实测保证；
> 实际可读通道数须装机验证。“未知芯片不写寄存器”仅可理解为没有该设备的监控写入，
> 识别阶段仍可能写配置模式握手。当前探针流程见 `A4-PROBE-READY.md`。

来源：`gh api repos/acidanthera/VirtualSMC`（访问日期 2026-10-22，head 分支 `master`）。
只读调研；未改 ESP/EFI/BIOS，未加载 kext。

## 1. 结论先行

| 问题 | 答案 |
|---|---|
| NCT668x 系（6683/6686/6687）在支持列表里吗？ | **在**。`Devices.cpp` 里有 `Device_0xC730` / `Device_0xD440` / `Device_0xD590` 三个类，分别报 `Nuvoton NCT6683D` / `NCT6686D` / `NCT6687D`。 |
| 它们能读什么？ | **风扇转速 + 电压**。对应基类 `GeneratedNuvotonDevice_13` 只实现了 tachometer 与 voltage；**没有 temperature**。 |
| 没有 temperature 会怎样？ | 基类 `SuperIODevice::getTemperatureCount()` 默认返回 `0`，所以不会注册温度 key，也不会报错——只是看不到温度。 |
| 未知芯片会怎样？ | 匹配不上就 `createDevice` 返回 0，插件跳过这个 SuperIO，不写寄存器，安全。 |

## 2. 证据

### 2.1 NCT668x 三个型号

`Sensors/SMCSuperIO/Devices.cpp`：

| 类 | 匹配 | 型号串 |
|---|---|---|
| `Device_0xC730` | `(deviceId & 0xFFF0) == 0xC730` | `Nuvoton NCT6683D` |
| `Device_0xD440` | `(deviceId & 0xFFF0) == 0xD440` | `Nuvoton NCT6686D` |
| `Device_0xD590` | `(deviceId & 0xFFF0) == 0xD590` | `Nuvoton NCT6687D` |

三者都继承 `GeneratedNuvotonDevice_13`。该基类的方法清单（同文件，class 起点约 1630 行起）只有：
`getTachometerCount` / `updateTachometer` / `getTachometerName` /
`getVoltageCount` / `updateVoltage` / `getVoltageName` / `getLdn` / `getModelName`。
**没有 `getTemperatureCount` / `updateTemperature`**。

注意匹配用的是掩码：`0xC730` 匹配 `0xC730–0xC73F`，所以读到 `0xC731`、`0xC733` 也算命中，
不必精确等于 datasheet 的完整 ID。

### 2.2 温度 key 的默认行为

`Sensors/SMCSuperIO/SuperIODevice.hpp`：

- 第 194 行：`virtual uint8_t getTemperatureCount() { return 0; }`
- 第 199/204 行：注册与更新都先判断 `index < getTemperatureCount()`。

所以 NCT668x 这类没覆写该方法的芯片，温度通道数就是 0，**不注册、不读、不报错**。

### 2.3 其他厂商（对照）

同文件里还有 ITE（`ITEDevice.cpp`）、Fintek（`FintekDevice.cpp`）、Winbond（`WinbondDevice.cpp`）、
EC（`ECDeviceGeneric.cpp` / `ECDeviceNUC.cpp`）四族，共 70 个 `createDevice` 入口。
`Docs/SuperIOs.md` 明确写：*"SMCSuperIO can read fan speeds on ITE/Nuvoton/Winbond and set fan
speeds on ITE."* —— 风扇是主打能力，温度覆盖因型号而异。

### 2.4 未知芯片

`Devices.cpp` 的 `getDevice` 链式匹配，全部 `createDevice` 返回 0 时就返回 `nullptr`；
调用方不创建 SMC key，不触碰 SuperIO 寄存器。

## 3. 拿到 chip id 后的三步判定

1. **读 ID**：`0x4E/0x4F` 或 `0x2E/0x2F`，寄存器 `0x20`（高字节）+ `0x21`（低字节）。
   本机脚本：`/Volumes/MTKCAP/superio-probe.sh`。
2. **对型号**：拿 16 位 ID 与 `Devices.cpp` 的 `(deviceId & 0xFFF0) == 0x????` 列表比对；
   命中即算出型号名（例如 `0xC73x` → NCT6683D）。
3. **看能力**：命中 NCT668x → 只有风扇 + 电压；若还需要 CPU 温度，得另找通道
   （BIOS/EC，或别的传感器方案），不能指望 `SMCSuperIO`。

## 4. 对本机的含义

| 情形 | 结果 |
|---|---|
| 读到 `0xC73x` / `0xD44x` / `0xD59x` | 装 `SMCSuperIO` 可得 **4 路风扇 RPM**（BIOS IFR 里确实是 CPU/Pump/Sys1/Sys2 四路），**无温度** |
| 读到其他已支持型号 | 看该型号是否覆写 `getTemperatureCount`；有则温度也能出 |
| 读不到或不在列表 | 维持现状（BIOS 控风扇），不再投入 |

## 5. 关于 SMCProcessor

`SMCProcessor` 走的是 Intel 的 MSR/温度模型（`Sensors/SMCProcessor`），在 AMD 上不产生有效的
CPU 温度。计划里 A3/A4 的结论（原版 `SMCAMDProcessor` 只支持 17h/19h、本机是 1Ah）与此一致：
**CPU 温度这条线在 AMD 上仍未解决**，`SMCSuperIO` 最多补风扇 RPM。

## 6. 不确定项

- MAXSUN B850BKB WIFI 实际用的 Nuvoton 型号未知，需 chip id 才能定；
- 若实际是 `0xD59x`（6687D），风扇通道数以插件实现为准（`getTachometerCount` 覆写值），
  未在本机验证；
- 风扇 RPM 的 SMC key 名称与 BIOS IFR 的四路风扇是否一一对应，需要装机后实测。
