# 每控制器 15 口上限：机制、候选路径与推荐（2026-10-20）

状态：**只读/离线研究，未写 ESP、未改 KC、未加载/卸载 kext、未重启、未联系其他会话。**
基线：macOS 15.8.1 / 24H32；ESP `disk0s1`（备份 `3dad2596…`）；USB 两条 15→17 补丁 `Enabled=false`，
XH00 实际只建 15 口；最终目标按 G1 不预设“全 17”或“15 口”。

## 0. 结论先行

1. **15 是“每控制器最多 15 个端口对象”的硬上限，分布在两层计数检查上**，不是单一字节：
   `createPorts` 限制“成功创建数”，`setPortLocation` 限制“逻辑位置号”。放宽这两处只能让第 16/17 个对象
   **被创建**，不改变它们的位置编码。
2. **位置编码是 4 bit 端口域（bit20–23），端口号 ≥16 会进位到控制器字节（bit24–31）。**
   这是格式约束，不是实现 bug；任何“只改阈值/掩码”的方案都必须接受这个进位。
3. **本机 17 个硬件口中只有 7 个接到实际连接器**（2 个 USB3 口各 HS+SS，加内置蓝牙、后置 USB2、麦克风）。
   其余 10 个对象对应未接线的前置面板。
4. **在“稳定性 > 无冲突 > 音频/USB 时序 > 更新维护 > 成本”的排序下，最优解是稀疏 map（≤15 对象、
   零内核补丁）**：把硬件 16/17 放进逻辑 1..7，物理功能覆盖现有全部连接器，且不触碰 locationID 格式。
   代价是同一控制器不再保留 17 个对象。
5. **第二选择是“版本锁定的阈值补丁 + 固定拓扑”**：能创建 17 个对象，但 16/17 的 locationID 落在
   控制器字节 `b+1` 的空间，且 16 号口下挂 Hub 时子口 1 会与根 17 别名。历史 0x40 恢复风暴根因未分离，
   因此它只能是受控实验，不能当成交付。
6. **公开方案里没有任何“完整超 15”的实现**：OpenCorePkg 只有阈值/掩码/换符号四个站点，
   USBToolBox 明确不 patch 上限，issue #63 是手工字节配方且不含分配器与 Hub。

## 1. 15 口到底卡在哪几层

以下偏移均在当前 24H32 `BootKernelExtensions.kc` 上复核（KC sha256 `cd013c14…9af3`）。

| 层 | 机制 | 符号 / 偏移（本次复核） | 放宽后是否解决 |
| --- | --- | --- | --- |
| L1 创建计数 | 成功创建数 ≥15 就跳过后续硬件口 | `AppleUSBXHCI::createPorts` `0xffffff8002deaa58` `+1344 cmp ebx,0xf` / `jae`；`+1353 lea r13d,[rbx+1]` | 只让对象被创建 |
| L2 位置上限 | 逻辑位置号 >15 直接报错 | `AppleUSBHostPort::setPortLocation` `0xffffff8002cd638a` `+54 cmp r15d,0xf` / `ja` | 只让位置号通过 |
| L3 编码宽度 | 从 bit20 起向下找空半字节；结果 `父locationID \| (端口号<<cl)` | 同函数 `+838 mov ecx,0x14`、`+846 shr eax,cl`、`+848 test al,0xf`、`+856 add ecx,-4`、`+987 shl`、`+990 or` | **否**；≥16 必进位 |
| L4 控制器字节 | 候选字节 = `(function<<5)\|(device&0x1f)`，再经保留器递增 | `AppleUSBXHCIPCI::generateBusNumber` `0xffffff8002e2c754` `+26/+35`（getDeviceNumber &0x1f）、`+49/+55/+58`（getFunctionNumber <<5 \|）；`AppleUSBHostResources::reserveBusNumberGated` `0xffffff8002cc7648` | 只能挪开占用，不解决编码 |
| L5 Hub 层级 | 每级 Hub 再吃一个半字节 | 同 L3 的 `+838..+862` | **否**；会产生父子/兄弟别名 |
| L6 端口表遍历上界 | `port-count` 是循环上界；map 按**硬件号** `port` 查表 | `createPorts +665 mov eax,[r15+0x4c8]` / `+672 dec` / `+674 cmp r14,rax`；`getPortPropertyTable` `0xffffff8002e2a1c8` `+182` 取属性、`+229` 读 dword、`+231 cmp eax,[rbp-0x2c]` | 这是“选哪些口”的原生开关 |

