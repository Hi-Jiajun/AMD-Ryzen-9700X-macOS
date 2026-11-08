# 交接：RTL8125 网卡驱动导致的内核 panic（待新开会话处理）

来源：2026-10-11，CX3 驱动会话在排查中发现的**与本项目无关**的崩溃。
本文件自包含，不需要读 CX3 会话的任何材料。**先读第 1 节，再动手。**

> ⚠️ **2026-10-11 15:00 复核修订**：本文 §2/§4/§5.2/§5.6 的若干结论经源码复核后**是错的**
> （断言方向读反、驱动出处判错、TSO 被高估、换老驱动的建议会丢网卡）。
> **动手前先读文末 §11「复核修订」，以 §11 为准。**

---

## 1. 结论摘要（一行版 + 三条待办）

**本机的 Realtek RTL8125D 网卡驱动 `com.insanelymac.RTL812xLucy` 1.1.1 在收包中断里
触发 mbuf 双重释放断言，开机后约 8–15 秒（网络刚起时）整机 panic。**

待办按优先级：

1. **先确认手上这份驱动的真实出处与版本**——它**不是** Mieze 的上游 `LucyRTL8125Ethernet`
   （见 §4 的对照表），所以"上游已修"不能直接当成"换一份就好"。要找到
   `com.insanelymac.RTL812xLucy` / `IOClass=RTL8125` 这个项目的仓库或发布页，
   看它有没有比 1.1.1 新的版本。
2. **用配置 A/B 试掉两个高嫌疑开关**：本机驱动参数里 `enableTSO4/6 = true`、`enableASPM = true`。
   上游同类驱动的发布说明里有"**TSO 与 VLAN 组合会导致 kernel panic，已修复**"这一条
   （Mieze v1.2.3, 2026-02-24），说明 TSO 在这类驱动里确有 panic 前科。改成 false 再跑同样负载。
3. **本机芯片是 RTL8125D rev.2**，而 Mieze 上游开放的 issue 里有"RTL8125B 在 Tahoe 上不工作"
   （#33，未解决）。换任何驱动前，先确认它匹配 `10ec:8125` 且支持 D 版本。

---

## 2. 现象与决定性证据

### 2.1 panic 报告（原文位置）

```
/Library/Logs/DiagnosticReports/Kernel-2026-10-11-134619.panic
```

关键字段（逐字摘自报告）：

```
panic(cpu 0 caller 0xffffff80137db63c): assertion failed:
  m->m_type == MT_FREE && m->m_flags == M_EXT,
  file: .../bsd/kern/uipc_mbuf.c, line: 5584 @uipc_socket.c:8291

System uptime in nanoseconds: 15157989866        <- 约 15.2 秒
Boot 时间 13:46:11，panic 时间戳 13:46:19        <- 约 8 秒
Panicked task: 200 threads: pid 0: kernel_task
```

### 2.2 调用栈（这就是定性依据）

```
_panic
  _panic_machine_check64
    _m_mclfree                      <- 断言在这里：释放一个已经空闲的 mbuf
      _mbuf_allocpacket
        com.insanelymac.RTL812xLucy : RTL8125LucyRxPool::getPacket(uint, uint) + 0x3c
        com.insanelymac.RTL812xLucy : RTL8125LucyRxPool::replaceOrCopyPacket(...) + 0x40
        com.insanelymac.RTL812xLucy : RTL8125::rxInterrupt(IONetworkInterface*, uint,
                                                          IOMbufQueue*, void*) + 0xdf
        com.insanelymac.RTL812xLucy : RTL8125::interruptOccurred(OSObject*, ...) + 0x86
      IOInterruptEventSource::checkForWork
        IOWorkLoop::runEventSources
          IOWorkLoop::threadMain
```

**读法**：崩在**接收中断的顶半部**。`getPacket()` 想为 RX 环申请一个 mbuf，
而 mbuf 区里那个对象已经是 `MT_FREE` 且带 `M_EXT` 标志——即**被释放了两次**，
或**在仍被使用时被释放**。这是流水线错误，不是随机内存损坏。

**与本项目无关**：字符串 `Cx3Mlx4` 在该 panic 报告里出现 **0 次**。

### 2.3 时间点

两次相关 panic 都发生在**开机后极早期**：

