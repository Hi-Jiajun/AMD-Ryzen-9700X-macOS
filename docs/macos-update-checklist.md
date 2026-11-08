# macOS 小版本更新检查清单（amd-boot-config 维护）

适用：本机 AMD Hackintosh（Ryzen 7 9700X / MAXSUN B850BKB WIFI / OpenCore / macOS 15.8.1 24H32）。
用途：计划 §5.1 的交付项。**更新前先读，更新后逐项跑。**

原则：一次只改一个变量；改 EFI 前先全量备份 + 逐文件 SHA256；失败只回滚到上一份已登记哈希。

## 1. 更新前（在旧系统上做）

1. 记录当前基线：

```
sw_vers; sysctl kern.boottime kern.osversion
shasum -a 256 /Volumes/OPENCORE/EFI/OC/config.plist /Volumes/OPENCORE/EFI/OC/OpenCore.efi
nvram boot-args
kmutil showloaded --list-only | rg -v 'com\.apple\.'
```

2. 全量备份 ESP 到 `outputs/<date>-amd-boot-config/private-backups/`，逐文件 SHA256，
   并确认救援 U 盘可启动（`BaseSystem.dmg` CRC 有效）。
3. 确认 timer 补丁的版本窗口：`Kernel > Patch` 里 `MinKernel = MaxKernel = 24.6.0`。
   新系统若改 Darwin 版本，这条补丁会**自动不生效**，必须重新标定，不能简单放宽版本号。
4. 确认 USB S2 稀疏 map 的格式要求：`UTBMap.kext/Contents/Info.plist` 必须是 **XML**；
   binary plist 会被 OpenCore prelink 拒绝，日志报 `Invalid Parameter` + `version unavailable`。
5. 记录 BIOS 关键项（IOMMU / SR-IOV / Secure Boot / CSM / Fast Boot）；更新系统不需要改它们。

## 2. 更新后第一轮（不改任何 EFI）

1. 启动成功判据：OpenCore 日志出现 `EXITBS:START`；`kern.boottime` 与日志时间对齐。
2. USB 拓扑：

```
ioreg -rc AppleUSBXHCIPCI -r -l -w0 | rg 'port-count|^ *\+-o (HS|SS)[0-9]+@'
```

   期望：XH00 为 7 个对象（硬件 2/3/7/8/15/16/17，位置 1..7），其他控制器 4/4/1。
3. 注入：日志里出现 `Prelinked injection UTBMap.kext ... - Success` 与 `UTBMap.kext v1.1`。
4. 音频短检：播放一段本地音频，确认无可听断音（不做长压测）。
5. 定时器（可选，确认没退化）：

```
clang -O2 -o /tmp/deadline-probe outputs/20261017-audio-current/deadline-probe.c && /tmp/deadline-probe
```

   判据：`mode=1` 的 1000 µs 中位 ≤ 1.05x；`mode=0` 的 1.25x 属正常合并行为，不算失败。
6. 第三方 kext 是否仍加载（NVIDIA / CX3 走 AuxKC，不受 ESP 影响，但要确认没被更新清掉）：

```
kmutil showloaded --list-only | rg -i 'nullmoth|Cx3Mlx4|AirportMtk|RTL812xLucy'
```

## 3. 按版本绑定的补丁

| 补丁 | 绑定 | 更新后要做的事 |
|---|---|---|
| AMD_Vanilla 系列（CPUID/MSR/内核早期） | Darwin 17–25 的区间段 | 新版本落在区间外就等上游更新，不要手改 Find/Replace |
| `IOPCIFamily` probeBusGated / IsHotplugPort | 21.0–25.99 | 越界后重新核对字节 |
| USB 15→17 两条（当前 `Enabled=false`） | 无 MinKernel | 需要时用 `tools/research/usbpatch-check.py` 重验命中 |
| timer LAPIC 标定 | **仅 24.6.0** | 新 Darwin 必须重新标定，不放宽版本号 |
| PAT/MTRR | 17.0–25.99 | 越界后重新核对 |

## 4. 失败回滚

- 单个 kext/文件：只回滚该文件到上一份已登记哈希。
- 整树：按 `RESTORE.md` 的文件级恢复流程（先 `mv` 旧 `EFI` 留底，再 `ditto` 备份，再逐文件校验）。
- 起不来：F11/F12 选救援 U 盘 → 恢复 `EFI/` → 重启；不要在坏状态上叠加第二个变量。

## 5. 记录

每次更新在 `outputs/20261020-amd-boot-config/WINDOW-REGISTER.md` 追加一行：
时间、变量、目标文件、变更前后 SHA256、重启次数、结果、回滚路径。
