# 把 macOS 扩到 1 TB：分区重划方案（2026-10-10 已完成 ✅）

日期：2026-10-09 制定 / 2026-10-10 完成　前置文档：`docs/disk-and-nic-plan.md`、`docs/install-runbook.md`

## 进度

| 阶段 | 内容 | 状态 |
| --- | --- | --- |
| 0 | 备份策略（不做离盘归档，旧容器即备份） | ✅ 已定 |
| 1 | Windows 侧：清理 / 压缩 / 建 2 GiB WinRE / WinRE 重建 / UTC / 关快速启动 | ✅ **已完成并核对**（见 §1b） |
| 2 | macOS 侧：建新容器 + CCC 克隆 + 验收 | ✅ **已完成**（见 §1c） |
| 3 | 删旧容器 + `resizeContainer 0` 吃满到 931.3 GiB | ✅ **已完成**（见 §1d） |
| 4 | 收尾：改名 / 卸载 CCC / 清理残留 | ✅ **已完成**（见 §1e） |

> **整件事已结束。** 最终成果：macOS 931.323 GiB = **1000.0 GB**，Windows **974.194 GiB**，
> WinRE 独立 2 GiB。唯一留下的待办是「Time Machine 稍后配置」（见 §1e 末尾）。

## 1. 现状读数（2026-10-09 只读实测）

> **⚠️ 阶段 1 已于 2026-10-09 执行完毕，§1 的读数是执行前的旧值。**
> 执行结果与核对记录见 §1b；当前布局以 §1b 为准。

磁盘 `disk0` = **ZHITAI TiPro9000 2TB** NVMe，2,048,408,248,320 B（1.86 TiB），GPT。

| # | 分区 | 大小 | 内容 |
| --- | --- | --- | --- |
| s1 | ESP（卷标 `OPENCORE`） | 209.7 MB | OpenCore 与 Windows Boot Manager 共存 |
| s2 | MSR | 16.8 MB | Windows 保留 |
| s3 | **C: NTFS** | **1,645,189 MiB** | 已用 871.3 GiB，空闲 735.3 GiB |
| s4 | **APFS 容器 disk1** | **308,108 MiB（323.075 GB）** | 卷内已用 67.0 GB，容器内另有 238.5 GiB 未分配 |

其他事实：

- macOS 15.8.1 (24H32)，FileVault 关，无 Time Machine 目标。
- `hiberfil.sys` 20.47 GB、`pagefile.sys` 53.69 GB，都在 C: 上（压缩量的两个拦路石）。
- **盘尾那个 907 MB 的 WinRE 分区在 2026-10-08 装 macOS 时被并进了 APFS 容器**（见 `install-runbook.md` §"磁盘状态"）。
- **Windows 是 25H2，不是 24H2**（详见 §9 的证据与判读）。
- `diskutil verifyDisk disk0`：分区表 OK。
- `diskutil apfs resizeContainer disk1 limits`：Maximum = 323.1 GB = 当前值 → **现有容器吃不到前面的空闲空间**，必须重建容器。

---

## 1b. 阶段 1 执行记录 + macOS 侧核对（2026-10-09）

### 分区结果 —— 逐字节命中方案

| # | 分区 | 计划 | 实测 | 判定 |
| --- | --- | --- | --- | --- |
| s1 | ESP `OPENCORE` | 200 MiB | 200 MiB | ✅ 未动 |
| s2 | MSR | 16 MiB | 16 MiB | ✅ 未动 |
| s3 | **C:** | 997,575 MiB | **997,575 MiB**（974.194 GiB） | ✅ 精确 |
| s4 | **WinRE** | 2,048 MiB | **2,048 MiB**（2.000 GiB） | ✅ 精确 |
| — | 空闲 | 645,566 MiB | **645,567 MiB**（676.9 GB） | ✅ |
| s5 | APFS 容器（旧，原 s4） | 308,108 MiB | 308,108 MiB | ✅ 原样，编号如预测变为 s5 |

布局 = `ESP / MSR / C: / WinRE / [空闲 645,567 MiB] / APFS(旧)`，与计划完全一致，空闲空间位置正确
（紧邻 WinRE 之后 → §5.1 的 `addPartition disk0s4` 可命中）。

### Windows 侧收尾项（只读读取 hive / 文件核对）

| 项 | 期望 | 实测 | 判定 |
| --- | --- | --- | --- |
| `HiberbootEnabled` | 0 | **0** | ✅ 快速启动已关 |
| `RealTimeIsUniversal` | 1 | **1** | ✅ UTC 已生效 |
| `TimeZoneKeyName` | China Standard Time | **China Standard Time** | ✅ |
| `hiberfil.sys` | 已重建（休眠恢复） | 20,472,221,696 B（19.07 GiB） | ✅ |
| `pagefile.sys` | 可调小 | 51,180,560,384 B（47.67 GiB，原 50 GiB） | ✅ |
| C: 剩余空间 | ≈102.9 GiB | **105 GiB** | ✅ 比预测还好 |
| WinRE 启用状态 | Enabled | `InstallState=1`、`IsAutoRepairOn=1`、新 `WinreBCD` GUID | ✅ 功能可用 |
| Winre.wim | 用我们提取的那份 | `C:\Recovery\WindowsRE\Winre.wim` = 737,547,656 B | ✅ |
| Winre.wim 指纹 | `29c0fa54…b23ee` | **`29c0fa54…b23ee`** | ✅ 逐字节一致 |

### ⚠️→✅ WinRE 落点问题：已解决（2026-10-09 21:39）

**最终状态（macOS 侧读取 `ReAgent.xml` 复核）**：

```
WinreLocation  offset = 1046260744192  (974.406 GiB)  = 分区 4   ✅
ImageLocation  offset = 1046260744192               = 分区 4   ✅
InstallState = 1    IsAutoRepairOn = 1
C:\Recovery\WindowsRE             已清空（wim 与 boot.sdi 均已移走）
C:\Windows\System32\Recovery\     仅剩 ReAgent.xml
C: 剩余 106 GiB     APFS 容器 308,108 MiB 原样未动
```

