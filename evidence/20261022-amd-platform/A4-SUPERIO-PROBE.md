# A4 SuperIO 型号识别：用 MTKCAP 盘查（2026-10-22，2026-10-23 修订）

> **旧说明已被 v2 替代，请勿按下文的旧协议或安装表操作。**
> 当前入口：[A4-PROBE-READY.md](A4-PROBE-READY.md)。2026-10-23 已将厂商区分的
> ID 探测接入自动启动并保存日志；不再强制加载监控驱动。旧说明中通用握手、
> “一定退出”、SMCProcessor 可用于 AMD、NCT668x 提供温度的说法均不成立。

背景：`docs/handoff-amd-power-smc.md` 卡在“MAXSUN B850BKB WIFI 的 SuperIO 型号未知”，
而 macOS 用户态不能做 port I/O。结论：**不需要 Windows**，用现成的 MTKCAP 盘（Alpine）即可。

## 1. 为什么 macOS 侧探针不行

`com.hiliang.usbmonitor` 只是读 macOS 日志（`log stream`）+ 采 `ioreg` 快照，
**完全不碰硬件寄存器**。SuperIO 挂在 LPC/eSPI 的 I/O 端口上，macOS 没有可用的 port I/O 通道。

## 2. 用 MTKCAP 盘（已就绪）

- 盘：`disk4s1` / `/Volumes/MTKCAP`，FAT32；Alpine，`kernel 6.6.134-0-lts`（`BOOTED.txt`）。
- 脚本：`/Volumes/MTKCAP/superio-probe.sh`（2026-10-22 放入；**2026-10-23 已换成解锁版**）。
  正本在仓库 `outputs/20261022-amd-platform/tools/superio-probe.sh`，U 盘上那份是它的副本。
- 跑法：从该盘启动后，`sudo sh /media/usb/superio-probe.sh`
  （挂载点按实际，可能是 `/media/usb` 或 `/media/sda1`）。

## 2.1 为什么 10-22 那版会读不到（2026-10-23 修订）

Nuvoton / ITE / Winbond 的 SuperIO 配置寄存器默认**上锁**：不进"扩展功能模式"就直接读
`0x20/0x21`，多数芯片返回 `0xff 0xff`。10-22 那版脚本没做解锁握手，大概率会得到一对 `0xff`，
然后被误判成"读不到 = 该端口没芯片"。

正确做法是标准探测握手（**不改任何配置寄存器**）：

1. 向 index 端口（`0x4E` 或 `0x2E`）**连写两次 `0x87`** 进入扩展功能模式；
2. 选寄存器 `0x20`/`0x21` 读 chip ID；
3. 选 `0x07` 读当前逻辑设备号（只读）；
4. 退出：写 `0xAA`（Nuvoton/Winbond）与 `0x02`（ITE）。

所以准确说法是「**不修改任何设置，但不是零写入**」——进出扩展功能模式本身要写 index 端口。
脚本结尾一定会退出该模式。

## 2.2 现在的脚本做什么

1. 先打印板卡 DMI（`/sys/class/dmi/id/board_*`、`dmidecode` 的 baseboard 项）——这是真实固件信息，
   不受 macOS 侧 SMBIOS 伪装影响，可作独立佐证；
2. 在 `0x4E/0x4F` 与 `0x2E/0x2F` 各做一次「不解锁读（对照）+ 解锁读（真值）+ 退出」；
3. 列 `/sys/class/hwmon/*/name`（内核若已认出来会直接显示型号）；
4. 列 LPC/eSPI 桥的 PCI ID（芯片组，不是 SuperIO 本身）；
5. 依次 `modprobe nct6683 / nct6687 / nct6775 / it87`，再抓 `dmesg` 里的型号行；
6. 有 lm-sensors 就顺带 `sensors`。

若 `/dev/port` 不存在（内核没开 `CONFIG_DEVPORT`），脚本会明确报出来，并继续走 hwmon/dmesg 两条替代路径。

判定：把两段 `unlocked read: 0x?? 0x??` 发回来（`locked read` 那两行只是对照，不作数）；
`dmesg` 里若出现 `nct6683: Found NCT66xx` 之类就更直接。板卡 DMI 型号也一并带回。

## 3. 拿到型号之后

| 结果 | 下一步 |
|---|---|
| Nuvoton NCT668x（6683/6686/6687） | 评估官方 `SMCSuperIO` 是否覆盖该 ID；覆盖则申请窗口加 `SMCProcessor` + `SMCSuperIO`，风扇 RPM 与部分温度可见 |
| 其他厂商（ITE/Winbond/Fintek） | 看 `SMCSuperIO` 支持表；不支持就停在“BIOS 控风扇”的现状 |
| 读不到 | 记录端口不可达，维持现状，不再投入 |

## 4. 代价与边界

- 需要**从该 U 盘启动一次**，macOS 会下线；不写 ESP、不改 BIOS，但要登记一次窗口。
- 探测只读寄存器；不改风扇曲线、不改电压。唯一写入是进出扩展功能模式的握手字节。
- 这一步只解决“型号未知”，不等于传感器可用——真正可用还要 kext 与 SMC key 验证。
