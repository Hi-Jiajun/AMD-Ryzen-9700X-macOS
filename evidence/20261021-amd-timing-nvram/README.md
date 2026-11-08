# 2026-10-21 AMD 定时器 / NVRAM 只读基线

状态：第一轮只读测量完成。未写 ESP、未改 EFI/BIOS、未重启。

结论：

- 当前启动为 macOS 15.8.1 / 24H32 / Darwin 24.6.0，`kern.boottime=2026-10-21 09:16:51 CST`。
- timer 补丁在本次启动日志中命中一次并返回 `Success`；当前 config 与 ESP 基线哈希一致。
- `mach_wait_until` 的实时线程模式已不再出现历史 7.93–7.99× 上界；本轮三轮测量中位为
  `1.027 / 1.007 / 1.004×`（100 / 500 / 1000 µs），各轮最坏上界为
  `1.074 / 1.013 / 1.009×`。
- “补丁本身已生效”有启动日志与离线 KC 四字节差异双重证据；“当前组合改善”不能单独归功于
  timer，因为 USB 端口补丁状态也发生了变化，正式单项归因仍需 A/B。
- `DisableVariableWrite=true` 保持。当前不建议改成 `false`；写记录器已从当前 ESP 移除，
  NVRAM 写者归属目前不可判断。
- 当前 OpenCore 日志只覆盖到 `EXITBS:START`；内核接管后、磁盘报告器尚未就绪时的 panic
  仍缺少可靠落盘通道。

建议的最小下一步是：如要完成验收，只做一次 `timer Enabled=false`、USB S2 与其他变量
全部保持不变的受控 A/B；这需要另开冻结窗口并重启，本轮不执行。NVRAM 不改；
若以后必须补写者证据，再单独申请 `NVRWLog` 第一阶段（只记录、不转发）窗口。

详细证据：

- `BASELINE.md`
- `TIMING.md`
- `PATCH-HIT.md`
- `NVRAM-PANIC.md`
- `evidence/`
- `../../docs/handoff-amd-timing-nvram.md`
