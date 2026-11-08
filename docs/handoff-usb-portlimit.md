# 交接：突破 macOS 的 15 端口上限（让 Port 16/17 可用）

> 新会话从这里开始。目标：让本机 XH00 控制器的 **Port 16（内置蓝牙）** 与 **Port 17（后置 USB2 Type-A）**
> 在 macOS 15.8.1 上可用。完整证据链见 `docs/device-driver-feasibility.md` §1、§6、§9；
> 本文件是可直接开工的摘要。

## 1. 现象与根因

| 侧 | XH00（`1022:43fc`，ACPI `\_SB.PCI0.GPP7.UP00.DP60.XH00`）的端口 |
| --- | --- |
| Windows | **17 个端口**，占用 2、3、7、8、15、**16**、**17** |
| macOS | 只创建 **15 个**：5×USB3（`0x00100000`–`0x00500000`）+ 10×USB2（`0x00600000`–`0x00f00000`） |

**`locationID` 是 32 位**：控制器 8 位（bits 24–31）+ **端口 4 位（bits 20–23）** + 每级 hub 一个半字节。
端口号只有 4 bit ⇒ 装不下 16/17。

## 2. locationID 编码算法（已反汇编确证）

`AppleUSBXHCIPCI::generateBusNumber` @ `0xffffff8002e2c754`：

```asm
+26  call [rax+0x8f0]        ; 取 acpiPort
+35  and  r15b, 0x1f
+49  call [rax+0x8f8]        ; 取 PCI 设备号
+55  shl  al, 5              ; devNum << 5
+58  or   al, r15b
+61  mov  [rbx], al          ; 字节 = (devNum << 5) | (acpiPort & 0x1f)
+86  jmp  [rax+0xad0]        ; 尾调用 setLocationID
```

实测验算全部吻合：

| 控制器 | `_ADR` | 实测字节 | 验算 |
| --- | --- | --- | --- |
| XHC0 | 3 | `0x60` | (3<<5)\|0 ✓ |
| XHC1 | 4 | `0x80` | (4<<5)\|0 ✓ |
| XHC2 | 0 | `0x01` | (0<<5)\|1 ✓ （`acpiPort`=1） |
| XH00 | 0 | `0x00` | (0<<5)\|0 ✓ （`acpiPort`=0） |

⇒ 字节是**拓扑的确定性函数**，不是自由参数。要重新分配控制器编号，改的就是 `+55`/`+58` 那两条
（或 `acpiPort` 的来源），**不是去补端口号的掩码**。

## 3. 为什么"只打开 XhciPortLimit"有害（本机实测）

| 补丁 | 模式 | 本机（15.8.1）是否命中 |
| --- | --- | --- |
| OpenCore part-1（上限 `0F`→`40`，在 `AppleUSBHostPort::setPortLocation`） | `[0F 0F 87]` | ✅ **仍命中**（+57） |
| OpenCore part-2（把 4bit 掩码放宽） | `[41 83 ?? 0F]`（`and eax,0xf`） | ❌ **已失效**（15.8.1 改成 `test al,0xf`） |

⇒ 只放宽上限、不放宽掩码 ⇒ 端口号被**截断**（17 变回 1，同控制器内自相混淆）。

## 4. 冲突是**可证明的**（不是抽象风险）

`tools/research/usbfix-sim2.py` 的输出：

```
A. 现状（端口 ≤15）            全机 27 个 locationID，冲突 0
B. 只放宽检查：XH00 port 17 -> 0x00100000   ⚠️ 与 XH00 port 1 撞车
C. 放宽检查+掩码 0x3F：XH00 port 17 -> 0x01100000   ⚠️ 与 XHC2 port 1 撞车
```

根因：**本机 XH00 的控制器字节恰好是 `0x00`**，端口字段紧贴 bit 20，**上面就是别的控制器的地盘**：

```
XH00 port 16 = 0x01000000  ← 等于 XHC2 的控制器基址
XH00 port 17 = 0x01100000  ← 完全等于 XHC2 port 1 的 locationID
```

## 5. 已定位的反汇编坐标（macOS 15.8.1 / 24H32，`BootKernelExtensions.kc`）

| 符号 | 地址 | 关键点 |
| --- | --- | --- |
| `AppleUSBHostPort::setPortLocation(unsigned int)` | `0xffffff8002cd638a` | `+54 cmp r15d,0xf`；`+58 ja`；`+649 test al,0xf`；`+846 test al,0xf`；`+987 shl r15d,cl`；`+990 or r12d,r15d` |
| `AppleUSBXHCI::createPorts()` | `0xffffff8002deaa58` | `+1344 cmp ebx,0xf`；`+1346` 命中 `0F 0F 83` |
| `AppleUSBXHCI::createPort(...)` | `0xffffff8002debb4a` | **无**上限比较 |
| `AppleUSBXHCIPCI::generateBusNumber(unsigned char&)` | `0xffffff8002e2c754` | 见 §2 |

