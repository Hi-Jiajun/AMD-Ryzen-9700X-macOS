# 社区一手源审计：超过 15 口的真实可行路径

日期：2026-10-20
范围：只读研究；未改 ESP、未加载/卸载驱动、未重启、未联系其他会话。
基线：XH00 硬件 17 口，实际只建 15 口；两条 USB 15→17 补丁 `Enabled=false`；HS11/HS12 不存在。
目标：不预设“全 17”或“15 口”，只回答社区方案到底做到了哪一层。

## 0. 结论先行

1. **社区没有任何公开的“完整超 15”实现。** OpenCorePkg 当前（2026-10-19 HEAD）
   的 `XhciPortLimit` 只有四个补丁站点：两个 15→0x40 的阈值比较、一个
   `and 0x0F`→`0x3F` 的掩码、一个 Tahoe 换符号。没有 locationID 分配器、没有
   Hub 层级、没有 `generateBusNumber`/`reserveBusNumber`、没有消费者适配。
   OpenCore 源码里与完整编码相关的唯一段落是 `TODO`，配置文档直接写
   “no possible way to workaround this without heavy OS modification”。

2. **USBToolBox 不 patch 端口上限。** README 原文 “This does **not** patch the
   port limit.”；`mergeProperties` 只做三件事：删 `ports`/`port-count`、把
   `IOProviderMergeProperties` 原样搬到控制器、结束。map 的字典键名是生成标签，
   真正的端口身份是每条记录的 `"port"`（硬件号）；`port-count` 是选中端口里的最大
   硬件号。改字典名不构成重映射。

3. **USBMap issue #63 是“阈值 + 掩码”的手工字节配方**，额外补了
   `createPortEPN`，但仍不碰控制器字节分配、`shift=20`、Hub 半字节和消费者。
   它绑定 macOS 26 的具体 KernelCollection 位移，配方作者也没有维护者复核
   （open、0 评论）。

4. **稀疏 map 是真实可行的一条路，但它不是“突破 15”。** 它把对象数压到 ≤15，
   因此可以把硬件 16/17 放进有限的逻辑位置；本机 2026-10-11 的
   “阶段 2A 实测通过”就是 7 条 map、补丁全关、7 个对象。代价是其余端口不创建。
   这属于“可用基线 / 下一实验”，不能当“全 17”。

5. **真正 >15（同一控制器保留 17 个对象）只有两类路线**：
   (a) 重写 locationID 编码及所有生成/消费点（OpenCore 所说的 heavy OS
   modification）；(b) 把端口拆到多个逻辑控制器（自研内核驱动 + 私有前端，
   或 Apple 的 VHCI，但 VHCI 的端口号字段本身只有 4 bit 且要 entitlement）。
   两者都没有公开可用实现。

## 1. 15 口到底卡在哪几层

| 层 | 机制 | 证据 | 社区是否处理 |
| --- | --- | --- | --- |
| L1 对象计数 | `createPorts` 用 `cmp ebx,0xF` 限制成功创建数；`setPortLocation` 用 `cmp r15d,0xF` | OpenCore `CommonPatches.c:686-708`、`:713-736`；本机反汇编 `outputs/20261010-usbportlimit/review/encoding-audit-20261017.md` §2 | 是：阈值改 `0x40` |
| L2 端口掩码 | `and reg,0xF` 后再左移（版本相关；15.8.1 实测写入点只有 `shl`+`or`，没有 `and`） | OpenCore `CommonPatches.c:754-798`；encoding-audit §1 | 部分：改 `0x3F`；15.8.1 上该站点是否命中需离线复核 |
| L3 移位位置 | 根端口从 bit20 开始找空半字节，端口域 4 bit（bit20–23） | OpenCore `CommonPatches.c:881-893` 的伪代码；encoding-audit §1 | 否 |
| L4 控制器字节 | locationID bits24–31；`generateBusNumber` 用 PCI `(function<<5)\|(device&0x1f)` 候选，`reserveBusNumberGated` 被占则递增 | encoding-audit §4 | 否；只能靠 `bus-number` 属性改候选 |
| L5 Hub 层级 | 每级 Hub 吃一个 4 bit 半字节；子端口从父 locationID 的 bit20 向下找空位 | OpenCore `CommonPatches.c:897-899`；USBMap `Information.md:118-120`；encoding-audit §3 | 否 |
| L6 消费者 | locationID 被音频 UID、HID、System Information 等读取 | 本机审计未穷尽（encoding-audit §7） | 否 |

