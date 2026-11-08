# Handoff：amd-boot-config（Wave 0）

更新时间：2026-10-23 01:10 CST　状态：**M0 通过；S2 跨重启 PASS 并关闭；C5 已解决；R2 结案（无害告警）；R10 已执行（可逆）；A4 待 chip ID；冻结可降为常态监控**

## 现状

- 实际启动源：内置 NVMe `disk0` 的 ESP `disk0s1`（GPT type `C12A7328-…`，分区 UUID `6D351E57-…`），
  引导 `EFI/OC/OpenCore.efi`。
- 当前 bootloader：OpenCore **DBG-107-2026-10-14**；`config.plist` 56,063 B，
  sha256 `3dad2596…e4292`。
- **基线哈希已漂移（2026-10-22 核实）**：当前 config 为 **56,573 B / `bb95cd36…2812`**，
  与 baseline 的唯一差异是 `Kernel/Add` 新增 `MT7922USBWakePolicy.kext`
  （WiFi/BT 会话于 10-21 10:50 写入，禁用 MT7922 USB 远程唤醒以抑制 HS11 恢复风暴，
  `Enabled=true`，24.0–24.99）。已在 `WINDOW-REGISTER.md` 补登记。
  **引用 config 哈希时以 `bb95cd36…` 为准**；baseline 备份仍是回滚基准。
- 运行态 `nvram boot-args` 与 ESP config **逐字符一致**（389/389）。
- **R1 PASS**：2026-10-21 03:55:25 CST 从写回后的内置 ESP 成功启动；
  最新日志 `opencore-2026-10-20-195500.txt`（19:55:00 UTC），
  `Loaded configuration of 56063 bytes`，boot path 明确指向
  `HD(1,GPT,6D351E57-…)/EFI\OC\OpenCore.efi`。
- R1 写回后（S 部署前）ESP：EFI **436/436** 与 baseline 0 差异；config/OpenCore/BOOT 哈希全部一致。
- R1 时运行态（S2 部署前）：USB 15 个 USB20 + 9 个 USB30；NVIDIA 2.4.3、CX3 0.44.36 已加载；
  无本次启动 panic。
- `shutdown_stall_2026-10-21-035427` 是重启前关闭阶段记录，不是本次启动 panic。
- **R6 PASS/已关闭**（2026-10-21 用户 BIOS 目视）：IOMMU **Enabled**、
  SR-IOV **Disabled**、XHCI Hand-off **无此选项（不适用）**、Secure Boot **Disabled**、
  CSM **Disabled**、Fast Boot **Disabled**。
- **M0 通过**：R1（写回 + 重启验收）+ R6（BIOS 目视）均完成。
- IOMMU Enabled 满足 AppleVTD 的前提之一；SR-IOV Disabled 使 D2 的 SR-IOV VF 路线不可用，
  只能先评估传统 PCI 直通。
- **S1 FAIL / 已回滚**：04:03 部署的 binary 候选 `83f47223…` 在 06:35 启动报
  prelink `Invalid Parameter` / `version unavailable`；06:39 已恢复 baseline `f9b484e8…`。
- **S2 XML 已部署（06:43）**：候选 `998c7fbb977765447b228a35d847ab8954c0cc3cce7225133ad03161a72c4d0d`，
  `file` 确认为 XML，5,665 B；XH00 7 对象/`port-count=17`，其他控制器 4/4/1。
- S2 写后：config/OpenCore/BOOT 未变；EFI 436/436，全树仅 UTBMap 一处变化；
  根目录 338 个非 EFI 文件未变；`fsck_msdos -n` exit 0（1,073 files，42,114 KiB free）。
- **S2 runtime PASS**：2026-10-21 07:55:46 CST 启动；
  `opencore-2026-10-20-235521.txt` 报 `Prelinked injection UTBMap.kext ... Success`
  和 `UTBMap.kext v1.1`，无 `Invalid Parameter` / `version unavailable`。
- **S2 跨重启 PASS（2026-10-22 20:34:50 重启）**：日志 `opencore-2026-10-22-123428.txt`
  （`Boot timestamp 2026.10.22 12:34:28` UTC = 20:34:28 CST，与 boottime 差 22 秒）；
  UTBMap Success + v1.1；16 口拓扑；蓝牙 On `02:1A:7D:DA:71:2A`；`patchreport.py` 无
  enabled 且 0 替换；最近 3 小时 USB 硬失败签名 **0**；探针 317 条快照跨重启无断档。
  详见 `S2-CROSS-REBOOT.md`。**S2 窗口关闭**。
- S2 运行态：XH00 7 口（SS02/HS02/SS03/HS03/HS10/HS11/HS12），`port-count=0x11`；
  其他控制器 4/4/1；USB20=10、USB30=6；USB 错误 0；蓝牙 State On
  （Address `02:1A:7D:DA:71:2A`）。