两条容易混淆的事实，本次逐指令确认：

- `createPorts` 的 `0xF` 比的是**成功创建计数**（`ebx`），不是硬件端口号；
  硬件下标是 `r14`（`+1673 inc r14`），逻辑号是 `r13d = ebx+1`，成功后 `+1676 mov ebx,r13d`。
- `getPortPropertyTable` 用条目里的 `port` 值（硬件号）与请求的硬件端口号比较，
  所以 **map 的键名只是标签，重命名不构成重映射**。

### 1.1 ACPI 不是这条链的杠杆

`AppleUSBXHCIPCI::getAcpiPort` `0xffffff8002e2a0da` 开头 `+14 mov rdi,[rdi+0xf10]`，
该指针为空就返回 0。本机 XH00 在 ACPI 平面没有设备节点（layout spec `acpi_node_present=false`），
所以它拿到 0。更关键的是：**`generateBusNumber` 根本不调用 `getAcpiPort`**（上一行已逐指令确认），
`_UPC` 也不决定创建多少个端口对象——对象集合由 `port-count` + map 决定。

因此“改 ACPI `_UPC`/port 号来绕开 15 上限”对本机 XH00 不成立；它能改的是连接器描述/可用性，
不能把 17 个对象塞进 15 个位置，也没有 ACPI 节点可改。

## 2. 为什么 16/17 一定会“借”下一个控制器字节

`locationID = (控制器字节 << 24) | (端口号 << 20)`，端口域只有 4 bit，所以：

```text
端口 16 = (b+1) << 24 | 0x000000     ← 看起来就是“字节 b+1 的 0 号位”
端口 17 = (b+1) << 24 | 0x100000     ← 就是“字节 b+1 的 1 号口”
```

| 控制器字节 b | 端口 1 | 端口 15 | 端口 16 | 端口 17 |
| --- | --- | --- | --- | --- |
| `0x00`（XH00） | `0x00100000` | `0x00F00000` | `0x01000000` | `0x01100000` |
| `0x02`（XHC2 注入后） | `0x02100000` | `0x02F00000` | `0x03000000` | `0x03100000` |
| `0x60`（XHC0） | `0x60100000` | `0x60F00000` | `0x61000000` | `0x61100000` |

XH00 的 `b=0`，于是 16/17 落进**字节 1** 的空间。当前 XHC2 已被注入到 2，
字节 1 暂时空着，所以根端口层面能凑出“无同值”；但这是**拓扑巧合**，不是格式保证：

- 任何控制器拿到字节 1（PCI 候选恰好为 1，或注入变化），端口 17 立刻与它的 1 号口撞车；
  端口 16 会等于它的控制器 locationID。
- **硬件 16 下挂 Hub** 时，Hub 设备继承 `0x01000000`，其子口 1 算得 `0x01100000`，与根 17 完全同值。
  本机 16 号是板载蓝牙（直连，无 Hub），所以当前拓扑碰不到；但不能写成“Hub 后代安全”。
- 端口 16 的编码在数值上等于“某控制器的 0 号口”，消费者是否接受 0 号位**未验证**。

`reserveBusNumberGated` 只保证“已占则递增”，**不会预留空出来的 1**。删掉 XHC2 注入或枚举顺序变化，
冲突就会回来（历史上 2B 实验出现过编号翻转）。

## 3. 本机物理口盘点：17 个对象，7 个真实连接器

| 控制器 | 物理连接器 | 对象 |
| --- | --- | --- |
| XH00 | USB3 口 A | `#2`(SS) + `#7`(HS) |
| XH00 | USB3 口 B | `#3`(SS) + `#8`(HS) |
| XH00 | USB2 麦克风 | `#15` |
| XH00 | 内置蓝牙 | `#16` |
| XH00 | 后置 USB2 Type-A | `#17` |
| XHC0 | Type-A / Type-C | 4 个对象 |
| XHC1 | 两个 USB3 口 | 4 个对象 |
| XHC2 | 后置 USB2（G2000） | 1 个对象 |

XH00 的 `#1,#4,#5,#6,#9–#14` 共 10 个对象未引出（前置面板未接线）。
所以“7 条稀疏 map”不是砍功能，而是**恰好覆盖现有全部可用连接器**。