关键事实：**硬件端口号与逻辑序号是两套编号**。`createPorts` 的 `0xF` 比较的是
“成功创建计数/逻辑序号”，硬件号只用于 MMIO 和 map 查表（encoding-audit §2）。
所以“写 17 条 map”不等于“建 17 个对象”，也所以稀疏 map 能把硬件 16/17 压进
逻辑 6/7。

## 2. OpenCorePkg 审计（固定 commit）

固定版本：`acidanthera/OpenCorePkg@45e7636f24bb4a50b32baeb5170cdfc9f589ea3c`
（2026-10-19）。详见 `community-evidence-opencorepkg.md`。

四个站点：

| 站点 | 符号 | 版本门 | 字节改动 | 语义 |
| --- | --- | --- | --- | --- |
| V1 | `AppleUSBXHCIPCI::createPorts` | 10.11–10.13 | `FF FF 10` → `FF FF 40` | `cmp 0x10`→`0x40`，阈值 |
| V2 | `AppleUSBXHCI::createPorts` | 10.13+ | `0F 0F 83` → `40 0F 83` | `cmp 0x0F`→`0x40`，阈值 |
| IoP1 part 1 | `AppleUSBHostPort::setPortLocation(UInt32)`（26 起换无参符号） | 10.14.5+ | `0F 0F 87` → `40 0F 87` | `cmp 0x0F`→`0x40`，阈值 |
| IoP1 part 2 | 同一函数 | 11.1+ | `and ...,0F` → `and ...,3F` | 4 bit 掩码→6 bit |

结论：

- 这些补丁能解释“为什么 15→17 的对象能建出来”，不能解释“17 个对象的 locationID
  为什么不冲突”。
- 补丁之间不叠加检查：`Kernel > Quirks > XhciPortLimit` 一个布尔值会把三个 quirk
  全跑（`OpenCoreKernelPatch.c:135-139`）；自定义 `Kernel > Patch` 与 quirk 同时开
  会重复改同一字节，必须二选一。
- 没有 `createPort`（单端口）站点；没有 Hub 代码；没有 allocator 代码。
- 官方文档明确建议避免，并说唯一有效方案是限制到 15 口（`Configuration.tex:2998-3001`）。

## 3. USBToolBox 审计

详见 `community-evidence-usbtoolbox.md`。

- kext `README.md:19`：**不 patch 端口上限**。
- `USBToolBox.cpp:215-247`：删 `ports`/`port-count`，复制 `IOProviderMergeProperties`，
  没有任何内核函数改动。
- `tool/Scripts/base.py:604-616`：键名 = `HS0001`/`SS0001` 计数标签；
  `"port"` = 硬件号；`port-count` = 选中端口最大硬件号。
- `tool/macOS.py:104-112`：`index` 读端口对象的 `port` 属性，`location_id` 单独读。

因此：

- 稀疏 map 的正确做法是“少选端口 + 保留硬件号”，不是“改键名”；
- 想让硬件 16/17 出现，`"port"` 必须是 16/17，且 `port-count` ≥17；
- 能不能真的建出第 16/17 个对象，仍取决于 Apple 的计数检查；USBToolBox 不管这一层。

## 4. USBMap issue #63 与 Wate 补丁

详见 `community-evidence-usbmap-issue63.md`。

issue #63（2025-07-11，open，0 评论）给的四处改动：

