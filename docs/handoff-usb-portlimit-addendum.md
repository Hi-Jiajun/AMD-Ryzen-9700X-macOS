# 补充：USB 15 端口上限 —— 复核结果与方案建议（2026-10-10）

> 本文件是 `docs/handoff-usb-portlimit.md` 的**补充**，不覆盖原文。
> 全部结论来自本机只读证据：macOS 15.8.1 (24H32) 的
> `/System/Library/KernelCollections/BootKernelExtensions.kc`（capstone 按符号逐函数反汇编）、
> `ioreg`、以及工作区内的 ESP 副本 `efi/EFI/OC/`。
> 反汇编脚本在 `/tmp/usbres/`（临时目录，未落进仓库；如需长期复用，再把校验器建到 `tools/research/`）。

## 0. 结论摘要

| 项 | 结论 |
| --- | --- |
| 可行性 | **中**。必须动内核（引导时内存补丁），但补丁点只有 **2 处**，字节与命中数都已确证 |
| 推荐路线 | 先上**最小两处补丁**，上机看现象；若 locationID 撞车引发问题，再叠加"挪开 XHC2 编号" |
| 唯一确定副作用 | XH00 的 port 16/17 会占用 `0x01000000`/`0x01100000`，**与 XHC2 的编号空间完全重叠**。冲突确定，**实际危害未验证**（不同控制器子树、仅属性层面同值） |
| 零风险替代 | PCIe USB 卡（每控制器独立 15 个额度） |

## 1. 对交接文档的三处更正（都有反汇编证据）

### 1.1 `setPortLocation` 的真实算法 ≠ "端口号固定在 bits 20–23"

`AppleUSBHostPort::setPortLocation(unsigned int)` @ `0xffffff8002cd638a`（size 1236）：

```
+ 37  test esi,esi / +41 mov r15d,[rbx+0xd0]   ; 参数为 0 时取对象里存的端口号
+ 54  cmp r15d,0xf                              ; ← 端口号上限（第 1 处硬限制）
+ 58  ja  → +1197（错误返回 0xe00002c2）
+171  mov [rbp-0x54],eax    ← 读父对象的 'locationID' 属性 = 基址
+838  mov ecx,0x14          ; 从 bit20 开始
+846  shr eax,cl / +848 test al,0xf   ; 检查该半字节是否"空"
+850  je → +987
+856  add ecx,-4 / +859 cmp ecx,-4 / +862 jne  ; 空则用；不空就往下退一个半字节
+987  shl r15d,cl / +990 or r12d,r15d  ; 端口号 << cl，OR 进基址
+1020 mov [rbx+0xb4],r12d
```

**即：端口号不是被"掩码截断"，而是"或者落在第一个空半字节、或者溢出到上面的半字节去"。**
对根端口基址 = 控制器字节 << 24，所以第一个空半字节就是 bit20 → 仍然是"4 bit 装端口号"。

⇒ 交接文档 §3 里"part-2 放宽掩码"这一条**根本不需要**（代码里没有 `and eax,0xf`）；
文档里 B 情形"17 被截断成 1"是模拟器的建模结果，真实代码是**溢出**到 `0x01100000` 与 XHC2 撞车。

### 1.2 端口号来自"硬件索引+1"，端口表（`ports` 字典）**不能重编号**

`AppleUSBXHCI::createPorts()` @ `0xffffff8002deaa58`（size 4338）的端口循环：

```
+1344  cmp ebx,0xf                 ; ← 端口上限（第 2 处硬限制）；ebx = 端口索引
+1347  jae → +3303                  ; 跳过该端口；+3370 有 `add r9d,-0xf`
                                    ;   （r9d = [+0x4c8] = 硬件端口数 → 日志打印"少建了 N-15 个"）
+1353  lea r13d,[rbx+1]             ; 端口号 = 索引 + 1
+1374  call [rax+0xbe0]             ; 虚调用 createPort(cap, ?, 索引+1)
```

`AppleUSBXHCIPCI::getPortPropertyTable(unsigned)` @ `0xffffff8002e2a1c8`：

```
+ 23  getProperty('ports') → OSDictionary
+ 64  迭代每个条目
+182  条目里取 'port' 值（OSData）
+231  cmp eax, [rbp-0x2c]      ; ← 与"被请求的端口号"比较
+234  je  → 返回该条目
```

⇒ `ports` 字典是**按编号反查**的（回答"第 N 号端口叫什么名字、什么连接器类型"），
**编号由调用方给定，字典改不了编号**。所谓的 `usb-port-number` 重编号路线不成立。

`AppleUSBXHCIPCI::createPortPropertyTable(IOACPIPlatformDevice*, j)` @ `0xffffff8002e2a336`：
`+17 mov [rbp-0x34],edx` —— 写进字典的 `port` 值就是**函数的第 2 个参数**，
ACPI 只提供 `_UPC`（UsbConnectable / UsbConnector）和 `name`。

### 1.3 `0xF` 上限在整份 KC 里只有 2 处（全 `AppleUSB*`/`IOUSBHost*` 符号扫描）

| 位置 | 字节 | 反汇编 | 说明 |
| --- | --- | --- | --- |
| `AppleUSBHostPort::setPortLocation+54` | `41 83 ff 0f 0f 87` | `cmp r15d,0xf` ; `ja` | 端口号上限；对应 OpenCore part-1（`0F 0F 87`→`0F 40 87` 仍命中） |
| `AppleUSBXHCI::createPorts+1344` | `83 fb 0f 0f 83` | `cmp ebx,0xf` ; `jae` | 端口条数上限；**part-2 不需要**，需要这一条 |

（扫描同时命中的其他 `cmp …,0xf` 均与端口上限无关：Hub 端口状态、EHCI/VHCI、Endpoint 属性等。）

### 1.4 macOS 15.8.1 根本不读 `usb-port-number` / `usb-port-type`

`BootKernelExtensions.kc` 与 `SystemKernelExtensions.kc` 里都**搜不到**这两个字符串。
代码实际引用的属性名是：`port-count`、`ports`、`port`、`UsbConnector`、`UsbConnectable`、
`UsbCPortNumber`、`UsbPowerSource`、`locationID`、`UsbHostControllerTierLimit`、
`AAPL,ExternalConnectorBitmap`、`_UPC`、`built-in`、`device-properties`。

### 1.5 本机已经在跑 USBToolBox（交接文档漏掉的事实）

- `kmutil showloaded`：`com.dhinakg.USBToolBox.kext (1.2.0)` **已加载**（ESP 里是 `USBToolBox.kext` + `UTBDefault.kext`）。
- ioreg ACPI 平面：XHC0/XHC1/XHC2 节点带 `"ACPI removed by USBToolBox" = Yes`。
- **XH00 在 ACPI 平面里没有设备节点** → 无 `ports` 字典 → 走"索引+1"默认编号，15 个端口
  （`locationID` = `0x00100000`…`0x00F00000`，正是本机实测）。
- XHC0/XHC1/XHC2 有 `"ports"` 属性（`{"HS01"={"UsbConnector"=3,"port"=<01000000>}, …}`）和 `port-count`。

## 2. 为什么"必须改内核"是硬结论

要让 port 16/17 可用，需要同时满足：

1. **端口对象被创建**：`createPorts` 的 `cmp ebx,0xf` 必须放宽（否则索引 ≥15 的端口根本不建）。
2. **端口号能通过 `setPortLocation` 的 `cmp r15d,0xf`**：端口号 = 索引+1 = 16/17，超限即报错。
3. **locationID 唯一**：4 bit 端口域只能装 15 个（0 被占用）；本机 XH00 控制器字节是 `0x00`，
   所以 16/17 必然溢出到 `0x01000000`/`0x01100000` —— 也就是 XHC2 的基址和 XHC2 port 1。

第 3 条决定了：**"只放宽检查"和"放宽检查+放宽掩码"两种公开配方在本机都必然撞车**（与 §5 模拟器一致）。

## 3. 方案对比

| 方案 | 改动 | 冲突 | 风险 | 评价 |
| --- | --- | --- | --- | --- |
| **A. 两处补丁（推荐先做）** | 2 条 `Kernel > Patch` | 有（vs XHC2） | 中（内存补丁、逐版本维护） | 最小必要集；字节已确证，回滚=删条目 |
| B. 两处补丁 + 挪开 XHC2 编号 | 2 条 Patch + 1 处 ACPI/属性 | 无 | 中高（依赖 acpiPort 是否可控，未验证） | 冲突出现症状时再做 |
| C. 只改 ACPI / 端口映射 | 0 条 Patch | — | 低 | **不可行**：编号由硬件索引+1 决定，字典不能重编号 |
| D. PCIe USB 卡 | 插卡 | 无 | 低 | 零风险替代，功能等价 |
| E. Lilu 插件在内存里做同样的两处改动 | 1 个 kext | 有（同 A） | 中 | 比 A 多写代码，无额外收益 |

## 4. 推荐路线（分步，每步都有验收点）

### 第 1 步（离线，不碰引导）：把校验器做出来

- 输入：候选补丁（Find/Replace/Base/Count）+ 机器布局文件（各控制器字节、端口数）。
- 输出：命中位置与改动字节、反汇编新旧对照、locationID 冲突数、`VERDICT: PASS/FAIL`。
- 先用它把方案 A 的两条补丁跑一遍，确认"命中 1 处 / 落在指定符号内 / 改动字节如预期"。

### 第 2 步：上机只加两条补丁

```xml
<!-- A: 端口号上限 -->
Identifier : com.apple.iokit.IOUSBHostFamily
Base       : __ZN16AppleUSBHostPort15setPortLocationEj
Find       : 41 83 FF 0F 0F 87
Replace    : 41 83 FF 40 0F 87
Count      : 1

<!-- B: 端口条数上限 -->
Identifier : com.apple.driver.usb.AppleUSBXHCI
Base       : __ZN12AppleUSBXHCI11createPortsEv
Find       : 83 FB 0F 0F 83
Replace    : 83 FB 40 0F 83
Count      : 1
```

注意：**保持 `Kernel > Quirks > XhciPortLimit = false`**（否则 part-1 会与补丁 A 重复作用）。
改前备份 `config.plist`，回滚 = 把两条条目的 `Enabled` 改回 0。

验收（开机后只读观察）：

1. `ioreg -p IOService -c AppleUSBXHCIPCI` 里 XH00 是否变成 17 个端口对象、出现 `0x01000000`/`0x01100000`；
2. 内置蓝牙（`0e8d:0616`）是否出现并可用；
3. 后置 USB2 Type-A（port 17）插设备是否识别、`system_profiler SPUSBDataType` 里的 Location ID 是否正常；
4. 其余端口（XH00 的 2/3/7/8/15、XHC0/1/2）是否照旧。

### 第 3 步（仅在出现症状时）：消掉与 XHC2 的编号重叠

两条路线，按代价排序：