- **S2 剩余验收改为探针观察**（计划 §5.2/G1 已取消人工 100 次插拔/30 分钟音频压测）：
  正常使用窗口内 `hardwareException` / `CommandRing::executeCommand` / `createDevice: failed` /
  `powerOn: deadline` / HS11(port16) 错误 / 控制器恢复风暴均为 0；并观察普通重启与睡眠/唤醒。
- 探针改为 **launchd 托管**（2026-10-22）：`com.hiliang.usbmonitor`，
  plist `~/Library/LaunchAgents/com.hiliang.usbmonitor.plist`，`KeepAlive=true`，
  输出仍写 `outputs/20261020-amd-usb-ports/candidate-S-20261021/S2-runtime-20261021/`。
  之前的 `nohup` 版本会话结束即被杀，只留下 10-21 08:42 之前的数据。
  停止：`launchctl bootout gui/$(id -u)/com.hiliang.usbmonitor`。
- **C5 音频断音已解决**（用户 2026-10-22 确认）：timer + USB S2 组合下不再复现，不再作为独立工作流推进；
  只保留“macOS 小版本更新后做一次短音频回归”的检查项。
- P（17 对象）未启动。
- 仓库 `efi/` 是 2026-10-07 旧快照（config `05919f3b…`，27 条 patch，10 个 kext），
  **不可用于覆盖 ESP**。
- R1 写回记录：baseline `EFI/` 436 文件 / 69,117,595 B，
  0 删除；写后 436/436、关键哈希一致；根目录 336 个非 EFI 文件内容未变；
  `fsck_msdos -n` exit 0（1,039 files，43,408 KiB free）。

## 证据

- 主基线：`outputs/20261020-amd-boot-config/BASELINE.md`
- 补丁表：`outputs/20261020-amd-boot-config/evidence/07-patch-registry.md`
- 差异表：`outputs/20261020-amd-boot-config/evidence/08-config-diff.md`
- BIOS：`outputs/20261020-amd-boot-config/evidence/10-bios-snapshot.md`
- 备份：`outputs/20261020-amd-boot-config/private-backups/esp-disk0s1-20261020-1146/`
  （EFI 436 文件 + baseline 根目录 335 文件；19:16 当前根目录 336 文件；MANIFEST 771/771 OK；Kexts 64/64 OK；
  整分区 UDRO 镜像 sha256 `b4ba67b0…182e7a`）
- 恢复：`outputs/20261020-amd-boot-config/RESTORE.md`
- R1 记录（PASS）：`outputs/20261020-amd-boot-config/RESTORE-DRILL.md`
- S 部署记录：`outputs/20261020-amd-usb-ports/candidate-S-20261021/DEPLOYMENT.md`
- 冻结：`outputs/20261020-amd-boot-config/FREEZE-WINDOW.md`
- 更新检查清单：`docs/macos-update-checklist.md`（2026-10-22 新增，计划 §5.1 交付项）
- A5 复测：`outputs/20261022-amd-platform/A5-TIMER-RECHECK.md`
- A7 补丁校验：`outputs/20261022-amd-platform/A7-PATCH-VERIFIER.md`
  （工具 `tools/patchreport.py` 权威、`tools/patchcheck-all.py` 预检）
- A8 安全姿态：`docs/amd-security-posture.md`
- R2 schema 判定：`outputs/20261022-amd-platform/R2-SCHEMA-VERDICT.md`
  （结论：`HideVerbose` 是 1.0.8 才有的字段，本机跑 1.0.7；4 条告警无害，**不要删键**——删了会让 config 变成 1.0.9 不合规）
- A4 型号支持面：`outputs/20261022-amd-platform/A4-SUPERIO-SUPPORT.md`
- A7 补丁注册表：`outputs/20261022-amd-platform/PATCH-REGISTRY-20261022.md`
- S2 跨重启验收：`outputs/20261020-amd-boot-config/S2-CROSS-REBOOT.md`
- 升级破坏面矩阵：`outputs/20261022-amd-platform/UPDATE-BREAKAGE-MATRIX.md`
  （30 条内核补丁 / 23 条注入 kext / 35 个 boot-args token；真正版本绑死的 11 条）
- OC 1.0.7→1.0.9 变更影响：`outputs/20261022-amd-platform/OC-1.0.7-TO-1.0.9-DELTA.md`
- R10 清理工具：`tools/oclog-prune.py`（默认 dry-run，`--apply` 需 `--yes-i-approve` 且目标可写；
  只做单文件 unlink 或 mv 到 `--trash`，无目录递归）
  - 干跑证据 `outputs/20261022-amd-platform/r10/DRY-RUN-20261023.txt`、清单 `manifest-dryrun.json`
  - 独立复核 `outputs/20261022-amd-platform/r10/VERIFY-20261023.md`

## AMD 平台层并行状态（2026-10-22）

