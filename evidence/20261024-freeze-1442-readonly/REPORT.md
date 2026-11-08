# 2026-10-24 14:42 冻屏只读取证

结论：用户确认冻屏后强制关机/重启；远端探针记录主目标失联及新boot。因此本次定为**整机失去响应后人工强制重启，未捕获panic，根因未定**。不是仅凭显示画面卡住作出的判断。没有证据证明本次发生自动panic重启；同样不能以没有panic文件排除全部内核故障。

本轮只读取60探针与本机已有记录；只在本目录保存副本/报告。没有修改探针、驱动、EFI或服务，没有调用CX3设备selector、安装加载或触发复现。

## 时间线（北京时间）

| 时间 | 直接证据 |
| --- | --- |
| 14:41:21.004、14:41:26.840 | broad流记录WindowServer transaction 20106/20108 timed out。更早也出现过同类超时，不能单独作为冻屏根因。 |
| 14:41:35 | 本机watchdog最后一条旧boot记录，stall=0，网关和60均ALIVE。 |
| 14:41:41.305620 | 本机最后一份旧boot环缓冲快照ring-001198.txt。末尾仍是NVRM映射/网络日志，无panic栈。 |
| 14:42:10.825 | 60最后一条成功HTTP state采样：seq=5742、load1=5.29、exec_ms=35.3、disk_ms=15.3。此处up是probe-server年龄，不是开机年龄。 |
| 14:42:11.075（Mac日志时间） | NVRMFB统计306 flips/10032ms，0超期、0跳帧，最大completion wait 6402us。这个采样不能证明后续没有GPU故障。 |
| 14:42:11.654（Mac日志时间） | broad/debug最后的有时间戳日志，NVRM映射记录，未报明确GPU致命错误。 |
| 14:42:13.776（60收取时间） | 最后UDP beacon，seq=5509，up=6785；此前WindowServer累计CPU时间持续递增。Mac与60记录时间有约数秒传输/取样差，不混成同一时钟精度。 |
| 14:42:15.719 | ping转DEAD，随后HTTP连续timeout。 |
| 14:42:48.212 | 失联31秒告警，ntfy=200；只证明服务端接受，不证明手机弹窗/用户看见。 |
| 14:43:34/35 | 本机kern.boottime为14:43:34；beacon推导BOOT为14:43:35，1秒取整差异。用户确认是人工强制重启。 |
| 14:43:55.979 | ping恢复，失联100.3秒。 |
| 14:43:45.961（新boot日志） | DumpPanic processed 0 files，preoslog buffer size 0；本机诊断目录没有本次panic报告。 |
| 14:45后 | AUTO_DIAG/AUTO_LOG来自重启后的新boot。load约45–49属于重启后，不能倒推为冻屏前负载。DUMP_FAIL(empty dump)也发生在恢复后。 |

## 直接验证及边界

- `ssh -o BatchMode=yes -o ConnectTimeout=6 hiliang@192.0.2.60 ...`读取events/state/beacon/ping/alerts及broad/debug；recorder与alert均active/running、NRestarts=0，outbox文件数0。60记录器自身没有同时重启证据。
- `/usr/bin/kmutil showloaded --list-only`显示当前`com.hijiajun.driver.Cx3Mlx4 (0.44.47)`，UUID `AA1A2B9E-6BA0-3A54-A28B-553A4F12C6A2`。离线`.125`不是本次运行候选。当前加载身份不自动证明旧boot的全部设备状态。
- debug流自12:49连接起到本次失联，没有捕获kernel进程的CX3日志。窗口里的`/tmp/cx3-stage...`和kmutil ownership/signature错误是离线构建校验输出，不是运行中的驱动错误。没有据此排除旧驱动的无日志故障。
- 保存的失联前窗口没有USB硬失败签名HX40/CRT/CDF/POD，也没有panic栈或明确NVIDIA Xid报错。APFS文本中的`xid`是事务ID，不能当GPU Xid。
- `last reboot shutdown`在本次14:43 reboot之前没有相应正常shutdown记录，与用户确认的强制重启一致。
- 探针BOOTINFO的`verdict=software-initiated`来自shutdown cause 5的标签。**本次不得沿用该标签当作正常软件关机证据**；cause 5不足以区分强制断电、冻屏或panic。

## 尚缺的证据与下一步

目前没有冻结当场的内核线程栈/CPU栈或具体阻塞资源，不能把最后打印日志的NVRM、WindowServer超时、CX3或磁盘当作根因。网络整体失联加用户体感冻屏使其超出单纯窗口假死，但现有记录仍不足以区分内核死锁、硬件卡住等底层原因。

本机两条采集分别在14:41:35和14:41:41停止，而远端beacon持续至14:42:13。这是约32秒的本机快照缺口，停止写快照的具体原因没有错误回执，不能直接判定磁盘坏或采集器就是根因。现有快照采集代码会静默吞掉异常，且含dmesg/top等待；“每2秒一份”不能作为严格保证。后续应先为采集步骤提供独立远端进度/错误回执，并保留boot隔离的最后快照，再选择最小单变量实验；本次未授权实施这些常驻采集改动或危险实验。

## 保存内容

- `probe60/`：14:35–14:47的探针原始窗口与自动诊断，含broad/debug连续行。
- `mac/`：最后6份旧boot ring、watchdog窗口、加载清单与DumpPanic检查。
- `ring-manifest.json`：旧boot快照原mtime及SHA-256。
- `broad-prefailure.log`、`debug-prefailure.log`：按记录块时间分离的新boot前日志；continuation跟随所属时间戳，避免把新boot的无时间戳堆栈误纳入旧boot。

用户关于“人工强制关机/重启”的回复为本次判类依据，未改写60原始账本。全部结论只针对本次14:42窗口。