## 4. 候选路径与评分

评分按给定优先级做字典序比较，不用星级掩盖证据不足。

| # | 路径 | 稳定性 | locationID 冲突 | 音频/USB 时序 | 更新维护 | 成本 | 定位 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S | **稀疏 map**：≤15 条，含硬件 16/17，`port-count=17`，补丁全关 | 高：零内核补丁；2026-10-11 阶段 2A 实测 7 对象通过 | 无：逻辑 1..7，不越 bit23 | 无新增：对象更少、无越限路径 | 最好：只维护 map | 低 | **最优解 / 建议交付** |
| P | **阈值补丁**（上游 `XhciPortLimit` 或本机两条 0x11）+ 保留 XHC2=2 | 未知：2026-10-17 曾出现 HS11 创建失败 2775 条、0x40 命令故障 2787 条，根因未分离 | 根端口可做到无同值（依赖字节 1 空着）；16/17 越出 4 bit；16 号下挂 Hub 必与 17 别名 | 中–高：17 个对象全部上电/枚举，时序面变大 | 差：每版 KC 重推字节（上游分担一部分） | 低–中 | 第二选择，仅限固定拓扑受控实验 |
| W | 拓宽端口字段 + 重划分所有控制器基址 | 未知 | 设计上可消除 | 需重验 | 最差：跟随 OS 深度改动 | 高 | 独立研发，不建议近期 |
| A | 改 ACPI `_UPC` / port 号 | 不改变上限 | — | — | — | — | 对本机无效（XH00 无 ACPI 节点） |
| V | 一个真实 xHCI 拆成多个逻辑控制器 | 未知，需自研内核前端 | 设计上可 | 需重验 | 最差 | 极高 | 不推荐（等于写 xHCI 虚拟化） |

补充：

- USBToolBox / USBMap 是 **map 工具链**，不是上限补丁；`mergeProperties` 只搬 `ports`/`port-count`。
- OpenCore `XhciPortLimit` 一个布尔值会同时跑三个 quirk；与自定义 `Kernel > Patch` 同时开会对同一字节
  重复修改，必须二选一。
- `issue #63` 的 `0x40` 是“拒绝 ≥64”，不是“64 个口可区分”；掩码只有 6 bit，且与控制器字节重叠。

## 5. 推荐

### 5.1 最优解：稀疏 map（S）

- `XhciPortLimit=false`，两条自定义阈值补丁继续关闭；XHC2 `bus-number=2` 保持不动。
- XH00 `ports` 只保留硬件 `{2,3,7,8,15,16,17}`，`port-count` 仍为 **17**（遍历上界，不能写 7）。
- 预期 7 个对象，硬件 16/17 压到逻辑 6/7（`0x00600000` / `0x00700000`），其他控制器 4/4/1 不变。
- 满足“现有全部物理连接器可用”，且完全不触碰 4 bit 格式；后续 macOS 更新不需要重推内核字节。
- 明确边界：**同一控制器只保留 7 个对象**；若以后接前置面板，可在 ≤15 范围内重新选口，
  但 17 个对象不可能同时以原生语义存在。
- 实验污染提醒：这是**单变量配置**，但硬件 16 一出现，当前启用的 `SimpleMtkBtFw` 可能开始匹配、
  下固件或复位子系统。验证时必须同时记录 `kmutil showloaded`、附着情况与原始日志，
  不能把“配置没改驱动”当成“运行行为没变”，也不在本任务里停用他人驱动。

### 5.2 第二选择：受控的 17 对象实验（P）

只有在你明确要“17 个对象全部存在”时才做，且必须按实验对待：

1. 离线命中审计：对当前 KC 核对上游四个站点与本机两条 0x11，确认命中次数、指令边界、是否叠加。
   已实跑 `tools/research/patcheck.py`：`part 2`（`and 0xF→0x3F`）**0 处命中**，
   `setPortLocation` 全文只有 `+649 test al,0xf` 与 `+848 test al,0xf`，没有 `and reg,0x0F`；
   issue #63 的 `83 FB 0F` 在 `createPorts` 只命中 `+1344`（与阈值补丁同址，没有独立
   `createPortEPN` 站点）。即：15.8.1 上实际生效的仍只有两处阈值比较。