| 报告 | 时间 | 距开机 | 类型 |
| --- | --- | --- | --- |
| `Kernel-2026-10-11-134619.panic` | 13:46:19 | 约 8–15 s | **RTL8125 mbuf（本文档主题）** |
| `Kernel-2026-10-10-210219.panic` | 21:02:19 | 约 25 s（报告内 "Time Awake Since Boot: 25s"） | **另一种**：`watchdogd[358] exited -- exit reason namespace 2 subcode 0x4`（看门狗判定卡死后主动 panic），**不要与上面混为一谈** |

=> 早期 panic 的复现线索：**开机 -> 网络开始收包**（DHCP / 首批广播）就会踩到。

---

## 3. 本机硬件与驱动身份（逐项实测）

| 项 | 值 | 来源 |
| --- | --- | --- |
| 芯片 | **Realtek RTL8125D rev. 2** | `system_profiler SPEthernetDataType` |
| PCI ID | `10ec:8125`，subsystem `10ec:0123`，rev `0x0c` | 同上 + ioreg `compatible = pci10ec,8125` |
| PCI 位置 | `5:0:0`（`GPP7@2,1 / pci-bridge@0 / pci-bridge@6 / ethernet@0`） | ioreg |
| 链路 | 2.5 GT/s x1 | 同上 |
| BSD 名 / MAC | `en0` / `00:11:22:33:44:58` | 同上 |
| 驱动 Bundle ID | `com.insanelymac.RTL812xLucy` | kext Info.plist |
| 驱动 IOClass | `RTL8125` | 同上 |
| 驱动版本 | `CFBundleShortVersionString 1.1.1` / `CFBundleVersion 1.1.100` | 同上 |
| 二进制 | 414064 字节，sha256 `ad6ca1836e07d9f689ffd1904d26fe7646c8a7075076005d26e7d58ef79435d5`，文件日期 2026-10-07 | ESP |
| 注入方式 | **OpenCore 注入**，不是 `/Library/Extensions` | ESP `/EFI/OC/Kexts/RTL812xLucy.kext`，并在 `config.plist` 的 `Kernel/Add` 里 `Enabled=true` |

驱动参数（`IOKitPersonalities -> RTL8125 -> Driver Parameters`，本机现值）：

```
enableTSO4 = true        <- 高嫌疑，见 §5.2
enableTSO6 = true        <- 高嫌疑
enableASPM = true        <- 高嫌疑
µsPollTime2G  = 160        (字符是微符号 µ，不是 u)
µsPollTime5G  = 130
µsPollTime10G = 100
fallbackMAC   = (空)
```

IOPCIPrimaryMatch：`0x812510ec 0x81251186 0x300010ec 0x812610ec 0x500010ec`
（本机的 `10ec:8125` 命中第 1 项）

---

## 4. 关键区分：这份驱动**不是** Mieze 的上游项目

这一点很重要，因为很容易顺手去升级上游：

| | 本机装的 | Mieze 上游 `Mieze/LucyRTL8125Ethernet` |
| --- | --- | --- |
| Bundle ID | `com.insanelymac.RTL812xLucy` | `com.insanelymac.LucyRTL8125Ethernet` |
| IOClass | `RTL8125` | `LucyRTL8125` |
| 个性名 | `RTL8125` | `LucyRTL8125` |
| PCI 匹配 | `8125, 1186x8125, 3000,` **`8126, 5000`** | `8125, 1186x8125, 3000`（**无 8126 / 5000**） |
| 崩溃栈里的类名 | `RTL8125LucyRxPool` / `RTL8125` | 会是 `LucyRTL8125...` |

=> 本机这份是**另一个（或改过的）项目**，只是沿用了相似的命名与同一个
`com.insanelymac.*` 前缀。所以"上游修了"不等于"换上游版本就好"。

### 4.1 上游的参考价值（仍然有用）

`Mieze/LucyRTL8125Ethernet` 的发布说明里，有两条与本崩溃方向一致的修复记录，
可以作为**排查思路**而不是**直接答案**：

| 版本 | 日期 | 发布说明原文要点 |
| --- | --- | --- |
| v1.2.3 | 2026-02-24 | "**Fixes a problem with TSO in combination with VLAN tagging which may have caused Kernel Panics in previous versions.**"（另：Tahoe 下 AppleVTD 支持、9k jumbo） |
| v1.2.2 | 2025-02-16 | "Fixed **IOFree()** bug." |

上游未解决的相关 issue（供判断风险面）：

