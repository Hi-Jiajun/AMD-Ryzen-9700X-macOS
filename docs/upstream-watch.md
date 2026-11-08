# nullmoth 上游追踪（nvidia-macos-driver / 1401）

最后核对：2026-10-08 07:29（当日第六次，v1.0.17 / v1.0.11 发布后 3 分钟）　核对方式：GitHub API + release 资产实测 + 官方 SHA256SUMS 校验

> 上游一天之内发了 10 个 Windows 版和 5 个驱动 tag，变动很快。这份文档记录**影响本机**的部分，
> 以及「现在该用哪个组合」。每次核对后更新。

## 1. 现在的可用组合（结论）

| 组件 | 现在能用的版本 | 状态 |
| --- | --- | --- |
| 1401 Windows | **1.0.17**（2026-10-07 23:26 UTC = 本机 10-08 07:26 发布） | ✅ 已发布；钉驱动 1.0.7 + Mac 1.0.11；本机实测哈希 `EF544C67…52F8` = 官方公布值 ✓（已存 `tools/1401-Windows-1.0.17.zip`） |
| NVIDIA 驱动包 | **1.0.7**（`v1.0.10` 与 `v1.0.11` 的资产**逐字节相同**） | ✅ 本机实测哈希 `5D66E1B6…EBF7` 与官方 `SHA256SUMS.txt` 一致 |
| 1401 Mac app | **1.0.11** | ✅ 已发布；本机实测哈希 `BF5EAF36…36BC` = 官方公布值 ✓ |
| 盘上实际在用 | 1401 1.0.15 的 config（我们打了 3 轮补丁）+ **NullMoth 目录 = 驱动 1.0.7 + Mac 1.0.11** | ✅ 见 §9 / §12 |

「已发布的最新」与「仓库里的最新」现在**一致**（都到 **1.0.17 / v1.0.11**），
§2 记录的那种「tag 有、资产 404」的半发布状态**已不复存在**。

⚠️ **仍然不要用 main 分支**建盘：1401 的 main 只在发布日期当天短暂领先 release，且没有校验哈希。
以 release tag 为准（`vX.Y.Z` + 它自己的 `SHA-256 (1401-Windows-X.Y.Z.zip)`）。

## 2. 上游处于「半发布」状态（重要）

驱动仓库的 tag 与 release 不同步：

| tag | 提交 | 有 release 资产吗 |
| --- | --- | --- |
| v1.0.0 | `de83e37` | ✅ 有 |
| v1.0.1 | `a7d2f91` | ✅ 有 |
| v1.0.2 | `59063bc` | ✅ 有 |
| **v1.0.3** | `5530574`（Driver 1.0.2 + Mac app 1.0.3） | ❌ **只有 tag，没有 release** |
| **v1.0.4** | `38e2fe4`（Mac app 1.0.4） | ❌ **只有 tag，没有 release** |

实测：`curl -I https://github.com/nullmoth/nvidia-macos-driver/releases/download/v1.0.3/nullmoth-nvidia-1.0.2.tar.gz`
→ **404 Not Found**（`/releases/tags/v1.0.3` 也返回 Not Found，即 release 对象不存在）。

所以作者是**先推 tag、还没传资产**。这不是我们的问题，等 v1.0.3 release 出现即可。

逐个 URL 实测（2026-10-08）：

```
200  v1.0.1/nullmoth-nvidia-1.0.1.tar.gz
200  v1.0.2/nullmoth-nvidia-1.0.1.tar.gz     <- v1.0.2 这个 release 给的是 1.0.1 的包
404  v1.0.2/nullmoth-nvidia-1.0.2.tar.gz
404  v1.0.3/nullmoth-nvidia-1.0.2.tar.gz
```

想提前拿到 1.0.2 那两条修复，唯一的路是**自己编译**（需要 Xcode 16 + Rust + Meson/Ninja +
NVIDIA `open-gpu-kernel-modules@610.57.04`，而且必须在能跑的 macOS 上编）——现阶段是死循环，
我们连 macOS 都还没装。**正确做法是用 1.0.1 先装，等 1.0.2 的包出现后再升级**（Mac app 或
`install.sh` 都能就地升级）。

### 用 main（1.0.10）做盘会怎么失败（顺序有意义）

`usbwriter.write()` 的执行顺序是：`check_script` → **擦盘** → 格式化 → **下载 macOS 恢复镜像** →
拷 EFI → `nullmoth.stage()` 下载驱动包。也就是说，那个 404 会在**盘已经被擦掉、恢复镜像也下完**之后
才炸。不是「一开始就被拦住」，而是白费一轮。

## 3. Driver 1.0.2：修了两个实机崩溃（都是 RTX 5060 这一代）

这条最值得看，因为两个崩溃都来自**用户上报的实机日志**，而且都在 5060 档硬件上：

### 崩溃一：Geekbench OpenCL 直接 abort

```
⛔ 10-07 (user crash report NM-W8WHES8G): Geekbench's OpenCL build aborted in
   -[NVMTLGLBinding doesNotRecognizeSelector:] under GLDComputeProgramRec::buildComputeProgram.
```

原因：Apple 的 GL-on-Metal 层走的是**老的** `MTLArgument` / `MTLType` 取值器，而插件的
`NVMTLGLBinding` / `NVMTLGLTexBinding` 没实现它们。修法是照着 Apple 二进制里的 selector 引用补齐
`arrayLength`、`textureType`、`alignment`、`dataSize`、`members`、`elementTypeDescription`、
`threadgroupMemory*` 等一批取值器。

### 崩溃二：Hackintool 崩溃

```
⛔ 10-07 (user crash report NM-W8WHES8G, RTX 5060 Ti): Hackintool died in -[__NSSetM member:]
   under -addObject:, called from setFragmentTexture on CoreAnimation's async-render workqueue thread.
```

原因：`NVMTLResSet` 直接继承 `NSMutableSet` 却**没有加锁**，而它会被 CoreAnimation 的异步渲染线程
并发调用。修法是全部操作加 `os_unfair_lock`，枚举改为先取快照（`nvmtlSnapshot`）而不是枚举活集合。

**对本机意味着**：驱动 1.0.1（现在能装的那版）**会有这两个问题** —— 跑 Geekbench 的 OpenCL 项目、
或用 Hackintool 时可能直接崩。等 v1.0.3 release 出来升级即可。

## 4. 1401 Mac app 1.0.2 / 1.0.3 / 1.0.4

| 版本 | 变更 | 对本机的意义 |
| --- | --- | --- |
| 1.0.2 | **USB map 终于能装了**（1.0.1 里每次都报 "the USB map the app wrote is not valid"，因为传错了文件夹）；找不到 OpenCore 配置时会说明各分区是什么（Clover / 别的机型的配置） | 装完后做 USB 端口映射要靠这版；对应我们 EFI 里那个 `UTBDefault.kext` 占位图 |
| 1.0.3 | 跟随 driver 1.0.2 | — |
| 1.0.4 | 「Send logs」会带上**显卡型号与 PCI ID**，并上传 NVRM/NVAccel/帧缓冲最近三天的内核日志与 macOS panic | 崩溃诊断更有效；同时也是**上传内容变多了**（见审计文档 §2.5） |

## 5. 1401 Windows 1.0.8 / 1.0.9 / 1.0.10

| 版本 | 变更 | 发布 |
| --- | --- | --- |
| 1.0.8 | 上报 U 盘日志时说明屏幕停在哪一步 | ✅ |
| 1.0.9 | **内核补丁「落在 mask 之下」修复**（某些 AMD 机器最终校验报 `Kernel->Patch[..]->Replace requires ReplaceMask`）；误点 Build 时给出明确提示而不是 `FileNotFoundError`；带上 Mac app 1.0.2 | ✅ |
| 1.0.10 | 把 driver 1.0.2 与 Mac app 1.0.3 打进 U 盘 | ❌ **未发布**，且指向的驱动包 404 |

1.0.9 那条 AMD mask 修复**对本机不适用**——我们这份 config 从一开始就是 ocvalidate 0 issues
（下面 §6 有实测）。

## 6. 本机 EFI 需要跟着重建吗：**不需要**（已实测）

用最新 main（1.0.10）在**同一份本机 Report.json + ACPI** 上重新构建，再与项目里现有 EFI 做整树 diff：

```
DIFF /DeviceProperties/.../layout-id          5 -> 1        # 每次构建引擎给的默认 ALC1220 layout 不同
DIFF /PlatformInfo/Generic/MLB                <sn-partial>... -> <sn-partial>...   # SMBIOS 序列号每次构建随机生成
DIFF /PlatformInfo/Generic/ROM                ...           # 同上
DIFF /PlatformInfo/Generic/SystemSerialNumber ...           # 同上
DIFF /PlatformInfo/Generic/SystemUUID         ...           # 同上
--- 其余全部一致 ---
```

也就是说 **1.0.7 → 1.0.10 对本机的 EFI 没有功能改动**，只有随机项和声卡 layout。

⚠️ 顺带一个实测发现：**每次构建都会重新随机生成 SMBIOS 序列号/UUID，并且引擎给的 ALC1220
layout-id 默认值也不稳定**（我四次构建分别得到 2 / 3 / 5 / 1）。所以：

1. **不要反复重建**——每建一次 SMBIOS 就换一套，装完系统后再改会很麻烦；
2. layout-id 是留给装完后按耳朵调的，别指望它一次就对。

## 7. 2026-10-08 晚：驱动 1.0.5 修掉了「只在我们这类板子上会失败」的 bug

这一天上游又连发 1401 1.0.12/1.0.14 与 driver 1.0.3/1.0.5/1.0.6/1.0.8。对我们**最关键**的是 driver 1.0.5：

> **Most Intel and Ryzen PCs:** the card's memory window is now placed inside the PCI window your firmware
> provides (**the old placement only worked on Core Ultra 200 boards**), and **enlarged by the driver itself
> when Resizable BAR is off in the BIOS**. Turning Resizable BAR on is still recommended.

作者唯一的实机测试板是 **Intel Core Ultra 200**，而 1.0.1 的 BAR 放置逻辑**只在那类板上成立**。
我们这台是 **AM5 锐龙 9700X**，用 1.0.1 有很大概率**装完驱动屏幕不亮**。1.0.5 修了这个，
并且会在 BIOS 没开 ReBAR 时自行放大 BAR（所以 ReBAR 从「必须」降级为「建议」）。

其余同日修复：

| 版本 | 修复 |
| --- | --- |
| driver 1.0.2 | Geekbench OpenCL `doesNotRecognizeSelector`；Hackintool / 多线程绘制的 `NVMTLResSet` 崩溃 |
| driver 1.0.3 | 延迟启动的 boot-hold 期限（原 40 秒计时器会挡住显示点亮的 100 秒等待）；持续重绘时 WindowServer 显示交换；厂商编译器对未知芯片名拒绝而非猜 Blackwell 目标 |
| driver 1.0.5 | 上面那条 BAR 放置（**本机最关键**）；RTX 30 系的复印引擎回退 |
| Mac 1.0.6/1.0.8 | 崩溃与启动日志收集更完整（含 WindowServer 崩溃报告、带空格卷名的 EFI 分区上的启动日志） |

device table 现在覆盖 235 个 Turing 及以后的显示卡 ID；**RTX 5060 在 macOS 15.8.1 上验证**。

### 本机 EFI 需要重建吗：**不需要**（已实测）

用 1401 v1.0.14 在同一份本机报告上重建，与现有 EFI 整树 diff：

```
DIFF .../layout-id                 5 -> 2        # 引擎随机给的 ALC1220 layout
DIFF .../MLB / ROM / SystemSerialNumber / SystemUUID   # 每次构建随机生成
--- 其余全部一致（policy.py 在 v1.0.9→v1.0.14 之间零改动）---
```

**保持现有 EFI**（它已有一组固定的 SMBIOS 值，别再来回换）。驱动版本是在引擎侧钉的，不在 config 里，
所以换 1401 版本不会影响 EFI。

## 8. 你的 fork（Hi-Jiajun/nvidia-macos-driver）

实测状态（2026-10-08）：

| 项 | 值 |
| --- | --- |
| HEAD | `38e2fe4b7a`（= 上游 main 最新） |
| 与上游对比 | `status: identical`，ahead 0 / behind 0 |
| 关键文件逐文件比对 | `README.md`、`plugin/NVMTLObjects.m`、`plugin/NVMTLGL.m`、`app/Resources/nullmoth-setup.sh` **全部相同** |
| tag | **无**（GitHub fork 默认不同步标签） |
| release | 0 个 |

结论：**现在它只是一个同步快照**，还没有任何自有改动或产物。

它能派上的用场（按性价比排序）：

1. **当稳定镜像点**——上游 force-push 或删库时你手上还有一份（我上次提过 AMD 补丁那个 4 star fork
   的风险，同理）。保持「Sync fork」即可。
