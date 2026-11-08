# R1 真机恢复演练（2026-10-20 19:16 – 2026-10-21 03:57 CST）

状态：**R1 PASS：写回 + 重启验收通过**。
写回阶段由本会话执行；重启由用户执行；本会话没有执行 `shutdown` / `reboot`，也没有改 BIOS。

## 1. 授权与前置条件

- 用户已插上救援 U 盘并明确批准 R1 窗口。
- 当前 ESP 写前复核：`disk0s1`，EFI **436/436** 与 Batch 0 baseline **0 差异**；
  `config.plist` `3dad2596…e4292`，`OpenCore.efi` `7754f6d6…273c8`，
  `BOOTx64.efi` `d3a4854c…b10f3`。
- ESP 根目录非 EFI 普通文件 **336 个**，写前清单：
  `evidence/pre-restore-root-files.sha256`。
- 连续两次关键哈希取样一致，rsync dry-run 为 **436 个文件、0 个 `*deleting` 标记**。

### 救援 U 盘复核

| 项 | 结果 |
|---|---|
| 设备 | Kingston DataTraveler 3.0，`disk2s1`，`/Volumes/1401`，FAT32 17.2 GB |
| `EFI/BOOT/BOOTx64.efi` | 53,248 B，sha256 `d3a4854c…b10f3` ✅ 与当前一致 |
| `EFI/OC/OpenCore.efi` | 872,448 B，sha256 `7754f6d6…273c8` ✅ 与当前一致 |
| `EFI/OC/config.plist` | 53,367 B，`plutil -lint` OK |
| Drivers / Kexts / Tools | `OpenRuntime.efi`、`HfsPlus.efi`、OpenCanopy、Lilu 等齐全 |
| `BaseSystem.dmg` | 888,615,275 B；`hdiutil verify` **CRC32 $6B9C5E7D VALID** |
| `BaseSystem.chunklist` | 3,352 B，在位 |

结论：救援 U 盘具备 OpenCore 引导 + macOS recovery 的最低条件。

## 2. 实际写回

只写 `EFI/` 子树；ESP 根目录日志、脚本、说明文件不删除、不覆盖。

```bash
sudo mount -t msdos -o rw /dev/disk0s1 /tmp/esp-rw
sudo rsync -r --inplace --ignore-times --no-perms --no-owner --no-group \
  "$BK/EFI/" /tmp/esp-rw/EFI/
sudo umount /tmp/esp-rw
```

结果（`evidence/restore-rsync.log`）：

- 传输 **436 个文件**，总 **69,117,595 B**；
- `Number of files transferred: 436`；
- **0 个删除**；
- rsync exit 0。

## 3. 写后校验

| 项 | 结果 |
|---|---|
| EFI 文件数 | **436** |
| baseline EFI manifest vs 写后 manifest | **436/436 行完全一致，diff 0** |
| `config.plist` | `3dad2596c1b3457c956e0c1308a560a9ca1cd9cc9c1cfd705963ace3f57e4292` ✅ |
| `OpenCore.efi` | `7754f6d66b23ad2410e1a6ef9ee63b7d527ce2dd8e8dbf58bca9c657f87273c8` ✅ |
| `BOOTx64.efi` | `d3a4854c82f183e2a73976e69490ce334bbaf8055c8efaa78f4d4626fddb10f3` ✅ |
| 根目录非 EFI 文件 | 336 个，写前/写后内容哈希一致 |
| AppleDouble / 临时文件 | `._*` = 0，`*.tmp` = 0 |
| `fsck_msdos -n /dev/rdisk0s1` | exit 0；1,039 files；43,408 KiB free；无孤儿簇报告 |

写后 manifest：`evidence/post-restore-efi.sha256`；
差异结论：`evidence/post-restore-manifest-diff.txt`。

### 容量与系统元数据说明