1. `AppleUSBXHCI::createPortEPN`：`0F`→`3F`
2. `AppleUSBXHCI::createPorts`：`0F`→`40`
3. `setPortLocation` 的 `cmp`：`0F`→`40`
4. `setPortLocation` 的其余 `0F`：`3F`

局限：

- **数据类型**：`cmp`/`and` 都是 8 bit 立即数，最多把阈值写到 0x40、掩码写到
  0x3F；6 bit 端口域占 bit20–25，与控制器字节 bit24–31 重叠。
- **64 阈值**：`0x40` 是“拒绝 ≥64”，不是“64 个端口可区分”；掩码只有 6 bit。
- **bus-number**：配方完全不碰 `generateBusNumber`/`reserveBusNumber`；端口 16
  的 `0x01000000` 需要“字节 1 未被任何控制器占用”才不撞。
- **版本绑定**：搜索串含 `E9 6A010000`、`0F 83 7C080000` 这类带位移指令，
  换 build 即失配；按字节串搜索还可能误命中。
- **Hub**：完全没有处理；端口 16 下挂 Hub 时，子端口 1 会算回 `0x01100000`，
  与根端口 17 同值（encoding-audit §3 已静态推导）。

Wate 补丁仓库（`WaTeZhuangJiDingZhi/wate@a2ba2e0`）只有
`IOUSBHostFamily` 的 `0F`→`3F` 掩码补丁，寄存器硬编码（`e6`/`e5`），
Sonoma 条目 `Count=0`；README 自述“Sonoma 需要重新查找”。不能作为 15.8.1 候选。

## 5. 其他社区方案

| 方案 | 做法 | 对 >15 的作用 | 结论 |
| --- | --- | --- | --- |
| USBMap / USBToolBox 注入 | 写 `ports`/`port-count` 到控制器 | 只决定“建哪些、建几个”，不越过计数检查 | 稀疏 map 的工具链，不是上限补丁 |
| USBInjectAll | 硬编码大量 Intel 端口表 | 让所有候选口可见，便于挑选 | 老方案，不含上限补丁 |
| ACPI `SSDT-RHUB` / `SSDT-Xhci` | 重定义 RHUB/端口 `_UPC` | 解决 ACPI 端口定义缺失/错误 | 不改变 4 bit 端口域 |
| OpenCore `XhciPortLimit` | 阈值/掩码 | 能多建对象 | 不解决别名；官方建议避免 |
| issue #63 / Wate | 手工字节补丁 | 同阈值/掩码，外加 `createPortEPN` | 版本脆弱、无 allocator |

没有任何一个方案声称并证明“同一控制器 17 个对象、无 locationID 别名、跨版本可用”。

## 6. 苹果侧机制与公共接口

详见 `community-evidence-apple-abi.md`。

- 公开 `IOUSBHostControllerInterface` 能新建用户态宿主控制器，但：
  - 需要 `com.apple.developer.usb.host-controller-interface` entitlement；
  - 其 `PortCount`、`PortNumber` 字段都是 bit16–19（4 bit），端口事件端口号是
    bit0–3（4 bit）；单实例最多 15 口；
  - 它面向“远端 USB 设备 / 合成设备”，没有公开入口接管真实 xHCI。
- `locationID` 只有公开属性键，没有公开 setter。
- 每级 Hub 吃一个 4 bit 半字节；Apple 自己的 `AppleUSBHostPlatformProperties.kext`
  里存在 “Mac Internal Hub Properties” 这类 Hub 匹配项，说明内部设备走 Hub 是
  正常设计。
- 结论：**公开接口不支持把一条真实 xHCI 软件拆成多条逻辑控制器**。要做只能自研
  内核驱动独占硬件，再向前端暴露；这是独立研发项目。

## 7. 候选矩阵

排序按：稳定性 > 无 locationID 冲突 > 音频/USB 时序 > 更新维护 > 成本。
“证据级”= A（多源一手+本机实测）/ B（一手源码可证，未在本机跑）/ C（只有社区转述）。