---

#### 修复过程记录（含两次失败与一次错误诊断，供以后复用）

**第 1 次尝试失败（21:06）**：只做了 `format` + `reagentc /enable`。

**⚠️ 一次错误诊断**：曾据 macOS 的 `diskutil`（`verifyVolume` → `Unrecognized file system (-69846)`、
`info` → `File System: None`）判定"format 没生效"。**这个结论是错的。**
`ReAgent.log` 里 reagentc 自己枚举到分区 4 有 2.03 GiB 空闲（总 2.00 GiB，约 16.7 MB 为 NTFS 元数据）
→ format 其实成功了。

> **教训：不要用 macOS 的 `diskutil` 判断这块"恢复类型 + 隐藏/no-automount"分区的内容** ——
> 它探不出、也挂不上（`failed to mount`），会给出误导性结论。NTFS 以 Windows 侧为准。

**真正的原因（`ReAgent.log` 原文，三次 `/enable` 判定完全一致）**：

```
--Install on target OS step 3: check if we can keep winre.wim in the same partition if it is staged.
CanKeepWinReOnSamePartitionIfStaged WinRE is staged.  Checking that the staged partition is valid for WinRE.
MeetPartitionRequirements Partition details: {Offset: 227540992, Free space: 108563836928, ...}
MeetPartitionRequirements Required free space: 356515840
NOTE: Find target partition: source partition will used      ← 决定用"源分区"
```

`Winre.wim` 暂存在 C:（`C:\Windows\System32\Recovery`），C: 空闲 108 GB ≫ 要求 340 MB，
于是 reagentc 判定"源分区就合格"，**直接把 WinRE 留在 C: 上**，那个已枚举到的恢复分区它不去用。

**第 2 次尝试失败（21:29）**：以为 `/disable` 会把 wim 搬回暂存目录 `C:\Windows\System32\Recovery`，
于是从那里 copy —— **实测 `/disable` 并不搬**，文件一直在 `C:\Recovery\WindowsRE\`，
三个 `copy` 全部落空，`/setreimage` 报"在指定的路径中找不到有效的映像"。

> **教训**：`/disable` 不保证把 wim 移回暂存目录；copy 前先 `dir` 确认真实位置，别假设。

**第 3 次成功（21:39）—— 可复用的正确做法**：**让"源分区"就是恢复分区。**
把 wim 先放进分区 4，再让 `/setreimage` 指向那里：

```powershell
$dk  = @(Get-Disk | Where-Object FriendlyName -match 'ZHITAI')
$rec = @(Get-Partition -DiskNumber $dk[0].Number |
         Where-Object { $_.Type -eq 'Recovery' -and [math]::Round($_.Size/1GB) -eq 2 })
$pn  = $rec[0].PartitionNumber
Add-PartitionAccessPath -DiskNumber $dk[0].Number -PartitionNumber $pn -AccessPath 'R:\'

# 源文件从 NAS 取（\192.0.2.50\Data\backup\winre-26100.8031\）
New-Item -ItemType Directory -Force -Path 'R:\Recovery\WindowsRE' | Out-Null
Copy-Item '\\192.0.2.50\Data\backup\winre-26100.8031\Winre.wim' 'R:\Recovery\WindowsRE\' -Force
Copy-Item '\\192.0.2.50\Data\backup\winre-26100.8031\boot.sdi'  'R:\Recovery\WindowsRE\' -Force
(Get-FileHash 'R:\Recovery\WindowsRE\Winre.wim' -Algorithm SHA256).Hash  # 期望 29C0FA54...B23EE

reagentc /disable
reagentc /setreimage /path R:\Recovery\WindowsRE     # 回显应含 partition4
reagentc /enable
reagentc /info                                        # 期望 partition4

Remove-PartitionAccessPath -DiskNumber $dk[0].Number -PartitionNumber $pn -AccessPath 'R:\'
```

**判据**：`ReAgent.xml` 的 `WinreLocation offset` = **1046260744192**（不是看界面文字）。

**另一个易错点**：`diskpart` 的分区号在**多盘在场时**可能与 macOS 的 `disk0sN` 不一致
（当时插着 4 个 U 盘）。所以一律用"型号 + 类型 + 大小"定位，别用 `select disk 0` + `select partition N`。

---

## 1c. 阶段 2 执行 + 克隆验收（2026-10-09 22:1x）

### 克隆完成方式

CCC 7.2.1 → **Bootable copy using Apple Software Restore**，实测底层命令：

```
asr restore --source /dev/disk1s4 --target /dev/disk4 \
    --erase --toSnapshot 764608B5-1D97-4C46-94BD-590857077BAB --noprompt