1. **搬走 XHC2**（改它的控制器字节，从 `0x01` 到 `0x02`+，让 `0x01000000`/`0x01100000` 空出来）；
   前提是先查清控制器字节低 5 位的来源（见 §5）。
2. **禁用 XHC2**（若它那 1 个端口本机不用）：用 `DeviceProperties`/SSDT 让驱动不匹配该 PCI 设备。

## 5. 未决问题（都已定位到"怎么离线查清"）

| 问题 | 现状 | 离线验证方法 |
| --- | --- | --- |
| 控制器字节低 5 位（交接文档称 `acpiPort`）到底来自哪个 ACPI 字段、能否改 | 已知：`AppleUSBXHCIPCI::generateBusNumber` @ `0xffffff8002e2c754` `+26 call [rax+0x8f0]` → `+35 and r15b,0x1f`；XHC0/XHC1/XHC2 的该值为 0/0/1，XH00 无 ACPI 节点故为 0 | 反汇编 `+26` 那个虚槽位（在 IOACPIPlatformDevice 上的实现），确认读的是 `_UPC`/`_UID`/unit-address 中的哪一个；再判断 SSDT/DeviceProperties 能否注入 |
| 控制器 `locationID` 属性能否被注入覆盖 | `setPortLocation+171` 读的是父对象的 `locationID` **属性** | 找 KC 里 `setProperty("locationID", …)` 的写入点，看是"无条件写"还是"缺失才写"；后者则可注入 |
| XH00 的硬件端口数是否真是 17 | 由 Windows 侧实测 17 + macOS 建 15 + 内核日志"少建 N-15"推断 | 开 USB 调试日志（boot-arg）后 `log show` 抓 `createPorts` 的 "max ports %d"，或上机后数 ioreg 端口对象 |

## 6. 坐标速查（macOS 15.8.1 / 24H32）

| 符号 | 地址 | 关键偏移 |
| --- | --- | --- |
| `AppleUSBHostPort::setPortLocation(uint)` | `0xffffff8002cd638a` | +54 上限；+171 读 `locationID` 属性；+184 读 `UsbHostControllerTierLimit`；+838/+848 找空半字节；+987/+990 写入；+1020 落属性 |
| `AppleUSBXHCI::createPorts()` | `0xffffff8002deaa58` | +20 `[+0x584]>>24` → 端口数；+1344 条数上限；+1353 端口号=索引+1 |
| `AppleUSBXHCI::createPort(...)` | `0xffffff8002debb4a` | 无上限比较；读 `UsbConnector`/`AAPL,ExternalConnectorBitmap` |
| `AppleUSBXHCIPCI::createPort(...)` | `0xffffff8002e2a808` | +33 读 `ports`；+146 `device-properties`；+195 `UsbCPortNumber` |
| `AppleUSBXHCIPCI::getPortPropertyTable(uint)` | `0xffffff8002e2a1c8` | +231 按 `port` 值反查（**编号不可改的证据**） |
| `AppleUSBXHCIPCI::createPortPropertyTable(ACPI*, uint)` | `0xffffff8002e2a336` | +17 `port` = 第 2 参数；+53/+122 `_UPC`；+546 UsbConnectable；+569 UsbConnector；+625 name |
| `AppleUSBXHCIPCI::getAcpiPort(uint)` | `0xffffff8002e2a0da` | 遍历 ACPI 子节点，按 `_UPC` 匹配 |
| `AppleUSBXHCIPCI::generateBusNumber(uchar&)` | `0xffffff8002e2c754` | +26 取低 5 位；+55 `shl al,5`；+58 `or al,r15b`；+61 写字节 |
| `AppleUSBHostController::generateBusNumber(uchar&)` | `0xffffff8002cd1cca` | +196 `shl edx,0x18` → `[+0x4b4]`；+292 尾调用 |
| `AppleUSBHostController::createPorts()` | `0xffffff8002cd1ea8` | 读 `port-count` → `[+0x4c8]` |

## 7. 本机布局（ioreg 实测，供模拟器与校验器使用）

| 控制器 | PCI | 控制器字节 | `locationID` 基址 | 端口数 | 备注 |
| --- | --- | --- | --- | --- | --- |
| XH00 | `1022:43fc` | `0x00` | `0x00000000` | 17（macOS 只建 15） | 无 ACPI 节点、无 `ports`；port 16=内置蓝牙、port 17=后置 USB2 Type-A |
| XHC0 | — | `0x60` | `0x60000000` | 4 | `port-count`=4，`ports` 有 HS01/HS02/SS01/SS02 |
| XHC1 | — | `0x80` | `0x80000000` | 4 | 同上 |
| XHC2 | — | `0x01` | `0x01000000` | 1 | `ports` 只有 HS01；**与 XH00 port 16/17 撞车的来源** |

## 8. 第二轮复核（2026-10-10）：原生路线的可行性边界

### 8.1 `getAcpiPort` 读的是 ACPI 子树，不是控制器的单个字段

`AppleUSBXHCIPCI::getAcpiPort(unsigned)` @ `0xffffff8002e2a0da` 实际调用的是
`IORegistryIterator::iterateOver(entry, plane, 1)`（`0xffffff8000ac1b80`）——
**遍历该控制器 ACPI 设备在 ACPI 平面下的子节点**，对每个子节点：

```
+ 14  rdi = [this+0xf10]           ; 控制器关联的 IOACPIPlatformDevice
+ 21  test rdi,rdi / je fail       ; 没有 ACPI 设备 → 直接返回 0
+ 48  IORegistryIterator::iterateOver(acpiDevice, plane, 1)
+ 71  getNextObject → safeMetaCast
+145  strtoul(<子节点上取到的字符串>, 0, 16)   ; 按 16 进制解析
+150  cmp rax, <入参> / jne 下一个
+155  hasProperty('_UPC')
```

⇒ 这是**逐端口的 `_UPC` ACPI 机制**（Apple 自家硬件声明端口的方式），服务的是"这个口是什么"，
**不是**"控制器编号是多少"。

### 8.2 XH00 的字节是 0，是因为它压根没有 ACPI 设备节点

- `ioreg -p IOACPIPlane` 实测：XHC0@3、XHC1@4、XHC2@0 三个节点存在；
  **GPP7 子树在运行时只有 `GPP7@20001` + `D010@ff`，没有 UP00 / DP38 / DP60 / XH00**。
- 固件（Windows 侧 Probe）声明的是 `\_SB.PCI0.GPP7.UP00.DP60.XH00`，与运行时不一致。
- 推论：`[this+0xf10]` 为 NULL → `getAcpiPort` 立即返回 0 → 字节 = `(0<<5)|0` = `0x00`。

⇒ 想用 ACPI 改 XH00 的字节，**不是"设一个字段"，而是要给它补出 ACPI 设备节点（可能连同端口子节点）**，
比原判断的工作量和不确定性都大。本机已有同类先例：`SSDT-Disable_Network_GPP7.aml` 的作用域就是
`_SB.PCI0.GPP7.UP00.DP38.EP00`（说明往这个子树里插 SSDT 是既有做法；但该 SSDT 在运行时是否真正生效，
需另查——运行时 ACPI 平面看不到 UP00/DP38）。

### 8.3 `locationID` 是驱动自己写的属性

`AppleUSBHostController::generateBusNumber(unsigned char&)` @ `0xffffff8002cd1cca`：

```
+196  shl edx,0x18            ; 字节 << 24
+203  mov [rbx+0x4b4],edx      ; 存成 ivar
+212  'locationID'             ; 属性名
+261  '%08x' / +278 snprintf   ; 格式化成 8 位十六进制
```

⇒ 控制器级 `locationID` 由驱动写入（ioreg 实测值 = 公式值，吻合）。
**能否用 DeviceProperties 注入覆盖，取决于该写入是否无条件** —— 这一条尚未定论，
但它是"零 DSDT 手术"的候选路线，值得用一次开机实验当判据（注入了就是注入赢，没注入就是驱动赢）。

### 8.4 关键结论：SSDT 不能替代那两条补丁

两处上限都是**写死的常量比较**，与 ACPI 无关：

- `setPortLocation` `+54 cmp r15d,0xf` / `ja`（严格大于 → 放行 ≤15）
- `createPorts` `+1344 cmp ebx,0xf` / `jae`（≥ → 放行索引 0..14）

⇒ 无论 ACPI 怎么描述端口，**这两处必须改**。ACPI/SSDT 只能用于"消冲突"（改控制器字节），
属于第二阶段的可选项。

### 8.5 取值收紧（按"最小偏离"原则）

| 位置 | 原字节 | 改为 | 语义 |
| --- | --- | --- | --- |
| `setPortLocation+54` `cmp r15d,0xf` | `41 83 FF 0F` | `41 83 FF 11` | `ja`：放行端口号 0..17（原 0..15） |
| `createPorts+1344` `cmp ebx,0xf` | `83 FB 0F` | `83 FB 11` | `jae`：放行索引 0..16 = 17 个端口（原 0..14） |

两者必须**成对**修改：只改 B → 端口对象建出来但 `setPortLocation` 拒绝，即 Dortania 描述的
`failed to create device` 现象；只改 A → 端口对象根本不建。

副作用面：`0x11` 只放行到 17，`0x40`（社区惯例）会让 `createPorts` 去尝试创建 18..64 号端口
（本机硬件不存在）——多出来的只是失败尝试与日志噪声，故 `0x11` 更贴合、暴露面更小。
代价：**没有余量**，若将来需要 >17 号端口需重新取值。

## 9. 2026-10-10 12:40：补丁已落地 ESP

**工具**（新增，都在 `tools/research/`）：

| 文件 | 用途 |
| --- | --- |
| `usbpatch-check.py` | 补丁校验器：KC 副本上验命中/改动字节/反汇编对照/冲突数，输出 `VERDICT` |
| `specs/portlimit-24H32.json` | 本机候选补丁规格（含"预期冲突"声明） |
| `specs/layout-msb850bkb.json` | 本机控制器布局（实测 ioreg 数据） |

跑法：`python3 tools/research/usbpatch-check.py --spec specs/portlimit-24H32.json --layout specs/layout-msb850bkb.json`

**校验器输出（本机 15.8.1/24H32 实测）**：

```
[A] 命中 1 处 @ AppleUSBHostPort::setPortLocation+54   41 83 ff 0f 0f 87 → 41 83 ff 11 0f 87
[B] 命中 1 处 @ AppleUSBXHCI::createPorts+1344          83 fb 0f 0f 83 → 83 fb 11 0f 83
    改前: cmp r15d,0xf ; ja    /  cmp ebx,0xf ; jae
    改后: cmp r15d,0x11 ; ja   /  cmp ebx,0x11 ; jae      → 生效上限 17
端口撞车 1 处 (预期 0x1100000): XH00 port17 == XHC2 port1
VERDICT: PASS        系统 KC 未被改动 ✓
```

**落地信息**：

