# USB 探针检查清单（S2，2026-10-21）

用途：供 `192.0.2.60` 的 mac-watch 探针实现 `USB_ERRORS`。
只读监测；不改 ESP、不重启、不加载/卸载 kext。

## 0. 结论先行

- **硬失败只看 4 个签名**：`hardwareExceptionThreadCallGated: 0x00000040`、
  `CommandRing::executeCommand ... timeout after Nms`、`createDevice: failed ... 0xe00002bc`、
  `powerOn: deadline passed`。任一命中即判失败，不需要连续两次。
- `CommandRing::executeCommand` **单独出现不是失败**：正常完成会打 `... got result ... after 0ms`。
- **不能再用 `HS11@01000000` 当当前判据**：S2 中 HS11（硬件 16）在 `0x00600000`；
  旧值只用于回放 10-17 历史日志。用旧值做探针会产生**假阴性**。
- S2 启动后至 08:01（约 5 分 50 秒）：4 个硬失败签名 **0 条**；有 1 条启动期
  `stopEndpoint ... result code 19`，它是已知噪声，不是命令超时。

## 1. 当前 S2 基线

| 项 | 值 |
| --- | --- |
| 启动 | `2026-10-21 07:55:46` |
| 注入 | `Prelinked injection UTBMap.kext ... - Success`、`UTBMap.kext v1.1` |
| XH00 | 7 对象：`2,3,7,8,15,16,17` |
| locationID | `0x00100000`..`0x00700000`（逻辑 1..7） |
| 其他控制器 | XHC0/XHC1/XHC2 = 4/4/1 |
| 蓝牙 | `State: On`，`Address: 02:1A:7D:DA:71:2A`，`SimpleMtkBtFw` 已加载 |
| 硬失败签名 | **0**（07:55:46–08:01，约 5m50s） |

已知基线噪声（不计失败）：

```text
2026-10-21 07:55:47.895 E kernel[0:35d] [com.apple.usb:all]
  AppleUSBXHCICommandRing::executeCommand: command[3] ... got result ... after 0ms
2026-10-21 07:55:47.895 E kernel[0:35d] [com.apple.usb:all]
  AppleUSBXHCICommandRing: AppleUSBXHCICommandRing::stopEndpoint: completed with result code 19
2026-10-21 07:57:03.462 E kernel[0:1ef4] [com.apple.usb:all]
  AppleUSBHostUserClient::openGated: failed to open Wireless_Device@00600000:
  provider is already opened for exclusive access by pid 159, bluetoothd
```

## 2. 硬失败模式（CRITICAL）

| ID | 精确匹配（子串/正则） | 含义 | 备注 |
| --- | --- | --- | --- |
| `HX40` | `hardwareExceptionThreadCallGated: 0x00000040` | XHCI 控制器命令失败并进入 recovery | 10-17 全天 2787 条；任一命中即失败 |
| `CRT` | `CommandRing::executeCommand` **且** `timeout after [0-9]+ms` | 命令环超时 | 10-17 为 1001/1078ms；`after 0ms` 不算 |
| `CDF` | `createDevice: failed to create device (0xe00002bc)` | 端口上设备创建失败（Set Address 超时） | 10-17 全天 2775 条，全部 HS11 |
| `POD` | `powerOn: deadline passed` | 端口上电超时 | 10-17 为 `HS02@00700000 (PORTSC 0x00000000)` |

判定：同一启动窗口内任意一条 `HX40` / `CRT` / `CDF` / `POD` → `USB_ERRORS critical`。
不要要求“连续两条”；不要用行数阈值；先报第一次命中。

## 3. 支持性证据（WARN，需与硬失败配对）

这些单独出现可能是正常/边缘事件；若在硬失败前后 ±5 秒内出现，附在 evidence 里：

| 匹配 | 说明 |
| --- | --- |
| `abortCommand: did not generate an event` | 命令环异常后的清理 |
| `hardware became unavailable` | 控制器/端口不可用 |
| `setPowerStateGated: going to state 1 returned 0xe00002e9` | 上电状态失败 |
| `attempting recovery` | recovery 开始 |

## 4. 历史签名 → S2 位置映射（防假阴性）

10-17 是 17 对象布局；S2 是稀疏 7 对象，位置号被压紧。**旧 locationID 不能复用为当前判据。**

| 名称（map 名） | 硬件口 | 10-17 旧 locationID | S2 当前 locationID |
| --- | --- | --- | --- |
| `SS02` | 2 | `0x00200000` | `0x00100000` |
| `SS03` | 3 | `0x00300000` | `0x00200000` |
| `HS02` | 7 | `0x00700000` | `0x00300000` |
| `HS03` | 8 | `0x00800000` | `0x00400000` |
| `HS10` | 15 | `0x00F00000` | `0x00500000` |
| `HS11` | 16 | `0x01000000` | **`0x00600000`** |
| `HS12` | 17 | `0x01100000` | **`0x00700000`** |