| # | 路径 | 支持硬件 16/17 | 同时保留 XH00 全 17 对象 | locationID 冲突 | 稳定性证据 | 音频/USB 时序 | 更新维护 | 成本 | 定位 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A | 15 口原生基线 | 否（16/17 不建） | 否 | 无 | A：当前运行态 | 当前基线 | 无补丁 | 0 | 可用基线 |
| B | 稀疏 map（≤15 对象，含 16/17） | 是 | 否 | 无（逻辑 1..15 内） | A：2026-10-11 阶段 2A | 无补丁，时序不变 | 只维护 map | 低 | **下一实验/可回退基线** |
| C | 阈值补丁（OpenCore quirk 或本地两条 0x11） | 是 | 是（对象可建） | 高：16→`0x01000000`，与字节 1 控制器/自身基址重叠 | B/C：能建对象，但 2026-10-17 探针有 HS11 失败 2775 条、0x40 命令故障 2787 条，根因未分离 | 未验证；社区无数据 | 每版重推字节 | 低 | 实验，不可交付 |
| D | C + #63 全量配方（含 createPortEPN、0x3F） | 是 | 是（对象可建） | 同上；Hub 下仍会别名 | C：单一 issue，无复核、无实测公开 | 未验证 | 更脆弱（位移绑定） | 低 | 不推荐直接上 |
| E | 重写 locationID 编码 + 所有生成/消费点 | 理论是 | 理论是 | 可设计成无冲突 | C：无公开实现 | 需重验 | 跟随 OS 深度改动 | 高 | 独立研发 |
| F | 多逻辑控制器（自研内核拆分 / VHCI） | 每实例 ≤15，靠数量 | 目标可以 | 每实例 4 bit 内可无冲突 | C：VHCI 有 ABI，但无真实 xHCI 后端 | 需重验 | 高 | 很高 | 独立研发 |
| G | 控制器字节重排（`bus-number` 注入） | 辅助 | 辅助 | 只能挪开“字节 1”，不解决编码重叠 | B：本机运行态 XHC2=0x02 | 不直接相关 | 低 | 低 | C/D 的辅助，不是独立方案 |

补充说明：

- C 的“对象可建”不等于“可用”。本机 2026-10-17 的探针期故障（HS11 创建失败
  2775 条、0x40 命令故障 2787 条）不能被归因到 locationID 别名；encoding-audit §5
  明确说现有证据不能把 0x40 超时归因于位置编码，根因仍在“补丁 + HS11 设备/驱动 +
  端口电源状态”之间。
- B 的“无冲突”是因为对象数 ≤15，逻辑号不越过 bit23，不是因为修好了编码。
- G 的单点收益依赖“字节 1 空着”；删掉注入或枚举顺序变化就会回来（encoding-audit §4）。

## 8. 推荐

### 8.1 可用基线

A：保持当前 15 口原生基线。零补丁、零别名、更新最省事；代价是 16/17 不可用。

### 8.2 下一实验（如果目标是“关键物理口都要在”）

B：稀疏 map。按 `outputs/20261010-usbportlimit/review/encoding-audit-20261017.md` §8
的最小验证做：

- `XhciPortLimit=false`、两条自定义阈值补丁关闭、XHC2 注入不变；
- XH00 map 只含硬件 `{2,3,7,8,15,16,17}`，`port-count=17`；
- 预期 7 个对象，硬件 16/17 落在 `0x00600000/0x00700000`，蓝牙/G2000/麦克风在位；
- 这不是“全 17”，只是“≤15 对象内的端口取舍”，必须如实标注。

### 8.3 如果坚持“同一控制器 17 个对象”

先做两件事，再谈实现：