```

即 Apple 自己的复制器把**已密封的系统快照**整组复制到新容器，产出完整卷组
（System / Data / Preboot / Recovery / VM）。用户随后从 OpenCore 菜单成功启动新系统。

### 验收结果（逐项实测）

| 项 | 基线 | 新系统 | 判定 |
| --- | --- | --- | --- |
| macOS 版本 | 15.8.1 (24H32) | 15.8.1 (24H32) | ✅ |
| nullmoth 驱动 | 4 行 | 4 行 | ✅ |
| 显卡 | RTX 5060 / Metal 3 | RTX 5060 / Metal 3 | ✅ |
| 网络（ping NAS） | 通 | 0.0% 丢包 | ✅ |
| `git` / `brew` | 可用 | 可用 | ✅ |
| LaunchAgents | 3 | 3 | ✅ |
| `~/.ssh` | 13 文件 | 13 文件 | ✅ |
| `~/.config` | 7 文件 | 7 文件 | ✅ |
| Desktop / Downloads / Pictures / Music | — | **逐文件完全一致** | ✅ |
| `~/Library/Preferences` | — | **完全一致** | ✅ |
| `~/Library/Keychains` | — | **完全一致** | ✅ |
| `~/Documents` | — | 差 1 个（见下，已捞回） | ✅ |
| `~/Library/Application Support` | — | 仅旧有 19 / 仅新有 23（Chrome IndexedDB、Codex 临时态等运行期churn） | ✅ 可忽略 |
| 新容器 Preboot | — | `18D8702D-…`（卷组 UUID）含 `boot.efi` + `SystemVersion.plist` | ✅ **删旧容器不影响启动** |

**结论：克隆完整，可以删旧容器。**

### 两个值得记录的细节

1. **克隆后旧系统仍运行了约 12 分钟**（ASR 于 21:54 前后完成，旧系统最后一次写入 22:06，
   重启约 22:07–22:09）。这个窗口内旧卷上的改动不会出现在新卷里 —— 实测差异只有下面的第 2 条。
   判断依据：对旧卷某文件做 25 秒 mtime 观察，值不变（22:06:26 恒定）→ **当前无进程在写旧卷**。
2. **旧卷上出现一个我没创建的文件**
   `Documents/Hackintosh-MacLocal/outputs/20261009-post-clone-baseline/expected-after-boot.txt`
   （22:02 写入，内容是"从 MacNew 卷采集的验收基准"，home 368620 / Apps 17 / brew 23）。
   无法确认来源（可能是另一个 Codex 线程或用户手动执行）。**已复制到新系统同一路径**，
   现在 `~/Documents` 与旧卷**逐文件完全一致（差 0）**。

> **教训**：并行跑多条 `exec_command` 时不要在 `/tmp` 用同名中间文件 —— 本次因此产生过一次
> 假结果（"Documents 仅旧有 104626 个"），实际差异只有 1 个。

### 重启后的编号变化（印证了 §5.1 的预判）

重启后 macOS 按偏移重排，**新旧容器互换编号**：

| | 重启前 | 重启后 |
| --- | --- | --- |
| 新容器（676.9 GB / MacNew） | `disk4` / `disk0s7` | **`disk2` / `disk0s5`** |
| 旧容器（323.1 GB / Macintosh HD） | `disk1` / `disk0s5` | **`disk1` / `disk0s6`** |

→ 删旧容器前**必须重新 `diskutil list` 认盘**。

---

**当前盘上状态**（2026-10-09 22:1x，克隆已完成、正从新系统运行）：

```
s1 ESP 200M | s2 MSR 16M | s3 C: 997,575M (剩 106G) | s4 WinRE 2048M
s5 新容器 disk2 645,566 MiB = MacNew（现役系统，卷组完整）
s6 旧容器 disk1 308,108 MiB = Macintosh HD（待删 → 成为空闲空间）
               合计 953,674 MiB = 931.3 GiB = 1000.0 GB
```

本文取代 `disk-and-nic-plan.md` 里"分 300 GB"的旧方案。三个已确认的决定：

| 决定 | 值 | 出处 |
| --- | --- | --- |
| macOS 容量 | **1000 GB（931.3 GiB）** | 用户：macOS 留 1 TB，其余给 Windows |
| WinRE 恢复分区 | **独立分区，2 GiB，位于 C: 与 APFS 之间** | 用户：用独立分区；位置按实测的邻接规则选中间 |
| 时间策略 | RTC 存 UTC，两侧时区都设 Asia/Shanghai | 见 §7 |
| 休眠策略 | **保留休眠，关闭快速启动** | 见 §8 |

---

## 1d. 阶段 3 执行完成 —— 全部结束（2026-10-09 22:2x）

### 最终布局（macOS 侧实测）

```
/dev/disk0  2.0 TB  GPT
  s1  EFI                 209,715,200 B    0.195 GiB
  s2  Microsoft Reserved   16,777,216 B    0.016 GiB
  s3  C: (NTFS)         1,046,033,203,200 B  974.194 GiB   剩 106 GiB
  s4  Windows Recovery      2,147,483,648 B    2.000 GiB
  s5  Apple_APFS          1,000,000,000,000 B  931.323 GiB   ← 唯一 APFS 容器 disk2