| 项 | 值 |
| --- | --- |
| 目标 | `disk0s1` / `/Volumes/OPENCORE`:`EFI/OC/config.plist` |
| 改动前 sha256 | `1848af45dbbb7d3ac3a69f5c92f70cad6795341a294f37a0fba842ec7a0a3fa0`（47175 B） |
| 改动后 sha256 | `c80604f159ebc1babdcbdfa2745cf1014a174a12e97b4ca044d251b426947afd` |
| 变更 | `Kernel > Patch` 追加 2 条（**删除 0 行 / 新增 72 行**，纯增量）：27 → 29 |
| 回滚备份（ESP 上） | `config.plist.hl-bak-before-20261010-124027`（sha 同改动前） |
| 回滚备份（本仓库） | `outputs/20261010-usbportlimit/config.base-live-124008.plist` |
| 未动 | `XhciPortLimit = False` 保持；并行 WiFi 会话的 3 个 kext 与 `Kernel > Block` 条目完好 |

**校验**：`plutil -lint` OK；plistlib 回读 29 条；与 spec 逐字节一致；按 `ocvalidate` 对
`Kernel > Patch` 的全部规则（Find/Replace 等长非空、Mask 尺寸、Identifier、整数域、Arch、
Min/Max 顺序）逐条检查 → **0 错误 0 警告**。（本机没有 `ocvalidate` 二进制，改用等价规则自查。）

**回滚方法（一句话）**：把 ESP 上 `EFI/OC/config.plist` 换成
`config.plist.hl-bak-before-20261010-124027`，或把那两条 `Comment` 以 `hl | USB port limit`
开头的条目 `Enabled` 改成 `false`。

### ⚠️ 落地时的并发情况（必须知道）

落地时**另一个会话正在同一 ESP 上做 MT7922 WiFi 驱动安装**：我挂载前 41 秒（12:38:54）
`config.plist` 刚被它改过，它还建了 `tools/backup-mtk-efi-20261010-123854/`、往 `Kernel > Add`
加了 `IOSkywalkFamily` / `IO80211FamilyLegacy` / `AirportMtk`。处置方式：

1. 以**该会话 12:38 的最新版本**为基线（不是仓库里 10-07 的旧副本）；
2. 只做**追加**，不重写任何既有条目；
3. 写前再核一次 sha，不一致就中止；
4. 写完比对 diff，确认**删除 0 行**；
5. 事后卸载 ESP，恢复到我发现它时的状态。

**遗留风险**：两个会话的改动会在**同一次重启**里一起生效，验收时无法把"端口"和"WiFi"的
现象完全分开归因；另外若那个会话在之后覆盖 `config.plist`，本次两条补丁会被静默抹掉 ——
重跑校验器 + 检查 `Comment` 里有没有 `hl | USB port limit` 即可发现。

## 10. 2026-10-10 12:48：重启后实测 —— **成功，且撞车无害**

开机 6 分钟后实测（macOS 15.8.1 / 24H32）：

### 10.1 结果：17 个端口全部建出来，两个新口都能用

```
AppleUSBXHCIPCI@00000000  (XH00)   ← 原 15 个端口，现 17 个
  ├─ AppleUSB30XHCIPort@00100000 .. @00500000   (5×USB3)
  ├─ AppleUSB20XHCIPort@00600000 .. @00f00000   (10×USB2，port 6..15)
  ├─ AppleUSB20XHCIPort@01000000 → Wireless_Device     ← 新增 port 16
  └─ AppleUSB20XHCIPort@01100000 → HECATE G2000        ← 新增 port 17
```

| 端口 | 实际设备 | 状态 |
| --- | --- | --- |
| port 15 (`0x00F00000`) | Samson C01U Pro Mic | 照旧（未受影响） |
| **port 16 (`0x01000000`)** | `Wireless_Device` `0e8d:0616`（MT7922 蓝牙） | ✅ **新出现**；`BT_ACL_If` / `BT_SCO_If` / `BT_ISO_If` 三个接口已 `registered, matched` |
| **port 17 (`0x01100000`)** | `HECATE G2000` `2d99:a002` | ✅ **新出现**（就是"以前插 G2000 那个口"）；音频 + HID 都能认 |

端口的 `port` 属性实测 = `<10000000>` / `<11000000>`，即**端口号真的编成了 16/17**，
不是靠掩码截断出来的假象。

### 10.2 撞车确认存在，但**对功能无影响**（关键结论）

```
AppleUSBXHCIPCI@01000000 (XHC2)
  └─ HS01@01100000 → MCHOSE G87 V2  （键盘）
```

`XH00 port 17` 与 `XHC2 HS01` **同 locationID `0x01100000`**（如预测），但：

- 两者是**不同控制器子树下的独立对象**（`id 0x100000320` vs `id 0x100000353`）；
- 两个设备**各自正常工作**：键盘 `port-status=4864` 且 HID 报文字段有持续计数，
  G2000 的音频与 HID 接口都 `matched`；
- 内核日志无 `AppleUSBHostPort` / XHCI 报错，无 panic。

⇒ **"不影响其他功能"这一条成立**（就现有证据而言）。

其他控制器未受影响：XHC0 4 口、XHC1 4 口、XHC2 1 口，均与补丁前一致。
并行会话的 WiFi 三件套（`IOSkywalkFamily` / `IO80211FamilyLegacy` / `local.hiliang.AirportMtk 0.1.0`）
正常加载，`en0` UP/active。`/Library/Logs/DiagnosticReports/shutdown_stall_*` 自 10-07 起共 18 份、
属**既有复发问题**，与本次补丁无关。

### 10.3 遗留缺口（可顺手补上）：XH00 的端口**没有连接器类型**

实测：XH00 控制器节点上**没有 `ports` 字典、也没有 `port-count`**，所以它 17 个端口
（含新增的两个）**都没有 `UsbConnector`、也没有名字**（其余三个控制器都有）。

**根因**：`UTBMap.kext` 里**本来就有正确的 XH00 表**（`port-count = 17`）：

| port | 名称 | UsbConnector |
| --- | --- | --- |
| 2 / 3 | SS01 / SS02 | 3（Type-A） |
| 7 / 8 | HS01 / HS02 | 3（Type-A） |
| 15 | HS03 | 0 |
| **16** | **HS04** | **255（Internal）** ← 蓝牙，正确的原生标注 |
| 17 | HS05 | 0 |

但它的匹配条件是 `IONameMatch = XH00`，而 **XH00 在 ACPI 平面里没有设备节点**
（就是 §8.2 那个根因），所以这条 personality **从未匹配上**，表一直没生效
（交接文档里"XH00 的 port 16/17 是死条目"说的其实是这件事）。

**改法（一行，可回滚）**：把 `UTBMap.kext/Contents/Info.plist` 里 XH00 那条 personality 的
`IONameMatch = XH00` 换成 `IOPCIPrimaryMatch = 0x43fc1022`（`IOProviderClass` 本来就是
`IOPCIDevice`，父节点实测就是 `pci1022,43fc@0`）。收益：蓝牙口被标成 Internal(255)，
17 个端口都有正确的类型与名字。

### 10.4 蓝牙的现状（供另一条会话参考）

- **USB 侧已完成**：设备枚举、三个蓝牙接口都 `matched`。
- **蓝牙协议栈未接管**：`system_profiler SPBluetoothDataType` 仍是占位
  （`Chipset: BCM_4350C2 / Address: NULL / State: Off / Transport: UART`），
  `IOBluetoothHCIController` 下挂的是 `IOBluetoothACPIMethods` 而不是 USB 传输 ——
  即**需要 MT7922 的蓝牙驱动**，与本补丁无关。

## 11. 2026-10-10 12:55：修好 UTBMap 的 XH00 匹配（端口类型/名字）

### 11.1 `UsbConnector` 数值的权威语义（反汇编）

`AppleUSBXHCIPort::getPortType` @ `0xffffff8002dfffe8`：

```
+ 13  取属性 'port-type'
+ 50  缺失 → 跳到 +107
+102  [port-type]==5 → 返回 2
+110  取属性 'UsbConnector'
+147  缺失 → 走硬件位分支（+171：[+0xa88] >> 30 & 1，×2 → 0 或 2）
+161  cmp eax,0xff ; sete al          → 只有 0xFF 被特殊对待
```

⇒ **端口类型只区分 `0xFF`（Internal）**；`0` / `3` / `9` 在「端口类型」这个维度上等价，
都算标准外部口。数值 3 / 9 的差别只体现在 Type-A / Type-C 的上报描述上。
（旁证：`AppleUSBEHCIPort::getPortType` 只区分 `2`→ExpressCard 与 `0xFF`→Internal。）

### 11.2 匹配不上的根因与改法

`UTBMap.kext` 的 XH00 personality 用 `IONameMatch = XH00`，而 XH00 **没有 ACPI 设备节点**
（§8.2），其 `IOName` 实测是 `pci1022,43fc`，所以这条 personality 从未匹配。

**改动**（`EFI/OC/Kexts/UTBMap.kext/Contents/Info.plist`，只动 2 行）：

```diff
-      <key>IONameMatch</key>
-      <string>XH00</string>
+      <key>IOPCIPrimaryMatch</key>
+      <string>0x43fc1022</string>
```

（格式 `0x<device><vendor>`，与仓库文档里 `0x061614c3` = `14c3:0616` 一致；
PCI 设备实测 `vendor-id=0x1022`、`device-id=0x43fc`。）

选 `IOPCIPrimaryMatch` 而不是把 `IONameMatch` 的值改成 `pci1022,43fc`：后者虽然更小，
但将来若给 XH00 补了 ACPI 节点（§8 那件事），`IOName` 会变成 `XH00` 而再次失配。

| 项 | 值 |
| --- | --- |
| 改动前 sha256 | `0cd3a353191efb0f71710990b64b8a650f9aefbd04e887e59a8a56dbca2c004e` |
| 改动后 sha256 | `7f8059ffa8153c17e3317954d596386c43ba282b1d9243fc2cc02a63fec52bcb` |
| diff | 仅 2 行（见上）；行数 270 → 270 |
| 校验 | `plutil -lint` OK；plistlib 回读；**XHC0/XHC1/XHC2 三个 personality 逐字节未变** |
| 备份 | ESP 上 `Info.plist.hl-bak-before-20261010-125521`；本仓库 `outputs/20261010-usbportlimit/UTBMap-Info.{before,after}.plist` |
| 生效 | **需要重启**（kext 属性在开机时读取） |

预期效果：XH00 的 17 个端口获得名字与 `UsbConnector`，其中 **port 16 会被标成
`UsbConnector = 255`（Internal）**，蓝牙获得正确的内置标注。

### 11.3 G2000 那个口的映射：**正确**

UTBMap 的 XH00 表（`port-count = 17`）逐端口对照实测设备：