## 6. 现成工具（都在 `tools/research/`）

| 脚本 | 用途 |
| --- | --- |
| `usbfix-sim2.py` | **冲突模拟器**：按实测布局算 locationID，报冲突数（已在用） |
| `kcdis.py` | 反汇编 KC 中的任意符号（capstone，支持指定偏移窗口） |
| `patcheck.py` | 检查 OpenCore 的补丁模式是否仍命中 |
| `xhci-addr.py` | 列 `AppleUSBXHCIPCI` 的函数符号 |
| `gen-busnum.py` | 专门反汇编 `generateBusNumber` / `registerService` |

⚠️ 注意：脚本名不要叫 `dis.py`（与 Python 标准库同名，会导致 capstone 循环导入失败）。

## 7. 参考配方（公开出现过的路线）

`corpnewt/USBMap` issue **#63「Removing USB Port Limit on macOS 26」**（2025-07）给了 IDA 步骤与字节对，改 4 处：

| 位置 | 动作 |
| --- | --- |
| `AppleUSBXHCI::createPort`（createPortEPN） | `0F`→`3F`（cmp） |
| `AppleUSBXHCI::createPorts` | `0F`→`40` |
| `AppleUSBHostPort::setPortLocation` | cmp `0F`→`40`；**其余 `0F`→`3F`**（放宽掩码） |
| `IOUSBHostFamily` 相关函数 | 同上放宽验证 |

其示例替换对：`4489F3E96A010000 4183FE0F 0F837C080000418D5E01` → 同串改 `4183FE3F`。（macOS 26 的字节，15.8.1 需重推。）

## 8. 建议的推进顺序

1. **先做"补丁校验器"**：给定候选补丁 → 在 KC **副本**上应用 → 反汇编出"确切改了哪几个字节、落在哪个函数"
   → 用 `usbfix-sim2.py` 算冲突数。目标：每版 macOS 适配都是"跑一条命令、看冲突数是否为 0"。
2. **设计"全控制器协同"的字段划分**：要把端口字段从 4 位加宽到 ≥5 位，且**所有控制器基址的划分同步调整**
   （否则冲突只是换个位置）。涉及三处协同：`AppleUSBHostController` 基址 + `AppleUSBHostPort` 端口字段
   + `IOUSBHostFamily` 解析。
3. ⚠️ 当前布局（XH00 字节 = `0x00`）下**冲突数永远不会是 0** —— 所以真正的第一步是**搞清 `acpiPort` 从哪来、
   能否改**（改它比改字段划分更省）。
4. 设计验证通过后再上机实测（**非关键端口优先**）。

## 9. 风险与缓解（逐条）

| 风险 | 有无解 |
| --- | --- |
| 误伤无关代码 | ✅ `Base` 限定符号 + `Count` + **改前离线比对字节** |
| locationID 冲突 / 数据损坏 | ✅ **能提前算出来**（模拟器已做），但本机必然冲突 ← 前提问题 |
| 逐版本重做 | ✅ 脚本化"模式是否命中"检测 |
| 签名/信任链 | ✅ **风险基本为零**：OpenCore `Kernel > Patch` 是**引导时内存补丁**，磁盘 KC 保持签名、不动 SIP；回滚 = 删条目 |
| 编码本身装不下 | ⚠️ 不是风险，是**硬约束** ⇒ 必须"改编码 + 全控制器协同" |

## 10. 时间线提醒（给上游用）

- 上游 1.1.0 的 `bootfix.py` 会在失败启动后**自动**把 `DevirtualiseMmio=True + SetupVirtualMap=True`，
  并在下次日志后加白名单 —— 但它**也会打开 `SetupVirtualMap`**，而 `policy.py` 对 Ryzen 是设 `False` 的。
  本机当前用 `False` 正常启动，**若将来重建盘导致它变 True，需重新验证能否启动**。
- 我们当前生效的配置（不要动）：`DevirtualiseMmio=True`、`MmioWhitelist=[0xFD000000 Enabled]`、
  `ResizeAppleGpuBars=0`、`ResizeGpuBars=-1`、`Kernel.Block=[]`、`ExposeSensitiveData=7`、`LauncherOption=Full`。

## 11. 用户结论（背景）

内置蓝牙（`0e8d:0616`）在 **Port 16**、失效的那个 USB2 Type-A 在 **Port 17**（三份独立来源交叉验证：
Linux `usb1-port11`/`port12` = Windows Port 16/17 = macOS `0x01000000`/`0x01100000`）。
用户已决定：**蓝牙等这个补丁做好后，与 MT7922 WiFi 会话一起开发**。
替代方案（零风险）：加一张 PCIe USB 卡（每个控制器独立享 15 个额度，FL1100/ASM1042 通常免驱）。