```

| 目标 | 结果 | 判定 |
| --- | --- | --- |
| macOS 1000 GB | 容器 = **1,000,000,000,000 B = 1000.0 GB = 931.323 GiB** | ✅ **精确命中** |
| Windows 974 GiB | C: = 974.194 GiB | ✅ |
| WinRE 独立 2 GiB | s4 = 2.000 GiB，类型 Windows Recovery | ✅ |
| 根卷可用 | **856 GiB** | ✅ |
| 文件系统自检 | `diskutil verifyVolume disk2s1` → **exit code 0** | ✅ |
| 卷组 | `18D8702D-…`：Data(disk2s1) + System(disk2s2) 成对完好 | ✅ |

---

## 1e. 收尾（2026-10-10 终检通过）

### 终检结果（macOS 侧实测）

| 项目 | 结果 | 判定 |
| --- | --- | --- |
| 卷名 | System = `Macintosh HD`、Data = `Macintosh HD - 数据` | ✅ **配套** |
| `/Volumes` 链接 | `Macintosh HD -> /` | ✅ 已刷新 |
| 分区表 | `ESP 0.195 / MSR 0.016 / C: 974.194 / WinRE 2.000 / APFS 931.323 GiB` | ✅ |
| 根卷可用 | **856 GiB** | ✅ |
| 文件系统自检 | `diskutil verifyVolume disk1s1` → **exit code 0** | ✅ |
| 卷组 | Data/System/Preboot/Recovery/VM 五卷齐全 | ✅ |
| 残留临时挂载 | 已消失 | ✅ |
| CCC | 应用/配置/prefs/安装包/进程/FDA 授权 **全部清干净** | ✅ |
| Windows C: | 974.194 GiB 完好 | ✅ |
| WinRE | 仍指向分区 4（`offset=1046260744192`） | ✅ |
| 关键目录 | `~/.ssh` 13、`~/.config` 7、nullmoth 4 行 | ✅ |

> 注：扩容重启后容器/卷编号由 `disk2s*` 回归为 **`disk1s*`**（与 §1d 记录一致的重排行为）。

### 遗留问题：全部已解决

1. **CCC 已卸载** —— 卸载前先删掉了那个危险任务（源已删除、目标为现役系统）。
   CCC 未安装 LaunchDaemon / PrivilegedHelper，所以清理很干净：应用、`~/Library/Application Support/com.bombich.ccc/`、
   `~/Library/Preferences/com.bombich.ccc.plist`、`~/Downloads/ccc-7.2.1.8415.zip`、FDA 授权项全部移除。
2. **卷名已配套** —— 首次 `diskutil rename` 时 Data 卷成功、**System 卷被拒**（`Volume must be mounted`）：
   Big Sur 之后当前系统的 System 卷**故意不挂载**，挂在 `/` 的是它的快照（`diskXsYs1`，sealed），
   而 `diskutil rename` 要求卷处于挂载态。**解法：先把它挂到临时路径再改名**（重启后 `/Volumes` 链接自动刷新）。
3. 残留 `Update` 卷挂载点、陈旧 NVRAM 启动偏好（后者由 OpenCore `RequestBootVarRouting` 维护，属设计内行为）—— 重启后均已消失/无需处理。

### 后续待办

| 项 | 说明 |
| --- | --- |
| **Time Machine（唯一待办，用户指定稍后配）** | 新空间 856 GiB 可用，可指向 fnOS：`sudo tmutil setdestination -ap smb://hiliang@192.0.2.50/backup`（前提：fnOS 侧共享开启 Time Machine 支持）。也可用外置盘。 |
| ~~休眠时钟漂移实测~~ | **用户决定不做。** 若将来在 Windows 侧遇到切换系统后时间偏移，再按 §8 末尾的协议实测并按需加"恢复时 `w32tm /resync`"计划任务。 |
| ~~缩小 pagefile~~ | **用户决定不做。** 当前 47.67 GiB，C: 剩 106 GiB，够用。 |

## 2. 目标布局

```
s1 ESP 209.7 MB → s2 MSR 16.8 MB → s3 C: 974.2 GiB → s4 WinRE 2 GiB → s5 APFS 931.3 GiB
```

> 单位约定：资源管理器把 GiB 显示成"GB"，所以下面出现两套数字。**看"压缩卷填"那一列**（MiB，Windows 输入框的实际单位）就不会错。

压缩量公式（填 Windows「压缩卷」对话框里"输入压缩空间量(MB)"那一栏，单位实际是 MiB）：

```
压缩量(MiB) = 目标GB × 953.674 + WinRE预留MiB − 308,108
```

| macOS 目标（十进制） | 压缩卷填(MiB) | C: 压后 | WinRE |
| --- | --- | --- | --- |
| **1000 GB = 931.3 GiB（本方案）** | **647,614** | 974.2 GiB（资源管理器显示 974） | 2 GiB |
| 1100 GB = 1024.3 GiB（备选） | 742,982 | 881.1 GiB（显示 881） | 2 GiB |
| 1200 GB = 1117.6 GiB（备选） | 838,349 | 787.9 GiB（显示 788） | 2 GiB |

压完 C: 约剩 **102.9 GiB 空闲**（清理由此往上加）。**压缩前先复核**：清理后已用 + 180 GiB ≤ 974 GiB。
注意这 102.9 GiB 是**已经含 hiberfil.sys（约 20 GB）** 的口径 —— 保留休眠的占用已经算进去了，不会再缩水。

---

## 3. 阶段 0 — 备份策略（2026-10-09 定稿：不额外备份）

**结论：不做离盘归档，以"旧容器即备份"为唯一保险，当天验证通过当天删除。**

理由：`~/` 下的一切都在 APFS Data 卷内，而**旧容器在新容器验证通过前保持完整且可引导** ——
它本身就覆盖 macOS 侧 **100% 数据**，且比任何归档都强（能直接开机用）。

### 3.1 做法（单日流程）

1. CCC 克隆完成 → 重启进新系统；
2. **跑完下面 6 项验收**（此时旧容器仍在，任何一项不过就直接切回旧系统，零损失）；
3. 全部通过 → 当天执行 §6：删旧容器 → `resizeContainer 0` 吃满到 931.3 GiB。

验收清单（6 项，逐条打勾）：

- [ ] 桌面、Dock、Finder 侧栏正常
- [ ] `ls -a ~ | wc -l`、`ls ~/.ssh`、`ls ~/.codex` 与旧系统一致
- [ ] `find ~ -type f 2>/dev/null | wc -l` 与旧系统同口径数量接近
- [ ] 常用 app 能开（Chrome / ChatGPT / VS Code / Obsidian / WeChat / FlClash）
- [ ] 网络（`ping 192.0.2.50`）、音频、显卡（`system_profiler SPDisplaysDataType`）
- [ ] 终端 `git --version`、`brew --version`（PATH 正常）

### 3.2 已知且接受的残余风险（2026-10-09 用户确认，不再备份）

| 缺口 | 说明 | 处置 |
| --- | --- | --- |
| Windows C:（871 GiB） | 旧容器完全帮不上忙 | **用户接受**：压缩卷是非破坏性操作 |
| 分区表本身 | `deleteContainer`/`resizeContainer`/`addPartition` 都在改 disk0 的 GPT；表若损坏，两个容器同时受影响 | **用户接受**；无离盘副本 |

> 这两条是自担风险的决定，不是"已覆盖"。写在这里是为了将来出问题时能立刻定位到缺口来源。

### 3.3 附：日后若需要离盘归档（踩坑记录 + 正确命令）