| port | hex | 名称 | UsbConnector | 实测设备 |
| --- | --- | --- | --- | --- |
| 2 / 3 | `0x02` / `0x03` | SS01 / SS02 | 3 | （USB3 Type-A 对接口） |
| 7 / 8 | `0x07` / `0x08` | HS01 / HS02 | 3 | （上面两个 USB3 口的 USB2 半边） |
| 15 | `0x0F` | HS03 | 0 | Samson C01U Pro Mic |
| 16 | `0x10` | HS04 | **255 (Internal)** | Wireless_Device（蓝牙 `0e8d:0616`） |
| **17** | **`0x11`** | **HS05** | **0** | **HECATE G2000** |

结论：**G2000 那个口（port 17）的匹配是对的**，理由三条：

1. **端口身份对**：G2000 实测挂在 `XH00 port 17`（`0x01100000`），与三重交叉验证
   （Linux `usb1-port12` = Windows Port 17 = macOS `0x01100000`）一致；
2. **类型值对**：它是**纯 USB2 Type-A**（G2000 实测 `Speed: Up to 12 Mb/s`，全速设备），
   `UsbConnector = 0`（Type-A）正是这种情况的取值；
3. **整套表自洽**：两个 USB3 对接口的 SS/HS 四端都是 3，三个纯 USB2 口（15/16/17）
   分别是 0 / 255 / 0 —— 与「combo 口标 USB3 类型、纯 USB2 口标 Type-A」的规则一致；
   另有一个独立旁证：XHC2 唯一那个后置 USB2 Type-A 口（实测 MCHOSE 键盘）也是 `0`。

唯一要留意的不是数值对错，而是**这张表此前根本没生效**（§11.2）—— 也就是说 G2000 那个口
在本次修复前既没有类型也没有名字。

## 12. 2026-10-10 13:00：撞车的实测复核（它确实存在，且**在用户可见处现形**）

§10.2 曾说"撞车对功能无影响"，这个说法**讲得太满**，此处更正为：
**功能未受影响，但标识符产生了真实歧义，而且在系统信息里直接可见。**

### 12.1 实测证据（三条）

**① 端口 ↔ 端口**

```
XH00 port 17   AppleUSB20XHCIPort@01100000        id=0x100000320
XHC2 port  1   AppleUSB20XHCIPort HS01@01100000   id=0x100000353
               ↑ 两个不同对象，locationID 同为 0x01100000
```

**② 端口 ↔ 控制器基址**

```
XH00 port 16   AppleUSB20XHCIPort@01000000        id=0x10000031f
XHC2（控制器）  AppleUSBXHCIPCI@01000000           id=0x1000002ae
               ↑ 端口占了另一个控制器的编号字段
```

**③ 用户可见（最关键）** —— `system_profiler SPUSBDataType` 打印的是
`Location ID: <locationID> / <USB Address>`，而：

| 设备 | locationID | USB Address | 系统信息显示 |
| --- | --- | --- | --- |
| HECATE G2000 | `0x01100000` | 1 | `Location ID: 0x01100000 / 1` |
| MCHOSE G87 V2 | `0x01100000` | 1 | `Location ID: 0x01100000 / 1` |

⇒ **两个物理上无关的设备（不同控制器）在系统信息里显示成完全相同的字符串。**
（第二个字段是 USB 总线地址，各控制器独立编号，所以正好也都是 1。）

### 12.2 区分"正常重复"与"真撞车"

全系统扫描会看到大量 locationID 重复（如 `0x00F00000` 下有 10 个对象），那些是
**正常的父子继承**——设备下面的各接口继承所属端口的 locationID，补丁前就有。
真正异常、且补丁前不存在的只有 12.1 的 ①②：**两个相互独立的端口/控制器共用同一数值**。

### 12.3 影响评估

**未坏**：两设备功能都正常 —— G2000 的音频引擎与 HID 均在，键盘 HID 报文字段有持续计数；
内核日志无报错、无 panic；IORegistry 路径因父节点不同仍唯一，IOKit 按路径匹配不受影响。

**有风险点**：凡"把 locationID 当唯一键"的环节都可能认错 —— 系统信息显示、
按设备保存的偏好设置、部分第三方 USB 工具。未找到实际出错证据，但也不能排除更隐蔽的路径
（如睡眠唤醒）。

### 12.4 为什么躲不掉 / 唯一干净的解法

撞车是**硬件布局决定的**，不是补丁写坏：XH00 控制器字节为 `0x00`，端口号 16/17
（`0x10`/`0x11`）左移 20 位后必然溢出到 bit24+，正好落进 XHC2 的编号空间（`0x01`）。

唯一干净的解法是把 **XHC2 的控制器字节从 `0x01` 挪开**（如 `0x02`），即让 XHC2 拿到不同的
`acpiPort` —— 也就是 §8 卡住的那条 ACPI 路。**禁用 XHC2 不是选项**：它那个口现在正插着
MCHOSE 键盘，是真实在用的。

## 13. 2026-10-10 13:25：**"能不能消掉撞车"的答案找到了 —— 能，而且不用改内核**

### 13.1 更正我之前的结论

§8 里我说过"控制器编号没法靠属性注入改，是死路"。**那句是错的**，原因是当时 vtable 解码
基准用错（`__ZTV` 槽位解到了无关符号）。用正确的 chained-fixup 基准
（`0xffffff8000100000 + (raw & 0x3fffffff)`，已用已知符号自校验）重查后，结构完全变了：

| vtable | `+0xad0` 槽 |
| --- | --- |
| `__ZTV22AppleUSBHostController`（基类） | `AppleUSBHostController::generateBusNumber` |
| `__ZTV15AppleUSBXHCIPCI`（派生） | `AppleUSBXHCIPCI::generateBusNumber` |

`AppleUSBXHCIPCI::generateBusNumber` 只有 92 字节：取 PCI 设备号与 acpiPort → 组字节
`(devNum<<5)|(acpiPort&0x1f)` → 写 `*outByte` → **尾调用基类实现**（`jmp [rax+0xad0]`）。

而基类实现的第一步就是读属性当**覆盖值**：

```
+ 37  mov rdi,[rdi+0x88] / +47 lea rsi,'bus-number'   → getProperty（OSData，取首字节）
+ 81  mov rdi,[rbx+0x88] / +91 lea rsi,'USBBusNumber' → getProperty（数字）
+160  mov [r15],al      ; ← 有属性就覆盖 *outByte
+196  movzx edx,[r15] / shl edx,0x18 / mov [rbx+0x4b4],edx   ; locationID = 字节<<24
+212  setProperty('locationID', <'%08x' 格式化的字符串>)
```

⇒ **控制器字节在开机时可以被属性覆盖**，这是苹果自己的代码路径。撞车只由字节决定，
所以把 XHC2 的字节从 `0x01` 挪到 `0x02`（XH00 的 0x01000000/0x01100000 就独占），
撞车归零。建议挪 XHC2（只有 1 个口），不动 XH00（会一次性移动 15 个既有端口）。

### 13.2 还差一步（诚实标注）

属性从**控制器持有的某个 IORegistryEntry**（`[this+0x88]`，不是控制器自己）读取；
该 ivar 由基类设置（USB 各函数里没有写 `+0x88` 的指令），最可能是 provider/设备节点。
候选注入通道：OpenCore `DeviceProperties`（落到 PCI 设备）或 kext 的
`IOProviderMergeProperties`。**需要一次重启实验判定**：注入了就生效，没注入则什么也不发生，风险低。

### 13.3 顺带发现的问题（需要处理）：UTBMap 的表**套错了位置**

§11 那次 UTBMap 改动**已生效**（XH00 控制器现在有 `port-count = 17`，14 分钟前那次重启后），
但暴露出一个真实问题：**USBToolBox 把 XH00 的 7 条端口表按"位置"套到了 port 1..7**，
而不是按表里声明的端口号（2/3/7/8/15/16/17）：

| 实际落位 | 名字 | UsbConnector | 表里声明的端口号 |
| --- | --- | --- | --- |
| port 1 (`0x00100000`) | SS01 | 3 | 2 |
| port 2 (`0x00200000`) | SS02 | 3 | 3 |
| port 3 (`0x00300000`) | HS01 | 3 | 7 |
| port 4 (`0x00400000`) | HS02 | 3 | 8 |
| port 5 (`0x00500000`) | HS03 | 0 | 15 |
| **port 6** (`0x00600000`) | **HS04** | **255 (Internal)** | 16 ← 应落在 0x01000000 |
| port 7 (`0x00700000`) | HS05 | 0 | 17 |

后果两条：

1. **Internal(255) 标错了口**（落在 port 6，而不是真正装蓝牙的 port 16）；
2. **port 3/4/5 的类别从 `AppleUSB30XHCIPort` 变成了 `AppleUSB20XHCIPort`**
   （名字前缀 `HS` 让它们按 USB2 处理）—— 这三个原本是 USB3 端口，**有降速风险**；
3. 真正的 port 16/17（蓝牙 / G2000）反而没有任何名字与类型。

对比：改动前（表完全不生效）XH00 的 17 个端口没有任何显式名字/类型，行为由硬件推导，
功能正常。**所以 §11 那次改动目前是净负面影响**，建议先回退
（把 `IOPCIPrimaryMatch` 改回 `IONameMatch = XH00`，或留 `IOPCIPrimaryMatch` 但从表中移除
XH00 那一节），待查清 USBToolBox 的表套用规则后再正确补上。

## 14. 2026-10-10 13:33：消撞车的注入已落地（机制已完全证实）

### 14.1 UTBMap 已回退

§11 那次改动**已完整回退**：`Info.plist` 从备份恢复，sha256 回到原始值
`0cd3a353191efb0f71710990b64b8a650f9aefbd04e887e59a8a56dbca2c004e`（`plutil -lint` OK）。
XH00 的 `ports` 表重新变为"匹配不上、不生效"，端口类型回到硬件推导（已知良好状态）。
正确补表的做法留待后续（关键点：USBToolBox 对 XH00 是**按 rank 套用**的，
所以表必须写成 ports 1..17 全覆盖且顺序正确）。

### 14.2 `+0x88` = provider（决定性证据）

`AppleUSBHostController::start(IOService *provider)` @ `0xffffff8002cc9444` 开头：

```
+ 17  mov r14, rsi            ; rsi = 第二个参数 = provider
+ 20  mov rbx, rdi            ; this
+ 30  call [rax + 0x5d0]
+ 44  mov [rbx + 0x88], r14   ; ★ 把 provider 存进 this+0x88
+ 51  test r14,r14 / je fail  ; 空指针检查
+ 60  mov rax,[r14] / call [rax+0x20]
```

⇒ `generateBusNumber` 里读 `[this+0x88]` 的属性，就是**从父 IOPCIDevice 读**。
而 OpenCore `DeviceProperties` 正是往 PCI 设备注入属性 —— 通道成立。

### 14.3 两个属性要求的类型（已解出，非猜测）

两处 `safeMetaCast` 用的元类不同，解链式绑定指针后：

| 属性 | 目标元类 | 注入时须用的 plist 类型 |
| --- | --- | --- |
| `bus-number` | `OSData::metaClass` | `<data>`（取首字节） |
| `USBBusNumber` | `OSNumber::metaClass` | `<integer>` |

