# macOS 公共 USB 宿主控制器接口（本机 SDK / 系统只读证据）

本机：macOS 15.8.1 / 24H32。
SDK：`/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk`
（`MacOSX26.2.sdk` 内容一致）。

## 1. 公开 ABI 自带“每控制器 ≤15 口”

`IOUSBHost.framework/Headers/IOUSBHostControllerInterfaceDefinitions.h:264`

```c
    IOUSBHostCICapabilitiesMessageControlPortCount                     = IOUSBBitRange(16, 19),
```

`:283`

```c
    IOUSBHostCIPortCapabilitiesMessageControlPortNumber                 = IOUSBBitRange(16, 19),
```

`:420`

```c
    IOUSBHostCIPortEventMessageData0PortNumber         = IOUSBBitRange(0, 3),
```

判定：

- 控制器的“端口总数”字段是 bit16–19，4 bit，最大 15；
- 每个端口的“端口号”字段也是 bit16–19，4 bit；
- 端口事件的端口号是 bit0–3，4 bit。

也就是说，4 bit 端口号不是某一个 kext 的局部检查，而是**公开消息协议本身**
的字段宽度。即使自己实现一个虚拟控制器，单个实例也只能有 15 个端口。

## 2. 用户态控制器接口存在，但要 Apple 授权

`IOUSBHost.framework/Headers/IOUSBHostControllerInterface.h:17-19`

```objc
 * @brief   The object representing a user-mode USB host controller
 * @details IOUSBHostControllerInterface enables a process to instantiate a USB host controller
 *          to provide access to remote USB devices or create synthetic USB devices.
 *          The entitlement com.apple.developer.usb.host-controller-interface is required to use this class.
```

判定：

- 这是“新建一个宿主控制器”（远端设备 / 合成设备），不是“接管或拆分一个真实 xHCI”；
- 需要 `com.apple.developer.usb.host-controller-interface` entitlement，普通开发者签名拿不到；
- 没有公开 API 把真实 xHCI 的 MMIO、命令环、设备槽、端点环交给这个用户态对象。

内核侧对应物（只读系统文件）：

`/System/Library/Extensions/IOUSBHostFamily.kext/Contents/PlugIns/AppleUSBVHCI.kext/Contents/Info.plist`

```
CFBundleIdentifier = com.apple.driver.usb.AppleUSBVHCI
CFBundleName       = I/O Kit Driver for USB VHCI Controllers
CFBundleVersion    = 1.2
```

## 3. locationID 只有“属性键”，没有 setter

`IOUSBHost.framework/Headers/IOUSBHostDefinitions.h:211`

```c
extern IOUSBHostPropertyKey IOUSBHostPropertyKeyLocationID;
```

公开头文件里没有 `setLocationID` / `setPortLocation` 的等价 API；locationID 是
内核计算并写入 IORegistry 的属性，用户态只能读。

## 4. 与 Apple 原生多口设计的关系

可以从公开证据确定的只有两点：

1. 每控制器 15 口是 4 bit 字段的结果；超过 15 个物理口需要更多控制器或 Hub
   （Hub 每级再吃一个 4 bit 半字节）。
2. Apple 自己的内部设备属性里存在 “Mac Internal Hub Properties” 这类
   `IOUSBHostDevice` 匹配项（`AppleUSBHostPlatformProperties.kext` 的
   `IOKitPersonalities`），说明内部设备走 Hub 是 Apple 的正常拓扑。

不能从公开证据确定：某个具体 Apple 机型是“几个控制器 + 几级 Hub”。本轮没有
拆机/ACPI dump，不能把某一机型拓扑当成通用结论。

## 5. 对“软件拆分一个真实 xHCI”的判定

公开接口层面没有这样的能力：

- `IOUSBHostControllerInterface` 只能新建控制器；
- 其端口字段仍是 4 bit；
- 需要 entitlement；
- 没有接管真实 PCI xHCI 的公开入口。

如果要做，只能自研内核驱动：独占真实 xHCI 的 MMIO/中断/命令环/设备槽/端点环/DMA，
再把端口分组，通过私有或自建前端暴露给 Apple 栈。这属于独立研发项目，不是
“打开某个开关/改几个字节”。本轮没有任何公开实现可引用。