实测踩过的三个坑，照这个来就不会重复：

1. **`rsync` 会在 SMB 上失败** —— 系统自带的 openrsync(2.6.9) 写临时文件后改名报
   `move_file: .DS_Store: No such file or directory` 并中止。加 `--inplace` 可绕过（实测 204 MB 目录通过），
   但**小文件极慢**：fnOS 单文件创建延迟实测 **24 ms**（本地同盘 0.24 ms），
   25 万个文件会跑到 100 分钟以上。
2. **`bsdtar` 遇到读不了的文件会直接中止**（不是跳过）——实测：三文件目录里中间的 blocked.txt 读不了时，
   **它后面的文件不会进归档**，只在最后给一个 `exit=1`。
   本次两次全量打包都因此死在同一个 TCC 保护文件上
   （`Library/Group Containers/group.com.apple.secure-control-center-preferences.av.plist`），
   两次产出都是 133,640 / 251,104 条目、14.57 GiB —— **残缺，不可当备份用**。
   `pax` 同样会中止。
3. 因此**必须排除受保护路径**（或给进程 Full Disk Access），并**核对条目数**才算数。

```bash
mkdir -p /tmp/fnos-rw && mount_smbfs //hiliang@192.0.2.50/Data /tmp/fnos-rw
cd ~
tar -cf /tmp/fnos-rw/backup/mac-home-<日期>.tar \
  --exclude='./Library/Caches' --exclude='./Library/Group Containers' \
  --exclude='./.cache' --exclude='./.Trash' --exclude='./.rustup' \
  --exclude='./.cargo' --exclude='./.npm' --exclude='./.DS_Store' \
  --exclude='*/.DS_Store' --exclude='./.Spotlight-V100' --exclude='./.fseventsd' \
  --exclude='./.TemporaryItems' \
  --exclude='./Documents/Hackintosh-MacLocal/tools/backup-kextpolicy-20261009-190105' \
  -C /Users/you .
# 校验：条目数应与预期同量级，且 tar 退出码为 0
tar -tf /tmp/fnos-rw/backup/mac-home-<日期>.tar | wc -l
umount /tmp/fnos-rw && rmdir /tmp/fnos-rw
```

### 3.4 规模实测（修正早前的错误估值）

| 口径 | 数值 |
| --- | --- |
| `~/` 全量（含缓存/回收站） | 约 45 GB、360,958 个文件 |
| **排除缓存后**（真正的备份量） | **23.1 GB、251,104 个文件** |

早前文档写的"`~/` 约 63 GB"是错的（那是 10-08 迁移时的估值）；顺带一提，首次实测读速
**121 MB/s** 也不是早前记的 64 MB/s —— 顺序大块写快得多，慢的只有小文件。

可引导的恢复手段：1401 启动盘（disk2，卷标 `1401`，含 `com.apple.recovery.boot/BaseSystem.dmg`）已在手。
实测检查：`BaseSystem.dmg` = 888,615,275 字节、`EFI/OC/OpenCore.efi` 与 `EFI/BOOT/BOOTx64.efi` 均在 → **完好**。

### 3.5 迁移前基线（2026-10-09 已记录，供 §3.1 验收比对）

目录：`outputs/20261009-pre-repartition/`（11 个文件 + brew/定时任务清单）

| 基线项 | 当前值 |
| --- | --- |
| home 文件总数 | **365,147** |
| home 顶层条目数 | 37 |
| `/Applications` 应用数 | 17 |
| brew formula / cask | 23 / 0 |
| nullmoth 驱动 | 4 行（符合 runbook 的"期望 4 行"） |
| LaunchAgents / crontab | 3 个 / 空 |
| macOS | 15.8.1 (24H32) |

克隆后在新系统跑同样的命令，与这些数字比对即可。

---

## 4. 阶段 1 — Windows 侧清理 / 压缩 / 建 WinRE 分区

顺序不能变：**清理 → `powercfg /h off` → `defrag` → 压缩卷 → 建 WinRE 分区 → 再交给 macOS**。

```powershell
powercfg /h off                       # 必须：hiberfil.sys 贴在尾部会顶住"可用压缩空间"
defrag C: /D /X
Get-Volume C | Select-Object @{n='已用GB';e={[math]::Round(($_.Size-$_.SizeRemaining)/1GB,1)}}
# 磁盘管理 → 右键 C: → 压缩卷 → 填 647,614
```

压缩量不够时（"可用压缩空间"小于目标）：先确认休眠已关 + 分页文件移走或无分页 → 重启 → 重试；
仍不够可在 Windows 侧用 DiskGenius 收缩 **NTFS**（那是它的强项），**只动 C:，不要碰 APFS**。
压出来的空间**不要分配盘符**。

建 WinRE 分区（管理员 cmd）：

```cmd
diskpart
  select disk 0
  list partition                    :: 确认 3 号是 C:，它后面就是刚压出来的空闲空间
  create partition primary size=2048
  format quick fs=ntfs label="Windows RE tools"
  set id=de94bba4-06d1-4d40-a16a-bfd50179d6ac
  gpt attributes=0x8000000000000001
  exit
```

2 GiB 而不是微软标准的 1 GiB：25H2 的 WinRE.wim 已 600–700 MB，且历史上出过两次"恢复分区太小导致 WinRE 更新装不上"（KB5034441 / KB5041580）。

---

## 5. 阶段 2 — macOS 侧建新容器 + 复制系统（不重装）

### 5.1 建新容器

**✅ 已执行（2026-10-09 21:5x）**：`sudo diskutil addPartition disk0s4 APFS MacNew 0` 一次成功。

结果（macOS 侧实测）：

| 项 | 值 |
| --- | --- |
| 新容器 | `disk4` / 物理存储 **`disk0s7`** / 645,566 MiB = **630.4 GiB** |
| 新卷 | `disk4s1`，卷名 **`MacNew`**，APFS，已挂载 `/Volumes/MacNew` |
| 旧容器 | `disk1` / `disk0s5` = 308,108 MiB，**原样未动** |
| WinRE | `disk0s4` 类型/大小未变 |

