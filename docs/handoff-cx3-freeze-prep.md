# 交接：CX3 `attachInterface` 冻屏取证准备

状态：**0.44.38 已安装但 12:46 再次硬冻；0.44.39 只增加 open 路径观测，已离线构建，未安装。**

## 2026-10-21 12:46 更新

- 0.44.38 最终 binary sha256
  `44ae83639f404d17ea7484877b2e14fef87ed37bd327970808042c4f284ec63c`，当前已加载；
- 12:46:43 最后一行为 `pre selector 46 netupstop 2`，12:46:44.686 60 侧记录
  `.196 DEAD`，12:48:05 重新 boot；
- 驱动没有写出 selector 46 的 `phase=1`，也没有 netup/hook；0.44.38 的 worker
  修正没有覆盖实际冻结点，不能视为修复；
- 复盘发现 `cx3ctl` 的精确 open breadcrumb 没启用：复现脚本没导出 `CX3_BC`，
  且安装的 `cx3ctl` 未签名；
- 0.44.39 增加 `cx3OpenPhase`、`cx3ctl bctest`、`cx3ctl opentest`，构建产物：
  `outputs/20261021-cx3-open-probe/build-0.44.39/`，kext sha256
  `5e48baab9b2d48975df4ab3d91448625db86c35e7a1f3a6bff396b249b26ca80`；
- 当前没有新的复现放行口令，禁止安装 0.44.39、运行 selector 或重启。

本轮审查阶段没有再次触发 attach、重启或执行 selector，也没有改
EFI/BIOS/kext/USB/WiFi/NVIDIA 或 60 侧探针语义。11:32 的冻屏是用户放行后的上一轮复现，
11:34 已冷启动恢复。

## 结论

1. 当前 boot 的 `Cx3Mlx4 0.44.37` 已加载，binary sha256
   `952e9515abf24600926390700c5dbf51f0c1cc9d63db44a654d1b84b58085cf5`。
2. 11:32:08.922 60 侧记录 `.196` DEAD；`cx3freeze` 最后驱动行是 selector 24 之后的
   `user client closed`，没有进入 selector 46。旧 shell watcher 最后样本是 11:32:04，
   滞后约 3 秒；不能据此把死亡精确归到 attach/hook。
3. 当前 boot USB/Bluetooth/MCHOSE 基线 PASS；键盘不再表现为 XH00/HS11 recovery storm。
   当前输入源为 SCIM 双拼，仍需按键前后 counter 对照分型。
4. 0.44.38 原始候选 `efeae121…` 与修正候选 `3416ce21…` 均独立构建 PASS；
   原始候选生命周期问题已修正，详见 `REVIEW-0.44.38.md`。独立仓库提交
   `8579394`，未 push。
   结论：**ACCEPT-WITH-CHANGES，仅限下一轮受控诊断，不是生产结论。**
5. 60 的 recorder/alert/digest.timer 均 active；目标 `.196/.130`。broad/debug 继续保留；
   专用 `cx3freeze` 与 `cx3watch` 只新增、不替换 P0。

## 已交付

- `outputs/20261021-cx3-freeze-prep/BASELINE.md`
- `outputs/20261021-cx3-freeze-prep/CAPTURE-CHECKLIST.md`
- `outputs/20261021-cx3-freeze-prep/STREAM-PLAN.md`
- `outputs/20261021-cx3-freeze-prep/REPRO-PROTOCOL.md`
- `outputs/20261021-cx3-freeze-prep/REVIEW-0.44.38.md`
- `outputs/20261021-cx3-freeze-prep/KEYBOARD-PROBE.md`
- `outputs/20261021-cx3-freeze-prep/tools/cx3-fsync-accept.py`（候选，未安装）
- `outputs/20261021-cx3-freeze-prep/tools/cx3-udp-fsync.py`（候选，未安装）
- `outputs/20261021-cx3-freeze-prep/tools/repro-netup-stop2.sh`（候选，未执行）

## 复现前的硬前置

1. 用户原话放行：“工作已暂停，可以复现”。
2. 其他 owner 暂停 WiFi/NVIDIA/USB/BT/ESP/AuxKC 写操作。
3. 不动现有 broad/debug/recorder/watchdog/outbox；只新增：
   - 60 侧 `cx3freeze` 专用流落盘；
   - 60 侧 UDP 9997 的 step/hook 落盘；
   - Mac 侧 `netup-watch.sh` 专用实例。
4. 两个新日志都必须先看到增长，再调用任何 CX3 选择器。
5. 实验只到 `netupstop(46) in=2`；若返回后机器仍活，记录 INCONCLUSIVE 并停止，
   不在同一 boot 自动继续 3/4/39。新 watcher 必须用
   `CX3_WATCH_REQUIRE_UDP=1`，并确认 `cx3watch` 收到了 `worker=`/`seen=` 样本。

## 硬冻时先看

```text
60: state.log 最后时间/seq
60: events.log 的 UNREACHABLE / DUMP_START / DUMP_FAIL / AUTO_*_FAIL
60: logs/cx3watch-*.log 最后 worker/step/hook
60: logs/cx3freeze-*.log 最后 netup N/5 / attach hook N/3
Mac: /var/log/freeze-capture/ring-*.txt 最后 5 个
Mac: /var/log/freeze-capture/capture-*/（若存在）
重启后: boot.log / bootinfo / DiagnosticReports/*.panic
```

自动 dump 失败不等于没有证据：它本身会写 `DUMP_FAIL`/`AUTO_*_FAIL`；硬冻的主要兜底是
60 的专用流 + broad/debug + Mac 每 2 秒 `dmesg` 环形快照 + 下次 boot 的 `boot.log`。

## 恢复

本实验不改持久配置。若复现后无法正常 boot：

1. 冷启一次；
2. Shift 安全模式或 Recovery；
3. 只有确认 CX3 是阻碍时才执行 `sudo tools/cx3-mlx4/rollback.sh`；
4. 不使用 `kmutil clear-staging`，不顺手改 ESP/NVRAM。

## 当前待办

- 0.44.38 仍是未安装的受控诊断候选，不能当作最终修复。
- 下一次运行前先跑 `tools/freeze-capture/check-usb-health.sh` 全 PASS。
- 下一次运行前确认 native watcher 经 ad-hoc codesign，且 UDP 到 60:9997 有实际回读；
  未签名字符串会被 macOS 拒绝为 `EHOSTUNREACH`。
- 若 `netup` 返回 timeout 而不是冻屏，后续 selector 会被 poison 闸拒绝；按重启处理，
  不要在未知 attach 状态上继续实验。