2. **放你真正依赖的东西**——比如把 `laobamac/AMD_Vanilla` 的 `patches.plist`（21 KB）拷进来，
   构建就不再依赖第三方 4 star fork。这是我之前建议过的 vendor 方案，放你的 fork 里最自然。
3. **当二进制兜底**——今天这种「tag 有了、资产没传」的 404 会直接卡住做盘。要避免，就得在你的 fork 上
   建 release 并上传 tarball。许可上：驱动是 PolyForm Noncommercial（允许使用/修改/**分享**，禁止售卖），
   GSP 固件按 NVIDIA 可再分发固件许可——所以**个人非商业镜像站得住**，但那是转发别人的构建产物，
   值得你有意识地决定，而不是顺手做。

要不要我做第 2 条（把 AMD 补丁 vendor 进你的 fork，并让本地构建从你的 fork 读）？第 3 条我建议先别做，
等上游 v1.0.3 release 出来看它是否稳定。

## 9. 2026-10-08 07:1x：1401 v1.0.16 / 驱动 1.0.7 / Mac 1.0.10

发布节奏：`1.0.14`(19:52 UTC) → `1.0.15`(21:22) → `1.0.16`(23:12)，本机时间分别是 03:52 / 05:22 / **07:12**。
我们是 06:40 用 1.0.15 建的盘，**07:12 上游又发了一版**。

### 1.0.16 到底改了什么（逐条判断对本机的影响）

| 上游 changelog | 对本机的影响 |
| --- | --- |
| **AMD 板不再因为 `There are enabled entries under Booter->MmioWhitelist, but DevirtualiseMmio is not enabled` 而构建失败**（1.0.15 的 bug）；白名单条目现在跟着 `DevirtualiseMmio` 一起关 | ⚠️ **说明存在"某些芯片组本来就会拿到 MmioWhitelist 条目"** 的逻辑 —— 这正是本机缺的那块拼图，见 §10 |
| 新增 **Update the NVIDIA driver**（起始页）：自动下载最新驱动 + Mac app，逐个校验 SHA-256，放进 1401 的 NullMoth 目录，插着盘就一并更新盘 | ✅ 以后不用重建整只盘就能更新驱动；我们这次是手动做的同样的事 |
| 修 `FileNotFoundError ... UefiPxeBcDxe.efi`（构建时清目录遇到文件被删） | 只影响构建期；我们上次没踩到 |
| 盘里带 **驱动 1.0.7 + Mac app 1.0.10** | ✅ 已手动同步到本机 E: 盘，见下 |

**重要：1.0.16 没有推翻「AMD 上 DevirtualiseMmio 关掉」这条策略**（changelog 只字未提，
git log 里 1.0.16 的三次提交也没有回退 `DevirtualiseMmio off` 那次改动）。
本机 A/B 实测（见 runbook §1.6/§1.9）表明这块板子**必须** `DevirtualiseMmio=True`，
所以「用 1.0.16 重建 ⇒ 还要再打一次补丁」这件事没变。

### 本机盘上已经做的（没重建整只盘）

按官方 `SHA256SUMS.txt` 逐字节校验后替换 `E:\NullMoth\`（旧的一对移进 `old-1.0.6\` 做回滚点）：

| 文件 | 大小 | SHA-256（本地实算 = 官方值） |
| --- | --- | --- |
| `nullmoth-nvidia-1.0.7.tar.gz` | 269,260,020 B | `5d66e1b6b0706de1056b76d5adcd3fb1cbe7e81af57c4dfbd2b31b7ec202ebf7` |
| `1401-Mac-1.0.10.zip` | 2,058,794 B | `eaf9e4d49ab37ff25050dcee2ad5f4abea0f262a97094ce763394e5026c0285b` |

驱动 1.0.7 的要点（都出自用户日志）：双显示器第二个屏不再抖（每个点亮的 head 拿到自己的真 vblank）、
GTX 16/RTX 20（Turing）不再因为找不到 copy engine 导致 Metal 起不来、RTX 50 沿用 1.0.5 的图像尺寸策略。
**这些都要等系统装完才有意义**（驱动由 Mac 侧 1401.app 装进 /Library/Extensions），本次安装阶段不受影响。

### 要不要重建整只盘

**不要。** 理由：

1. 1.0.16 的三项改动里，只有「MmioWhitelist 构建失败」与本机相关，而我们的盘**已经建好且能过 0x16**；
2. 重建会重新随机 SMBIOS 四项（MLB/ROM/序列号/UUID），并把我们三轮补丁清零；
3. 驱动/Mac app 是**载荷**不是 EFI 结构，直接替换 `E:\NullMoth\` 即可（已做）。

只有当「上游再改 AMD 的 `DevirtualiseMmio` 策略」或「OpenCore 本体升级」时才值得重建。

## 10. ⭐ 顺手挖到的关键事实：OpCore-Simplify 自己的 AM5 白名单

`tools/1401/engine/app/upstream/OpCore-Simplify/Scripts/config_prodigy.py` 里有：

```python
def mmio_whitelist(self, motherboard_chipset):
    booter_mmiowhitelist = []
    if "Ice Lake" in motherboard_chipset:
        ... 4284481536  # 0xFF600000
    elif "B650" in motherboard_chipset or "X670" in motherboard_chipset:
        booter_mmiowhitelist.append({
            "Address": 4244635648,          # 0xFD000000
            "Comment": "MMIO 0xFD000000",
            "Enabled": True})
    return booter_mmiowhitelist

# 同一文件紧接着：
config["Booter"]["Quirks"]["DevirtualiseMmio"] = len(config["Booter"]["MmioWhitelist"]) != 0 or ...
```

也就是说 **AM5（B650/X670）的标准配方 = MmioWhitelist 里放 0xFD000000 + 打开 DevirtualiseMmio**。
本机（B850）之所以什么也没拿到，只是因为嗅探器把芯片组读成裸 `"AMD"`，两条 `elif` 都没命中 ——
这正是「官方 v1.0.15 关掉 DevirtualiseMmio 反而害了我们」的同一个根因。
0xFD000000 落在本机内存映射的 region #58（0xF7000000–0xFEE00000）里，而那次 reboot panic 的
`CR2 = 0xFEA802B0` 也落在同一个 region 里。详细证据链见 runbook §1.10。

## 11. 2026-10-08 07:25 复核：主分支已经跑到 1.0.17 / Mac 1.0.11，但**还没发 release**

release 列表（GitHub API，07:25 实测）：

```
1401:                 最新 release = v1.0.16 (10-07 23:12 UTC = 本机 07:12)
nvidia-macos-driver:  最新 release = v1.0.10 (10-07 23:12 UTC = 本机 07:12)  ← 仍是 driver 1.0.7 + Mac 1.0.10
```

而两分钟后的提交已经走到下一步（**只有 commit，没有 tag/release/资产**）：

| 仓库 | commit | 时间 (UTC) | 内容 |
| --- | --- | --- | --- |
| nvidia-macos-driver | `682e0242` | 23:20:39 | **per-system driver rules**：新增 `nullmoth-rules.json`，按「GPU 代数/具体卡/CPU 厂商/笔记本/eGPU/macOS 大版本/**芯片组**」匹配规则，命中后把 `NVMTL_*` 开关写进驱动的 `nvrm610.conf` 的一个标记块；profile 落到 `/Library/NullMoth/system-profile.json` 并随日志上报。同时修了 Update driver 用错哈希的问题 |
| nvidia-macos-driver | `2f3cefe3` | 23:22:28 | 版本号 → **1401 Mac 1.0.11** |
| 1401 | `f7b188c9` | 23:20:39 | 写盘时多写一份 **`NullMoth/system-profile.json`**（真实主板/芯片组/形态/CPU/每张 GPU 的 PCI ID 与 ReBAR 状态），供 Mac 侧的 per-system 规则用 |
| 1401 | `aa3ce812` | 23:22:48 | 版本号 → **1401 Windows 1.0.17** |

### 对本机有没有影响：现在**没有**，但值得提前铺好

1. **已发布的 Mac app 1.0.10 根本不读这个文件**（per-system 逻辑是 1.0.11 才进来的，尚未发布），
   所以「没重建 1.0.17 的盘」不会让驱动安装失败。
2. `nullmoth-rules.json` 目前的 5 条规则里，唯一带 `conf` 的是
   `nvenc-blackwell-only`（`match: {arch: [turing, ampere, ada]}`）—— **本机是 Blackwell（设备 ID `2D05`），不匹配**，
   即"硬件编码器只在 Blackwell 上点亮"这条对我们有利而不是有害。其余 4 条是 `builtin`，
   已经固化在驱动 1.0.6/1.0.7 代码里（含 `bar1-resize-8gb`，`match: {}` 恒真）。
3. 但规则的匹配键里有 **`chipset`**，数据来源正是我们盘上的 `NullMoth/system-profile.json`。
   我们没重建 1.0.17 的盘，所以这个文件**本来是缺的**。已按上游同一份代码
   （`p1401/nullmoth.py: system_profile`）从本机真实报告生成并写入 `E:\NullMoth\system-profile.json`：

   ```json
   {"board": "MAXSUN MS-TERMINATOR B850BKB WIFI", "chipset": "AMD", "platform": "Desktop",
    "cpu_vendor": "amd", "cpu": "AMD Ryzen 7 9700X 8-Core Processor", "cpu_codename": "Granite Ridge",
    "gpus": [{"name": "NVIDIA GeForce RTX 5060", "vendor": "NVIDIA", "id": "10DE-2D05",
              "type": "Discrete GPU", "rebar": "Disabled"}]}
   ```

   ⚠️ `"rebar": "Disabled"` 是**照抄上游探测结果**（它的注册表法假阴性，见 §1.8），
   **不改成实测值**：这个文件本来就是给上游做规则匹配用的，写假数据会让作者按错误前提改规则；
   实测真相（BAR1 = 8192 MiB，ReBAR 开着）记在盘上 `NullMoth\README.txt` 和 runbook 里。

### 主分支的 `policy.py` 仍会把 AMD 的 DevirtualiseMmio 关掉（源码实测）

`git diff` 我们手上的 v1.0.15 引擎与 main 版 `p1401/policy.py`，**唯一的相关改动**是**新增**：

```python
# ⛔ 10-07 (NM-KYT0Q2KS, NM-Y7891C9Z, NM-ZCSP67EX, NM-ADB3TNN9; 1401 1.0.15, B650): ... They go off together.
wl = [w for w in cfg.get("Booter", {}).get("MmioWhitelist", []) if isinstance(w, dict) and w.get("Enabled")]
for w in wl: w["Enabled"] = False
```

即 **main 依然对 AMD 关 `DevirtualiseMmio`**（`if cpu AMD and chipset not in AMD_DEVMMIO_ON: quirks["DevirtualiseMmio"] = False`），
只是现在会**顺手把白名单条目也关掉**以过 ocvalidate。**对我们的结论不变**：
未来若用 1.0.16+ 重建盘，**仍然要手动把 `DevirtualiseMmio` 打开并加回 `0xFD000000`**，
也就是 `~/.codex/tmp/hackintosh-macos-setup/patch_stick_v3.py` 那一步。

### 什么时候再回来重建

只有这两种情况值得：① 上游真的改了 AMD 的 `DevirtualiseMmio` 策略；② OpenCore 本体或 config 结构升级。
**载荷更新（驱动 / Mac app）永远不需要重建**——直接换 `E:\NullMoth\` 下的文件即可，
1.0.16 甚至把这个动作做成了 GUI 按钮（Update the NVIDIA driver）。

## 12. 07:26 上游把 1.0.17 / Mac 1.0.11 **正式发布了**（不是半发布，tag+资产都在）

就在上一节核对完 3 分钟后（23:26:01 / 23:26:02 UTC），两个 release 一起落地：

| release | 资产 | 本机校验 |
| --- | --- | --- |
| `nvidia-macos-driver v1.0.11`：**1401 Mac 1.0.11**（驱动仍是 1.0.7，"unchanged"） | `1401-Mac-1.0.11.dmg` 268,094,222 / `1401-Mac-1.0.11.zip` 2,071,195 / `nullmoth-nvidia-1.0.7.tar.gz` 269,260,020 / `SHA256SUMS.txt` | `1401-Mac-1.0.11.zip` 本地算出 `bf5eaf368a51174e5c5327d7ecc72044be12e7dea1c979774826dc90fa6936bc` = 官方值 ✓；驱动 tarball 与 v1.0.10 的**字节相同**（`5d66e1b6…ebf7`）✓ |
| `1401 v1.0.17` | `1401-Windows-1.0.17.zip` 289,077,093 | 本地算出 `ef544c670011a8ebb28f777e669e06cb9e95a0f4c3516d43cd3287905e3952f8` = 官方公布值 ✓ |

官方对 1.0.11 的描述里那句 **"Fixes for your exact machine"** 指的就是 per-system 规则，
并且明确写了第一条："on GTX 16 and RTX 20/30/40 cards the hardware video encoder is switched off …
(it works on RTX 50 only so far)" —— **本机是 RTX 50，不命中这条**，所以对我们的驱动行为零变化；
真正有价值的是它顺手修掉的 "Update the driver"（1.0.10 会用自己编译时的哈希去校验新包而卡在 checksum 步骤，
所以**必须用 1.0.11 或更高**才能在线升级驱动）。

### 已发布版 1.0.17 的 `policy.py`：AMD 策略**仍未改**（源码实测）

从 `1401-Windows-1.0.17.zip` 里直接抽出 `engine/app/p1401/policy.py` 与 main 版对比（两者实质相同）：

```python
AMD_DEVMMIO_ON = {"TRX40", "TRX50"}                       # ← 我们不在这两个里
AMD_SVM_ON     = {"AM1","A68H","A75","A78","A85X","A88X","A320","B350","X370","X399"}
...
if cpu AMD and chipset not in AMD_DEVMMIO_ON and quirks["DevirtualiseMmio"] is True:
    quirks["DevirtualiseMmio"] = False                    # ← 对 B850（芯片组报成裸 "AMD"）仍然照关
    ... wl = [...]; for w in wl: w["Enabled"] = False     # ← 1.0.16 起连白名单一起关
```

⇒ **结论不变**：任何 1.0.16+ 的 Windows 版重建出来的盘，**仍然需要 `patch_stick_v3.py` 那一步**
（打开 `DevirtualiseMmio` + 加回 `0xFD000000`）。而我们在盘上已经手工做到了，所以**不重建**。

### 本机这次落地的产物

```
E:\NullMoth\1401-Mac-1.0.11.zip        2,071,195 B   bf5eaf36…36bc ✓（新）
E:\NullMoth\nullmoth-nvidia-1.0.7.tar.gz 269,260,020 B  5d66e1b6…ebf7 ✓（未变）
E:\NullMoth\SHA256SUMS.txt             ← 取自 v1.0.11
E:\NullMoth\system-profile.json        ← 见 §11（Mac 1.0.11 的 per-system 规则要读它）
E:\NullMoth\old-1.0.10\                ← Mac 1.0.10（回滚）
E:\NullMoth\old-1.0.6\                 ← 驱动 1.0.6 + Mac 1.0.9（更早的回滚点）
C:\Users\hiliang\Documents\Hackintosh\tools\1401-Windows-1.0.17.zip   289,077,093 B  ef544c67…52f8 ✓
```

`tools/1401-Windows-1.0.17.zip` 只是留档（万一将来要重建/给别的机器做盘时用最新的引擎），
**没有解包、也没有用它重建任何东西**。

## 13. 2026-10-08 22:00 复核：Mac 1.0.18 / 驱动 1.0.11 / 1401 Windows 1.0.23 —— 并且 **EFI 被写的 BUG 上游已确认并修复**

### 13.1 版本现状

| 组件 | 最新可用 | 说明 |
| --- | --- | --- |
| 1401 Windows | **v1.0.23**（10-08 12:58 UTC） | 盘里 stage **Mac 1.0.17 + 驱动 1.0.11** |
| 1401 Mac app | **1.0.18**（`nvidia-macos-driver v1.0.18`，10-08 13:49 UTC） | 比 1401 Windows 里 stage 的 1.0.17 更新一档 |
| 驱动 helper | **1.0.11**（269,264,266 B，v1.0.17/v1.0.18 相同） | 从我们原来的 1.0.7 上来了 |
| 盘上现状 | **已更新为 Mac 1.0.18 + 驱动 1.0.11**（旧版移入 `old-1.0.11\` 等） | 见 13.3 |

### 13.2 ⛔ 你遇到的「驱动脚本写坏本地 EFI、Windows 引导消失」= 上游已确认的缺陷

`nvidia-macos-driver v1.0.16` release 原文（**逐字**）：

> **1401 for Mac 1.0.16 repairs a boot-file preservation defect in USB-to-internal OpenCore promotion.
> Earlier versions could replace the destination EFI tree and remove active Windows or vendor boot
> files; a backup did not keep those paths active.**
> Automatic copying onto another EFI partition is now **disabled**. Driver installation continues on the
> verified or explicitly selected startup OpenCore partition. **If the machine starts from a USB stick,
> keep it attached for every restart until the internal boot setup is separately reviewed.**
> **This update does not restore boot files already removed by an earlier installation.**

`1401 v1.0.21` 又用醒目提示重复了一遍：

> **Installation preservation notice: avoid USB-to-internal OpenCore promotion in the bundled Mac
> companion. This path can remove existing Windows/vendor boot files from the active destination EFI
> partition.** A corrected companion is being validated.

即：**「USB 上的 OpenCore 被自动"提升"到内置 EFI 分区」这个动作，在 Mac 端 1.0.16 之前会覆盖目标
EFI 树并删掉 Windows/厂商引导文件，而且它的备份并不能保住那些路径。** 我们在 10-08 一直用的是
Mac 1.0.9 / 1.0.10 / 1.0.11 —— **全都在有问题的范围里**。

**本机取证（只读，`esp_inspect.ps1`，报告见 `docs/bios/09-ESP-检查报告.txt`）**：

```
内置 ESP（磁盘0 分区1，200 MB）现在的内容：
  \EFI\BOOT\BOOTx64.efi            24,576 B   2026/10/7 17:10:28   ← 与 U 盘镜像同一时间戳
  \EFI\Microsoft\Boot\bootmgfw.efi  3,087,400 B  2026/9/9 11:25
  \EFI\Microsoft\Boot\BCD             24,576 B   2026/10/8 13:45:48  ← 今天 dism++ 重建
  \EFI\OC\{ACPI,Drivers,Kexts,Resources,Tools}                       ← 整套 OpenCore 被搬进来了
  \EFI\OC\Tools\NullMothSafe.efi      25,088 B                        ← Mac 端 setup 脚本装进去的
  \EFI\OC\config.plist.nullmoth-20261007-171033 / -174336 / -20261008-063000
  \EFI\OC\config.plist.nullmoth-usb-20261007-175521
  \EFI\OC\oldConfig.plist
  \opencore-2026-10-08-{005649,011932,012622,132811,133558,134541}.txt
```

`config.plist.nullmoth-*` 这些**带 Mac 侧时间戳的备份文件名**就是 Mac app 的 `nullmoth-setup.sh`
留下的；`NullMothSafe.efi` 是它装的引导项工具。也就是说**内置 ESP 已经被那个"promotion"动作改造过**，
这正是 Windows 引导条目消失的原因。

### 13.3 已做的处置

1. **盘上 Mac 端升到 1.0.18**（含 1.0.16 的修复），驱动 helper 升到 **1.0.11**，逐个按官方
   `SHA256SUMS.txt` 校验：`1401-Mac-1.0.18.zip` = `f7094567…cfa70` ✓，
   `nullmoth-nvidia-1.0.11.tar.gz` = `75869a4c…03ff3` ✓。旧的一对移入 `old-1.0.11\`。
   顺手把盘上遗留的旧 `1401.app`（10/07 16:21 解出来的）也移进 `old-1.0.11\`，避免 Mac 端误用。
2. 盘上 `README.txt` 已写明"**不要回退到 Mac 1.0.16 以下**"及原因。
3. **没有重建整只盘**（重建会重新随机 SMBIOS 四项并清掉 `patch_stick_v3.py` 的三项补丁）。

### 13.4 由此产生的两条操作纪律（以后照做）

* **每次重启都插着启动盘**（上游明说）。OpenCore 现在只待在"被选定的启动分区"上。
* **不要再手动把 U 盘 EFI 往内置 ESP 复制**；要内置引导就让 1401 自己按新流程做，或手工只复制
  `EFI\OC` 而不碰 `EFI\Microsoft`（并先备份 BCD）。

## 14. BUG 二：Mac 端 USB 端口映射太弱（Windows 侧已有素材）

本机 `E:\EFI\OC\Kexts\UTBMap.kext\Contents\Info.plist` 实测解析结果：

```
XHC0           → AppleUSB20XHCIPort, AppleUSB30XHCIPort        共 2 个端口
XHC1           → AppleUSB20XHCIPort, AppleUSB30XHCIPort        共 2 个端口
XHC2           → AppleUSB20XHCIPort                            共 1 个端口
pci1022,43fc   → AppleUSB20XHCIPort, AppleUSB30XHCIPort        共 2 个端口
```

一块 B850 板子的实际端口数是**十几个**（本机 Windows 报告里仅主 Setup 变量就列了 13 个
`USB*` 端口开关：USB2_1..USB2_4 / USB3_1_LAN / USB3_2_LAN / USB3_3 / USB3_4 / TYPE-C /
F_USBC / F_USB3_1 / F_USB3_2 / F_USB2_1_1 .. F_USB2_2_2）。所以 Mac 端生成的这张表**基本是空的**
—— 与你"探针不好用"的感受一致。

**Windows 侧其实已经把原料采全了**（`Report.json` → `USB Controllers`，4 个控制器，
含 PCI 路径与 ACPI 路径）：

```
1022-15B6  \_SB.PCI0.GP17.XHC0   AMD USB 3.10
1022-15B7  \_SB.PCI0.GP17.XHC1   AMD USB 3.10
1022-15B8  \_SB.PCI0.GP19.XHC2   AMD USB 2.0
1022-43FC  \_SB.PCI0.GPP7.UP00.DP60.XH00  AMD USB 3.20
```

建议路线（按推荐顺序）：

1. **USBToolBox（Windows 版）** —— 就是在 Windows 里枚举每个物理端口、勾选类型、直接生成
   `UTBMap.kext` 的那套工具（本机已经有 `USBToolBox.kext`，缺的只是一张正确的 map）。
   这正好实现你说的"在 Windows 侧做 USB 探测"。
2. 若能进 macOS：用 **Mac 1.0.13+ 修好的 USB map 步骤**（1.0.13 的 release note 就写着
   "repair installer, updates, removal and **USB mapping**"；我们盘上原来是 1.0.11，尚未包含）。
3. 只能当兜底：继续用 `UTBDefault.kext`（默认映射），端口功能不全但能用。

## 15. 2026-10-09：新增 `nullmoth/1401-probe`（离线硬件扫描盘）评估 + 本机已做的三件事

### 15.1 1401 Probe 是什么（仓库 + 官网 + 资产实测）

| 项 | 值 |
| --- | --- |
| 仓库 | `nullmoth/1401-probe`，GPL-3.0，2026-09-25 建库，10-09 04:11 最后推送 |
| 最新版 | **2.0.1**（10-09）—— **只发布在官网 nullmothsystems.com，GitHub release 停在 v2.0.0** |
| 组成 | 离线 Alpine Linux U 盘镜像（`1401-probe-img.zip` 448,172,517 B／解包后 `1401-probe.img` 564,133,888 B）＋ Windows 写盘器 ＋ Linux/「Mac」脚本 |
| 做什么 | 裸机启动后扫描整机（CPU／主板／PCI／ACPI／VBIOS／内存／存储／USB 拓扑／固件内存图／音频 codec 引脚），**最后用 mmiotrace 记录每个驱动读写显卡与声卡寄存器**（NVIDIA 卡用官方 610 驱动） |
| 不做什么 | 无网卡驱动（开局询问，默认否）；内部盘全部锁只读且不挂载；**不写 NVRAM**；只往自己那只标 `PROBE1401` 的 U 盘写一份报告 |

**版本与校验链（实测）**：官网 `SHA256SUMS.txt` 给出
`1401-Probe-Setup.exe = eaf4203…`、`1401-probe-img.zip = dd054d8c…`；前者与 DNSSEC 签名的
DNS 锚 `dig TXT _probe-sha256.nullmothsystems.com` **完全一致**，所以同一份 SHA256SUMS 里的镜像哈希也可信。
v2.0.0 的镜像哈希是 `5fa27706…`（不同），即官网当前这份确实是 2.0.1。

**两个上游瑕疵（实测，已踩）**：

1. 官网「On a Mac?」给的 `releases/latest/download/1401-probe-mac.sh` 与官网
   `/download/1401-probe-mac.sh` **都是 404**；仓库里只有带 `@VERSION@`/`@IMG_ZIP_SHA@` 占位符的模板。
2. `1401-probe-linux.py`（v2.0.0 资产）里钉的是 `VERSION="2.0.0"` + `IMG_ZIP_SHA=5fa27706…`，
   **照官网做 Linux/Mac 路径会拿到有 trace bug 的 2.0.0**。macOS 侧直接下官网 `img.zip`、
   按上面哈希校验后 `unzip -p | dd` 即可（等价于那个坏脚本）。

**对本机的用途**：不修任何现有故障，它是**数据源**——① `usb/ports.json` 的端口拓扑 + `usb_roles`
（可交叉验证 UTBMap 与内置蓝牙到底挂在哪个端口）；② `efi/memmap.tsv` + `map.firmware_memory`
（DevirtualiseMmio / MmioWhitelist 的判定输入，是我们"手工开回 DevirtualiseMmio + 补 0xFD000000"
这条经验的硬证据）；③ 40 MB 的驱动 trace；④ `audio_endpoints` 的 codec 引脚（挑 AppleALC layout-id）。

### 15.2 本机已做（2026-10-09）

1. **内置 ESP 全量备份到 NAS**：`/Volumes/Data/backup/EFI/internal-esp-20261009-142458/`
   —— `esp-raw-disk0s1.img`（209,715,200 B，sha256 `d56ae876…7671`，分区级逐字节镜像）、
   `disk0-gpt-head-1MiB.img`（GPT 头 + 分区表）、`EFI/{OC,Microsoft,BOOT}` 文件级副本、
   `config.plist`、`Library-NullMoth-*.tar.gz`（491 MB，含驱动与全部备份）、`nvram-dump.txt`、
   `config-facts.txt`、`UTBMap-*.plist`。
   **回滚办法**：进 macOS（或 Linux）后把 `esp-raw-disk0s1.img` 原样 `dd` 回 `disk0s1` 即可。
2. **ESP 卷标 `NO NAME` → `OpenCore`**（`diskutil rename disk0s1 OpenCore`）。
   已确认**不影响任何引导**：UEFI 启动项用设备路径 + 分区 GUID，不用卷标；Windows 的 BCD 同理；
   1401 Mac app 的驱动回滚读的是 `EFI_UUID` + 相对路径 `EFI/OC/config.plist.nullmoth-*`（`nullmoth-setup.sh`
   L171-174），绝对路径 `CONFIG_BACKUP` 只出现在报错文案里。
3. **1401 Probe 2.0.1 启动盘已做好**（写入 s1 那只 124 GB TESLADRIVE，原 Ventoy 内容已先整体
   备份到 `/Volumes/Data/backup/ventoy-stick-20261009/`，13 GB）。
   自带的 `SHA256SUMS` 8 个文件（`EFI/BOOT/BOOTX64.EFI`、`boot/vmlinuz`、`boot/initramfs.zst`、
   `boot/grub/grub.cfg`、`1401-probe.marker`、`drivers/nvidia-{open-610.57.04,closed-580.178.04,radios-*}.tar.xz`）
   **全部 OK**。注意：写完后 macOS 自动挂载会往 FAT 里加 `.Spotlight-V100`/`.fseventsd`，
   所以"整盘 dd 读回比对哈希"必然不一致 —— 用盘内自带的 SHA256SUMS 校验才是正确姿势。
   分区剩余 **104 MiB**（536 MiB 分区），报告要装在这里面。

### 15.3 从备份里读到的当前生效配置（比之前的记录更准）

```
Booter.Quirks.DevirtualiseMmio      = True        ← 我们手工开回的
Booter.Quirks.SetupVirtualMap       = False
Booter.Quirks.AllowRelocationBlock  = False       ← 1.1.0 引擎会把它设 True（STOP 0x16 兜底）
Booter.MmioWhitelist                = 0xFD000000 Enabled=True
Misc.Security.ExposeSensitiveData   = 6           ← 1.1.x 的 Mac app 需要 bit1，否则找不到启动分区
Misc.Boot.LauncherOption            = Full
UEFI.Quirks.RequestBootVarRouting   = True        ← 注意：这条在 UEFI.Quirks，不在 Misc.Boot
机型                                = iMacPro1,1  (SN <redacted-sn>)
```

Kext 启用情况：Lilu / AppleALC / AppleMCEReporterDisabler / NVMeFix / RTL812xLucy / RestrictEvents /
USBToolBox / VirtualSMC / WhateverGreen / **UTBMap（16 个端口条目，USBToolBox v1.1 生成）**；
`UTBDefault` 关闭。ESP 上另有一堆历史备份（`config.plist.nullmoth-*`、`oldConfig.plist`、
`_backup-20261008-224929`），需要时可清理成一份。

### 15.4 USB / 蓝牙现状（2026-10-09 实测）

- macOS 侧 4 个 XHCI 全部由 `AppleUSBXHCIPCI` 驱动：`1022:43fc`（XH00）、`1022:15b6`（XHC0）、
  `1022:15b7`（XHC1）、`1022:15b8`（XHC2，纯 USB2）。当前接的设备：Samson 麦克风（43fc）、
  无线鼠标 dongle（15b6）、MCHOSE 键盘（15b8）、TESLADRIVE（15b7）。
- UTBMap 覆盖的端口与 Windows 侧 USBToolBox "Enable All Populated Ports" 的 16 个端口**完全对应**，
  没有漏掉当时有设备的端口。⇒ 用户报告"以前插 HECATE G2000 的那个 USB2 Type-A 口现在不能用"
  **最可能的原因是那次扫描时该口是空的**，所以没进 map、在 macOS 里被禁用（而不是 map 写错）。
  待验证：把设备插到那个口，看 macOS 是否枚举；或等 probe 报告的 `usb/ports.json`。

  ⚠️ **本节的这条推测当天下午已被推翻** —— 真正原因是 macOS 的 15 端口硬上限，见 §16.4。
- **内置蓝牙**：`system_profiler SPBluetoothDataType` 显示 `Chipset: BCM_4350C2 / Transport: UART /
  State: Off / Address: NULL`，USB 树上**没有**蓝牙设备。RZ616 = MediaTek MT7922，Wi-Fi 与蓝牙
  macOS 都没有驱动；蓝牙挂在 USB 上，需要先能枚举出来才有戏。probe 报告的 `usb_roles`
  （接口类 `0xE0` = Bluetooth）能一次性确认它到底挂在哪个端口。

## 16. 2026-10-09（下午）：驱动升到 1.1.0，并查清了那个 USB 口与内置蓝牙

### 16.1 本机已执行（全部逐字节或哈希校验过）

| # | 动作 | 证据 |
| --- | --- | --- |
| 1 | 内置 ESP 与 `/Library/NullMoth` 备份到 NAS | `/Volumes/Data/backup/EFI/internal-esp-20261009-142458/`（分区级镜像 `esp-raw-disk0s1.img`，sha256 `d56ae876…7671`） |
| 2 | ESP 卷标 `NO NAME` 改成 **`OpenCore`** | `diskutil rename disk0s1 OpenCore`；不影响引导（UEFI 启动项用设备路径与分区 GUID，不看卷标） |
| 3 | 1401 Probe 2.0.1 探针盘 | 见 §15.2；盘内 `SHA256SUMS` 8/8 全部 OK |
| 4 | `Misc.Security.ExposeSensitiveData` 6 改 **7**；`Booter.Quirks.AllowRelocationBlock` false 改 **true** | config sha256 `87c047be…c139` 变 `a172b2b8…579f`，plist 级 diff **恰好 2 处** |
| 5 | **1401 Mac 1.1.1 与驱动 1.1.0** | 见 16.2、16.3；备份 `/Volumes/Data/backup/EFI/internal-esp-20261009-144214/` |

`AllowRelocationBlock` 的依据是 OpenCore `Configuration.tex` 原文：relocation block 是低 4 GB 里的一块
临时缓冲，只在「没有任何可用 slide / 强制 `slide=0`（含安全模式）/ KASLR 不支持」时才用，
并且 *"will only be used if required when the quirk is enabled"*；它需要 `ProvideCustomSlide` 与
`AvoidRuntimeDefrag` 同时为真（本机两条本来就是 true）。唯一代价：用到它时不支持休眠（本机不用休眠）。

驱动是这样装的（等价于 app 的 GUI 流程，用的就是 app 里那份审计过的安装器）：

```bash
sudo bash /Applications/1401.app/Contents/Resources/nullmoth-setup.sh \
  --pkg /tmp/nm110/nullmoth-nvidia-1.1.0.tar.gz \
  --sha 22478d83ed4d2b95711c2f001b40245e3198b6fb9716078c192e6ff18955d587 \
  --tool /Applications/1401.app/Contents/Resources/NullMothSafe.efi \
  --app  /Applications/1401.app/Contents/MacOS/1401 \
  --efi disk0s1
```

（`--efi` 是显式指定，用来绕过"先证明启动分区"；等 `ExposeSensitiveData` 生效后可以省略。）

### 16.2 驱动安装器自己改了 config 的哪 5 处（plist 级 diff 实测）

| 键 | 前 到 后 | 作用 |
| --- | --- | --- |
| `UEFI.Quirks.ResizeGpuBars` | -1 改 **13** | OpenCore 在引导前把 GPU BAR 调到 8 GB（2^13 MiB） |
| `Booter.Quirks.ResizeAppleGpuBars` | 0 改 **-1** | 不为 macOS 缩小 BAR（-1 即关闭该 quirk），macOS 看到完整 8 GB |
| `Kernel.Block` | 空 增 **1 条** | 屏蔽 `com.apple.iokit.IONDRVSupport`，否则固件帧缓冲会抢走 NVRMFB 的显示 |
| `NVRAM.Delete.7C436110-…` | 3 条 增到 **4 条** | 把 `boot-args` 加进删除列表，让 OpenCore 每次引导都重写它 |
| `Misc.Tools` | 空 增 **1 条** | 装 `NullMothSafe.efi`，即开机菜单里的 "1401: Remove NVIDIA driver" |

我们那两处改动（`ExposeSensitiveData=7`、`AllowRelocationBlock=true`）、以及 `DevirtualiseMmio=true`
与 `MmioWhitelist 0xFD000000`，全部原样保留。

### 16.3 装完的 kext 与上游 `VALIDATION.json` 逐字节一致

| kext | 本机实算 sha256 | 与上游 1.1.0 公布值 |
| --- | --- | --- |
| `NVRM` | `a2e76a41…acad0c` | 一致 |
| `NVRMFB` | `5993cc56…eae8ab51` | 一致 |
| `NVAccel` | `cac6763c…1e3cc8` | 一致 |

`/Library/NullMoth/driver-version` = **1.1.0**；`1401.app` = **1.1.1**（旧的 1.0.17 留在
`/Applications/1401.app.nm-old`）；`LaunchAgent com.nullmoth.crashcheck` 指向新 app；
`LaunchDaemon com.nullmoth.recover` 与开机菜单里那个工具组成「看不见画面也能卸驱动」的退路：
`NullMothSafe.efi` 只写一个 NVRAM 变量，开机后 daemon 自动跑 `nullmoth-setup.sh --remove` 再重启。

### 16.4 查实：那个 USB2 Type-A 口和内置蓝牙都在 macOS 的「15 端口硬上限」之外

**证据链（全部实测）**

1. Windows 侧 USBToolBox 报告：`1022:43fc` 这块 XHCI（ACPI 名 `XH00`）共 **17 个端口**，
   被占用的是 Port 2、3、7、8、15、16、17。
2. macOS 侧 `ioreg` 实测同一个控制器只创建了 **15 个端口对象**：
   `AppleUSB30XHCIPort@00100000…00500000`（5 个 USB3）加上 `AppleUSB20XHCIPort@00600000…00f00000`
   （10 个 USB2），合计 15。`locationID` 的高字节就是端口号，因此 **Port 16（0x1000000）与
   Port 17（0x1100000）在这台 macOS 上根本不存在**。
3. OpenCore `Configuration.tex`（`XhciPortLimit` 条目）原文：*"USB port limit is imposed by the amount
   of used bits in `locationID` format and there is no possible way to workaround this without heavy OS
   modification. The only valid solution is to limit the amount of used ports to 15 (discarding some)."*
4. Dortania 原文：*"This is due to `XhciPortLimit` breaking with macOS 11.3 and newer, to resolve this
   you must disable `XhciPortLimit`"*，代价是 `AppleUSBHostPort::createDevice: failed to create device`
   造成的重启循环。本机是 macOS 15.8.1，该 quirk 不可用（config 里它本来就是 false，别去开）。

**结论**

- `UTBMap.kext` 里 `XH00` 的 `port 16` 与 `port 17` 两条是死条目（对应的端口对象不存在）。
  map 没写错；之前"当时扫描时那个口是空的"的推测**不成立**，此处更正。
- 这两个口分别是 **Port 16 = Windows 报的 `Wireless_Device`（基本可以确定是内置 RZ616 蓝牙）** 与
  **Port 17 = 一个 USB2 Type-A（就是"以前插 G2000 那个口"）**。两者都永远枚举不到，所以那个口
  插什么设备在 macOS 里都没反应，与驱动、与 map 都无关。
- 蓝牙是**两层问题叠加**：其一，端口对象不存在，连枚举都到不了；其二，即使能枚举，MT7922 蓝牙在
  macOS 没有原生驱动，只有实验性的 `laobamac/SimpleMtkBtFw`（见 `disk-and-nic-plan.md` §6）。
  `system_profiler` 里那行 `BCM_4350C2 / Address: NULL / Firmware v0 c0` 是 macOS 找不到真硬件时的
  兜底占位，不是识别到了什么。
- **剩下一个待物理验证的点**：具体哪一个后方 USB2 口焊在 Port 17 上。定法是把设备插到那个口，
  在 Windows 跑一次 USBToolBox 看它落在哪个控制器/Port；或等 Probe 报告（Linux 会把 17 个端口
  连同 `usb_roles` 全部列出）。

**可选补救**：如果那个口必须能用，软件层面无解，只能换 XHCI 端口数不超过 15 的板子，
或加一张 PCIe USB 卡（在 macOS 上通常免驱，且独立计数端口）。

## 17. 2026-10-09 下午：驱动安装器的 OpenCore 改动会卡死本机（已回退）

### 17.1 现象与定位

安装驱动 1.1.0（安装器自动改了 5 处 config）之后，从**内置 ESP** 启动会：跑码停住 →
几分钟后黑屏 → 只能强制重启。**从 1401 U 盘启动同一套系统却完全正常**（驱动 1.1.0 已加载，
Metal 3、4K 显示都在）。内置于此之前最后一次成功启动是当天 13:56，安装动作发生在 14:41–14:42，
首次卡死 14:56 —— 回归窗口明确。

内置 ESP 上留下 6 份 OpenCore 日志（14:56 / 15:00 / 15:02 / 15:03 / 15:10），
**全部终止于 `#[EB|LOG:EXITBS:START]`**：OpenCore 与 boot.efi 都成功完成并交棒，
所以卡的是**接管之后的 macOS 内核阶段**，不是引导器。ESP 根目录没有 panic 转储、
NVRAM 里也没有 `boot-failure`/panic 变量 → **是卡死，不是崩溃**。

### 17.2 两份 config 的差异只有 8 处，其中 7 处是安装器/我们改的

| 键 | 1401 U 盘（能启动） | 内置 ESP（卡死） | 来源 |
| --- | --- | --- | --- |
| `Booter.Quirks.ResizeAppleGpuBars` | 0 | -1 | 安装器 |
| `UEFI.Quirks.ResizeGpuBars` | -1 | 13 | 安装器 |
| `Kernel.Block` | 空 | IONDRVSupport | 安装器 |
| `NVRAM.Delete.7C436110-…` | 3 条 | 4 条 | 安装器 |
| `Misc.Tools` | 空 | NullMothSafe.efi | 安装器 |
| `Booter.Quirks.AllowRelocationBlock` | false | true | 安装器 |
| `Misc.Security.ExposeSensitiveData` | 6 | 7 | 我们 |
| `Misc.Boot.LauncherOption` | Disabled | Full | 我们（10-08 的最优解） |

### 17.3 安装器自己的注释说明了一切

`nullmoth-setup.sh` 原文（逐字）：

> `# The macOS installer boots with a small GPU BAR (ResizeAppleGpuBars 0, so its fallback screen survives PCI setup); the`
> `# driver was tested with the card's full 8 GB BAR, so the installed system gets that back.`
> `# 13 = 8 GB: the full BAR of an 8 GB card, measured 10-07 on the RTX 5060 (NVRM moves BAR1 out of the console, display armed).`
> `# boot.efi stops with STOP 0x16 … and **the full BAR set above moves memory around**.`

即：**小 BAR 是让 fallback 屏幕在 PCI 初始化中活下来的取值**；作者是在自己的
Intel Core Ultra 200 测试板上把系统切换成"完整 8 GB BAR"。我们这块 AM5 板子上，BIOS 的
ReBAR 本来就已开启（Probe 实测 BAR1 = 8 GiB @ `0xfa00000000`），于是这个改动在本机
**至少是多余的，实际把内核卡住**。

### 17.4 为什么卡住：两处源码级证据

1. **`ResizeGpuBars=13` 在本机是空操作**。OpenCore `Library/OcDeviceMiscLib/SetResizableBar.c`：

   ```c
   if ((Bit < 0) || (Entries[Index].ResizableBarControl.Bits.BarSize == (UINT32)Bit)) {
     continue;      // 目标尺寸 == 当前尺寸：什么都不写
   }
   ```

   Probe 显示 BIOS 已经给出 8 GiB（= 2^13 MB），因此这条不该背锅 —— 它只是"没作用"。
2. **真正的差别是 `ResizeAppleGpuBars`**：`-1` = OpenCore 不再介入 macOS 的 PCI BAR 设置，
   macOS 直接拿到**完整 8 GiB BAR**；`0` = 交棒给 macOS 时把 GPU BAR 压到显卡支持的最小尺寸
   （本卡 capabilities `0x3fc0`，最小 64 MiB）。NVRMFB 里对应的重试逻辑：

   ```
   aperture retry round %d: BAR1 not CPU-writable yet -- waiting 5 s, then rolling again   // 最多 10 轮
   ```

   与"卡住 → 等几分钟 → 黑屏"的现象吻合。当前能启动的 U 盘侧，macOS 最终看到的 BAR1 仍是
   8 GiB（`ioreg IODeviceMemory` 实测），说明**驱动自己会在运行期把 BAR1 放大**
   （内置规则 `bar1-resize-8gb`），根本不需要这套引导期改动。

### 17.5 已做的回退（2026-10-09 15:22）

只改这 4 个键，回到与"可启动 U 盘 config"一致的状态：

| 键 | 改成 |
| --- | --- |
| `Booter.Quirks.ResizeAppleGpuBars` | `0` |
| `UEFI.Quirks.ResizeGpuBars` | `-1` |
| `Kernel.Block` | `[]`（撤掉 IONDRVSupport 屏蔽；本机根本没有 IONDRVFramebuffer，是空操作） |
| `Booter.Quirks.AllowRelocationBlock` | `false`（6 份日志都走到交棒，从未出现 STOP 0x16） |

**保留**：`ExposeSensitiveData=7`、`LauncherOption=Full`、`Misc.Tools`（开机菜单里的
"1401: Remove NVIDIA driver" 退路）、`NVRAM.Delete` 里的 `boot-args`。
改后 sha256 `93e98b27…2981c` → `55b00947…38d7a`；与 U 盘 config 的剩余差异只剩
上面那 4 个"有意保留"的键。

备份：ESP `EFI/OC/config.plist.bak-revert-20261009-152232`、
NAS `/Volumes/Data/backup/EFI/revert-20261009-152232/`、`/tmp/config.plist.before-20261009-152232`。

⚠️ 若回退后仍卡，就用差分法一处一处加回去定位（这些数据对上游提 issue 很有价值）。

### 17.6 Probe 报告带来的额外结论：那个 USB 口与蓝牙的位置被三重锚定

三份独立来源第一次完全对上（Linux 侧用 Samson 麦克风做锚点：Linux `usb1-port10`
= Windows「Port 15」= macOS `locationID 0x00f00000`，偏移 +5）：

| 设备 | Linux (Probe) | Windows USBToolBox | macOS 端口 | macOS 是否创建 |
| --- | --- | --- | --- | --- |
| Samson C01U Pro Mic | bus1 port 10 | Port 15 | `0x00f00000` (15) | ✅ |
| **内置蓝牙 `0e8d:0616`** | bus1 port 11 | Port 16 | `0x01000000` (16) | ❌ 超出 15 端口窗口 |
| **HECATE G2000** | bus1 port 12 | Port 17 | `0x01100000` (17) | ❌ 超出 15 端口窗口 |

即"以前插 G2000 那个 USB2 Type-A 口"就是 Port 17，蓝牙紧挨着它占 Port 16。
两者都落在 macOS **15 端口硬上限**之外 —— 这也是为什么 `system_profiler` 里只看得到
`BCM_4350C2 / Address: NULL` 的占位。Probe 还确认蓝牙硬件本身在 Linux 下**能正常枚举**
（USB 类 `0xE0` 无线控制器，三个接口），所以是"macOS 端口不存在 + 无原生驱动"两层问题。

Probe 报告本体：`/Volumes/PROBE1401/REPORTS/…20261009-0705_cd23.zip`
（2.0.1，15:05:28–15:08:58 完成，全部 16 个阶段 ok，含 NVIDIA open 610 驱动的 40 MB 寄存器 trace）。

## 18. 2026-10-09 15:28：回退后内置 ESP 启动成功（验证完毕）

| 检查项 | 结果 |
| --- | --- |
| 启动分区 | ✅ `nvram 4D1FDA02-…:boot-path` = `…/HD(1,GPT,6D351E57-EE46-4CE7-AACC-0BDA08532F8C,0x800,0x64000)/EFI\OC\OpenCore.efi` —— **正是内置 ESP**，且这是 `ExposeSensitiveData=7` 生效的直接证据 |
| 4 个 kext | ✅ 全部加载，UUID 与 1.1.0 的 `VALIDATION.json` 一致 |
| `driver-version` | ✅ 1.1.0 |
| 显示 | ✅ RTX 5060 / Metal 3 / 3840×2160 @60Hz |
| GPU BAR1 | ✅ **8589934592 B = 8 GiB**（地址 `0xFA00000000`），而 `ResizeAppleGpuBars` 已经是 `0` ⇒ **驱动确实在运行期自己把 BAR1 放大到 8 GiB**，不需要 §17 里那套引导期改动 |
| config sha256 | `55b00947…38d7a`（= 回退后的版本，未被再次改动） |
| 本次 OpenCore 日志 | 同样终止于 `#[EB|LOG:EXITBS:START]`（这是**正常**终点，交棒给内核）——与卡死那几次的日志末行形式相同，差别在于这次内核继续跑起来了 |

**§17 的结论成立**：卡死由驱动安装器写入的 OpenCore 改动引起，回退那 4 个键即恢复。

## 19. 探针报告要点（2026-10-09 07:05 那次）

报告：`/Volumes/PROBE1401/REPORTS/MAXSUN-MS-Terminator-B850BKB-WIFI_Ryzen-7-9700X_GeForce-RTX-5060_20261009-0705_cd23.zip`
（16 个阶段全 ok；16:00 = `radios` 47.6s、`gpu_drivers` 8.6s、`trace` 100.6s）

### 19.1 值得立刻用上的三条

1. **USB 端口 ↔ 物理位置终于对上**（三重锚定，见 §17.6）：蓝牙 = `usb1-port11`（后置下方左侧），
   失效的那个 Type-A = `usb1-port12`，两者都在 macOS 15 端口窗口之外。
2. **固件内存映射印证了 `MmioWhitelist 0xFD000000`**：EFI runtime map 里有一块
   `0x0000f7000000` 起、**126 MiB**、`type=0xb`（`EfiMemoryMappedIO`）的区域
   （`0xF7000000–0xFEE00000`），我们注释里写的"PCIe MMIO high window"就是它；
   当年那次 reboot panic 的 `CR2=0xFEA802B0` 正落在这块里。这条对上游很有价值。
3. **GPU 驱动 trace 成功**：`nvidia_driver = nvidia-open-610.57.04`，trace 里录到了
   `nouveau` 与 `nvidia-open-610` 两轮（后者 1.15 MB 压缩、model/IRQ/VBIOS/GPU UUID 齐全），
   VBIOS = `98.06.21.40.98`，DMA 52-bit。

### 19.2 音频（可以顺手打磨的一项）

codec = **Realtek ALC1220**，`Vendor Id 0x10ec1220`，`Subsystem Id 0x66881220`。
引脚实测（标准桌面配置，**没有内部扬声器**）：

| 节点 | 引脚 |
| --- | --- |
| 0x14 | `[Jack] Line Out at Ext Rear`（后置绿色） |
| 0x18 | `[Jack] Mic at Ext Rear` |
| 0x19 | `[Jack] Mic at Ext Front` |
| 0x1a | `[Jack] Line In at Ext Rear` |
| 0x1b | `[Jack] HP Out at Ext Front`（前置耳机） |
| 0x1e | `[Jack] SPDIF Out at Ext Rear` |
| 0x15/0x16/0x17 | `[N/A] Speaker`（未接） |

AppleALC 1.9.9 为 `0x10ec1220` 带了 **24 个 layout**（1/2/3/5/7/11/13/15/16/17/18/20/21/25/27/28/29/30/34/35/69/98/99/100），
其中 **layout 69「Lorys89 ALC1220 for AMD B450/B550 - SwitchMode」** 与
**layout 20「CaseySJ ALC1220 for Gigabyte B550 Vision D」** 是 AMD 平台专用条目，值得优先试。

⚠️ **一个待查的不一致**：config 的 `DeviceProperties` 注入的是 `layout-id = 2`，
但运行中的内核报告 `"layout-id" = <07000000>`（7）与 `"alc-layout-id" = <02000000>`（2）。
即"注入值"与"实际生效值"不同 —— 现在的音频是好用的（枚举出 Built-in Output、两个 Line Output、
Digital Output、Built-in Microphone），所以这只是"能否更贴合这块板子的 5 个模拟口"的打磨项。
验证方式：改 layout-id → 重启 → 看 `ioreg -l -n HDEF -r | grep layout`，逐个试
1/2/3/7/20/69（每个都要重启一次）。**优先级低**，等其它功能性事项收尾后再做。

## 20. 2026-10-09 17:00：高刷新率黑屏/闪烁 —— 1.1.0 新加的模式超出了驱动的呈现能力

### 20.1 现象（用户实测）

在"显示器"里改到高刷新率 → 黑屏或闪烁；一旦黑屏，很难改回 60 Hz；**即使改回 60 Hz 也仍然黑屏，
必须重启才恢复**。

### 20.2 证据一：这些高刷新率是 1.1.0 新"报出来"的，不是显示器原本就有的

驱动属性 `NVRMRefreshModesAdded = 10`。用 CoreGraphics 列出当前 4K 全部模式（实测）：

| 模式 ID | 分辨率 | 刷新率 | 来源 |
| --- | --- | --- | --- |
| `0x1` | 3840x2160 | 60 Hz | macOS/EDID |
| `0x10300` | 3840x2160 | 50 Hz | **驱动添加** |
| `0x10900` | 3840x2160 | 100 Hz | **驱动添加** |
| `0x10a00` | 3840x2160 | 120 Hz | **驱动添加** |
| `0x10f00` | 3840x2160 | 144 Hz | **驱动添加** |
| `0x10e00` | 3840x2160 | 160 Hz | **驱动添加** |

ID 形态正是源码里的 `0x10000 | (NVKMS 模式下标 << 8)`（`addKmsRefreshModes()`，1.1.0 新增；
上游 changelog：「NVRMFB: offer every refresh rate NVKMS validated from the EDID, not only the ones
macOS proposed」）。上游当时只验证了 **60 / 75 / 100 Hz**，且验证场景是双屏。

显示器自己是 **`Mi Monitor`（XMI 0x27b3，EDID 1.4），接在 DP-1**；其 CTA-861 的 VIC 列表里
确实有 **117 (4K100) / 118 (4K120)**，所以 100/120 是显示器真能力，**144/160 是 NVKMS 自己推导出来的**。

### 20.3 证据二（决定性）：flip 耗时在 100 Hz 以上根本放不下一帧

驱动自带的计数器（本次 60 Hz 启动实测）：

| 计数器 | 值 |
| --- | --- |
| `debug.nvrmfb_flip_us_sum` | 21,766,530 µs |
| `debug.nvrmfb_flip_n` | 1,763 |
| ⇒ 平均每次 pure flip | **12.35 ms** |
| `debug.nvrmfb_flip_us_max` | **38.44 ms** |

（源码：这两个计数器测的是 `submitConfig()` 里 `applyModeSetConfig` 的耗时，即零拷贝 flip 的调用时长。）

对照各刷新率的一帧预算：

| 刷新率 | 一帧 | 平均 flip 占比 |
| --- | --- | --- |
| **60 Hz** | 16.67 ms | **74 %** ✅ 能稳住 |
| 75 Hz | 13.33 ms | 93 % ⚠️ 临界（上游验证过） |
| 100 Hz | 10.00 ms | **124 %** ❌ 放不下 |
| 120 Hz | 8.33 ms | 148 % ❌ |
| 144 Hz | 6.94 ms | 178 % ❌ |
| 160 Hz | 6.25 ms | 198 % ❌ |

⇒ 60 Hz 有余量；**75 Hz 起就贴着上限，100 Hz 以上每帧必然超时** —— 与"低刷新率正常、高刷新率闪烁或黑屏"
的边界完全吻合。闪烁 = 偶发丢帧；黑屏 = flip 一直等不到完成。

### 20.4 证据三（源码级）：为什么"改回 60 Hz 也必须重启"

`kexts/NVRMFB/fb/nvrm-fb.cpp` 里有一个**一次失败就永久锁死到重启**的闩锁：

```c
// submitConfig(): 等上一次 flip 完成，最多 100 × 1 ms
if (!complete) { FBLOG("presentation blocked: prior completion unavailable on head %u", fHead);
                 fSubmissionFaulted = true; return NV_FALSE; }
...
FBLOG("presentation blocked: submitted completion unavailable on head %u", fHead);
fSubmissionFaulted = true; return NV_FALSE;

// switchMode(): 之后所有模式/刷新率切换一律拒绝
if (fSubmissionFaulted) return kIOReturnNotReady;
```

源码里那段注释写得就是同一个病：

> 「Latching here turned one refused 8x8 cursor image at login into **no zero-copy flips and no mode or
> refresh changes on every head until reboot**.」

1.1.0 只修掉了其中一条路径（"commit 被拒"不再闩锁）；**另外两条（上一次/本次 flip 完成超时）仍然会闩锁**。
闩锁在**内核对象里**，`NVRMNVDAFramebuffer` 这次启动只实例化了 1 个（`ioreg` 实测），
所以整个显示都受影响，改回 60 Hz 也会被 `kIOReturnNotReady` 拒绝 → 只能重启。

这也解释了为什么"屏幕黑着但系统还活着"：内核没崩、没有 panic，只是拒绝再配置显示。

### 20.5 与上游已知问题的关系

上游 1.1.0 / 1.1.1 的 release notes 里明确列着：

> **Known issue** – With two displays, changing the refresh rate or resolution of one can make the other
> flash or go blank **until you restart**. A fix is planned for 1.2.

我们是单屏，所以这条描述不完整，但属于同一族（刷新率切换导致呈现卡死）。**结论：这是上游未修的问题，
1.2 才修**。

### 20.6 现在怎么办（按优先级）

1. **暂时只用 60 Hz**（当前状态，稳定）。不要碰 100 Hz 以上，包括"试一下"。
2. 想给上游提 issue 的话，本文 20.2–20.4 正好是它需要的东西：模式 ID 与来源、flip 耗时基线、
   闩锁的源码位置。缺的是**失败瞬间的计数器**（见下）。
3. **要抓失败现场**：这些计数器**每次启动清零**，所以黑屏后重启就看不到了。可行的办法是在黑屏那一刻
   从另一台设备读 `sysctl debug.nvrm*`（需要先开"远程登录"），或装一个每几秒把 `debug.nvrm*`
   快照写盘的守护脚本 —— 后者不需要开 SSH，黑屏后重启即可读到最后一份快照。
4. 想彻底躲开这些模式：可用第三方工具（如 BetterDisplay）隐藏不需要的分辨率/刷新率；
   另有一个**未经验证**的思路 —— 让 OpenCore 的控制台/引导光栅是 1080p 而不是 4K，
   因为 `addKmsRefreshModes()` 只给「与启动光栅一致的分辨率」补刷新率（`fBootW/fBootH` 来自
   驱动接管控制台时的模式），改小启动分辨率理论上就不会再补出 4K 的 100/120/144/160。
   代价是引导界面与 `-v` 文字变大，且需要实测确认。

⚠️ 我**没有**在这台机器上复现黑屏（复现一次要付一次重启的代价），所以"触发点"部分是推断；
但 20.2/20.3/20.4 三条本身都是实测或源码原文。

## 21. 2026-10-10：上游一天内连发 5 版（1.1.1 → 1.6.0），并新增 `/help` 页

### 21.1 版本现状（本次实测）

| 组件 | 最新 | 我们的现状 |
| --- | --- | --- |
| `nvidia-macos-driver` | **v1.6.0**（10-10 03:16 UTC） | 驱动 **1.1.0**、Mac app **1.1.1** → **落后 5 版** |
| `1401`（Windows） | **v1.6.0**（03:25 UTC） | 盘上用 1.0.x；仅在需要重做盘时才相关 |
| `1401-probe` | **v2.0.2**（10-09 19:52 UTC） | 我们的盘是 2.0.1（那次跑得正常） |

当日节奏：`1.2.0`(19:09) → `1.3.0`(00:10) → `1.4.0`(01:48) → `1.5.0`(02:15) → `1.6.0`(03:16)，约 8 小时内 5 版。

v1.6.0 官方 `SHA256SUMS.txt`（已取回）：

```
bd145265292f00dd26f0eea4ee980284d0dcbc60ca01a3da9857368913029851  nullmoth-nvidia-1.6.0.tar.gz
bb22339d87573f397a91b0d26f73d1fa8a6ca726ba6e7bbe07e9935fb2b3625a  1401-Mac-1.6.0.zip
fc5498222af06277035d61f1a28f83f28d35759a7380b1b5a773da1f0bcec28f  1401-Mac-1.6.0.dmg
```

### 21.2 ⭐ 新增官方「按症状处置」页：`https://nullmothsystems.com/help`

这次最实用的发现——官方自己维护的"看到什么现象 → 怎么做"清单。摘录与本机相关的条目：

**① 1.1.0 有一个已知的黑屏 bug（我们躲过了）**

> **Black screen or a looping verbose screen right after installing 1.1.0**
> Start macOS with the driver off (add `-nvoff` to boot-args), run
> `sudo chmod 644 /Library/GPUBundles/nvmtl-allow.txt`, then restart.
> 1.2.0 and newer install that file correctly.

根因（1.2.0 release note 原文）：1.1.0 用 **app 的私有权限**拷 `nvmtl-allow.txt`，WindowServer
读不到 → 没有 Metal 设备 → CoreDisplay 每次启动都拦下（`Failed to create MetalDevice`）。

**本机实测：`/Library/GPUBundles/nvmtl-allow.txt` 是 `-rw-r--r--`（644，可读）⇒ 未受影响。**
原因是我们当初**用 `nullmoth-setup.sh` CLI 直接安装**，没走 app 的 GUI 路径。这条同时反向印证了 §17 的诊断：
我们的黑屏是 config 那 4 个键造成的（改回来就好了），不是这个权限 bug —— 否则改 config 不可能修好它。

**② ⭐ 高刷新率/双屏黑屏：官方给了「免强制重启」的恢复办法**

> **Two monitors: the other screen flashes or goes blank after changing resolution or refresh**
> **Known issue we are working on.** Change settings with one monitor connected, or restart after changing them.
> **To recover without restarting: from another computer, ssh in and run `sudo killall -9 WindowServer`.**

**本机实测：SSH 已经开着**（`systemsetup -getremotelogin` = **On**，`*.22` 正在 LISTEN）
⇒ 这条恢复办法**现在就能用**，不必再长按电源强制重启。

**③ 刷新率类问题他们正在收日志**

> **Stuck at 60 Hz, glitches in the first minute, slow cursor, an app crashes**
> We are fixing these from your logs. Send the logs from the Mac app right after it happens.

v1.4.0 为此新增了**逐显示器的刷新率与帧缓冲 flip/vblank 捕获**。
⇒ 想让上游能修我们的案例，**必须先升到 1.4.0+**，否则日志里没有他们需要的数据。

**④ AM5 的 `EB.MM.AKM` 官方认为是"随机"的**（细化我们 §17 的结论）

> **Ryzen 7000 / 9000 (AM5): restart and pick the installer again 2 or 3 times first.**
> On these boards this stop is often **random from one start to the next**; several AM5 PCs that hit it once
> installed fine on the next start with the same stick.

同页还给了**手工五键配方**：`DevirtualiseMmio=true`、`SetupVirtualMap=true`、`RebuildAppleMemoryMap=false`、
`EnableWriteUnprotector=true`、`SyncRuntimePermissions=false`。

⚠️ **这与本机当前生效的配置不同**（我们是 `SetupVirtualMap=false`、`RebuildAppleMemoryMap=true`、
`EnableWriteUnprotector=false`），但**我们的配置能稳定启动多次** ⇒ 那套五键是给"停在 EXITBS:START"的机器用的，
**本机不要动**。

**⑤ 其它**：「Send logs 卡住 app」→ 1.3.0 已修；「app 卡在 Finding the OpenCore partition」→ 升 1.3.0；
「driver 装好但没运行」→ 再重启一次（OpenCore 下次启动才应用 SIP/boot-args），并给出核对 `csr-active-config=430A0000` 的方法。

### 21.3 v1.2.0 – v1.6.0 的实质变更（按对本机的价值排序）

| 版本 | 变更 | 对本机 |
| --- | --- | --- |
| **1.6.0** | **驱动不再在登录几秒后自我移除**：某些板上 macOS 写 NVRAM 到不了固件，导致"移除驱动"的旧请求每次启动都回来（装上→跑几秒→重启→驱动没了）。现在把这个残留请求交给 OpenCore 在 macOS 启动前清掉 | ⚠️ **有价值**：我们 NVRAM 当前**没有**该标志，但装了 `com.nullmoth.recover` LaunchDaemon 会响应它 ⇒ 消除一个真实隐患 |
| **1.6.0** | **Ryzen + 自有 EFI：不再停在 `RmInitAdapter failed! (0x25:0x40:1310)`** —— config 里 Shaneee 的 Fix PAT 开、Algrey 的关时，安装会切换它们 | ✅ **本机无需改动**：实测我们已是 Algrey 开（idx 23/25）、Shaneee 关（idx 24/26） |
| **1.6.0** | Send logs 带上 **OpenCore config**（已去序列号/板序列号/UUID/ROM）+ EFI 里 kext 与驱动的清单 | ✅ 报障更有用 |
| **1.4.0** | **登录闪烁自动清除**：每次启动后桌面首次出现时给合成器一次干净重启（opt-out：建 `/Library/NullMoth/no-wsreset`） | ✅ |
| **1.4.0** | **Send logs 增加逐显示器刷新率与 flip/vblank 捕获** | ⭐ 报"高刷新率"问题的**前置条件** |
| 1.3.0 | **16 GB RTX 50 黑屏/崩溃循环修复**（5060 Ti / 5070 Ti 重载下会掀掉 WindowServer） | 相关（我们是 8 GB 5060） |
| 1.3.0 | 登录后插上的显示器**会点亮**（不再等到重启） | ✅ 热插拔 |
| 1.3.0 | Blender 4.2 能启动；Geekbench OpenCL 不再崩（RTX 30/40）；**pre-RTX 卡不再被接管** | ✅ / 安全边界 |
| 1.3.0 | Send logs 保留**整次启动**（驱动不再每分钟刷常规消息） | ✅ |
| 1.3.0 | Mac app：**Send logs 不再冻结 app**（1.2.0 里整个 app 会挂住，导致大多数人发不了日志）；安装不再卡在 "Finding the OpenCore partition"；setup **用序列号自己找到启动分区** | ✅ 与我们的 `ExposeSensitiveData=7` 同一类需求 |
| 1.2.0 | 修 1.1.0 的 allow-list 权限 bug；boot picker 的 "Remove NVIDIA driver" **保持生效**；驱动关着时 macOS 自己的显示驱动接管（不再黑到第二次重启）；重装会清掉未完成的移除 | ✅ 与我们的退路工具链相关 |
| 1.2.0 | Windows：盘上失败启动的日志能进下一次构建；含 EFI/MSR 分区的盘被接受；Apple 下载断点续传 | 与做盘相关 |

### 21.4 1401 Windows 1.3.0 – 1.6.0

| 版本 | 变更 |
| --- | --- |
| 1.5.0 | **网络在 1 MB 处切断下载**的问题：改小块抓取 + 更长重试等待，仍逐个对 Apple 签名列表校验（就是我们遇到过的那种"下到 33 MB 不动"的环境） |
| 1.6.0 | **每次读取序列号都变的 U 盘（SMI 控制器）被接受**；macOS 下载能等过最长约 15 分钟空连接、顺路向 Apple 要新链接；**Linux 探针报告现在可直接用来构建 EFI** |
| 1.3.0 | 断点续传 / 复用已有 macOS 或 Windows 的盘（先说明盘上有什么再问）/ 擦写与格式化失败的处理 / **EXITBS:START 的阶梯式修复** / 仅不支持的机器也能构建 / picker 里不用按 Space 就能看到 macOS |

### 21.5 1401-probe 2.0.2

> **No more freezes in the last step on laptops.** Every driver test in the last step now runs under the same
> automatic power-off as the graphics test: if one hangs, the PC turns itself off and the next start skips it.
> The HDMI audio of an NVIDIA card that is not driving the screen is no longer tested; it hung RTX 4050 laptops.
> **A freeze while listing PCI devices no longer loses the graphics card.**

⇒ 我们那次 2.0.1 跑完了 16 个阶段全 ok，**不需要重跑**；将来若要再扫，用 2.0.2。

### 21.6 结论与建议

**值得升级（1.1.0 → 1.6.0，跨 5 版）**，理由：

1. 消除"驱动自我移除"隐患（1.6.0）；
2. 拿到 Send logs 的**逐显示器刷新率/flip 捕获**（1.4.0）—— 让上游能修我们高刷新率问题的**前置条件**；
3. 拿到登录闪烁自动清除（1.4.0）、热插拔点亮（1.3.0）、RTX 50 崩溃循环修复（1.3.0）；
4. Mac app 的 Send logs 不再冻结（1.3.0）。

**升级前已确认无变化**：`Kernel.Patch` 已是正确状态（Algrey 的 Fix PAT 开、Shaneee 的关）⇒ 1.6.0 的
切换动作对我们应是 no-op；`nvmtl-allow.txt` 已是 644。

**新获得的能力**：SSH 已开启 ⇒ 高刷新率黑屏时可用
`ssh <user>@192.0.2.147 'sudo killall -9 WindowServer'` 恢复，**不必强制重启**。

---

## 22. 2026-10-12：metal2vulkan 重写版（0.2.0）开源，reims-vgpu 整树换掉 —— 驱动**现在不能换**

> 这一节是另一条上游线（不是 nullmoth / 1401），但 nollmoth 驱动里 vendored 的 `translator/`
> 就是它，所以记在同一份追踪里。核对方式：GitHub API + 稀疏 clone 实测 + 真实 `cargo check`。

### 22.1 上游动作（实测）

| 项 | 值 |
| --- | --- |
| 公开提交 | `13b279df1bf87893d389f955a90c55b6ff3d0f9b`，2026-10-11 17:09 UTC，Squash **11203** 次私有提交 |
| 等价性 | commit message 自述「Its tree is identical to the tree of private master at `b3c77f2d`」 |
| 结构 | 5 个 crate：`metal2vulkan`（翻译器 + CLI，712 文件 / 55.4 万行）、`metal2vulkan-runtime`（**Metal API on Vulkan**，379 文件 / 21.0 万行）、`metal2vulkan-air`（AIR 前端）；`validation` / `devtools` 不发布 |
| 许可 | **`GPL-2.0-or-later OR LGPL-3.0-or-later`**（双选；0.1.0 是单一 LGPL-3.0-or-later ⇒ 对我们不退步） |
| 依赖/门槛 | `ash 0.38`；**不再依赖 `spirv` crate**；Rust 1.98；Vulkan 1.2 + `timelineSemaphore`；不 build-time 链接 Vulkan |
| 发布状态 | **0.2.0 未上 crates.io**（那边只有 0.1.0 老设计）⇒ 只能钉 git rev；自标 alpha，API 未冻结 |
| 差异规模 | `43c46ac8 → 13b279df`：5033 文件，+1,808,772 / −355,140 |

**实测**：本机（rustc 1.99.0）`cargo check -p metal2vulkan --features cli -p metal2vulkan-runtime`
→ 42 秒通过、exit 0，仅 1 条 deprecated 警告（`Atomic::fetch_update`）。编译可用性不是风险。

### 22.2 三方现在各自钉在哪

| 使用方 | 钉的版本 | 说明 |
| --- | --- | --- |
| 本仓 `Hi-Jiajun/metal-api-emulator` | `43c46ac8a24adf1a6e872b8a52c706ec9614fad0` | 0.1.0 老设计的**最后一个公开状态**（2026-09-06）；此后只有重写，这条线不会再收修复 |
| nollmoth nvidia-macos-driver v1.13.0 | vendored 0.1.0（`translator/translator/`，version = 0.1.0） | 原生 Rust 依赖；C ABI 由它自己的 `translator/wrapper`（`crate-type = ["cdylib"]` → `libnvmtl_translate.dylib`）提供 |
| steelbrain/reims-vgpu（当前 HEAD） | **`13b279df`**（workspace Cargo.toml 直钉） | 它自己就是重写版的第一个消费者 |

> 更正一条旧记录：本仓 `metal-api-emulator/Cargo.toml` 钉的是 `43c46ac8`，**不是** `22358a1`。

### 22.3 ⭐ 驱动能不能直接换成重写版：**不能**（实测）

做法：取上游驱动 **v1.13.0** 的 `translator/wrapper`（8 文件 2108 行）原文，把 `metal2vulkan`
依赖从 vendored 0.1.0 改指到 `13b279df`，跑 `cargo check`：

```
33 errors —— could not compile `nvmtl_translate` (lib)
```

失败**全部**是老 API 被删，且集中在三个整块消失的模块路径：

| 0.1.0 里驱动在用的 | 0.2.0 |
| --- | --- |
| `metal2vulkan::passes`（`Stage`、`TransformOptions`） | 不存在 |
| `metal2vulkan::reflect`（`reflect_sanitized`、`ResourceKind`、`RuntimeSamplerState`、`SYNTHETIC_BINDING_BASE`） | 不存在 |
| `metal2vulkan::mesh_lower`（`lower_mesh`） | 不存在 |
| `translate_native_no_retry{,_with_options,_constructed_with_options}` | 不存在 |
| `specialize_air_function_constants` / `specialize_runtime_table_module` / `specialize_linked_module` / `linked_functions::*` / `canonicalize_spirv_bytes` / `meta::TextureComponent` | 无对应 |

另有一条硬性的：**输入格式变了**。wrapper 现在收 LLVM 文本（插件里 `nvmtl_air_to_text(bc)` 生成，
`nvmtl_translate(const char *ll, ...)`），而 0.2.0 文档写死「LLVM text is not an input」——
`Library::parse` / `from_air` / `from_metallib` 全收 `&[u8]`，只认 metallib、bitcode wrapper、raw AIR bitcode。

### 22.4 但要迁的话，对应物基本都有

| 驱动现在怎么做（0.1.0） | 0.2.0 的对应 |
| --- | --- |
| `translate_native_no_retry(文本)` → SPIR-V | `Device::compile_compute/render/tile/mesh_pipeline`（配 `Library::from_air`） |
| `nvmtl_translate_fc` 函数常量特化 | `FunctionDescriptor.constants: FunctionConstantValues` |
| `specialize_linked_module` / `link_extern_definitions` | `FunctionDescriptor.linked: LinkedFunctions` / `libraries: DynamicLibraries` / `LibraryType::Dynamic` |
| `reflect_sanitized` 取绑定与采样器 | `pipeline.reflection(&budget)` → `Compute/RenderPipelineReflection::bindings()` |
| `apply_tg`（`with_threadgroup_memory_length`） | plan 层自带 `threadgroup_memory_lengths` + `max_threadgroup_memory_length` |
| `mesh_lower::lower_mesh` | 直接 `compile_mesh_pipeline` |
| 自己拼描述符表（`table_ubo` / `buffer_addresses` / `g2_inline`） | 新 `bind/plan`：直接给「这次 dispatch/draw 写哪些描述符、push constant 多少」 |

新增的一个前置件：0.2.0 是 **device-description 驱动**的，必须给一份 Vulkan 设备描述
（`DeviceDescription::from_khronos_json`，或 `ash` feature 下的 `unsafe DeviceDescription::query`）
—— 驱动现在没有这个，得给 NVK 造一份 profile。

工作量判断：wrapper 约 27 个调用点重写 + 插件输出从文本改 bitcode（插件本来就 dlopen 了
GPUCompiler 的 libLLVM，`LLVMWriteBitcodeToMemoryBuffer` 已在 `NVMTLGL.m` 里用）+ NVK profile。
`align_guard` / `linearize` / `wide_loads` 是针对 NVK/NAK 的 workaround，与 metal2vulkan 无关，保留。

⚠️ 不换的代价：0.1.0 公开线停在 `43c46ac8`（2026-09-06）之后不再收修复 ⇒ **等于封版**。

### 22.5 顺带：reims-vgpu / qemu-reims-vgpu 也换了

| 仓库 | 动作 |
| --- | --- |
| `steelbrain/reims-vgpu` | `98fe2a3b`（10-11 17:27，+1.86M / −488k）整树换成 **libreims_vgpu + C API**（`reims_vgpu.h`）；crate 变为 core / metal / window / reims-vgpu / reims-vgpu-c / vulkan；workspace 直钉 `metal2vulkan` 与 `metal2vulkan-runtime` 到 `13b279df` |
| `steelbrain/qemu-reims-vgpu` | submodule 分支从 `host-reims-vgpu-vmapple` 换成新的 `steelbrain/reims-vgpu`（tip `a771139e`，10-11 17:17，Linux host 构建 + MMIO 移出 BQL） |

⚠️ **对 metal-api-emulator 的影响**：新 reims 树的 requirements 只有两条路 —— **Apple silicon Mac**
（Metal，二进制全是 arm64）与 **x86-64 Linux**（KVM + Vulkan 平台，可选 MoltenVK）。树里没有任何
Windows/WHPX 文件，我们那条已合并的 PR #57（Windows host port）只留在旧分支血统上
（新分支与旧血统 diverged，旧血统有 38 个提交不在新分支里）。即
`metal-api-emulator` 的「Windows + WHPX + reims-vgpu live VM path」在上游新线上没有对应物。

### 22.6 建议与**未做完**的事

- **驱动先别动**：不换 = 封版；换 = 一次移植（见 22.3 / 22.4）。
- 真正该先吃的是 `metal2vulkan-runtime` —— reims 已经替我们验证它能用；reims 自己要不要跟新树
  （等于放弃 Windows）是产品选择。
- 值得直接读的上游文档：`docs/RUNTIME-PORTING.md`（**Item 列就是 reims 那种 seam 的逐项映射**）、
  `docs/FAST-PATHS.md`（每条「速度为精度让路」的默认值都带 RTX 3090 / 5080 实测数字，
  与我们 offline parity 是同一件事）。

### 22.7 ✅ spike 已跑（2026-10-12 02:0x，本机实测）

**做法**：不用造测试用例 —— 直接拿**驱动插件自己内嵌的那份 metallib**（`plugin/nvconv_lib.h`，
55,322 字节，即驱动的格式转换内核 `nvconv3/4/5_b/_s` + `nvconv_cmp`），让 **0.2.0** 和
**本机实际装着的驱动翻译器**（`/Library/GPUBundles/NVMTLDriver.bundle/Contents/MacOS/libnvmtl_translate.dylib`，
0.1.0 血统）在**同一份输入**上对撞。

链路上的三个现成件（都在这台机器上）：`nvconv_lib.h` → 还原成真 metallib；0.1.0 仓库的
`scripts/mtlb-extract/mtlb_extract.py` 切出 **7 个 AIR blob**；驱动自带的
`/Library/GPUBundles/nvmtl/air-opt -S` 把 blob 变回 LLVM 文本（这就是驱动 `nvmtl_air_to_text` 那一步）。

| 环节 | 结果 |
| --- | --- |
| 0.2.0 CLI 编 metallib（6 内核，profile = 上游 `nvidia-rtx50-linux.json`） | **6/6 PASS**，self-check `spirv-val vulkan1.2, vulkan1.4: PASS` |
| 驱动现装翻译器 `nvmtl_translate(ll,"kernel",…)`（探针 dlopen + `set_caps(3)`，模拟运行时） | **7/7 rc=0** |
| 0.2.0 CLI 编同一批 AIR blob（`Library::from_air`，`--function` 同名） | **7/7 PASS**，`spirv-val vulkan1.2 / 1.4: PASS` |

**产出对比**（`spirv-val` 两边各自独立复验；Op 数来自 `spirv-dis`）：

| blob | 函数 | 驱动 0.1.0 | 0.2.0 | 驱动 Op | 0.2.0 Op |
| --- | --- | --- | --- | --- | --- |
| blob_00 | nvconv3_b | 14,484 B | 19,668 B | 271 | 361 |
| blob_01 | nvconv3_s | 14,016 B | 18,696 B | 245 | 312 |
| blob_02 | nvconv4_b | 14,776 B | 21,252 B | 278 | 378 |
| blob_03 | nvconv4_s | 14,308 B | 20,296 B | 252 | 329 |
| blob_04 | nvconv5_b | 14,972 B | 22,192 B | 284 | 389 |
| blob_05 | nvconv5_s | 14,516 B | 21,236 B | 258 | 340 |
| blob_06 | nvconv_cmp | 2,408 B | 4,468 B | 64 | 61 |

### 22.8 ⚠️ 唯一浮出来的迁移要点：0.2.0 的产出**依赖 `uniformBufferStandardLayout`**

用朴素 `spirv-val --target-env vulkan1.2` 校验 0.2.0 的产出会报错（**7/7 全报**）：

```
Array stride 4 must satisfy alignment 16. Instantiated via '40[%40]' in the Uniform storage
class using extended layout rules. … enable the uniformBufferStandardLayout feature
%_arr_uint_uint_60 = OpTypeArray %uint %uint_60
```

**这不是缺陷，是设备特性差异**：0.2.0 把那张 uint 表（60 / 36 / 20 项）按 stride 4 放进 Uniform，
所以要求该特性；带上 `--uniform-buffer-standard-layout` 后 **7/7 全 OK**（`--scalar-block-layout` 也行），
而 0.2.0 自己的 CLI 本来就带着 profile 的特性去校验（该 profile 里
`uniformBufferStandardLayout: true`）。0.1.0 的产出不需要这个特性。

**结论：NVK 有这个特性**（本机实测，用 0.2.0 的 `DeviceDescription::query` 直查驱动的 ICD
`/Library/GPUBundles/nvmtl/nvk_icd.json`）：

```
DEVICE NVIDIA GeForce RTX 5060 (NVK GB206-A): api_version=1.4.363 vendor=0x10de device=0x2d05
  UniformBufferStandardLayout = true
  ScalarBlockLayout            = true
  features(127) = { …, ScalarBlockLayout, UniformBufferStandardLayout, …, TaskShader, MeshShader, RayQuery, … }
```

⇒ 0.2.0 对驱动这几个内核的产出，在驱动自己的目标设备上是**合法用法**。
顺带证明：**给 0.2.0 造那份 NVK profile 是可行的**——`DeviceDescription::query` 在本机跑通，
直接吐出该设备的 127 项特性（上面那行就是它打的）。

**判读**：迁移的技术前提成立（重写版能编驱动的真实着色器、目标设备满足它的特性要求）；
真正的成本仍在 wrapper 那 27 个调用点重写 + 插件改喂 bitcode + 造 NVK profile（§22.3 / §22.4）。
0.2.0 的 Op 数普遍比 0.1.0 多约 33–48%（`nvconv_cmp` 反而更少），这是**产出差异、不是结论**——
要判是否等价，得跑真机对拍（对齐 §22.6 的 spike 思路），本文不给等价性结论。

## 本机 NullMoth 驱动安装状态与刻意偏离（2026-10-17 07:52 记）

当天上游连发 2.2.0（2026-10-16 20:55Z 发布，包从 ~1 GB 缩到 269 MB：只带 NVIDIA 驱动，
AMD/Intel、NPU、音频拆成独立产品；去掉 boot logo 与三个后台任务——displaytracker、每次开机重启
WindowServer 的 wsreset、crashcheck 登录项）。本机当天 05:21 曾被"移除流程"卸掉驱动
（`/Library/NullMoth/recover.log` 有记录，显示设置备份在 `display-prefs-20261017-052146`），
所以 07:49 这次是**全新安装 2.2.0**（内核侧 `pkgroot/install.sh` + 按 app 真流程跑
`nullmoth-setup.sh`）。

**三处刻意偏离上游 setup 的默认行为**（每次升级/重跑 setup 后都要重新确认，否则会被改回去）：

1. **不加 `amfi_get_out_of_my_way=0x1`** —— 上游 WANT_ARGS 里有它，我们靠 `amfi=0x80` + AMFIPass
   定点放行，加了只有副作用（setup 每次都会提议这一条）。
2. **`Misc > Boot > HideAuxiliary` 保持 `true`** —— 上游 setup 会置 false。
3. **两个 1401 引导项（`Misc > Tools`，Path=`NullMothSafe.efi`）的 `Auxiliary` 保持 `true`**
   —— 上游 setup 会写成 false（默认摊开显示）；用户的偏好是默认隐藏、按空格调出。

另外按用户要求加了 `nvfbhidpi`（裸开关，驱动二进制里的原话：`NVRM-fb: nvfbhidpi: scaled desktops
up to %ux%u on a %ux%u raster`），用于放开更多 HiDPI 桌面档位。
当前 boot-args 尾部：`… alcid=3 nvfb=1 nvaccel=1 nvfbheads=4 -nvkmsnosmooth nvfbhidpi`。
GPU 侧无 BAR 相关改动需要：驱动自报 `nvrm-gpu-profile` 里 `bar1-mb=8192`（大 BAR1/ReBAR 开），
OC 的 `ResizeAppleGpuBars=0`、`ResizeGpuBars=-1` 保持不动即可。

**2026-10-17 17:49 升到 2.3.3**（当天上游四连发：2.3.0 → 2.3.1 → 2.3.2 → 2.3.3）。要点：
2.3.2 把 Sequoia 搞出空白/撕裂窗口与 Dock 缺图标，2.3.3 撤回它的显示改动、**重新发 2.3.1 的驱动**，
只保留"Sequoia 安装报错（`runtime payload requires macOS 26.0.0`）"与"版本记录跟随真实文件"两个修复。
实测 **2.3.3 包里的四个 kext 与 2.3.1 逐字节相同**（NVRM cb3775fa / NVAccel f0edef4e /
NVRMFB 6b8befff / NVRMAGDC 223a327f），所以升级后 `kmutil showloaded` 的 UUID **不会变**——
判断是否升级成功的依据是 `/Library/NullMoth/driver-version` 与 AuxKC 重建时间，不是 UUID。
2.3.3 的 `nullmoth-setup.sh --dry` 仍然只提那三条已知偏离（amfi_get_out_of_my_way、HideAuxiliary、
两个 1401 条目的 Auxiliary），**ESP 配置本次无需改动**。

**2026-10-20 14:12 装 / 18:10 重启验证 2.4.3**（2.4 系列当天连发四版：2.4 与 2.4.1 都"更新后起不来显示"，
2.4.2 修显示、2.4.3 修 2.4 引入的 panic——`thread_call group 'high' reached max thread cap`，显示驱动给每个
vblank/每帧鼠标都新起内核线程，现已串行化）。装法：官方 `install.sh`（不跑 1401 setup，ESP 未改）→ 用音频会话
按 2.4.3 源码重建的 NVRMFB（`3a157a41…`，plist 2.4.3）覆盖 → 同款 `kmutil create --repository /Library/Extensions`
重建 AuxKC（先 `kmutil inspect | grep -c com.nullmoth.NVRMFB` ≥ 1 再 `mv`）。
重启后：四 kext 均 2.4.3（`NVRMFB 02FEEE1D…` / `NVRM 0952A192…` / `NVAccel F3560057…` / `NVRMAGDC EF61483E…`）、
Metal 3 + 4K@160 HiDPI、Chrome GPU 进程加载 `NVMTLDriver`、HDMI/DP 音频 verify GREEN 且与 2.3.7 基线逐项一致、
`max thread cap` 计数 0。回滚点：`pre-2.4.3-backup-20261020-141153`（2.3.7 + 音频补丁）、
`backup-20261020-141200.1lS05L`、`NVRMFB.kext.official-2.4.3-20261020-141211`。

**2026-10-22 11:14 装 / 11:23 重启验证 2.5.2**。上游当天连发 2.4.4 / 2.5.0（换 NVIDIA
615.78.08 内核驱动与固件）/ 2.5.1 / 2.5.2。音频 PR #284 已并入 2.5.2：五个 framebuffer 属性 +
`audio-codec-info` 默认 pin 4 成为默认行为，可用 `NVRMFBDisplayAudioCodecInfo` 覆盖 —— 因此
**本地补丁版 NVRMFB 退役**，改为官方包直接装（`install.sh` 不替换任何 kext）。
装完四个 kext 与官方 payload 逐字节一致（NVRM `d81b8fcb` / NVAccel `c68b70df` /
NVRMFB `bdf6a2dd` / NVRMAGDC `223a327f`），插件 `aa16ed89`；固件迁到 `/Library/NullMoth/nvfw`
（220 MB，root:wheel），`/Users/Shared/nvfw` 已移除。
重启后：四 kext 均 2.5.2、Metal 3 + 4K@160 HiDPI、Chrome GPU 进程加载 `NVMTLDriver`、
`verify-nvrmfb-link.sh` **GREEN** 且与 2.4.3 基线逐项一致（`NMHDALink` not loaded）、
`sysctl debug.nmhda` 仍是 `10DE:22EB now accepted`、`max thread cap` 0、无 panic/hang。
**第二步（可选）**：2.5.2 顺带改了 NMHDA 本身（#311：去掉写死 9D70 表项，改用 AppleHDA 的
`HwFactoryPrefixTranslation` 把 9D70 翻译回真实 22EB；probe/start 按符号名找镜像；去掉写死的
macOS 15.8 结构体偏移）。上游版本已由音频会话编好：`outputs/20261022-nmhda-252/NMHDA.kext`
（`74057441…4307`，40,304 B），ESP 里当前仍是旧版 `8b9c4839…`（35,600 B，工作正常），
旧版已备份到 `pre-2.5.2-backup-20261022-111124/NMHDA.kext.old-esp-8b9c4839`。
