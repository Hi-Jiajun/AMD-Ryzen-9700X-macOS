# mac-watch 探针 P0 交接

更新时间：2026-10-21 08:06 +08:00

## 当前状态

- 主机：`hiliang@192.0.2.60`（`dockerserver-ms`）
- `mac-watch-recorder.service`：active，`Type=notify`，`WatchdogSec=240`
- `mac-watch-alert.service`：active
- 主目标：`192.0.2.196`；secondary：`192.0.2.130`
- `.130` 当前长期 down：不单独推送，但保留账本并进入日报
- `.alert-outbox` 当前为空；`.alert-offset` 与 `events.log` 大小一致
- 生产 `state.log` 已实际轮转；当前写入约每 5 秒一行
- USB 回归观察器每 5 分钟扫描最近 300 秒 unified log；当前 boot CRITICAL=0

## P0 已落地

1. `state.log` / `beacon.log` 日切或 256 MiB 轮转，旧归档进入
   `~/mac-watch/logs/`；gzip 保留 mtime。
2. `logs/*.gz` 保留 14 天，`auto-*` 压缩后保留 7 天，判断依据为文件名日期。
3. `state.log` 正常 5 秒采样；1 秒 HTTP/ping 检测不变，异常立即写。
4. `mac-watch-alert.py` 使用持久 outbox、per-sink 指数退避；至少一个 sink 成功
   后才推进 offset/cooldown。
5. recorder 真发 `sd_notify READY/WATCHDOG`；主服务与 alert 都有 `OnFailure=`。
6. `.130` 降噪；`.196 + .130` 双失联合并为 `machine-lost` high。
7. `DUMP_FAIL` 在 primary down 时不再单独刷屏；`TARGET_MOVED` 增加恢复消息。
8. `USB_ERRORS` 只认 HX40/CRT/CDF/POD 四个硬失败，按
   `(signature, port, 60s)` 去重；告警 1 小时 cooldown，零命中每小时汇总。

## 常用检查

```bash
ssh hiliang@192.0.2.60 \
  'systemctl --user status mac-watch-recorder.service mac-watch-alert.service'

ssh hiliang@192.0.2.60 \
  'systemctl --user show mac-watch-recorder.service \
   -p Type -p NotifyAccess -p WatchdogUSec -p OnFailure -p ActiveState'

ssh hiliang@192.0.2.60 \
  'tail -n 5 ~/mac-watch/state.log; tail -n 20 ~/mac-watch/events.log'

ssh hiliang@192.0.2.60 \
  'cat ~/mac-watch/.alert-offset; stat -c %s ~/mac-watch/events.log; \
   find ~/mac-watch/.alert-outbox -maxdepth 1 -type f | wc -l'

ssh hiliang@192.0.2.60 \
  '/usr/bin/python3 ~/probe-report.py --hours 8'
```

## 重要路径

```text
远端脚本：
  ~/mac-watch-recorder.py
  ~/mac-watch-alert.py
  ~/mac-watch-digest.py
  ~/probe-report.py

远端 units：
  ~/.config/systemd/user/mac-watch-recorder.service
  ~/.config/systemd/user/mac-watch-alert.service
  ~/.config/systemd/user/mac-watch-recorder-failure.service
  ~/.config/systemd/user/mac-watch-alert-failure.service

运行状态：
  ~/mac-watch/.alert-offset
  ~/mac-watch/.alert-state.json
  ~/mac-watch/.alert-outbox/
  ~/mac-watch/.usb-observer-state.json
  ~/mac-watch/state.log
  ~/mac-watch/beacon.log
  ~/mac-watch/logs/

回滚：
  ~/probe-ops-backups/20261021-p0-pre/

本地交付：
  outputs/20261021-probe-ops/P0-REPORT.md
  outputs/20261021-probe-ops/USB-OBSERVER-REPORT.md
  outputs/20261021-probe-ops/changes.patch
  outputs/20261021-probe-ops/tests/
```

## 验证入口

```bash
# 用已部署代码跑临时目录测试；不接触生产 OUT，也不推生产 topic
cd /home/you/probe-ops-staging-20261021-p0/tests
PROBE_SCRIPTS_DIR=/home/you /usr/bin/python3 test_recorder_p0.py -v
PROBE_SCRIPTS_DIR=/home/you /usr/bin/python3 test_alert_p0.py -v
PROBE_SCRIPTS_DIR=/home/you /usr/bin/python3 test_digest_p0.py -v
```

## 回滚

完整命令见 `outputs/20261021-probe-ops/P0-REPORT.md` 第 6 节。

推荐只回滚 recorder/alert/units，保留新的 `probe-report.py` 与
`mac-watch-digest.py`，否则旧报告器看不到已经生成的 state/beacon 轮转归档。

## 已知注意事项

- `state.log` 正常 5 秒一行；不要把 5 秒间隔误判为探针卡死。watchdog 仍每 60 秒
  以内部 thread/seq 心跳判断。
- 已关闭的 state/beacon 归档会在 600 秒后由 housekeeping 自动 gzip。
- 当前 `.130` down 是预期 secondary 状态，不应单独触发推送。
- `USB_ERRORS` 的当前 boot 状态以最新事件里的 `boot_epoch/status` 为准；
  2 小时窗口在 `09:55:46` 结束，24 小时窗口在 2026-10-22 `07:55:46` 结束。
- 生产 events.log 中被早期 staging 测试误写了两条 GZIP 账本行，已追加
  `TEST_ARTIFACT` 说明；历史文件没有被改写。