> **编号注意**：新分区拿到的是 **`disk0s7`**（s6 被跳过了），旧容器仍挂在 `disk0s5`。
> `diskutil list` 按偏移排序显示为 `s3 C: → s4 WinRE → s7 新 → s5 旧`，看着别扭但不是错乱。
> **重启后会按偏移压缩重排**（实测过这个行为），所以下面每一步都要重新 `diskutil list` 认盘，
> 不要复用旧编号。

**合并后容量核对**：`645,566 + 308,108 = 953,674 MiB = 931.3 GiB = 1000.0 GB` —— 与目标 1 TB 精确一致。

> 早前文档/对话里出现过"新容器 582.6 GiB"，那是**旧方案**（压 596,613 MiB）的数字，已作废；
> 最终方案压 647,614 MiB，新容器是 **630.4 GiB**。

**实测约束**：`diskutil addPartition` 要求指定的分区**紧邻**空闲空间，否则报
`Error: -69519 ... a gap is required in your partition map`（实测：布局 `ESP/AAA/空档/BBB` 时，写 `addPartition disk4s1` 失败三次，改 `disk4s2` 立即成功）。

所以必须点名**紧邻空闲空间的那个分区**，即刚建的 WinRE 分区：

```bash
diskutil list disk0                       # 确认 WinRE 分区号（预计 s4）
sudo diskutil addPartition disk0s4 APFS MacNew 0    # 0 = 吃满
# diskutil addPartition disk0s3 ...      # ❌ C: 后面紧挨着 WinRE，会报 -69519
```

或用磁盘工具 GUI：选中那块"可用空间" → 加号 → APFS。

### 5.2 复制系统（Apple 的 APFS 复制器）

Big Sur 起的可引导副本只能由 Apple 的 APFS 复制器（ASR）建立 —— 官方 KB 原话："自 macOS Big Sur 起，我们必须使用 Apple 的 APFS 复制器（ASR）来建立可引导克隆"。CCC 只是调用它。

用 **CCC 7.2 最新版**（7.0.3 有个 Intel + Sequoia 上跑"旧版可引导副本"报错的 bug，后续版本已修；本机是 x86 + Sequoia 15.8.1，正好落在修复范围内）。
CCC 7 里 **Legacy Bootable Copy Assistant 已并入 Backup Volume Setup Assistant**，在向导里选 "Legacy bootable copy"。

**✅ 安装包已下载（2026-10-09）**：`~/Downloads/ccc-7.2.1.8415.zip`（28.7 MB，`unzip -t` 校验通过，
含 `Carbon Copy Cloner.app`；官方最新为 7.2.1）。解压后拖进 `/Applications` 即可，30 天试用。

> CCC 提示"同一 APFS 容器内加第二个卷做不了可引导副本"是**同一容器**的限制；Bombich KB 的解法就是"在启动盘上另建一个分区"，即本方案的形态（独立容器的独立分区）。

备选（全免费）：从 1401 启动盘进恢复环境，`asr restore --source /dev/<旧容器物理存储> --target /Volumes/MacNew --erase`。
注意 macOS 11 之后磁盘工具的"恢复"做可引导克隆在不少版本上会失败，所以它只是备选。

### 5.3 验收（不能跳过）

重启 → OpenCore 菜单选新卷 → 确认桌面、文件、应用、网络都在。

---

## 6. 阶段 3 — 删旧容器 + 扩容（空间这时才真正给到 macOS）

> **前置条件（2026-10-09 定稿）：§3.1 那 6 项验收必须全部通过再执行本节。**
> 通过即当天执行，不做"并存观察期" —— 旧容器就是唯一的备份，删掉它就没有退路了，
> 所以验收这一步不能省；反过来，验收过了也就没必要拖。
> 执行本节前建议先跑一次 `diskutil apfs list` 存档，记录删除前的完整布局。

```bash
diskutil list                              # 两个容器：300.9 GiB 那个是旧的
sudo diskutil apfs deleteContainer diskY   # 按"容量+卷名"认，别复用旧编号
sudo diskutil apfs resizeContainer diskX 0 # 新容器吃满 → 931.3 GiB
diskutil rename diskXsN "Macintosh HD"     # Data 卷会成对改名
```

**编号会变，且不能自己指定**（实测，临时磁盘镜像 5 步验证）：

1. 新分区拿到的是下一个空槽位（旧容器占着 s4 → 新容器是 **s5**），`diskutil list` 按偏移排序，会看到 `s3(C:) → s5(新) → s4(旧)`；
2. 重新枚举（模拟重启）时 macOS 会按偏移**压缩重排**，实测 `s5` 自动变回 `s3`；删掉旧容器 + 重启后，新容器应回到 `disk0s4`、容器号回到 `disk1`；
3. 实测踩坑：重排后按上一轮抄的编号敲命令 → `Could not find APFS Container Reference or Physical Store /dev/disk4s5`。**每步都重新 `diskutil list`，用容量+卷名认盘。**
4. 容器号（disk N）是运行期分配的，插拔 U 盘都会变（当前两个 U 盘占着 disk2/disk3），本来就不该人肉记忆。

**APFS 只吃正后方连续空闲空间**（实测：`resizeContainer 0` 让容器 500 MB → 790.6 MB → 2.9 GB 一路长过去）。所以 WinRE 放在中间不会挡 macOS；但**以后 C: 想扩展必须先删掉 WinRE 分区**（只有紧邻 C: 的空闲空间能被 C: 吃掉）。

---

## 7. 阶段 4a — Windows 侧收尾：WinRE + 时间

### 7.1 重建 WinRE

