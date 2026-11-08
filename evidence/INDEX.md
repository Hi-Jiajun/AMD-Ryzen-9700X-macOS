# Evidence index

每个目录是一次会话的测量记录，命名规则 `YYYYMMDD-领域`。里面的结构大体一致：
baseline（改动前是什么样）、跑过什么命令、回来什么数据、什么假设被否掉。

这些是 `../README.md` 里每一条 "measured" 结论的原始依据。它们**不是**教程，
里面有失败尝试和被推翻的中间结论，读的时候看日期和"结论"段落。

| 目录 | 内容 |
| --- | --- |
| [`20261013-applevtd-amdvi/`](20261013-applevtd-amdvi/) | AppleVTD / AMD-Vi：`amd-vi-port-map.md` 是本机 AMD-Vi 的端口/设备映射；`appleVTD-source-notes.md` 与 `appleVTD-reverse-map.md` 是对 AppleVTD 的反查；`synthetic-dmar-and-injection.md` 是合成 DMAR 与注入实验的设计 |
| [`20261020-amd-boot-config/`](20261020-amd-boot-config/) | 启动配置基线：`BASELINE.md` 记录真实 ESP、config 哈希、771 文件备份；`RESTORE.md` / `RESTORE-DRILL.md` 是还原演练；`S2-CROSS-REBOOT.md` 是稀疏 USB map 的跨重启验证；`WINDOW-REGISTER.md` / `FREEZE-WINDOW.md` 是串行写者窗口的登记 |
| [`20261020-amd-usb-ports/`](20261020-amd-usb-ports/) | 15 端口上限：`2026-10-20-portlimit-mechanism-and-paths.md` 是机制反汇编与可选路线；`community-*.md` 是社区方案的对照证据（USBToolBox / USBMap issue 63 / OpenCorePkg / Apple ABI）；`USB-PROBE-CHECK.md` 是探针核对 |
| [`20261021-amd-power-smc/`](20261021-amd-power-smc/) | CPU 电源与 SMC：`BASELINE.md` 是加载状态与 IORegistry 普查；`SMC-KEYS.md` 是 65 个 key 的完整表与分类；`OPTIONS.md` 是上游方案评估与建议 |
| [`20261021-amd-timing-nvram/`](20261021-amd-timing-nvram/) | Zen5 定时器与 NVRAM：`PATCH-HIT.md` 是补丁命中审计（含离线二进制 diff）；`TIMING.md` 是 `mach_wait_until` 前后对比；`NVRAM-PANIC.md` 是 NVRAM 清单与早期 panic 取证缺口 |
| [`20261022-amd-platform/`](20261022-amd-platform/) | 平台层：补丁注册表、A5 定时器复核、A4 SuperIO 支持性与探针、A7 补丁校验器、OpenCore 1.0.7→1.0.9 差异、更新破坏性矩阵、R2 schema 判定 |
| [`20261024-freeze-1442-readonly/`](20261024-freeze-1442-readonly/) | 一次冻屏事件的只读取证报告。注意结论：经用户确认是人工强制重启，未捕获 panic，根因未知 |
| [`20261025-amd-sensors/`](20261025-amd-sensors/) | 传感器后续：`PLAN.md` 是路线；`smcdump.c` 是只读 SMC 枚举器（MIT，见 LICENSE）；`test_temp_decode.py` 是温度解码原型的测试 |
| [`20261025-tahoe-acceptance/`](20261025-tahoe-acceptance/) | macOS 26.7.1 升级后的验收：`PLAN.md` 是分域验收计划，`STATUS.md` 是逐域状态快照（含各域 owner 与阻塞项） |
| [`20261025-amd-program-status/`](20261025-amd-program-status/) | 一个小的用户反馈记录，保留因为它是 2.5.4 版 NVIDIA 驱动在 Chrome 上的现场观察 |
| [`20261012-metal2vulkan-spike/`](20261012-metal2vulkan-spike/) | Metal→Vulkan 可行性 spike 的结果与上游 issue 草稿。属于"依赖功能"分支，不是 AMD 平台本身 |

## 证据里常见的几类文件

- `BASELINE.md` — 动手前的状态，带命令输出和哈希
- `*-CHECK.md` / `*-VERDICT.md` — 某条判据的结论
- `community-*.md` — 对社区方案的对照，不是自己的实测，文中会注明
- `*.json` — 机器可读的采集结果（`docs/bios/04-ifr-struct.json` 是 BIOS IFR 结构）

## 不在这个仓库里的东西

原始采集树里有大量整棵 EFI 备份、内核日志、二进制 dump 和临时脚本输出
（合计几十 GB）。这里只保留分析报告。需要原始数据的话，那些文件在参考机器本地，
不在公开仓库里——这也是为什么部分报告会提到某个 `outputs/` 路径却打不开。
