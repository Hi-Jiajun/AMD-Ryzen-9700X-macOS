# Handoff：amd-bt-mt7922（C2）

更新时间：2026-10-21 09:31 CST  
状态：**C2 runtime PASS（基础连接 / 音频 / 断开重连）**；重启与睡眠持久性待验。

## 本轮范围

- 只做 MT7922 蓝牙运行态验收，不改 EFI、ESP、BIOS、USB S2、WiFi 驱动或旧 WiFi 线程。
- 不做 100 次插拔、不做 30 分钟音频压测。
- 测试设备：`Liang的AirPods Pro`（`74:3F:8E:A6:43:AA`）。

## 结论

- **控制器枚举 PASS**：本次启动 `2026-10-21 09:16:51 CST`；XH00 的 HS11（硬件 16）在
  `Wireless_Device@00600000` 枚举，`locationID=0x00600000`、`PortNum=6`、`Built-In=Yes`。
- **控制器运行 PASS**：`State: On`，Address `02:1A:7D:DA:71:2A`，
  `BluetoothTransportConnected = Yes`，`bluetoothd` PID 159 持有 HCI 用户客户端。
- **协议栈加载 PASS**：`SimpleMtkBtFw 1.0.0`、`SimpleMtkBtPatcher 1.0.0`、
  `BlueToolFixup 2.7.2` 已加载。
- **设备连接 PASS**：AirPods Pro 已连接，服务为
  `HFP AVRCP A2DP AACP GATT ACL`；RSSI 约 -52～-56 dBm，左右电量 80/79%。
- **音频路由 PASS**：AirPods 同时是默认输入、默认输出和默认系统输出；
  输入 1 声道 / 8 kHz，输出 2 声道 / 48 kHz，Transport 均为 Bluetooth。
- **播放/暂停 PASS**：QuickTime 对 6–7 秒本地 Aiff 报告
  `playing_before=true; playing_while_paused=false; playing_after_resume=true`；
  A2DP 在暂停时 `off`、恢复时 `on`。
- **断开/重连 PASS**：09:30:07 通过系统设置主动断开；09:30:34 重连成功，
  A2DP 于 09:30:43 重新 `on`。重连后播放系统音效返回码 0。
- **稳定性 PASS（本轮窗口）**：09:24–09:31 无 panic 文件、无蓝牙控制器 reset；
  USB 硬失败签名 `HX40=0 CRT=0 CDF=0 POD=0`。

## 时间线

| 时间 | 事件 |
| --- | --- |
| 09:16:51 | 本次启动；控制器随后进入 On |
| 09:17:15 | 启动期 HCI 仍有 `status=65535` / `STATUS 408` 告警，但初始化继续 |
| 09:25:44 | AirPods `DeviceConnected`，`BT_SUCCESS` |
| 09:27:06–09:27:22 | QuickTime 播放、暂停、恢复；状态 `true → false → true` |
| 09:30:07 | 系统设置主动断开 AirPods |
| 09:30:34 | AirPods 重连成功 |
| 09:30:43–09:30:45 | 重连后 A2DP 重新启动并结束，`afplay` 返回 0 |
| 09:30:47 | 终态复核：Controller On、AirPods Connected、默认输入/输出仍是 AirPods |
| 09:31 | 关闭蓝牙设置页；`Discoverable` 恢复 Off |
| 09:32:03 | 验收后 AirPods 进入 `InCase` / `LidClosed`，随后正常变为 Not Connected |

## 证据命令

```bash
sysctl -n kern.boottime
system_profiler SPBluetoothDataType
system_profiler SPAudioDataType
ioreg -r -c IOBluetoothHCIController -l -w 0
ioreg -p IOUSB -l -w 0
kmutil showloaded | rg -i 'SimpleMtkBtFw|SimpleMtkBtPatcher|BlueToolFixup'
/usr/bin/log show --style compact --start '2026-10-21 09:24:00' \
  --end '2026-10-21 09:31:00' \
  --predicate 'process == "bluetoothd" AND eventMessage CONTAINS[c] "74:3F:8E:A6:43:AA"'
/usr/bin/log show --style compact --start '2026-10-21 09:16:51' \
  --end '2026-10-21 09:31:00' \
  --predicate 'process == "kernel" AND subsystem == "com.apple.usb"'
find /Library/Logs/DiagnosticReports "$HOME/Library/Logs/DiagnosticReports" \
  -maxdepth 1 -type f -name '*.panic' -newermt '2026-10-21 09:16:51'
```

## 已知残余

1. 启动期仍有 HCI 事件 `status=65535` 和
   `ReadLocalVersionInformation failed: STATUS 408`；控制器随后继续初始化并完成
   全部 runtime 验收，故本轮不判失败，但应保留为后续重启回归观测点。
2. 第一轮用 `SIGSTOP/SIGCONT` 暂停 `afplay` 时，播放器返回 145；随后改用
   QuickTime 的正式 `play/pause/play` 状态机验证通过。该返回码未伴随蓝牙、
   USB 或控制器异常，不作为蓝牙失败。
3. `adb-2026-10-21-091847.ips` 是 `adb` 用户态崩溃报告，不是内核 panic。
4. 09:32:03 的 Not Connected 是 AirPods 收回充电盒导致，日志为
   `InCase` / `LidClosed`，不是测试中的掉线。
5. 重启、睡眠/唤醒持久性未在本轮验收。

## 下一步

- 下次计划内重启后复核：Controller On、Address 非 NULL、HS11 枚举、
  AirPods 自动或手动重连。
- 再做一次睡眠/唤醒后的短连接测试即可；无需长压测。
- 如启动期 `STATUS 408` 持续出现且影响重连，再另开最小修复任务，不在本轮改驱动。

完整验收记录与日志摘录：
`outputs/20261021-bt-mt7922/RUNTIME-ACCEPTANCE.md`。
