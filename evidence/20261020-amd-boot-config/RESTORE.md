# ESP 备份与恢复演练（2026-10-20）

## 1. 备份内容

目录：`outputs/20261020-amd-boot-config/private-backups/esp-disk0s1-20261020-1146/`

| 项 | 值 |
|---|---|
| 源 | 内置 NVMe `disk0` 的 ESP `disk0s1`（GPT type `C12A7328-F81F-11D2-BA4B-00A0C93EC93B`，209,715,200 B） |
| 快照方式 | `hdiutil create -srcdevice /dev/disk0s1 -format UDRO`（整分区 409,600 扇区） |
| 文件级备份 | `EFI/`（436 文件）+ `root/`（335 个 ESP 根目录普通文件，含 OpenCore 日志/脚本） |
| 字节级镜像 | `esp-disk0s1-20261020-1240-UDRO.dmg`（178,430,242 B，sha256 `b4ba67b024b8ea3b4fe4122ed4cf95336961f016246affe9d7cbbda5e0182e7a`） |
| 逐文件清单 | `MANIFEST.sha256`（771 行） |
| 权限/大小快照 | `TREE.txt`（771 行） |
| 校验输出 | `VERIFY.txt`（771/771 OK） |
| Kext 清单 | `KEXTS.sha256`（64 文件）+ `KEXTS-VERIFY.txt`（64/64 OK） |

未做文件级复制的目录：`$RECYCLE.BIN`、`System Volume Information`、`.Trashes`、`.Spotlight-V100`、`.fseventsd`。
它们不是启动文件；**整分区 UDRO 镜像仍包含这些扇区**。

## 2. 关键哈希

| 文件 | sha256 | 大小 |
|---|---|---|
| `EFI/OC/config.plist` | `3dad2596c1b3457c956e0c1308a560a9ca1cd9cc9c1cfd705963ace3f57e4292` | 56,063 B |
| `EFI/OC/OpenCore.efi` | `7754f6d66b23ad2410e1a6ef9ee63b7d527ce2dd8e8dbf58bca9c657f87273c8` | 872,448 B |
| `EFI/BOOT/BOOTx64.efi` | `d3a4854c82f183e2a73976e69490ce334bbaf8055c8efaa78f4d4626fddb10f3` | 53,248 B |

另有历史救援集 `EFI/OC-RESCUE-20261014-230948/`：
`OpenCore.efi` = `a3cfa9ac…c00ae`（630,784 B，REL-109），`BOOTx64.efi` = `19fa90b9…f6474`（24,576 B）。

## 3. 已完成的离线恢复演练

2026-10-20 12:42–12:51 CST：

1. `shasum -a 256 -c MANIFEST.sha256` → **771/771 OK**；
2. `shasum -a 256 -c KEXTS.sha256` → **64/64 OK**；
3. 把 UDRO 镜像独立重新挂载到 `/tmp/esp-restore-verify`，对 `EFI/` 全树逐文件比对 →
   **436/436 文件、0 缺失、0 多余、0 哈希不一致**；
4. 关键文件从镜像读出的哈希与上表一致。
5. 12:49 用 `sudo mount -t msdos -o ro /dev/disk0s1 /tmp/esp-live` 直接只读挂载实时 ESP，
   与文件级备份逐文件比对 → **436/436、0 差异**；随后 `umount`。
6. 12:50 用 200 MB 临时 FAT32 镜像做写入预演：`ditto` 备份 `EFI/` → 临时镜像，
   再跑 `shasum -c` → **436/436 OK**。没有碰真实 ESP。

**结论：备份可读、完整、可独立校验。**

## 4. 真机恢复演练状态（2026-10-21 03:57：R1 PASS）

用户已批准 R1 窗口并插好救援 U 盘；19:16–19:19 已按本文件流程把 baseline `EFI/` 写回
`disk0s1`（436 文件 / 69,117,595 B，0 删除），写后 436/436、关键哈希一致、
`fsck_msdos -n` exit 0。用户已于 **2026-10-21 03:55:25 CST 成功从内置 ESP 启动**，
R1 PASS；详见 `RESTORE-DRILL.md`。

本次执行前置：救援 U 盘 `disk2s1` / `/Volumes/1401` 的 OpenCore、config lint、
`BaseSystem.dmg` CRC 均通过复核；写前连续哈希稳定，rsync dry-run 436 文件、0 删除。

## 5. 恢复步骤（参考；本次实际用 rsync in-place）

> 危险操作：会覆盖 ESP 的 `EFI/`。执行前先确认 `disk0s1` 的 GPT type、大小、当前
> `config.plist` 哈希；不要对 `/dev/disk0` 整盘操作。

> 本次 R1 因 ESP 只剩约 45 MiB、无法同时保留旧 `EFI/` 和新 `EFI/`，改用
> `rsync --inplace --ignore-times` 原地覆盖（见 `RESTORE-DRILL.md`）。下面这套
> `mv` + `ditto` 方案保留给 ESP 空间充足的情况。

```bash
BK="$HOME/Documents/Hackintosh-MacLocal/outputs/20261020-amd-boot-config/private-backups/esp-disk0s1-20261020-1146"

# 1. 只读确认目标
diskutil info disk0s1 | grep -E 'Partition Type|Disk Size|Volume Name'
sudo diskutil mount disk0s1
TARGET=/Volumes/OPENCORE
shasum -a 256 "$TARGET/EFI/OC/config.plist"

# 2. 把当前 EFI 先改名留底（同一 ESP 内，不删除）
sudo mv "$TARGET/EFI" "$TARGET/EFI.before-restore-$(date +%Y%m%d-%H%M%S)"

# 3. 从备份复制
sudo ditto "$BK/EFI" "$TARGET/EFI"
sudo chmod -R u+rwX "$TARGET/EFI"

# 4. 校验
cd "$TARGET"
grep '  EFI/' "$BK/MANIFEST.sha256" > /tmp/efi-manifest.sha256
sudo shasum -a 256 -c /tmp/efi-manifest.sha256

# 5. 卸载并重启（保留救援 U 盘）
sudo diskutil unmount disk0s1
```

注意：`MANIFEST.sha256` 里的 `root/...` 是备份目录布局，不对应 ESP 根目录；
恢复后至少校验 `EFI/OC/config.plist`、`EFI/OC/OpenCore.efi`、`EFI/BOOT/BOOTx64.efi`
与 `EFI/OC/Kexts` 的哈希。

## 6. 字节级恢复（备选，破坏性更大）

```bash
hdiutil attach -readonly -nobrowse "$BK/esp-disk0s1-20261020-1240-UDRO.dmg"
# 确认挂载点后，用 asr 恢复到 disk0s1（会擦除该分区）
# sudo asr restore --source /Volumes/<镜像卷> --target /dev/disk0s1 --erase --noverify
```

该路径未演练；优先使用第 5 节的文件级恢复。

## 7. 回滚

- 文件级：把 `EFI.before-restore-*` 改回 `EFI`，删掉新 `EFI`（或再改名）。
- 引导器级：用 `EFI/OC-RESCUE-20261014-230948/` 里的 REL-109 `OpenCore.efi` / `BOOTx64.efi`
  覆盖当前 DBG-107；`config.plist` 仍需匹配（当前 config 是 1.0.9 时代字段，见 BASELINE 风险 R2）。