| 流 | 现状 | 下一步 | 是否需窗口 |
|---|---|---|---|
| A5 定时器 | 8× 异常未复现；mode 1 中位 1.004x；mode 0 的 1.25x 是定时器合并 | 不动补丁；升级系统后重新标定 | 否 |
| A3/A4 电源/SMC | 见 `docs/handoff-amd-power-smc.md`：原版 SMCAMDProcessor No-Go，自研只读原型 Conditional Go | 用 MTKCAP 盘跑 `superio-probe.sh` 拿 chip ID（不需要 Windows），见 `outputs/20261022-amd-platform/A4-SUPERIO-PROBE.md`；脚本已于 10-23 改成「解锁→读→退出」版，旧版不解锁会读回 0xff | 需从该盘启动一次 |
| A6 NVRAM | `DisableVariableWrite=true` 保持；不做接管 | 无 | 否 |
| A7 更新维护 | 检查清单 + 两个校验器已交付；当版 17 条应用 / 9 条版本跳过 / 0 条静默失效 | 每次更新后跑 `patchreport.py` | 否（离线） |
| A8 安全姿态 | 已交付 `docs/amd-security-posture.md`（SIP 0xA43、amfi=0x80、FileVault Off，分级+建议） | 保持现状；收紧需单独立项 | 否 |

## 阻塞

1. ~~**R2 开放**~~ → **2026-10-23 结案**：4 条 `HideVerbose` 告警是「config 比引导器新」造成的无害噪音
   （该键 1.0.8 才引入，本机跑 1.0.7）。**不要删键**：删掉后 1.0.9 的 `ocvalidate` 报 4 条
   `Missing key HideVerbose`。现状 config 已通过 1.0.9 `ocvalidate` 全量校验；要彻底消告警只能升级
   bootloader，且须单独窗口。详见 `outputs/20261022-amd-platform/R2-SCHEMA-VERDICT.md`。
2. ~~**R10 待批准执行**~~ → **2026-10-23 01:08 已执行（可逆）**：323 个 `opencore-*.txt`
   搬到卷外 `outputs/20261022-amd-platform/r10/trash-20261023/`（323 份、84,672,512 B、
   逐文件 sha256 全对、无重名）。ESP 日志 356→33 份；`df` 82%→**41%**（avail 37,988→120,352 KiB）。
   EFI 438 文件与 config `bb95cd36…2812` **均未变**；干净卸载后 `fsck_msdos -n` 无错误。
   **没有删除任何日志**，trash 保留原样，搬回即可回滚。详见
   `outputs/20261022-amd-platform/r10/APPLY-20261023.md`。
   遗留：trash 占 80.75 MiB（在内置盘 APFS 上），是否真删另行决定；日志会继续每启动 +256 KiB。
3. **A4 待输入**：SuperIO 型号未识别，等 MTKCAP 盘 `superio-probe.sh` 的结果。

R6 已关闭。S1 已回滚。**S2 已 PASS 并关闭**：2026-10-22 20:34:50 普通重启后
16 口拓扑、蓝牙 On、UTBMap Success/v1.1、USB 硬失败签名 0，探针 317 条快照跨重启无断档
（详见 `S2-CROSS-REBOOT.md`）。USB 补丁仍保持 disabled。

## 下一步

- 冻结窗口继续保持，其他会话停写 ESP；
- 按计划 §5.2 做探针观察 + 普通重启/睡眠唤醒（不做人工 100 次插拔/30 分钟音频）；
- 观察期无复发后关闭 S2 窗口；复发只回滚 UTBMap 单文件；
- P（17 对象）未启动，不要与 S2 叠加；
- bootloader 去留已判：**当前 DBG-107 可继续用**（R2 结案，4 条告警无害）；若要消告警再单独立项做
  「升级到 1.0.8+ 同代引导器」的窗口。注意 `tools/backup-oc-debug-20261014-230155/stage-OC-1.0.9-master-DEBUG/`
  虽然叫 1.0.9，但与 ESP 上的 `7754f6d6…73c8` 逐字节相同、行为是 1.0.7，**目录名不可信**；
- 日志清理另立任务（R10），本次不删日志、不改 EFI；
- 任何 ESP 写入先走 `FREEZE-WINDOW.md` 的候选/登记流程。

## 计划正文状态更新建议（本会话未改计划正文）

- §1 基线表：把 OpenCore 版本改成 `DBG-107-2026-10-14`，config 哈希改成 `3dad2596…`；
  USB 行注明 S2 XML 已 runtime PASS、XH00 7 口，进入探针观察期；C5 音频已解决，不再列为待办。
- §1 IOMMU 行：改成“2026-10-21 用户 BIOS 目视确认 Enabled；10-08 Disabled 为解码/快照误差”。
- §3.2 Batch 0 状态：改成“基线/备份完成；R1 PASS；R6 PASS；M0 通过”。
- §5.1 Acceptance：R1 写回 + 重启验收、R6 BIOS 目视均通过；R2/R10 仍开放。
- S2 窗口：XML 候选 `998c7fbb…` runtime PASS；改为探针观察口径；失败只回滚 baseline `f9b484e8…`。
- §7 规则 1：标注“冻结已生效，其他会话停写 ESP，候选走 inbox”。

§1、§6.4、§10 已由协调会话更新；本文件不修改计划正文。