```
#35  open     Recurring kernel panic on Tahoe 26.6: assertion now >= *pkt_enq_time
                (classq_fq_...)                  <- 断言不同，且是 Tahoe 26；本机 macOS 15.8.1
#33  open     Tahoe 26.4 - Not working with RTL8125B
#27  closed   Crash when using vlan              <- 已关闭，与 v1.2.3 的 TSO/VLAN 修复呼应
#23  closed   IOFree bug                         <- 已关闭，对应 v1.2.2
```

版本号规律（便于比对）：上游 `CFBundleVersion = <major>.<minor>.<patch x100>`，
即 `1.2.300` = 营销版本 `1.2.3`。**本机的 `1.1.100` = 1.1.1**。

---

## 5. 候选原因与最小实验（按性价比排序）

> 这些都是**待验证假设**，不是结论。每条给一个最小实验和一个"结果怎么读"。

### 5.1 先做的事：把当前状态固化成可回退

```bash
sudo diskutil mount disk0s1
cp -a /Volumes/OPENCORE/EFI/OC/Kexts/RTL812xLucy.kext ~/Desktop/RTL812xLucy-1.1.1-backup.kext
shasum -a 256 ~/Desktop/RTL812xLucy-1.1.1-backup.kext/Contents/MacOS/RTL812xLucy
#  期望: ad6ca1836e07d9f689ffd1904d26fe7646c8a7075076005d26e7d58ef79435d5
```

**回退方法**：把备份拷回 `/Volumes/OPENCORE/EFI/OC/Kexts/`，或把该 kext 在
`config.plist -> Kernel/Add` 里的 `Enabled` 改为 `false`（改前先备份 config.plist）。
（OpenCore 会在每次开机时注入这个 kext，所以**单纯重启不会恢复**；回退必须改配置或换文件。）
改 `config.plist` 前**务必先备份**。

> 注意：2026-10-11 12:33 前后，**另一个会话**改过 ESP 上的 `config.plist` 与
> WiFi kexts（`IOSkywalkFamily` / `IO80211FamilyLegacy` / `AirportMtk`）。
> 动 config.plist 前先确认当前状态、并备份。

### 5.2 关掉 TSO4/TSO6（成本最低、嫌疑最高）

依据：§4.1 里上游把"TSO + VLAN 导致 kernel panic"作为**已修复的 panic 原因**列出来了，
而本机这两个开关都是 `true`。

做法：编辑 ESP 上 `RTL812xLucy.kext/Contents/Info.plist` 的
`IOKitPersonalities -> RTL8125 -> Driver Parameters`，把 `enableTSO4`/`enableTSO6`
改成 `<false/>`（先 `plutil -convert xml1` 转成可读格式再改），重启后在同样负载下观察。

判读：

- **不再 panic** => 找到直接原因。可再单独开回 TSO4（不开 TSO6）做二分确定是哪一个。
- **仍然 panic** => 与 TSO 无关，继续 §5.3。

### 5.3 关掉 ASPM

本机 `enableASPM = true`。注意上游新版把这项**改名并默认反转**成 `disableASPM = true`
（即上游默认关 ASPM），这本身是个提示：ASPM 在这类驱动上常被关掉。

### 5.4 读驱动的 RX 池实现（定位到行）

崩溃点非常具体：`RTL8125LucyRxPool::getPacket` 调 `mbuf_allocpacket` 时，
mbuf 区里那个对象已是 `MT_FREE|M_EXT`。典型成因三种：

1. **同一个 mbuf 入池两次**（`replaceOrCopyPacket` 的替换语义下，旧 buffer 没有被
   正确交接，或交接后又被释放一次）；
2. **释放后仍在用**：RX 环描述符还指向已归还的 mbuf，下一轮 `getPacket` 拿到同一对象；
3. **`M_EXT` 外部存储被提前释放**：`m_ext` 的 free 回调与池的生命周期不匹配。

要在源码层面确认，需要拿到这个项目的源码——即 §1 待办 1。

### 5.5 关掉 VLAN / 少用 jumbo（若上面都不成立）

上游的 panic 修复与 VLAN 有关。若本机有 VLAN 接口或巨型帧，先撤掉再看。

### 5.6 更换驱动的备选

若要换成 Mieze 上游：本机芯片 `10ec:8125` 在上游的匹配列表里（`0x812510ec`），
**理论上可替换**，但必须注意：

- 本机是 **RTL8125D rev.2**，而上游有"RTL8125B 在 Tahoe 不工作"的开放 issue；
  换之前先确认目标版本对 D 版本的支持；
- 两驱动**不要同时启用**（会抢同一个 PCI 设备）；
- 换完要重新确认 `en0` 名称与 MAC 是否变化（会影响其它网络配置）。