（`bus-number` 先被检查，命中即用；`USBBusNumber` 是后备。两者都在
`+160 mov [r15],al` 处写入同一个字节。）

### 14.4 离线验算：撞车归零

```
现状（补丁后）        : 25 个 locationID，冲突 1 处（0x01100000: XH00 p17 vs XHC2 p1）
XHC2 字节 0x01→0x02  : 26 个 locationID，冲突 0 处  ✓
```

XHC2 新占 `0x02000000`（控制器）/ `0x02100000`（port 1），与任何现有值均不重合。
选挪 XHC2 而不是 XH00：XHC2 只有 1 个口，XH00 挪一下会同时移动 15 个既有端口。

### 14.5 已落地

`EFI/OC/config.plist` → `DeviceProperties > Add` 新增一条（纯插入）：

```xml
<key>PciRoot(0x0)/Pci(0x8,0x3)/Pci(0x0,0x0)</key>
<dict>
    <key>USBBusNumber</key><integer>2</integer>
    <key>bus-number</key><data>Ag==</data>     <!-- 0x02 -->
</dict>
```

路径推导：XHC2 在 `GP19@8,3`（ACPI `_ADR`=0x80003 → 设备 8 功能 3）下、自身 `_ADR`=0
→ `PciRoot(0x0)/Pci(0x8,0x3)/Pci(0x0,0x0)`。同格式旁证：已有的
`PciRoot(0x0)/Pci(0x8,0x1)/Pci(0x0,0x6)` 正是 `GP17@8,1` 下的 HDEF（音频）。

| 项 | 值 |
| --- | --- |
| 改动前 sha256 | `c80604f159ebc1babdcbdfa2745cf1014a174a12e97b4ca044d251b426947afd` |
| 改动后 sha256 | `aadce97b542d32a154ef1ad119b62232eb00b82491dc232307269e08b14f19a9` |
| diff | **删除 0 行 / 新增 9 行**；原有 2 条 DeviceProperties 条目保留；其余段逐字节未变 |
| 校验 | `plutil -lint` OK；plistlib 回读；`Kernel`/`ACPI`/`Misc`/`NVRAM`/`UEFI`/`PlatformInfo`/`Booter` 断言未变 |
| 备份 | ESP 上 `config.plist.hl-bak-before-20261010-133334` |
| 生效 | **需要重启** |

### 14.6 风险与验收

风险低：若属性未被读取，则完全无副作用；唯一变更是 XHC2 的编号从 `0x01` 移到 `0x02`，
其 1 个端口（现插 MCHOSE 键盘）的 locationID 随之变化并重新枚举。

验收（重启后）：

1. `ioreg -c AppleUSBXHCIPCI -r -l -w0 | grep locationID` → XHC2 应为 `0x02000000`，
   其端口 `0x02100000`；
2. **冲突应消失**：`system_profiler SPUSBDataType` 里两个设备的 Location ID 不再相同；
3. 键盘仍可用（XHC2 口）；`XH00` port 16/17（蓝牙 / G2000）照旧；
4. 若 byte 未变（仍是 `0x01000000`）→ 说明该路径没命中，改试
   `PciRoot(0x0)/Pci(0x8,0x3)`（少一层）或把属性挂到 XH00 的 PCI 设备
   `PciRoot(0x0)/Pci(0x8,0xC)/Pci(0x0,0x0)`（`GPP7@20001`→`_ADR` 0x20001 = 设备 8 功能 0xC）。

## 15. 2026-10-10 13:53：重启后实测 —— **撞车已消除，全绿**

### 15.1 注入生效，一次命中

| 控制器 | 控制器 `locationID` | 首个端口 |
| --- | --- | --- |
| XH00 | `0x00000000` | 17 个端口：`0x00100000` … `0x01100000` |
| XHC0 | `0x60000000` | `0x60100000` |
| XHC1 | `0x80000000` | `0x80100000` |
| **XHC2** | **`0x02000000`** ← 原 `0x01000000` | **`0x02100000`** ← 原 `0x01100000` |

`PciRoot(0x0)/Pci(0x8,0x3)/Pci(0x0,0x0)` + `USBBusNumber=2` **一次命中**，无需备用路径。

### 15.2 冲突归零（用户可见判据）

改写前（同一串，无法区分）：

```
HECATE G2000   Location ID: 0x01100000 / 1
MCHOSE G87 V2  Location ID: 0x01100000 / 1     ← 完全相同
```

改写后：

```
HECATE G2000    Location ID: 0x01100000 / 3    ← XH00 port 17
Wireless_Device Location ID: 0x01000000 / 1    ← XH00 port 16（蓝牙）
Samson C01U     Location ID: 0x00f00000 / 2    ← XH00 port 15
MCHOSE G87 V2   Location ID: 0x02100000 / 1    ← XHC2 port 1
无线鼠标 dongle  Location ID: 0x60100000 / 1    ← XHC0 port 1
```

全机端口级/控制器级扫描：**0 处跨对象同值**（只剩控制器与其 `AppleUSBController`
子对象的父子继承，属正常）。`0x01100000` 现在只属于一个对象。

### 15.3 UTBMap 回退已生效

XH00 控制器节点**无 `ports` / `port-count`**，其 17 个端口**无 `name`、无 `UsbConnector`**，
端口类别由硬件推导（`0x00100000`–`0x00500000` 为 `AppleUSB30XHCIPort`，其余 12 个为
`AppleUSB20XHCIPort`）—— 与改动前已知良好状态一致。其余控制器命名照旧
（XHC0 `HS01`/`conn=3`、XHC2 `HS01`/`conn=0` 等）。

### 15.4 设备与配置完好

- 蓝牙：`BT_ACL_If` / `BT_SCO_If` / `BT_ISO_If` 均 `registered, matched`；
- MCHOSE 键盘（XHC2 口）与 HECATE G2000（XH00 port 17）都在；
- 内核无 USB 报错、无 panic；
- `config.plist` sha `aadce97b542d32a154ef1ad119b62232eb00b82491dc232307269e08b14f19a9`；
  我的 2 条 `Kernel > Patch` 仍 `Enabled=True`、`XhciPortLimit=False`、
  `DeviceProperties` 3 条（原 2 条 + 我 1 条）；
- 并行 WiFi 会话的 `IOSkywalkFamily` / `IO80211FamilyLegacy` / `AirportMtk` 与
  `Kernel > Block` 条目全部完好。

### 15.5 还没做的一步：给 XH00 正确补端口表

现状是"能用的最小集"：XH00 的 17 个端口没有名字与 `UsbConnector`（蓝牙口因此没有
Internal 标注）。要补上需先搞清 USBToolBox 对 XH00 的**套表规则**（§13.3 已确认是
**按 rank 顺序**套用，不是按端口号），然后写一份 ports 1..17 全覆盖、顺序正确的表。
这一步纯属优化，不影响功能，可随时单独做。

## 16. 2026-10-10 14:00：读 USBToolBox 源码后的定论与一处更正

上游仓库现名 `USBToolBox/kext`（原 `dhinakg/USBToolBox` 已 404），版本分支 **`tahoe`**，
tag `1.2.0` = commit `a22ea1eb`。工具仓库 `USBToolBox/tool`（默认分支 `master`，Python 源码）。

### 16.1 kext 侧不做任何逐端口处理

`USBToolBox::mergeProperties` 只把 `IOProviderMergeProperties` 的键**原样复制**到控制器：

```cpp
while (OSSymbol* key = ...getNextObject()) {
    OSObject* value = properties->getObject(key);
    if (key == portsSymbol && version_major >= 25) value = fixMapForTahoe(value);
    this->controllerInstance->setProperty(key, value);
}
```

`fixMapForTahoe` 被 `version_major >= 25`（Darwin 25 = macOS 26）守着，**本机 Darwin 24 不执行**；
它只做 `port`→`usb-port-number`、`UsbConnector`→`usb-port-type` 的复制，与本问题无关。
⇒ **端口归属完全由苹果的 `createPort` 决定**，套表规则不在 USBToolBox 里。

### 16.2 ⚠️ 更正 §13.3：那份 map 不是"名字错位"，而是**丢了 10 个端口**

内核侧证据（`AppleUSBXHCIPCI::createPort`）：单端口查 `getPortPropertyTable(port)` 为 null 时
直接 `return NULL`；`createPorts` 里 `cmp [rax+r14*8],0` / `jne` 只在**非空**时累加端口索引
⇒ **map 里没列的端口不会被创建（白名单语义）**。

实测吻合：map 未生效时 XH00 = **17** 个端口对象；map 生效时 XH00 = **7** 个（名字落在 port 1..7）。
⇒ 那份 Windows map（7 条：2/3/7/8/15/16/17）**丢掉了 10 个端口**，包括蓝牙口与 G2000 口。
§13.3 当时只注意到名字前移、说成"3 个 USB3 端口降速"，**低估了严重程度**，此处更正。
（该改动只生效了一个很短窗口，13:33 已回退。）

### 16.3 tool 侧：为什么在 macOS 上生成是**正确**的

`macOS.py` 的端口枚举：`"index": int(read_property(port_properties["port"], 6), 16)`
—— `index` 取自**该端口自己的 `port` 属性**（本机 XH00 = 1..17，连续）。
`base.py` 建表时 `"port": <hex(port["index"])>`、`port-count` = 最大的已选 index。
⇒ **macOS 侧生成的表天然连续**。Windows 那份之所以错，是因为它按**硬件端口号**
（2,3,7,8,15,16,17，不连续）写，按 rank 套用时就整体前移了。

其余从源码确认的点：

| 事实 | 依据 |
| --- | --- |
| 15 只是**红色警告色**，不是硬上限 | `base.py:314` `...red if selected_count > 15 else ...green` |
| 未勾选的端口会被省略 | `base.py:596` `if not port["selected"]: continue` |
| 每个勾选端口**必须**有连接器类型 | `validate_selections()`："Port N is missing a connector type!" |
| macOS 侧不猜类型 | 上游 README；`"type": None`，`guessed` 取端口自身 `UsbConnector`（本机为空） |
| 类型取值：3=USB3TypeA、9=USB3TypeC_WithSwitch、**255=Internal**、0=USBTypeA | `Scripts/shared.py` `USBPhysicalPortTypes` |

### 16.4 ⚠️ 实测：本机工具会崩 —— 根因是**我们自己注入的 `bus-number`**

工具在 macOS 15.8.1 上**能正常启动**（0.1.1 的 `macOS.zip`，PyInstaller 打包的 Python，x86_64、
adhoc 签名、无 quarantine；实测到主菜单）。但按 `D` 发现端口时崩溃：

```
File "macOS.py", line 95, in get_controllers
ValueError: invalid literal for int() with base 16: ''
```

