# corpnewt/USBMap issue #63 与相关社区配方

## 1. issue 元数据

- 仓库：`corpnewt/USBMap`
- 编号：#63
- 标题：`Removing USB Port Limit on macOS 26`
- 作者：`Wate321`
- 创建：2025-07-11
- 状态：open
- 评论：0 条（发布时未经过维护者复核）

## 2. 配方原文（节选，逐字）

> 2. Locate the instruction controlling the USB port limit (e.g., `cmp reg, 0Fh`) and
> change `0Fh` to `3Fh` (i.e., increase from 15 ports to 63).
> 3. Similarly, modify the relevant functions in IOUSBHostFamily to loosen verification.
> 4. Convert these changes into binary patches (patched bytes).

补丁构造示例：

```
Search Pattern (A + D + C):
4489F3E9 6A010000 4183FE0F 0F837C08 0000418D 5E01

Replace Pattern (A + B + C):
4489F3E9 6A010000 4183FE3F 0F837C08 0000418D 5E01
```

四个站点：

1. `AppleUSBXHCI::createPortEPN(...)`：`0Fh` → `3Fh`
2. `AppleUSBXHCI::createPorts(void)`：`0Fh` → `40h`
3. `AppleUSBHostPort::setPortLocation(...)` 的 `cmp`：`0Fh` → `40h`
4. `AppleUSBHostPort::setPortLocation(...)` 的其余 `0Fh`：`3Fh`

## 3. 配方能覆盖什么、覆盖不了什么

能覆盖：

- 放宽 `createPorts` / `setPortLocation` 的 15 计数比较；
- 放宽 `and reg,0xF` 的 4 bit 掩码（到 6 bit）；
- 额外补了 `createPortEPN`（OpenCore 当前源码没有这个站点）。

覆盖不了：

- 不碰 `generateBusNumber` / `reserveBusNumberGated`，控制器字节（locationID bits24-31）
  的分配与保留规则不变；
- 不碰 `setPortLocation` 的 `shift = 20` 与向下找空半字节的循环；
- 不碰 Hub 层级（每级 Hub 吃掉一个 4 bit 半字节）；
- 不碰 locationID 的消费者（音频 UID、HID、System Information 等）。

## 4. 数据类型与阈值细节

### 4.1 立即数是 8 bit，掩码只有 6 bit

`cmp reg, 0Fh` / `cmp reg, 40h` 用的是 `83 /7 ib` 形式的 8 bit 立即数，所以
`0F`→`3F`/`40` 在编码长度上成立；但 `and reg, 0Fh`→`3Fh` 只给出 6 bit 端口域，
最大可表示 63。配方里 `cmp` 写 `0x40`（允许 0..63）与 `0x3F` 掩码是配套的，
不存在“64 个端口”的编码空间。

### 4.2 6 bit 端口域与控制器字节重叠

locationID 的端口域从 bit20 开始。6 bit 端口域占 bit20–25，而控制器字节占
bit24–31；两者在 bit24–25 重叠。端口 16 的 `16<<20 = 0x01000000` 正好是
“控制器字节 = 1”的基址，端口 17 是 `0x01100000`。因此：

- 若某个控制器的 bus 字节是 1，端口 16/17 与它的基址/端口同名；
- 把该控制器挪到字节 2 只能避开这一组冲突，不代表编码本身可区分。

### 4.3 搜索模式绑定具体构建

示例里的 `E9 6A010000`（相对跳转）与 `0F 83 7C080000`（`jae rel32`）都是带
位移的指令。换一个 macOS build，位移变化就会失配；按字节串搜索还可能命中
同序字节的其它函数。配方本身也写明是“macOS 26”的 `BootKernelExtensions.kc`。

### 4.4 没有编号分配器

配方从头到尾没有提到 `bus-number` / `USBBusNumber` 属性，也没有提到
`generateBusNumber`。它默认“端口 16/17 算出来的值不会撞”，这在本机拓扑上
只在“控制器字节 1 未被占用”时成立；一旦枚举顺序或注入变化，冲突会回来。

## 5. 同源补丁仓库（Wate）的状态

`WaTeZhuangJiDingZhi/wate@a2ba2e0b0f2296883cb8638ab8ec5a049d1c387a`
（2026-05-07）里的 `Wate-macOS-XhciPortLimit-Patch.plist`：

| 条目 | Identifier | Find | Replace | Count |
| --- | --- | --- | --- | --- |
| BigSur-Monterey | `com.apple.iokit.IOUSBHostFamily` | `eb 0a 41 83 e6 0f 41 d3` | `eb 0a 41 83 e6 3f 41 d3` | 1 |
| Ventura | `com.apple.iokit.IOUSBHostFamily` | `41 83 e5 0f 41 d3 e5 44 09 eb` | `41 83 e5 3f 41 d3 e5 44 09 eb` | 1 |
| Sonoma | `com.apple.iokit.IOUSBHostFamily` | 空 | 空 | **0** |

README 原文：

> 抱歉Sonoma apple已经更换补丁，我需要重新查找，所以以上补丁适用于11-13，
> Sonoma的需要重新查找，最近比较忙

判定：这是一组只改 `0F`→`3F` 的掩码补丁，而且是**寄存器硬编码**（`e6`/`e5`），
跨版本立即失配；Sonoma 条目 `Count=0`，等于没实现。它不能作为 15.8.1 的候选。

## 6. USBMap 文档对 15 口与补丁风险的说明

`corpnewt/USBMap@e07b6c0f19e8112887741bd486545d3c8411040e` 的 `Information.md`：

`:114`

```
Apple now imposes a 15 USB port per controller limit. ... some have third party chipsets
that can share the load (since *each* controller has its own 15 port limit).
```

`:118-120`

```
it's actually a way to limit the addressing of ports to a single 4 bit address. ...
the first port we add to that RHUB shows up at address `@14100000`, the second at `@14200000`,
the 10th at `@14a00000`, and the 15th at `@14f00000`. ... *anything* above that `f` address
**gets ignored** though...
```

`:180-186`

```
OpenCore has a quirk that attempts to do this on any OS version called `XhciPortLimit`.
... The port limit is *hardcoded* in a ton of places all over the OS, and as we're only
lifting it in a few, this causes access outside the bounds of a fixed array. ...
Ultimately, it's considered *best practice* to **only** leverage the port limit patch for
the mapping process, and then to disable it.
```

`:237`

```
Each of these ports are referenced by their `port` number (HS02 is port number `<02000000>` -
which when converted from little-endian Hex to an integer is port 2).
```

这几段与 OpenCore 源码互证：15 来自 4 bit 端口域；社区补丁只动少数几处；
map 的键名（HS02）不是端口号，`port` 字段才是。