---

## 6. 怎么收集证据（命令）

```bash
# panic 报告列表（最近的在前）
ls -lat /Library/Logs/DiagnosticReports/*.panic | head

# 读 .panic 文件：它其实是 JSON 行，panic 正文在 macOSPanicString 里
#   （注意：spindump -i 对 .panic **不适用**，只认 .shutdownStall/.diag 那种二进制格式）
python3 -c "
import json
p='/Library/Logs/DiagnosticReports/Kernel-2026-10-11-134619.panic'
for line in open(p, errors='replace'):
    line=line.strip()
    if not line.startswith('{'): continue
    try: d=json.loads(line)
    except Exception: continue
    s=d.get('macOSPanicString')
    if s: print(s)
" | head -40

# 关机停顿报告（shutdownStall）才用 spindump：
#   sudo spindump -i /Library/Logs/DiagnosticReports/shutdown_stall_*.shutdownStall -o /tmp/s.txt

# 本机网卡与驱动
system_profiler SPEthernetDataType | head -30
ioreg -l -p IOService | grep -i -A25 "ethernet@0" | grep -E "device-id|vendor-id|compatible|model"

# 该驱动的加载与报错
/usr/bin/log show --last 30m --predicate 'eventMessage CONTAINS "RTL8125" OR eventMessage CONTAINS "RTL812x"' | tail -20

# 驱动参数现值
plutil -p /Volumes/OPENCORE/EFI/OC/Kexts/RTL812xLucy.kext/Contents/Info.plist | grep -A12 "Driver Parameters"
```

注意：`kernelmanager_helper` 会为这个 kext 报一行
`Could not find identifier (com.insanelymac.RTL812xLucy) in helper to process load notification`
——因为它是 OpenCore 注入的、不在预链接缓存里，**这行是无害噪声，不是故障**。

---

## 7. 容易混淆的其它崩溃（避免误判方向）

| 报告 | 断言 / 类型 | 归属 |
| --- | --- | --- |
| `Kernel-2026-10-11-112135.panic` | `general protection` at `Mlx4Hw::parseDevCap` | **CX3 项目自己的**，已于 0.23.1 修复（dev-cap outbox 窗口加宽到错误的结构体，160 字节栈越界）。与本网卡无关，**不要**把它算在这张网卡头上 |
| `Kernel-2026-10-10-210219.panic` | `watchdogd[358] exited` | 看门狗判定卡死后主动 panic，**签名与 mbuf 无关**；根因未定，可作为第二条独立线索保留 |
| `shutdown_stall_*.shutdownStall`（01:49、11:51、12:41 等） | 关机停顿 1.00 s，栈里是 NVRM 线程在跑 | **NVIDIA 驱动**的旧毛病（01:49 那份出现在 CX3 相关工作之前），与本文档主题无关 |

---

## 8. 与本项目（CX3 驱动）的关系

**没有关系。** 唯一交集是两张卡都是 PCI 以太网设备，并存于本机：

```
15b3:1007   Mellanox ConnectX-3 Pro     <- CX3 项目（kext 在 /Library/Extensions）
10ec:8125   Realtek RTL8125D rev.2      <- 本文档主题（kext 在 OpenCore ESP）
```

排查 CX3 时若遇到"看起来像硬件/驱动异常"的现象，先按 §7 区分，不要混为一谈。

---

## 9. 明确列为未知的问题

- `com.insanelymac.RTL812xLucy` 的**出处仓库/发布页**：**未知**（§1 待办 1 的第一步就是找它）。
- 该驱动 1.1.1 与上游 1.2.x 的代码关系：**未知**（是不是上游某版本的改名分支，需比对源码）。
- mbuf 双重释放的**确切成因**：未知（需源码 + 复现；§5.4 给了三种候选）。
- 本机是否还有其它触发条件（VLAN / jumbo / 特定包型）：未知。
- 崩溃是否与 macOS 15.8.1 的某个 API 行为变化有关：未知。

---

## 10. 一页速查

