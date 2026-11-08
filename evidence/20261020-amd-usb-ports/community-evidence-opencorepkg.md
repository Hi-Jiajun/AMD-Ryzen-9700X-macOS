# OpenCorePkg XhciPortLimit 一手证据（固定 commit）

固定版本：`acidanthera/OpenCorePkg@45e7636f24bb4a50b32baeb5170cdfc9f589ea3c`
（2026-10-19，取数日 2026-10-20 的上游 HEAD）

## 1. 配置开关只做一件事：把三个 quirk 全跑一遍

`Library/OcMainLib/OpenCoreKernelPatch.c:135-139`

```c
    if (Config->Kernel.Quirks.XhciPortLimit) {
      OcKernelApplyQuirk (KernelQuirkXhciPortLimit1, CacheType, DarwinVersion, Context, NULL);
      OcKernelApplyQuirk (KernelQuirkXhciPortLimit2, CacheType, DarwinVersion, Context, NULL);
      OcKernelApplyQuirk (KernelQuirkXhciPortLimit3, CacheType, DarwinVersion, Context, NULL);
    }
```

`Include/Acidanthera/Library/OcAppleKernelLib.h:626-630`

```c
  //
  // Apply port limit patches to AppleUSBXHCI and AppleUSBXHCIPCI kexts.
  //
  KernelQuirkXhciPortLimit1,
  KernelQuirkXhciPortLimit2,
  KernelQuirkXhciPortLimit3,
```

## 2. 实际补丁只有四个站点（阈值 / 掩码）

文件：`Library/OcAppleKernelLib/CommonPatches.c`

### 2.1 `AppleUSBXHCIPCI::createPorts`（10.11–10.13，V1）

`CommonPatches.c:659-682`

```c
  mRemoveUsbLimitV1Find[] = {
  0xFF, 0xFF, 0x10
};
  mRemoveUsbLimitV1Replace[] = {
  0xFF, 0xFF, 0x40
};
  mRemoveUsbLimitV1Patch = {
  .Comment     = DEBUG_POINTER ("RemoveUsbLimitV1"),
  .Base        = "__ZN15AppleUSBXHCIPCI11createPortsEv",
```

判定：把比较立即数 `0x10` 改成 `0x40`，只是放宽阈值。

### 2.2 `AppleUSBXHCI::createPorts`（10.13+，V2）

`CommonPatches.c:686-708`

```c
  mRemoveUsbLimitV2Find[] = {
  0x0F, 0x0F, 0x83
};
  mRemoveUsbLimitV2Replace[] = {
  0x40, 0x0F, 0x83
};
  mRemoveUsbLimitV2Patch = {
  .Base        = "__ZN12AppleUSBXHCI11createPortsEv",
```

判定：`cmp ...,0x0F` + `jae` 的立即数 `0x0F` 改成 `0x40`；仍是阈值。

### 2.3 `AppleUSBHostPort::setPortLocation(UInt32)` part 1（10.14.5+）

`CommonPatches.c:713-751`

```c
  mRemoveUsbLimitIoP1Find1[] = {
  0x0F, 0x0F, 0x87
};
  mRemoveUsbLimitIoP1Replace1[] = {
  0x40, 0x0F, 0x87
};
  mRemoveUsbLimitIoP1Patch1 = {
  .Base        = "__ZN16AppleUSBHostPort15setPortLocationEj",
```

macOS 26（Darwin 25）起同一补丁换符号，仍改同一字节：

`CommonPatches.c:738-751`

```c
  mRemoveUsbLimitIoP1Patch1Tahoe = {
  .Base        = "__ZN16AppleUSBHostPort15setPortLocationEv",
```

判定：`cmp r15d,0xF` + `ja` 的立即数 `0x0F` 改成 `0x40`；仍是阈值。

### 2.4 `AppleUSBHostPort::setPortLocation` part 2（11.1+，掩码）

`CommonPatches.c:754-798`