> **✅ 本节已于 2026-10-09 21:39 完成** —— 最终状态与完整过程（含两次失败与一次错误诊断）见 **§1b**。
> WinRE 现位于 `\device\harddisk0\partition4\Recovery\WindowsRE`（`WinreLocation offset` = 1046260744192）。
> 下面是当时的准备记录，保留供以后重装参考。

现状证据：`C:\Windows\System32\Recovery\ReAgent.xml` 里
`<WinreLocation path="\Recovery\WindowsRE" offset="2047455789056">` = 1906.84 GiB，
正是旧恢复分区起点，而该区间现在在 APFS 容器（1606.63–1907.74 GiB）内部 → **配置指向 APFS 内部，恢复功能是坏的**；`Winre.wim` 也已不存在。

**✅ Winre.wim 已在 macOS 侧准备好（2026-10-09），Windows 里只差复制+两条命令。**

提取记录：从 fnOS 上的 `backup/ventoy-stick-20261009/Win11_25H2_Chinese_Simplified_x64_v2.iso`
（`install.wim` = build 26200.8037 / ge_release，6 个镜像，index 6 = **Windows 11 Pro for Workstations**，与系统版型一致）
用 `wimlib-imagex extract` 取出，位置：

```
\\192.0.2.50\Data\backup\winre-26100.8031\Winre.wim
  大小    : 737,547,656 字节（703 MiB）
  内部版本: 26100.8031 / x86_64 / 1 个镜像
  SHA-256 : 29c0fa544fed4a8705458138e668410a690a7fd4f809eb223a7b793f102b23ee
  完整性  : wimlib verify = "successfully verified"
```

同目录下另有 `README-在Windows里怎么用.txt`，内容与下面相同。

```cmd
:: 在 Windows 管理员 cmd 里，三步搞定（不用挂 ISO、不用 DISM）
copy "\\192.0.2.50\Data\backup\winre-26100.8031\Winre.wim" C:\Windows\System32\Recovery\
reagentc /setreimage /path C:\Windows\System32\Recovery
reagentc /enable
reagentc /info                            :: 期望"Windows 恢复环境状态: Enabled"
```

> **版本说明（如实记录）**：取出的是 **26100.8031**，比系统当前的 **26100.9457** 旧一档。
> 这是正常的 —— WinRE 跟随 26100 服务分支，不跟启用包版本（26200）；ISO 本身是 RTM 级，
> 所以任何"从 ISO 取 WinRE"的做法都会得到偏旧的版本。这与"全新安装 Windows"的处境相同，
> 恢复分区建好、`reagentc` 状态正常之后，Windows Update 会自动把恢复分区里的 WinRE 补齐。
> 703 MiB 这个体积也解释了为什么 §4 给 WinRE 分区留 2 GiB 而不是微软标准的 1 GiB。

### 7.2 时间（RTC 存 UTC）

```cmd
reg add "HKLM\SYSTEM\CurrentControlSet\Control\TimeZoneInformation" /v RealTimeIsUniversal /t REG_DWORD /d 1 /f
```

理由：macOS **只会**把 RTC 当 UTC，Apple 没有官方开关（要改 macOS 得靠非官方 AppleRTC 补丁，黑苹果上不做）；Windows 一个键就能对齐。
两侧时区都设 (UTC+08:00) 北京/上海 → 看到的仍是本地时间。
实测证据：SYSTEM hive 里**没有** `RealTimeIsUniversal` → 当前未设，与"切回 Windows 差 8 小时"吻合。

---

## 8. 阶段 4b — 休眠 / 快速启动

实测现状：`HiberbootEnabled = 1`（SYSTEM hive → `ControlSet001\Control\Session Manager\Power`）→ **快速启动开着**；`hiberfil.sys` 20.47 GB（≈ RAM 的 40%）→ 休眠也开着。

快速启动 = 关机时把**内核会话+驱动状态**写进 hiberfil.sys 的小映像，下次开机直接恢复（重启不走这条路，重启永远是冷启动）。好处是开机快几秒；代价是每次"关机"都把 NTFS 留在未干净卸载状态：

1. 其他系统只能只读挂载（本机 macOS 现在就是 `read-only` 挂载），若用第三方 NTFS 写驱动强行写入会损坏卷；
2. 关机不是真关机，固件/驱动层状态被保留 → "改了 BIOS 没生效"这类问题的经典原因；
3. 卷处于 in-use 时，Windows 磁盘管理与分区工具会拒绝操作 —— **压缩卷这步大概率直接失败**；
4. 从休眠态恢复后时钟可能被错误校正（与用户既往症状吻合，但机理未取得一手证据，列为待实测）。

**决定（2026-10-09 用户确认）：保留休眠、关闭快速启动。**

```powershell
# ① 关闭快速启动（只改这一个值，休眠文件保留 → 休眠仍然可用）
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Power" /v HiberbootEnabled /t REG_DWORD /d 0 /f
# 或：控制面板 → 电源选项 → 选择电源按钮的功能 → 更改当前不可用的设置 → 取消"启用快速启动"

# ② 分区操作前（阶段 1 之前）：连休眠一起关掉，删除 hiberfil.sys
powercfg /h off

# ③ 分区操作后：恢复休眠
powercfg /h on
# 检查：期望仍为 0（我们显式写过 0，不是"默认无值"）
reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Power" /v HiberbootEnabled
```

机制澄清（`powercfg` 与快速启动的关系）：

| 命令 | 对 hiberfil.sys | 快速启动 | 休眠 |
| --- | --- | --- | --- |
| `HiberbootEnabled=0` | 保留 | **关** | 保留 |
| `powercfg /h off` | **删除** | 关（无文件可用） | 关 |
| `powercfg /h on` | 重建（默认 = RAM 40%） | 取决于 `HiberbootEnabled` | 恢复 |

所以 `powercfg /h on` **不会**把快速启动带回来 —— 它只重建休眠文件；快速启动是否生效取决于 `HiberbootEnabled`，而我们已经显式写成 0（不是"无值走默认 1"）。仍按上表核一次值即可。