2. 阈值取值的取舍：上游 `XhciPortLimit` 把上限改成 `0x40`（放行 ≤63），本机两条改为 `0x11`（≤17）。
   前者随上游维护、后者暴露面更小；P 若走，需在“可维护性 vs 最小暴露”之间明确选择，不能两者都开。
3. 拓扑门槛：字节 `b+1` 必须保留；16 号不得挂 Hub；确认 16 号“0 号位”语义可被消费者接受。
4. 单变量窗口：向 `amd-boot-config`（thread `01a1579b…`）申请冻结窗口；R1 真机恢复未做前不写 ESP。
5. 验收按计划 §5.2：100 次插拔 + 30 分钟音频并发，`hardwareException`/`CommandRing`/`powerOn: deadline` 为 0；
   任何一次明确回归立即回滚并保留证据。

注意：现有独立仓库校验器对 16/17 判 `topology_model=FAIL` 是**设计约束的正确表达**，
不应该为了得到 PASS 去放宽模型；P 若要走，必须单独写一份“显式风险接受 + 固定拓扑假设”的模型，
而不是把 FAIL 改成 PASS。

### 5.3 不建议

- 不建议把“对象数变 17”当成“超 15 已解决”；
- 不建议同时开 `XhciPortLimit` 与自定义补丁；
- 不建议在没有 Hub 实测时宣称“全拓扑安全”；
- 不建议用改字典键名冒充端口重映射；
- 不建议为这条线增加硬件（本轮约束）。

## 6. 对旧结论的更正（重要）

| 旧说法 | 现状 | 证据 |
| --- | --- | --- |
| 控制器字节 = `(PCI设备号<<5)\|(acpiPort&0x1f)` | **不成立**。24H32 是 `(function<<5)\|(device&0x1f)`；`getAcpiPort` 不参与 `generateBusNumber` | `0xffffff8002e2c754` `+26 getDeviceNumber &0x1f`、`+49 getFunctionNumber <<5 \|`、`+86 jmp reserveBusNumberGated` |
| “只放宽检查”会算成 port16→`0x00000000` | **不成立**。那是模拟器先做了 `port & 0xF`；内核是 `shl/or`，16→`0x01000000`、17→`0x01100000` | `setPortLocation +987 shl / +990 or` |
| 共享目录 `tools/research/usbpatch-check.py` 结论 `PASS` | 那是**根端口静态模型**；独立仓库版对 16/17 报 `topology FAIL`。前者会把真实越界洗成 PASS | 两版对同一 KC 的不同 verdict |
| 稀疏 map 会“丢掉 10 个端口” | 对未接线对象是预期行为；7 条正好覆盖全部已引出连接器 | `docs/handoff-usb-portlimit-addendum.md` §22.4 实测表 |

## 7. 未验证 / 不得写成已通过

- `setPortLocation(0)` 的确切调用点与逻辑号 ivar（只确认到 `+0xd0`）。
- `UsbHostControllerTierLimit` 的精确门控。
- 16 号“0 号位”编码在音频/HID/System Information 等消费者中的行为。
- Hub 后代（无 Hub 实机）；2026-10-17 的 0x40 风暴根因；睡眠/唤醒。

## 8. 复现命令

```sh
# 机制：补丁命中 + 根端口拓扑（独立仓库版，结论为 topology FAIL 属预期）
cd /Users/you/Documents/usb-port-limit-macos
PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 tools/usbpatch-check.py \
  --spec tools/specs/portlimit-24H32.json \
  --layout tools/specs/layout-msb850bkb.json

# 运行态：声明/创建/硬件集合、位置与冲突（只读）
PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 tools/post-update-check.py --profile full17 --json
```

## 9. 参考

- 本机 KC 反汇编：`outputs/20261010-usbportlimit/review/encoding-audit-20261017.md`
- 事故时间线：`outputs/20261010-usbportlimit/review/incident-20261017.md`
- 消费者审计：`outputs/20261010-usbportlimit/review/location-consumer-audit-20261017.md`
- 社区一手源：`outputs/20261020-amd-usb-ports/community-paths.md` 及 `community-evidence-*.md`
- 计划：`docs/plans/2026-10-19-amd-adaptation-program.md` §5.2 / §6.2
- 基线：`outputs/20261020-amd-boot-config/BASELINE.md`