```c
  mRemoveUsbLimitIoP1Find2[] = {
  0x41, 0x83, 0x00, 0x0F,  ///< and whatever, 0x0Fh
  0x41, 0xD3, 0x00,        ///< shl whatever, cl
  0x00, 0x09, 0x00         ///< or ebx, whatever
};
  mRemoveUsbLimitIoP1Mask2[] = {
  0xFF, 0xFF, 0x00, 0xFF,
  0xFF, 0xFF, 0x00,
  0x00, 0xFF, 0x00
};
  mRemoveUsbLimitIoP1Replace2[] = {
  0x00, 0x00, 0x00, 0x3F,
  0x00, 0x00, 0x00,
  0x00, 0x00, 0x00
};
  mRemoveUsbLimitIoP1Patch2 = {
  .Base        = "__ZN16AppleUSBHostPort15setPortLocationEj",
```

判定：把 `and reg,0x0F` 的立即数改成 `0x3F`，只把 4 bit 掩码放宽到 6 bit。
`.Mask` 里把 ModRM/寄存器字节置 0，所以能匹配不同寄存器编码；仍然只改一个立即数。

## 3. 上游自己承认没有 locationID 方案

`CommonPatches.c:881-899`（`PatchUsbXhciPortLimit2` 函数体开头）

```c
  //
  // TODO: Implement some locationID hack in IOUSBHostFamily.
  // The location ID is a 32 bit number which is unique among all USB devices in the system,
  // and which will not change on a system reboot unless the topology of the bus itself changes.
  // See AppleUSBHostPort::setPortLocation():
  // locationId = getLocationId();
  // if (!(locationId & 0xF)) {
  //   int32_t shift = 20;
  //   while (locationId & (0xF << shift)) {
  //     shift -= 4;
  //     if (Shift < 0) { setLocationId(locationId); return; }
  //   }
  //   setLocationId(locationId | ((portNumber & 0xF) << shift));
  // }
  // The value (e.g. 0x14320000) is represented as follows: 0xAABCDEFG
  // AA  — Ctrl number 8 bits (e.g. 0x14, aka XHCI)
  // B   - Port number 4 bits (e.g. 0x3, aka SS03)
  // C~F - Bus number  4 bits (e.g. 0x2, aka IOUSBHostHIDDevice)
  //
  // C~F are filled as many times as many USB Hubs are there on the port.
  //
```

这是上游源码里唯一与“完整编码”相关的段落，且是 `TODO`；代码没有任何
allocator、hub 层级或消费者改造。

## 4. 官方配置文档的结论

`Docs/Configuration.tex:2991-3002`

```
  \texttt{XhciPortLimit}\\
  \textbf{Type}: \texttt{plist\ boolean}\\
  \textbf{Failsafe}: \texttt{false}\\
  \textbf{Requirement}: 10.11+ (not required for older)\\
  \textbf{Description}: Patch various kexts (AppleUSBXHCI.kext, AppleUSBXHCIPCI.kext,
  IOUSBHostFamily.kext) to remove USB port count limit of 15 ports.

  \emph{Note}: This option should be avoided whenever possible. USB port limit
  is imposed by the amount of used bits in locationID format and there is no
  possible way to workaround this without heavy OS modification. The only
  valid solution is to limit the amount of used ports to 15 (discarding some).
  More details can be found on \href{https://applelife.ru/posts/550233}{AppleLife.ru}.
```

## 5. 覆盖范围结论

| 站点 | 符号 | 版本门 | 改的是什么 | 是否触及编码/分配器/hub |
| --- | --- | --- | --- | --- |
| V1 | `AppleUSBXHCIPCI::createPorts` | 10.11–10.13 | `cmp 0x10`→`0x40` | 否 |
| V2 | `AppleUSBXHCI::createPorts` | 10.13+ | `cmp 0x0F`→`0x40` | 否 |
| IoP1 part 1 | `AppleUSBHostPort::setPortLocation` | 10.14.5+（26 换符号） | `cmp 0x0F`→`0x40` | 否 |
| IoP1 part 2 | `同上` | 11.1+ | `and 0x0F`→`0x3F` | 否 |

没有 `createPort`（单端口）补丁，没有 `AppleUSBHub` 补丁，没有
`generateBusNumber` / `reserveBusNumber` 补丁，没有 locationID 消费者适配。
所谓“part-1/2/3/13+”在本仓库对应的是上述四个站点加版本门，不是四段编码改造。