`macOS.py:95` 是 `int(hexswap(read_property(parent_properties["bus-number"], 6)), 16)`；
`read_property` 砍掉末尾 6 个十六进制字符（**按 4 字节解析**）。而 §14 给 XHC2 注入的
`bus-number` 只有 **1 字节**（`<02>`）→ `hexlify` 仅 2 字符 → 砍成空串 → 崩。
⇒ §14.6 预判的"该属性会撞上工具"应验。`bus_number` 在工具里**只用于选匹配键**
（`choose_matching_key` 的 M1 分支），无其他用途；XH00/XHC0/XHC1 无此属性，故只有 XHC2 触发。

### 16.5 已修（需重启）

`EFI/OC/config.plist`：`bus-number` 由 `<02>` 补成 **4 字节 `<02 00 00 00>`**（`Ag==` → `AgAAAA==`）。
基类 `generateBusNumber` 读的是**首字节**（`mov al,byte ptr [rax]`），故行为**完全不变**（仍是 `0x02`），
但工具能正常解析了。

| 项 | 值 |
| --- | --- |
| 改动前 sha256 | `aadce97b542d32a154ef1ad119b62232eb00b82491dc232307269e08b14f19a9` |
| 改动后 sha256 | `d5f5c4c5352edc7dbe110de3fe244e41f73a5aad26875291b1395cd71564381f` |
| diff | **仅 1 行**（`Ag==` → `AgAAAA==`）；其余段逐字节未变；`plutil -lint` OK |
| 备份 | ESP 上 `config.plist.hl-bak-before-20261010-140410` |
| 生效 | **需要重启** |

备选（更干净，但走未验证的代码路径）：删掉 `bus-number`、只留 `USBBusNumber=2` —— 反汇编显示
`USBBusNumber` 是后备路径（`bus-number` 缺失时使用），且工具会改走正常匹配键
（XHC2 → `IONameMatch: XHC2`）而不是 M1 分支。

### 16.6 重启后（若工具可用）macOS 侧的正确流程

1. `D` 发现端口 → 应看到 4 个控制器，XH00 **17 个端口**；
2. `S` 进入选择 → `A` **全选**（15 只是警告色，可全选 17）；
3. 逐个设连接器类型（macOS 侧不能猜）：USB3 端口 → 3，纯 USB2 Type-A → 0/3，
   **蓝牙那个口（XH00 port 16）→ 255 (Internal)**；
4. `K` 生成 kext → 先在临时目录验证 **XH00 仍是 17 个端口**、名字/类型对位，再考虑装 ESP。

注意：工具自动生成的名字是 `SS0001` / `HS0001` 这类**序号名**，不是可读的物理位置名；
真正收益是 `UsbConnector` 被正确设上（含蓝牙口的 Internal 标注）。

## 17. 2026-10-10 19:57：XH00 端口表补全（17 条，等重启验证）

### 17.1 前置：`bus-number` 的 4 字节修复生效，工具可用

重启后实测（用户同时移除了导致卡跑码的 CX3 驱动，与本任务无关）：

- `XHC2@02000000` ✓ 撞车修复仍在；`system_profiler` 里 G2000 `0x01100000` 与键盘 `0x02100000`
  已不再同串；5 个设备全部正常；CX3 痕迹（`Cx3Mlx4` / mlx4）已清空。
- 运行态 `bus-number = <02000000>`（4 字节）⇒ **USBToolBox 工具不再崩溃**，实测能启动并
  正确列出 `pci1022,43fc | 17 ports`。

### 17.2 工具给出的权威逐端口数据

工具把发现结果存在可执行文件同目录的 `usb.json`（已留档
`outputs/20261010-usbportlimit/usbtool/usb.json`）。XH00 的关键事实：

| 事实 | 值 |
| --- | --- |
| 端口数 | **17** |
| 端口 index | **1..17 连续**（= 硬件端口号），与 `locationID` 一一对应 |
| 类别 | index 1–5 = `AppleUSB30XHCIPort`（USB3）；6–17 = `AppleUSB20XHCIPort`（USB2） |
| 类型 | `type=None`、`guessed=None`（macOS 侧无法推断），所以必须手工指定 |
| 有设备的 index | 15（Samson 麦克风）、16（蓝牙）、17（G2000） |

⇒ 因为 index 是连续 1..17，**表里 `port` 写 1..17 就是 1:1 对应**，不会再发生 §13.3/§16.2 那种整体前移。

### 17.3 类型取值依据（逐条可查）

| index | 类别 | 类型 | 依据 |
| --- | --- | --- | --- |
| 1–5 | USB3 | 3 (USB3TypeA) | macOS 端口类别判定（`AppleUSB30XHCIPort`） |
| 6–10 | USB2 | 3 (USB3TypeA) | USB3 口的 **USB2 半边**。旁证：Windows 侧工具自动绑定 companion，把硬件端口 7、8 判为 `UsbConnector=3`；而 2↔7、3↔8 恰为偏移 +5，推得 (1,6)(2,7)(3,8)(4,9)(5,10) 成对 |
| 11–15 | USB2 | 0 (TypeA) | 纯 USB2；15 已被 Windows 数据确认（`HS03=0`） |
| **16** | USB2 | **255 (Internal)** | 蓝牙；Windows 数据确认（`HS04=255`） |
| 17 | USB2 | 0 (TypeA) | Windows 数据确认（`HS05=0`，G2000 就插这里） |

其中 1–4、6–10、11–14 是**推断**（标注在地：USB3 类别来自 macOS 实测；USB2 半边来自 +5 配对推断），
其余均有实测/文档支撑。类型取值错误的最坏后果是端口**描述**不准（外部口 0 与 3 在行为上等价，
端口类型接口只对 `0xFF` 特殊处理，见 §11.1），不影响功能。

### 17.4 已落地

`EFI/OC/Kexts/UTBMap.kext/Contents/Info.plist`：

- XH00 匹配键：`IONameMatch = XH00` → **`IOPCIPrimaryMatch = 0x43fc1022`**（该键已在 §11 实验中被证明能命中）；
- XH00 `ports`：**7 条 → 17 条**，`port` = 1..17 连续唯一，`port-count` = 17；
  命名 SS01–SS05（index 1–5）、HS01–HS12（index 6–17）；
- **XHC0 / XHC1 / XHC2 三条 personality 逐字节未变**。

| 项 | 值 |
| --- | --- |
| 改动前 sha256 | `0cd3a353191efb0f71710990b64b8a650f9aefbd04e887e59a8a56dbca2c004e` |
| 改动后 sha256 | `f9b484e82f70b669452bb6782d568ccc6bf4b22fa8e0daeea82210f9242a1ea3` |
| diff | 删除 16 行 / 新增 106 行；`plutil -lint` OK |
| 备份 | ESP 上 `Info.plist.hl-bak-before-20261010-195730`；仓库 `outputs/20261010-usbportlimit/UTBMap-Info.dense17.{before,after}.plist` |
| 生效 | **需要重启** |

**设计上已消除 §16.2 那个风险**：表是 17 条全覆盖，白名单语义下不会丢任何端口。

### 17.5 重启后验收

1. `ioreg` 里 XH00 **仍是 17 个端口对象**（关键：不能变成 7 个）；
2. 各端口出现 `UsbConnector`：index 16 应为 **255**；
3. 蓝牙 / G2000 / 麦克风三个设备仍在、可用；XHC2 键盘不受影响；
4. 若端口数变少或设备消失 → 立刻回滚（换回备份的 `Info.plist`，或把该 personality 删掉）。

**回滚**：`cp Info.plist.hl-bak-before-20261010-195730 Info.plist`（ESP 上备份），或整条
XH00 personality 删除（其余三个控制器不受影响）。

## 18. 2026-10-10 20:22：重启后实测 —— **XH00 端口表补全成功，全绿**

### 18.1 验收结果（逐项对上）

| 检查项 | 结果 |
| --- | --- |
| XH00 端口数 | **17** ✓（未退化为 7 —— §16.2 的风险已排除） |
| XH00 端口名 | `SS01`–`SS05`（port 1–5）、`HS01`–`HS12`（port 6–17）✓ |
| `UsbConnector` | port 1–5=3、6–10=3、11–15=0、**16=255**、17=0 ✓ |
| 蓝牙口标注 | `0x01000000 / HS11 / port 16 / UsbConnector 255 (Internal)` ✓ |
| 蓝牙接口 | `BT_ACL_If` / `BT_SCO_If` / `BT_ISO_If` 三个都 `matched` ✓ |
| 设备 | G2000 `0x01100000`、蓝牙 `0x01000000`、麦克风 `0x00f00000`、键盘 `0x02100000`、鼠标 `0x60100000` 全部在 ✓ |
| 撞车 | 无（G2000 与键盘不再同串）✓ |
| 其他控制器 | XHC0 = 4、XHC1 = 4、XHC2 = 1（含 `Bus Number: 0x02`）✓ |
| 内核报错 / panic | 无 ✓ |

### 18.2 最终生效配置一览（本会话累计改动）

| # | 位置 | 内容 | 作用 |
| --- | --- | --- | --- |
| 1 | `Kernel > Patch` | `setPortLocation` `cmp r15d,0xF`→`0x11`（`41 83 FF 0F 0F 87`→`41 83 FF 11 0F 87`） | 放行端口号 ≤17 |
| 2 | `Kernel > Patch` | `AppleUSBXHCI::createPorts` `cmp ebx,0xF`→`0x11`（`83 FB 0F 0F 83`→`83 FB 11 0F 83`） | 创建到 17 个端口 |
| 3 | `DeviceProperties` | `PciRoot(0x0)/Pci(0x8,0x3)/Pci(0x0,0x0)` = `USBBusNumber 2` + `bus-number <02 00 00 00>` | 把 XHC2 控制器字节 `0x01`→`0x02`，消除与 XH00 port 16/17 的编号撞车 |
| 4 | `UTBMap.kext` | XH00：`IOPCIPrimaryMatch = 0x43fc1022` + 17 条 `ports`（port 1..17） | 给 XH00 的 17 个端口补名字与连接器类型（含蓝牙口 Internal） |

`XhciPortLimit` 保持 **False**；`Kernel > Patch` 总计 29 条（我的 2 条 + 并行 WiFi 会话的改动原样保留）。

### 18.3 两点说明

- **命名风格**：工具惯例是序号名（`SS01`/`HS01`…），不是可读的物理位置名。port 15/16/17
  对应 `HS10`/`HS11`/`HS12`。这是预期行为，不影响功能。
- **仍属推断的两处**（§17.3 已标）：port 6–10 的 `UsbConnector=3`（USB3 口的 USB2 半边，
  由 +5 companion 配对推断）与 port 1–5 假定为 Type-A。取值偏差只影响端口**描述**
  （外部口 0 与 3 行为等价），不影响功能。
- **可选的功能确认**：拿一个 USB3 设备插到蓝色的 USB3 口，确认速率是 USB 3.x（而不是降到 USB2）。
  这是唯一还没做过的物理验证。

### 18.4 回滚（三个层次，按需选）

1. UTBMap：`cp Info.plist.hl-bak-before-20261010-195730 Info.plist`（ESP 上备份），或删掉整条
   XH00 personality（其余三个控制器不受影响）；