1. **离线命中审计**：对当前 15.8.1 的 `BootKernelExtensions.kc` 逐站点检查
   OpenCore 四个站点与 #63 第四个站点（`createPortEPN`）是否存在、命中几次、
   指令边界是否正确；`part 2` 在 15.8.1 上是否命中必须先证实（本机历史观察是
   `test al,0xf`，与 OpenCore 的 `and` 模式不同）。
2. **拓扑门槛**：确认 16/17 都是根端口、下面不挂 Hub；确认控制器字节 1 不被
   任何控制器占用；确认音频/HID/System Information 对 locationID 的消费方式。

门槛过不了就不进入 EFI 写入。满足门槛也只是“本机本版本可能可用”，不是通用方案。

### 8.4 不建议

- 不建议把 C/D 的“对象数变 17”当成“超 15 已解决”；
- 不建议用改字典键名的方式宣称重映射；
- 不建议在没有 Hub 实测的情况下宣称“全拓扑安全”；
- 不建议为这条线买额外硬件（本轮约束）。

## 9. 明确未知项

1. 15.8.1 上 `setPortLocation` 的 `and 0xF` 掩码站点是否存在、OpenCore part 2
   是否命中（历史观察与上游模式不一致，需离线复核）。
2. 15.8.1 是否存在 `createPortEPN`、#63 的第四个站点是否必要。
3. `setPortLocation(0)` 的确切调用点、逻辑序号存放的 ivar（本机只确认到 `+0xd0`）。
4. `UsbHostControllerTierLimit` 对扫描窗口的精确门控。
5. Hub 后代只有静态推导，没有硬件实测。
6. locationID 的非内核消费者（音频 UID、HID、虚拟机、第三方工具）未穷尽。
7. VHCI entitlement 的获取条件与真实 xHCI 后端的可行性，均无公开资料。
8. 2026-10-17 的 HS11/0x40 故障根因未分离；不能归因于位置编码。

## 10. 引用清单

全部为固定 commit / 固定文件：

- OpenCorePkg `45e7636f24bb4a50b32baeb5170cdfc9f589ea3c`
  - `Library/OcAppleKernelLib/CommonPatches.c:659-798`（四个站点）
  - `Library/OcAppleKernelLib/CommonPatches.c:881-899`（locationID TODO）
  - `Library/OcMainLib/OpenCoreKernelPatch.c:135-139`（一个开关跑三个 quirk）
  - `Docs/Configuration.tex:2991-3002`（官方警告）
- USBToolBox/kext `b9887b8dd86f3fdbb2300b2938a902ecbc436267`
  - `README.md:19`
  - `USBToolBox/USBToolBox.cpp:215-247`
- USBToolBox/tool `0c6823a2c643a4488888938451194723dc9ae29a`
  - `Scripts/base.py:604-616`
  - `macOS.py:104-112`
  - `README.md:29`
- corpnewt/USBMap `e07b6c0f19e8112887741bd486545d3c8411040e`
  - `Information.md:114,118-120,180-186,237`
  - issue #63（2025-07-11，open，0 评论）
- WaTeZhuangJiDingZhi/wate `a2ba2e0b0f2296883cb8638ab8ec5a049d1c387a`
  - `Wate-macOS-XhciPortLimit-Patch.plist`
- 本机 SDK（macOS 15.8.1 / MacOSX26.2.sdk）
  - `IOUSBHostControllerInterface.h:17-19`
  - `IOUSBHostControllerInterfaceDefinitions.h:264,283,420`
  - `IOUSBHostDefinitions.h:211`
- 本机系统文件
  - `AppleUSBVHCI.kext/Contents/Info.plist`
  - `AppleUSBHostPlatformProperties.kext/Contents/Info.plist`
- 本项目既有离线审计（机制对齐用，不是社区证据）
  - `outputs/20261010-usbportlimit/review/encoding-audit-20261017.md`
  - `outputs/20261010-usbportlimit/review/PLAN-sparse.md` 阶段 2A
  - `docs/../usb-port-limit-macos/docs/2026-10-17-full-port-stability-design.md`