**实际生效的验证**（改注册表只是设置值，不是路径证据）：关机（不是重启）→ 再开机 → 任务管理器 → 性能 → CPU，看"正常运行时间"是否从接近 0 重新开始；或事件查看器 → Windows 日志 → 系统 → 源 `Microsoft-Windows-Kernel-Boot` 的事件 ID 27，数值 `0x0` = 完整启动、`0x1` = 从休眠/快速启动恢复。**必须用"关机"验证，重启永远是冷启动，验证不了。**

代价（已计入 §2 的余量）：保留休眠意味着 `hiberfil.sys` 长期占着 C: 约 20 GB（当前 20.47 GB，约等于 48 GB 内存的 40%）。§2 里"压完剩余 102.9 GiB"就是含它的口径。

休眠时间漂移待实测协议：进 Windows 记 `w32tm /query /status` → 手动休眠 → 唤醒 → 立刻再看；若整体偏 8 小时则加一条"恢复时 `w32tm /resync`"的计划任务。

---

## 9. Windows 版本：为什么"看起来没升级到 25H2"（含一次错误结论的纠正）

2026-10-09 曾据 `C:\Windows\servicing\Version\` 目录名与 `WinSxS` 组件名（全是 26100）判定系统是 24H2 —— **该结论错误**。

实际证据（只读读取 SOFTWARE hive）：`Microsoft\Windows NT\CurrentVersion`

```
CurrentBuild = 26200     CurrentBuildNumber = 26200
DisplayVersion = 25H2    UBR = 9457        LCUVer = 10.0.26100.9457
EditionID = ProfessionalWorkstation        BuildLab = 26100.ge_release.240331-1435
```

以及 `C:\Windows\Logs\CBS\CBS.log` 的 `Device: OS version 10.0.26200.9457`。
即 **系统已是 25H2（26200.9457，Pro for Workstations）**。

为什么组件名还叫 26100：**25H2 是"启用包(eKB)"式发布，与 24H2 共享 26100 服务分支**，只把版本号抬到 26200，组件库名字留在 26100.x（同 Win10 22H2=19045 而组件名是 19041.x）。所以"组件名 26100"不能用来判断版本。

附带确认：`Policies\Microsoft\Windows\WindowsUpdate` 键不存在 → 没有版本钉死策略；25H2 的 eKB 是 Windows Update 自动下发的，本机已吃到。

因此**不需要 UUP dump 下载**：fnOS 上那份 25H2 ISO 与系统同代（26200），直接用它取 Winre.wim。

> 小提示：该 ISO 根目录有 `__chunk_data`（235 KB，与其他文件同一时间戳），来源未能确认；文件名带 `_v2` 后缀，也不是微软官方命名。核心内容实测正常（合法 MSWIM、6 个官方消费版镜像），但取出的 Winre.wim 仍建议先用 `dism /Get-ImageInfo` 核版本。
> 另：`ProductName` 显示 "Windows 10 Pro for Workstations" 是历史遗留字段，所有 Win11 都这样，不是问题。

---

## 10. 关键坑速查

| 坑 | 现象 | 规避 |
| --- | --- | --- |
| `addPartition` 邻接要求 | `Error: -69519 a gap is required` | 必须点名**紧邻空闲空间**的那个分区（本方案是 WinRE 分区） |
| 设备号重排 | `Could not find APFS Container Reference` | 每步重新 `diskutil list`，按"容量+卷名"认盘 |
| APFS 只在后方生长 | 中间有分区就长不过去 | WinRE 放中间安全；但会挡 C: 的"扩展卷" |
| 休眠/快速启动 | 压缩卷失败、NTFS 脏状态 | 压缩前 `powercfg /h off`；压完 `powercfg /h on` 并确认快速启动仍关闭 |
| WinRE 位置记录 | `ReAgent.xml` 指向 APFS 内部 | 重建分区后 `reagentc /setreimage` + `/enable` |
| 双系统时间 | Windows 差 8 小时 | `RealTimeIsUniversal=1` |

---

## 11. 验收清单

**A. 克隆后、删旧容器前**（§3.1 的 6 项，这是删旧容器前唯一的把关）：

- [ ] 新卷能进桌面，常用 app 能开（Chrome / ChatGPT / VS Code / Obsidian / WeChat / FlClash）
- [ ] `ls ~/.ssh`、`ls ~/.codex` 与旧系统一致；`find ~ -type f | wc -l` 数量接近
- [ ] 网络、音频、显卡正常；`git` / `brew` 可用

**B. 删旧容器、扩容之后**（§6 尾）：

- [ ] `diskutil apfs list`：容器容量 = 931.3 GiB，只有一个容器
- [ ] `diskutil list disk0`：`ESP / MSR / C: / WinRE / APFS` 顺序正确，编号已回归
- [ ] 卷名 `Macintosh HD` / `Macintosh HD - 数据`
- [ ] Windows 侧 `Get-Volume C` 约 974 GB，能正常启动、进桌面
- [ ] `reagentc /info` 显示 Enabled
- [ ] 切回 Windows 时间显示正确（UTC 生效）
- [ ] macOS 侧 `kmutil showloaded --list-only | grep nullmoth`、`system_profiler SPDisplaysDataType` 正常

## 12. 回退

- **阶段 3 的 `deleteContainer` 之前**：旧系统一字节未动 —— 重启回旧卷、删掉新容器、C: 右键"扩展卷"即可还回去（只要 WinRE 分区还在中间，扩展卷前需先删它）。
- 阶段 3 之后：**新容器是唯一系统，回退退化为"重装"** —— 因此 §3.1 那 6 项验收是唯一的把关，
  不过关就不要执行 `deleteContainer`（旧系统还在，回头零成本）。
- 本方案不做离盘归档（§3.2 的风险由用户接受）：阶段 3 之前有旧容器兜底，之后无兜底。