2. 撞车注入：`DeviceProperties > Add` 里删掉 `PciRoot(0x0)/Pci(0x8,0x3)/Pci(0x0,0x0)` 那条
   （`config.plist.hl-bak-before-20261010-133334` 是未注入版；`…-140410` 是 `bus-number` 1 字节版）；
3. 端口上限：把两条 `hl | USB port limit` 开头的 `Kernel > Patch` 条目 `Enabled` 改回 `false`。

## 19. 2026-10-10 20:30：USB3 速率物理验证（U 盘 + 手机）

用户实测：U 盘插 USB3 口、手机插 Type-C 口。

| 设备 | 落点 | 速率 | 判定 |
| --- | --- | --- | --- |
| TESLADRIVE（`13fe:6400`，Phison） | **XHC1 SS01** = `0x80300000`（port 3） | **5 Gb/s** | ✓ USB 3.0 SuperSpeed，满速 |
| Xiaomi 15S Pro（`2717:ff48`） | **XHC0 SS02** = `0x60400000`（port 4） | **10 Gb/s** | ✓ USB 3.1 Gen2，满速 |

### 19.1 结论

- **全系统 USB3 速率未被破坏**：5 Gb/s 与 10 Gb/s 都拿到，说明 2 条内核补丁 + XHC2 编号注入没有降速副作用。
- **交叉验证**：手机 10 Gb/s 印证了仓库文档"Type-C 是 USB 3.1 Gen 2 / 10 Gbps"的判断，
  也与 `usb-mapping-plan.md` 里 XHC0 `HS02/SS02 = USB-C(9)` 的记录一致。
- **蓝牙口标注生效**：`Wireless_Device` 现在带 **`Built-In: Yes`**（`UsbConnector=255` 的效果），
  速率 480 Mb/s。

### 19.2 覆盖范围（如实标注）

这两个口**都不在 XH00 上**（分别在 XHC1 / XHC0）。所以本次验证的是
"**没有全局降速**"，而**没有**直接测到 XH00 自己的 USB3 口。

XH00 侧已被验证的仍是 **port 16（蓝牙）** 与 **port 17（G2000 / 现在可用的那个 Type-A）**，
两者都是 USB2 设备。

### 19.3 可选的后续：把 XH00 的物理端口映射出来

XH00 在 macOS 侧有 5 个 USB3 能力端口（`SS01`–`SS05` = `0x00100000`–`0x00500000`），
但**哪几个真正接在 USB3 连接器上**未验证 —— 这同时能校验 §17.3 里那几处推断的
`UsbConnector`。做法：逐个插拔机箱 USB3 口，每次读一次落点，即可得到
"物理口 ↔ locationID" 对照表。属可选，不影响已达成目标。

## 20. 2026-10-10 20:40：**在 XH00 上实测到 USB3 满速**（补上 §19.2 的缺口）

用户把 U 盘换到另一个口后：

```
TESLADRIVE   Up to 5 Gb/s   Location ID: 0x00200000 / 4
```

### 20.1 这条数据的意义

| 项 | 结论 |
| --- | --- |
| 落点 | **XH00 的 `0x00200000`** = port 2 = 表里的 **`SS02`** |
| 速率 | **5 Gb/s** = USB SuperSpeed（未降速） |
| 意义 1 | **XH00 自己真的有可用的 USB3 口**，且能跑满速 —— 首次在 XH00 上验证到 USB3（§19 那次落在 XHC1，没覆盖到） |
| 意义 2 | §17.3 里"port 1–5 标 `UsbConnector=3`（USB3 Type-A）"这处**推断被实证**：该物理口确实是 USB3 连接器 |
| 意义 3 | `0x00200000` 这个 locationID 在 15 口上限时代**根本不存在**，现在能正常承载设备 ⇒ 补丁 + 编号重排的完整链路有效 |
| 意义 4 | 蓝牙口 `Built-In: Yes`（Internal=255 标注生效），且与 XH00 的 USB3 口同时工作，互不干扰 |

### 20.2 当时的全机设备状态

| 控制器 | 设备 | 速率 | 位置（端口） |
| --- | --- | --- | --- |
| **XH00** | TESLADRIVE | **5 Gb/s** | `0x00200000`（port 2 / SS02） |
| XH00 | HECATE G2000 | 12 Mb/s | `0x01100000`（port 17） |
| XH00 | Wireless_Device（蓝牙） | 480 Mb/s | `0x01000000`（port 16）· Built-In: Yes |
| XH00 | Samson C01U Pro Mic | 12 Mb/s | `0x00f00000`（port 15） |
| XHC0 | Xiaomi 15S Pro | 10 Gb/s | `0x60400000`（port 4 / SS02） |
| XHC0 | 无线鼠标 dongle | 480 Mb/s | `0x60100000`（port 1） |
| XHC2 | MCHOSE G87 V2 | 480 Mb/s | `0x02100000`（port 1） |

7 个设备全部正常、速率就位、无冲突、无报错。

### 20.3 剩余未实测项（可选）

XH00 的 port 1、3、4、5 尚未逐个实测（表里同样标 `3`）。既然 port 2 已是真 USB3、
且前 5 个端口在 macOS 侧同属 USB3 能力组（`AppleUSB30XHCIPort`），该标注的置信度已较高。
彻底钉死的方法：把 U 盘依次插到其余 USB3 口，每次读一次落点。

## 21. 2026-10-10 20:50：校验器基线同步 + 失配演示（维护流程说明）

### 21.1 校验器抓出两处漂移并已修正

撞车消除后重跑 `usbpatch-check.py`，它立刻报出两处与实际不符的基线：

| 文件 | 原值 | 改为 | 原因 |
| --- | --- | --- | --- |
| `specs/layout-msb850bkb.json` | XHC2 `ctrl_byte = 1` | **`2`** | 已用 DeviceProperties 注入 `bus-number=2`，XHC2 实际占 `0x02000000` |
| `specs/portlimit-24H32.json` | `expected_collisions = ["0x1100000"]` | **`[]`** | 撞车已消除；将来若注入失效，会作为"非预期撞车"报出 |

修正后：`补丁命中 2/2，端口撞车 0 处，VERDICT: PASS`（退出码 0）。

**这正是校验器的用途**：基线漂移会被它顶出来，而不是靠人记得。

### 21.2 失配时长什么样（已实测）

把 `find` 故意改成一个不存在的模式后：

```
补丁命中      : 1/2
VERDICT       : FAIL
系统 KC 未被改动: ✓        退出码 = 1
```

⇒ 系统升级后若 Apple 改了那段代码，**跑一条命令就会明确告诉你"补丁没命中"**，
表现是 FAIL，不会静默出错。

### 21.3 `UsbConnector` 的功能影响范围（把 §11.1 说全）

`AppleUSBHostPort::start` 把 `UsbConnector` 读出来存进 ivar `+0xd4`。全 KC 扫描：

- 访问 `[reg+0xd4]` 的 `AppleUSB*` / `IOUSBHost*` 函数：**0 个**（存进去之后没人再读）；
- 唯一的功能性读取点是 `AppleUSBXHCIPort::getPortType`，而它**只对 `0xFF` 分支**
  （`cmp eax,0xff` / `sete`），其余值一律按"外部标准口"处理。

⇒ 端口类型标错（0 与 3 互换）**不影响功能**，只影响上报描述（系统信息里的连接器类型、
内建口是否显示 `Built-In: Yes`）。真正有功能含义的只有 **255（Internal）**，
而它来自 Windows 侧的实测数据（§17.3），置信度最高。

## 22. 2026-10-11：三处更正 + 稀疏 map 方案（最终结论）

> 本节推翻了本文档前面若干结论。**以本节为准。**

### 22.1 ⚠️ 更正 §9.1：编号公式错了（`acpiPort` 是误读）

**错误根源**：解 vtable 槽位时把 `_ZTV` 符号地址当成了派发偏移基准。
Itanium ABI 里 `_ZTV` 指向 vtable 起点，前两槽是 `offset_to_top` + `typeinfo`，
**派发偏移须从 `vtable + 16` 算起**。加上这个 +16 修正后：

```
IOPCIDevice vtable:  +0x8f0 -> IOPCIDevice::getDeviceNumber
                     +0x8f8 -> IOPCIDevice::getFunctionNumber
  （我原先错读成 findPCICapability / getBusNumber —— 差 2 个槽）
```

函数体（反汇编确证）：

```
getDeviceNumber()   = (config[0xa8] >> 11) & 0x1f
getFunctionNumber() = (config[0xa8] >>  8) & 0x7
```

⇒ **编号字节 = `(PCI function << 5) | (PCI device & 0x1f)`**，**与 ACPI 无关**。
本机验算：

| 控制器 | PCI dev:fn | 公式 | 实测 |
|---|---|---|---|
| XH00 | 0:0 | `0x00` | 先占 → `0x00` |
| XHC0 | 0:3 | `0x60` | `0x60` ✓ |
| XHC1 | 0:4 | `0x80` | `0x80` ✓ |
| XHC2 | **0:0** | **`0x00`** | 被占 → `0x01` |

§8.2/§9.1 里"XH00 没有 ACPI 节点所以 acpiPort=0""XHC2 的 acpiPort=1"**全部作废**。

### 22.2 ★ 新发现：编号由 `reserveBusNumber` 分配，同配置下**会翻转**

反汇编确认 `AppleUSBHostResources::reserveBusNumberGated` 在候选编号被占用时
`inc al` 递增（最多 256 次尝试）。因为 **XH00 与 XHC2 的 PCI 地址都是 `0:0`**，
两者候选值都是 `0x00` ⇒ **谁先注册谁拿 0**。

**实测（同一份配置、两次启动）**：

| 启动 | XH00 | XHC2 |
|---|---|---|
| 第 1 次 | `0x01` | `0x00` |
| 第 2 次 | `0x00` | `0x01` |

设备位置随之整体漂移 `0x01000000`（麦克风 `0x01500000`↔`0x00500000`、
蓝牙、G2000、键盘同样位移）。

**后果**：`AppleUSBAudioControlInterface::GetGlobalUniqueID` 在设备无可用序列号时
**用 locationID 构造 UID** ⇒ 无序列号的音频设备 UID 每次启动漂移，
默认音频设备与按设备保存的偏好可能反复重置。

⇒ **这就是编号注入的价值：把 XHC2 挪到 `0x02`，让 XH00 稳定占 `0x00`。**

### 22.3 ★★ 更正 §1.2 / §16.2：`0xF` 上限数的是"成功创建个数"⇒ 稀疏 map 可行

`AppleUSBXHCI::createPorts` 的端口循环（逐指令核对）：