探针应按以下优先级归属：

1. 端口对象上的 `port`（硬件号）与 `locationID` 组合；
2. `HS11` / `HS12` 名称 + **当前** locationID；
3. 绝不能只用 `HS11@01000000`。

## 5. 正常噪声（不要判失败）

| 现象 | 为什么正常 |
| --- | --- |
| `executeCommand ... got result ... after 0ms` | 命令正常完成；只有 `timeout` 才失败 |
| `stopEndpoint: completed with result code 19` | 启动期端点停止；S2 已出现 1 条，不是 timeout |
| `provider is already opened for exclusive access by pid ..., bluetoothd` | 蓝牙守护进程独占设备；其他进程打开被拒 |
| `SimpleMtkBtFw selected configuration 1` | 蓝牙驱动正常接管 |
| `0e8d/0616` 枚举 → `0a12/0001` → `0e8d/0616` | 10-16 的固件重载循环；当天 16 次，`createDevice failed`=0 |
| `destroying ...: port is powering off` | 驱动主动 reset/断电，不是控制器故障 |
| `0x100 (lost power)` 同时出现在 XHC0/1/2 | 空闲睡眠整组掉电，不能当 XH00 根因 |
| CoreAudio `HALS_OverloadMessage` / `0xE00002EE` | 高负载音频过载；07:58 load1≈20 已有基线，与 USB 无关 |

## 6. 阈值与观察窗口

| 窗口 | 判据 |
| --- | --- |
| 启动后 0–2h | 任一 CRITICAL → **FAIL**；无 CRITICAL → 临时 PASS（继续观察） |
| 启动后 24h | 任一 CRITICAL → **FAIL**；0 条 → **PASS（无旧补丁症状）** |
| 7 天（可选） | 只作长期报告；出现 CRITICAL 仍按 FAIL 处理 |
| 恢复/重启后 | 窗口重置为 `since_boot`，重新记录 baseline；不继承上一启动的计数 |

去重：同一 `(signature, port, 60s bucket)` 只算 1 个事件，但保留 `count` 与 `first/last seen`。
10-17 的 2775/2787 行本质是同一类 recovery 风暴，不能按行数当独立事件。

## 7. `USB_ERRORS` 事件建议

```json
{
  "event": "USB_ERRORS",
  "severity": "critical",
  "window": "since_boot",
  "signature": "CDF",
  "controller": "1022:43fc",
  "port": 16,
  "locationID": "0x00600000",
  "count": 1,
  "first_seen": "2026-10-21T08:00:00+0800",
  "last_seen": "2026-10-21T08:00:00+0800",
  "evidence": ["HS11@00600000: AppleUSBHostPort::createDevice: failed ... (0xe00002bc)"]
}
```

采集 predicate（Mac 侧）：

```text
process == "kernel" AND subsystem == "com.apple.usb"
AND (eventMessage CONTAINS "hardwareExceptionThreadCallGated"
  OR eventMessage CONTAINS "CommandRing::executeCommand"
  OR eventMessage CONTAINS "createDevice: failed"
  OR eventMessage CONTAINS "powerOn: deadline")
```

分类顺序：先判 `HX40` → `CRT` → `CDF` → `POD`；再判 WARN；其余 INFO。

## 8. 历史证据（10-17）

来源：`outputs/20261010-usbportlimit/review/incident-20261017.md`、
`timeline-audit-20261017.md`。

```text
05:25:12.316 AppleUSBXHCICommandRing::executeCommand: ... timeout after 1078ms
12:32:43.941 AppleUSBXHCICommandRing::executeCommand: ... timeout after 1001ms
12:32:44.046 HS11@01000000: AppleUSBHostPort::createDevice: failed to create device (0xe00002bc)
12:32:44.380 HS02@00700000: AppleUSBXHCIPort::powerOn: deadline passed (PORTSC 0x00000000)
12:32:44.387 AppleUSBXHCIPCI@00000000: hardwareExceptionThreadCallGated: 0x00000040 (command failure)
```

- 分界 `2026-10-17 13:22:08` 前：HS11 `createDevice failed` 2775 行，`0x40` 2787 行；
  之后到 15:17 均为 0。
- 风暴固定循环约 2.61 秒；首次失败常落在唤醒/启动后 ±1.5 秒。
- 10-16 的 `0e8d→0a12→0e8d` 是 16 次固件重载，**没有** `createDevice failed`，
  不能与 10-17 的 Set Address 失败混为一类。

## 9. 已知限制

- 探针只能看到有日志覆盖的窗口；10-17 05:24:52–05:25:11 有约 20 秒睡眠断流。
- `log stream` 某些字段可能被 `<private>` 脱敏；本轮 `com.apple.usb` 实测可见。
- S2 尚无 Hub 实机；Hub 后代冲突只有静态模型。
- 本清单只定义“旧补丁症状”监测，不把蓝牙协议栈、音频或睡眠的单独问题算作 USB 失败。
