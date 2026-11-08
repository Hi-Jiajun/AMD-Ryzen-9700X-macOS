# SuperIO 自动采集 v2（2026-10-23）

## 已准备的行为

从 MTKCAP 启动后，OpenRC local 服务先运行新增的
`etc/local.d/00-superio.start`，再运行原有 `zz-mtkcap.start`。
结果写入盘上的 `out/superio-<UTC时间>/probe.log` 和 `status.txt`。
原有 MT7922 抓包、最终自动重启行为保持原样。不需要手工输入命令。

只读取 ID 寄存器 0x20/0x21、已有 hwmon 名称和板卡 DMI；
进出配置模式需要端口写入，不能称为零写入。不会选择 LDN、修改风扇曲线，
也不会加载 nct6683/it87 等监控驱动。先检查 `/proc/ioports`，已被驱动占用的
端口跳过。此检查不是原子资源预留，探测安排在早期、已有抓包之前运行。

Nuvoton 响应出未知非空 ID 时，不再试 ITE 协议；保留原始 ID 交回分析。
NCT6683D/6686D/6687D 的已知掩码可直接标记 MATCH，其他结果不自动宣称型号。
若 `/dev/port` 缺失，明确记录退出码 2，不能据此判定没有芯片。
EXIT/INT/TERM/HUP 做退出配置模式清理；断电或 SIGKILL 仍无法保证清理。

## 源码依据

本轮核对 Linux master 提交 `734bef548a96e8774d6eef5dfa2026c6edc4e80b`：

- [nct6683.c](https://github.com/torvalds/linux/blob/734bef548a96e8774d6eef5dfa2026c6edc4e80b/drivers/hwmon/nct6683.c)：
  进入 87/87，退出 AA、index 02/data 02；芯片 ID 掩码 FFF0。
- [it87.c](https://github.com/torvalds/linux/blob/734bef548a96e8774d6eef5dfa2026c6edc4e80b/drivers/hwmon/it87.c)：
  进入 87/01/55，再按端口写 55(2E) 或 AA(4E)；退出 index 02/data 02。

## 验证与边界

- `sh -n`：probe 和启动钩子通过。
- `python3 tools/test-superio.py`：macOS shell 下 9/9 通过。
- `SUPERIO_TEST_SSH=hiliang@192.0.2.60 python3 tools/test-superio.py`：
  Linux BusyBox shell/dd/od/tr 下 9/9 通过；仅临时文件 fixture 和模拟协议，无硬件访问。
- `python3 tools/build-superio-overlay.py`：原 archive 所有成员的内容、mode、类型、
  symlink 目标保持；仅新增可执行钩子。
- 尚未在本机 Alpine 真正读取 SuperIO，不能声称型号已识别或传感器已可用。

已部署至卷 UUID `D73A06E7-C98D-3C14-97BD-21BAAE2549CB`（MTKCAP，部署时 disk2s1），
sync 后与候选逐字节 cmp 一致：

- `superio-probe.sh` SHA256：`854ef76df1d0ad64bbe9fe5527b997f4eec5d3abe151baf646462cbf03f72aa4`
- `mtkcap.apkovl.tar.gz` SHA256：`049ff2ea73ee7c76647b5d1bb31409f5818319fdeb619fc4b0f066bbf47f5128`

以上命令相对目录为 `outputs/20261022-amd-platform/`。

## 备份与恢复

旧启动包和旧探针保存在 `superio-v2/mtkcap.apkovl.before.tar.gz`、
`superio-v2/superio-probe.usb-before.sh`。恢复时分别复制回 MTKCAP 根目录的
`mtkcap.apkovl.tar.gz` 和 `superio-probe.sh`，再核对 SHA256。
不涉及 ESP、BIOS、macOS 配置或驱动变更。

## 下一次启动

正常从 MTKCAP U 盘启动一次，等待原流程结束并自动重启回 macOS，随后读取
`out/superio-*`。若没有 MATCH，先根据原始 ID/错误解释原因，不直接安装 kext。
若匹配 NCT668x，评估 SMCSuperIO 风扇/电压监测；不能保证四路均可读。
CPU 温度需要独立 AMD 通道，SMCProcessor 不适用。