```
+1344 cmp ebx,0xf / jae → +3303       ; ebx = 成功创建计数
+1391 ports[r14] = createPort(...)     ; r14 = 硬件端口下标
+1402 cmp qword [rax+r14*8],0 / jne → +1667
成功 +1667:  inc edx ; inc r14 ; mov ebx,r13d
跳过 +3303:  mov r14,rbx ; ... ; +3408 jmp   ; 计数器不动，循环继续
循环头 +674: cmp r14,[r15+0x4c8]-1 / jbe     ; 遍历全部硬件端口
```

⇒ **跳过某硬件端口不会推进那个被 `0xF` 比较的计数器**；端口在 locationID 里的
编号来自**成功创建集合中的逻辑序号**（存进端口对象 `+0x398`，`setPortLocation(0)` 读它）。

配合"map 按条目 `port` 值（=**硬件号**）查表"，得到关键结论：

> **用一份只列出必要硬件端口的稀疏 map（条目 ≤15、但可包含硬件口 16/17），
> 这些口会被放进逻辑位置 ≤15 ⇒ 不需要任何内核补丁。**

⇒ §1.2「硬件索引+1、不能重编号」**作废**；
§16.2「那份 7 条 map 丢了 10 个端口」**作废**（那其实是预期的位置压紧）。

### 22.4 物理端口盘点（逐口插拔实测）

| 控制器 | 物理连接器 | SS 半边 | HS 半边 |
|---|---|---|---|
| XH00 | USB3 口 Ⓐ | `#2` `0x00200000` | `#7` `0x00700000` |
| XH00 | USB3 口 Ⓑ | `#3` `0x00300000` | `#8` `0x00800000` |
| XH00 | USB2 ①（麦克风） | — | `#15` `0x00F00000` |
| XH00 | USB2 ②（内置蓝牙） | — | `#16` `0x01000000` |
| XH00 | USB2 ③（原坏口） | — | `#17` `0x01100000` |
| XHC0 | Type-A | `#3` | `#1` |
| XHC0 | Type-C | `#4` | `#2` |
| XHC1 | USB3 口 1 | `#4` | `#2` |
| XHC1 | USB3 口 2 | `#3` | `#1` |
| XHC2 | USB2 口 | — | `#1` |

XH00 未触发的 10 个对象（`#1,#4,#5,#6,#9–#14`）判定为未引出（前置面板未接线）。

两条规律：① **USB3 连接器占两个端口对象**（HS+SS）；
② **配对方式因控制器而异** —— XH00 是 `+5` 正序，XHC1 是反序 ⇒ 只能实测。

### 22.5 实测验证的三阶段（全部通过）

| 阶段 | 配置 | 结果 |
|---|---|---|
| 1 | 稀疏 map + **补丁开** + 注入 | 7 个对象、位置正确、设备全在 ✅ |
| 2A | 稀疏 map + **补丁关** + 注入 | 与阶段 1 **完全相同** ⇒ **补丁无收益** ✅ |
| 2B | 稀疏 map + 补丁关 + **注入也撤** | 功能正常但**编号两次启动翻转** ❌ 不稳定 |

### 22.6 最终配置（两个外部模型独立给出同一结论）

```
Kernel > Patch      那两条 = Enabled false   （无收益；条目保留便于回溯）
Kernel > Quirks     XhciPortLimit = false
DeviceProperties    PciRoot(0x0)/Pci(0x8,0x3)/Pci(0x0,0x0)
                      USBBusNumber = 2
                      bus-number   = <02 00 00 00>   ← 4 字节，读方取首字节
UTBMap.kext         XH00: IOPCIPrimaryMatch=0x43fc1022, 稀疏 7 条, port-count=17
                    XHC0/XHC1/XHC2 未改动
```

目标效果：XH00 稳定 `0x00000000`、端口 `0x00100000`–`0x00700000`；
XHC2 稳定 `0x02000000`（键盘 `0x02100000`）；蓝牙 `0x00600000`、原坏口 `0x00700000`、
麦克风 `0x00500000`；无同值冲突。

### 22.7 退化路径（两个模型给的同一张表）

| 场景 | 结果 |
|---|---|
| map 失效 + 补丁开 + 注入有效 | 17 口；XHC2 在 `0x02` ⇒ 不撞车（退化为原方案） |
| map 失效 + 补丁关 | 只剩 15 口 ⇒ 丢蓝牙和原坏口 |
| 任一场景 + 注入也失效 | XHC2 回 `0x01` ⇒ 撞击重现 |

### 22.8 残余风险 / 未验证项

- 编号依赖未公开属性 `bus-number`/`USBBusNumber`（苹果私有代码路径，随版本可能变）；
- **睡眠/唤醒未测**；hub 下游未测；长期稳定性未测；
- `reserveBusNumberGated` 的完整输入排序未彻底反汇编；
- 前置面板接线后需重新盘点；
- **不要用工具重新生成 map**（稀疏后逻辑位置与硬件号已分离，工具会混淆）。

## 23. 2026-10-11：**最终方案（全部 26 个对象可用）** + 升级流程

### 23.1 结论

用**两个部件**同时启用，XH00 建出全部 **17 个端口对象**，四个控制器共 **26 个对象**全部创建：

```
XH00 17（后置 2×USB3 口的 4 个半边 + 3 个纯 USB2 口 + 10 个内部插针对象）
XHC0  4（后置 Type-A + Type-C）
XHC1  4（后置 2× USB3 Type-A）
XHC2  1（后置 USB2 Type-A）
```

| 部件 | 值 | 作用 |
|---|---|---|
| `Kernel > Quirks > XhciPortLimit` | **`true`** | 放宽内核两处 `0xF` 阈值（上游 OpenCore 补丁） |
| `DeviceProperties`（XHC2 的 PCI 设备） | `bus-number = <02 00 00 00>` | 把 XHC2 挪到 `0x02`，腾出 `0x01000000`/`0x01100000` |
| `UTBMap.kext` XH00 | **17 条** ports（`port` = 硬件号 1..17），`port-count = 17` | 建哪些端口、名字、类型 |

**两者缺一不可**：

```
port 16/17 的 locationID 必然溢出到 0x01000000/0x01100000（4 bit 端口字段装不下）
  放宽阈值 → 让 port 16/17 能被创建
  编号注入 → 让 0x01000000/0x01100000 空出来
只放宽阈值，不挪号 → 与 XHC2 撞车（本项目最初的问题）
只挪号，不放宽阈值 → port 16/17 仍建不出来（15 上限）
```

**新认识**：port 16/17 现在**又落回** `0x01000000`/`0x01100000`（当初"撞车"的那些值），
不撞的原因**只是 XHC2 被挪走了**。⇒ 当初用于消冲突的编号注入，
实际上是"全支持"得以成立的**前提条件**，而不是可选的优化。

### 23.2 实测验收（2026-10-11，macOS 15.8.1 / 24H32）

| 验收项 | 期望 | 实测 |
|---|---|---|
| XH00 端口对象 | 17 | **17** ✅ |
| 端口位置 | `0x00100000`–`0x01100000` | **连续 17 个** ✅ |
| 内置蓝牙（硬件口 16） | `0x01000000` / conn 255 / Built-In | **全对** ✅ |
| 后置 Type-A（硬件口 17，原损坏） | `0x01100000`，设备在位 | **对** ✅ |
| 麦克风（口 15） | `0x00F00000` | **对** ✅ |
| XHC0 / XHC1 / XHC2 | 编号未变 | **对** ✅ |
| 同值冲突 | 无 | **无** ✅ |
| `failed to create device` | 0 | **0** ✅ |

### 23.3 为什么改用上游 `XhciPortLimit` 而不是手写补丁

核实了上游的维护情况（本机 `ocpkg` 源码 + GitHub 提交历史）：

```
OpenCore 源码里内置（CommonPatches.c）：
  mRemoveUsbLimitV2Patch          Base=__ZN12AppleUSBXHCI11createPortsEv
                                  Find 0F 0F 83 → Replace 40 0F 83
  mRemoveUsbLimitIoP1Patch1       Base=__ZN16AppleUSBHostPort15setPortLocationEj
                                  Find 0F 0F 87 → Replace 40 0F 87
  mRemoveUsbLimitIoP1Patch1Tahoe  同上，但符号是 setPortLocationEv（macOS 26 签名变了）

版本分派：PatchUsbXhciPortLimit1/2/3  →  IOUSBHostFamily / AppleUSBXHCI / AppleUSBXHCIPCI

上游提交历史：
  2025-11-19  000227e4  Improve XhciPortLimit compatibility with macOS Tahoe (#590)
  2023-06-13  fb43d903  Better XhciPortLimit patch on macOS 11+
  2023-06-12  d52fc46b  Integrate USB restriction patches (#471)
```

⇒ **上游确实在维护**，最近一次就是为 macOS 26 做的适配。这是选它的理由：
**"逐版本重推内核字节"这件事外包给了 OpenCore 上游。**

取舍要如实说：上游用**短模式**（`0F 0F 87`，3 字节）比手写的（6 字节）**更容易误命中**，
靠 `Base` + `Limit` 窗口 + `Count=1` 约束。所以**手写的那两条没有删**，
保持 `Enabled=false` 作为备份 —— 万一上游在某一版失配，可以改用它们。

### 23.4 升级 macOS 的流程

**两条命令**：

```bash
# 升级前（备份 + 记录基线）
sudo bash tools/pre-upgrade-backup.sh

# 升级后（自检，退出码 0 = 通过）
python3 tools/post-update-check.py
```

新增脚本 `tools/pre-upgrade-backup.sh`：备份 `config.plist` 与 `UTBMap.kext`
（名字带 build 号），并生成一份基线记录（含校验值、关键配置、预期端口数、回滚命令）。

自检脚本 `tools/post-update-check.py` 已把期望值同步为 17 条 / `0x01100000`，
并新增检查 `XhciPortLimit` 是否为 `true`。

**维护责任的分布**：

| 部件 | 谁维护版本适配 |
|---|---|
| `XhciPortLimit`（阈值） | **OpenCore 上游** |
| `bus-number` 注入 | 自己核（自检会报） |
| 17 条端口表 | 无需维护（与版本无关） |

**升级顺序**：先更新 OpenCore 到最新版，**再**升级 macOS —— 这样新系统若改了字节，
上游的新补丁已经就位。

**最需要警惕的失效形态**：阈值补丁失效 ⇒ 只剩 15 个对象 ⇒ **蓝牙与原坏口消失**
（正是本项目要修的那两个）。退路依次是：① 确认 `XhciPortLimit=true`；
② 更新 OpenCore；③ 改用手写的两条（按新版重推字节）；
④ 应急退回 7 条稀疏 map（≤15，不依赖任何内核补丁，后置 9 口仍可用）。

### 23.5 并发变更提醒

本机 `config.plist` 在多个会话间被反复修改（另一个会话在调 NVRM / boot-args 等）。
本轮所有修改都采用**定向替换**（只改目标键，其余逐字节不动），并写入前复核 sha，
避免覆盖他人改动。**本项目相关设置已连续多次确认完好**。

各次备份（ESP 上）：`config.plist.hl-bak-7map-*`、`config.plist.pre<build>-*`。