```
症状      开机后约 8-15 秒整机 panic，栈在 RTL812xLucy 的 rxInterrupt -> getPacket
断言      m->m_type == MT_FREE && m->m_flags == M_EXT   (uipc_mbuf.c:5584)
归属      com.insanelymac.RTL812xLucy 1.1.1（OpenCore 注入，非 /Library/Extensions）
硬性证据  /Library/Logs/DiagnosticReports/Kernel-2026-10-11-134619.panic
第一假设  enableTSO4/6=true（上游 1.2.3 明说 TSO+VLAN 会致 panic）
第二假设  enableASPM=true（上游新版默认改成 disableASPM=true）
注意      手上这份不是 Mieze 上游项目（Bundle ID / IOClass / PCI 匹配都不同）
回退      备份过的 kext 拷回，或 config.plist 里 Enabled=false（改前先备份）
```

---

## 11. 复核修订（2026-10-11 15:00，源码级复核；**以本节为准**）

复核方式：按 `v1.1.1` tag 取回 `Mieze/RTL812xLucy` 源码与崩溃报告里的 UUID
（`91E96EF6-…`）对账；另取 `Mieze/LucyRTL8125Ethernet` 1.2.3 与
`xnu-11417.140.69` 的 `uipc_mbuf.c` / `kpi_mbuf.c` 对照；并把本文当作待纠错材料
过了一遍两个独立模型（未采纳其未经验证的断言，逐条自验）。

### 11.1 结论性修订（先看这四条）

1. **断言方向原来读反了。** `m->m_type == MT_FREE && m->m_flags == M_EXT` 是
   **期望成立**的状态；panic 表示它**不成立**（至少一个字段不是期望值）。
   报告没有打印对象的实际字段值。所以正确表述是
   「复合 mbuf 分配路径取到的缓存对象状态非法」，**不是**「已经 MT_FREE 所以被释放两次」。
   含义：**崩溃点不是破坏者**。RTL8125 只是开机时唯一在申请复合 mbuf、
   因而最先撞上这块损坏对象的那一方。
2. **这份驱动就是 Mieze 的项目，不是来历不明的第三方分支。**
   源码 remote = `https://github.com/Mieze/RTL812xLucy.git`；本机 1.1.1 与上游
   tag `v1.1.1` 的 `RTL8125LucyRxPool.cpp` / `rxInterrupt()` 完全一致。
   §4 的对照表结论作废（Bundle ID 改名不能推出作者不同；老驱动的池类名
   同样叫 `RTL8125LucyRxPool`）。
3. **TSO 不应再当第一假设。** 上游 1.2.3 修的是「TSO+VLAN」另一类 panic；
   开机踩到的是 DHCP（UDP），不走 TSO 分段。手上有 `enableTSO4/6=true`
   只是**嫌疑线索**，不构成证据。
4. **不要换回 `LucyRTL8125Ethernet` 1.2.3。** 实测其 `identifyChip()` 只到
   `CFG_METHOD_2..5`，**没有 RTL8125D（新版对应 `CFG_METHOD_11`）的识别分支**，
   换过去很可能直接不给网卡；而且两版 RX 池/`rxInterrupt` 代码本来就一样，
   换了也修不了这个断言。§5.6 作废。

### 11.2 新增的关键事实（实测，带证据）

| 事实 | 证据 |
| --- | --- |
| 这个签名**目前只出现过一次** | 本地全部 `.panic` 共 3 份，命中该签名 1 份（`Kernel-2026-10-11-134619.panic`） |
| 崩溃那次开机只活了约 **15.16 秒**，死在 `en0` attached + DHCP + **`AirportMtkInterface` 注册** + `wifiFirmwareLoader` 活跃的同一两秒内 | panic 报告 `15157989866` ns；日志 13:46:19.35 `en0: attached`、19.77 `…/AirportMtk/AirportMtkInterface`、19.79 `DHCP en0: start` |
| 崩溃那次开机自己的内核日志**已丢失**（panic 重启不落盘），只能用报告 + 相邻开机日志推断 | 13:46:12 与 14:14:32 是两次干净开机；13:46:19–13:46:34 的日志属于**上电后的下一次开机** |
| 当前这次开机（14:26 起）**WiFi HAL 被自己的门禁挡住**，RTL8125 已带 `TSO4/TSO6` 稳定跑 34 分钟以上 | `ioreg -n pci14c3,616` → `AirportMtkGate = blocked (previous bring-up never finished)`；无 `AirportMtk` 服务；`ifconfig en0` → `options=46b<…,TSO4,TSO6,…>`、`inet 192.0.2.147` |
| 崩溃点精确落在**替换分支**：收包 >128 字节时申请 4KB 复合 mbuf | `replaceOrCopyPacket+0x40` = `0x2b956`，紧跟 `callq *0x120(%rax)`（虚调 `getPacket`），且该调用在 `maxCopySize < len` 的 fall-through 侧 |