- 写前 `df`：46,442 KiB free；写后 `fsck`：43,408 KiB free。
  约减少 3.0 MiB，属于 FAT 重写后的簇分配/松弛变化，文件哈希无差异。
- 只读/读写挂载过程中，macOS 在 `.fseventsd` 新建了 3 个元数据文件（19:17:38）。
  它们不是启动文件；本次未删除、未覆盖根目录日志/脚本/说明。

## 4. 重启步骤（已执行）

1. **保持救援 U 盘插着**，不要拔。
2. 正常重启，BIOS 启动项选**内置 ESP / OpenCore**（不是 U 盘）。
3. 在 OpenCore 菜单选 **macOS**；不要选两个 `Mac OS X` 直连条目。
4. 进入桌面后先不要改 EFI/BIOS；把结果告诉我，我继续做启动后验收。

实际结果：用户已按上述步骤重启，2026-10-21 03:55:25 CST 成功进入 macOS；
详见 §6。

如果重启后没有 OpenCore 菜单、黑屏、卡住或 panic：

1. 长按电源键关机；
2. 开机按 **F11/F12**（本机固件启动菜单）选 **UEFI: Kingston / 1401**；
3. 从 U 盘 OpenCore 进入 macOS recovery，或直接从 U 盘 OpenCore 启动现有 macOS；
4. **不要先 Reset NVRAM**，避免把启动项问题扩大。

## 5. 紧急回滚路径（R1 已通过，保留备查）

从救援 U 盘启动后：

```bash
sudo mount -t msdos -o rw /dev/disk0s1 /tmp/esp-rollback
sudo rsync -r --inplace --ignore-times --no-perms --no-owner --no-group \
  "$BK/EFI/" /tmp/esp-rollback/EFI/
cd /tmp/esp-rollback
grep '  EFI/' "$BK/MANIFEST.sha256" > /tmp/efi-manifest.sha256
sudo shasum -a 256 -c /tmp/efi-manifest.sha256
sudo umount /tmp/esp-rollback
```

其中 `BK` 指向：
`outputs/20261020-amd-boot-config/private-backups/esp-disk0s1-20261020-1146/`。
回滚只覆盖 `EFI/`，不动根目录日志/脚本。

## 6. 重启验收（R1 PASS）

验收采样：2026-10-21 03:57 CST（协调会话只读采样）。

| 项 | 结果 |
|---|---|
| `kern.boottime` | `2026-10-21 03:55:25 CST` |
| 最新 OpenCore 日志 | `opencore-2026-10-20-195500.txt`（19:55:00 UTC = 03:55:00 CST） |
| 日志版本/配置 | `Loaded configuration of 56063 bytes`；`OpenCore DBG-107-2026-10-14` |
| 日志 boot path | `.../NVMe(...)/HD(1,GPT,6D351E57-EE46-4CE7-AACC-0BDA08532F8C,...)/EFI\OC\OpenCore.efi` |
| ESP 来源 | 内置 ESP `disk0s1`（GPT 分区 1，UUID `6D351E57…2F8C`） |
| EFI 内容 | **436/436** 与 baseline 0 差异 |
| 关键哈希 | config `3dad2596…`、OpenCore `7754f6d6…`、BOOT `d3a4854c…` 全部一致 |
| 运行态 USB | XHC0 15 个 USB20 + 9 个 USB30 端口 |
| 运行态驱动 | NVIDIA 2.4.3、CX3 0.44.36 已加载 |
| panic | 无本次启动 panic |
| 关闭阶段记录 | `shutdown_stall_2026-10-21-035427` 属于重启前关闭阶段记录，不是本次启动 panic |
| ESP 容量 | 79% 使用；43,152 KiB free；323 个 OpenCore 日志，84,672,512 B（80.8 MiB） |

**结论：从 baseline 写回的 ESP 已实际启动成功，R1 关闭。**
没有在验收后继续写 ESP；日志清理（R10）另立任务，不在本次处理。