### 11.3 目前最值得查的一条：**自研 WiFi 驱动把 mbuf 交出去后还在读它**

优先级高于 RTL8125 自身的问题，因为 BSD mbuf 池是**全内核共享**的，
破坏者不必出现在最终 panic 栈里。

`driver/AirportMtk/mtk80211/openbsd/sys/_mbuf.cpp:42` 的 `_if_input()`
用 `MBUF_LIST_FOREACH(ml, m)` 遍历，循环体里把 `m` 交给
`ifq->iface->inputPacket(m, 0, kInputOptionQueuePacket)`（**接管所有权**），
而循环的推进表达式（`_mbuf.h:87`）随后仍然读 `mbuf_nextpkt(m)`
——**读的是已经交出去的那个 mbuf 的链接字段**。

这条路径在 bring-up 阶段就会跑：`SimpleMtkWlan/hal_mwx/MtkMwx.cpp:3551`
的 `mwx_dma_rx_process()`（HAL 收包路径，含 MCU/固件事件）在末尾调
`if_input()`。

修法（3 行，脱链后再交接，交接后不再触碰该对象）：

```c
while ((m = ml_dequeue(ml)) != NULL) {   /* 先脱链 */
    if (m == NULL) continue;
    ifq->iface->inputPacket(m, 0, IONetworkInterface::kInputOptionQueuePacket);
    ...
}
```

**归因边界**：这是真实的所有权违规，**但尚未与本次断言闭合**——没有证据表明
它一定就是那一次踩坏 mbuf 的那一击；而且必须先确认 13:46 那次开机加载的
二进制与当前源码对应。它属于**待修的缺陷 + 待证伪的头号嫌疑**。

### 11.4 RTL8125 自身的真实缺陷（确认存在，但**解释不了这次断言**）

`RTL812xLucy.cpp:1231-1240` 附近：jumbo 最后碎片扣掉 FCS 后 `pktSize <= 0` 时，
先 `mbuf_free(newPkt)`，随后仍可能调 `getChecksumResult(newPkt, …)` 写它
——条件性 UAF（需要「已有碎片链 + 最后碎片 + 长度扣 FCS 后 ≤0」）。
它写的是包头 checksum 字段，不是本次断言的 `m_type/m_flags`；
且那条路径走的是 128 字节以下的**复制分支**，与本次 4KB 申请分支不符。
另有替换分支失败时 `newPkt` 无人回收（更像泄漏）。

### 11.5 不要做的事

- 不要同时关 TSO4/TSO6/ASPM（违反单变量）。
- 不要往 1.1.1 的 plist 加 `enableAspmL0s/enableAspmL1`——那是 1.1.2 才有的键，
  1.1.1 的 `enableASPM=false` 只影响 L1，**L0s 仍被允许**。
- 不要升 1.1.2 当作「RX 修复」：v1.1.1→1.1.2 只改 PCI 命令寄存器/ASPM 细分，
  **没碰 RX 池和 `rxInterrupt`**。
- 不要动全局 AppleVTD/DMAR：影响面远大于单网卡，且本次走的是普通 `rxInterrupt()`
  （有 `AppleVTD` 服务、有 `RTL8125LucyRxPool` 实例，**不等于**走了 `rxInterruptVTD()`）。
- 不要为了让 WiFi 起来去清 `AirportMtkAttempt` 门禁——那条线归 MT7922 会话
  （见 `docs/handoff-mt7922-wifi.md`）。

### 11.6 下一步（推荐顺序）

1. **零成本观测**：先按现在这个「WiFi 被挡住、RTL8125 正常工作」的状态继续跑，
   它本身就是一次免费的隔离实验（记录起始时间、`netstat -m` 与 `netstat -ibdn -I en0`）。
2. **修 §11.3 那 3 行**（归 MT7922 线），这是当前唯一「有源码依据、代价小、
   且方向对」的改动。
3. 若还要跑一次 RTL 侧实验：**一次只改一个变量**，优先
   （a）`enableASPM=false`，（b）再单独关 TSO4，（c）再单独关 TSO6；
   每次改前备份 ESP 上的 kext 与 `config.plist`。
4. 验收口径分三层，不要混为一谈：**网络可用**（`en0` active + 默认路由走 en0）／
   **未复现**（同签名 0 新增，含至少 3 次自然重启）／**根因修复**
   （必须先有一条被证实的所有权或越界写路径，否则只能写「隔离后未复现」）。
