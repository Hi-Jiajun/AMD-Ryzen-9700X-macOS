# CPU 虚拟化：Apple Hypervisor.framework 没有 AMD 后端

结论先行：**这台机器上 Apple 的虚拟化栈永远不可用，没有任何 OpenCore 配置能改变这一点。**

## 观测事实（2026-10-19，macOS 15.8.1）

```
$ sysctl kern.hv_support kern.hv_vmm_present machdep.cpu.vendor
kern.hv_support: 0
kern.hv_vmm_present: 0
machdep.cpu.vendor: AuthenticAMD

$ sysctl -a | grep -i vmx     # 空
```

BIOS 里 **SVM 是 Enabled**（R6 BIOS 目视核对，2026-10-21）。所以缺的不是固件开关。

## 为什么

XNU 的虚拟化支持只有一条实现路径：

1. `hv_support_init()` 调用 `vmx_hv_support()`；
2. `vmx_hv_support()` 检查 Intel CPUID 的 VMX 位；
3. 通过之后，后续初始化还要读 VMX MSR、执行 `VMXON` 进入 VMX root operation、建立 `VMCS`。

AMD 的对应物是 SVM（`SVM` CPUID 位、`EFER.SVME`、`VMCB`），指令集和 MSR 完全不同。XNU 里没有任何代码去走这条路。

**CPU 伪装解决不了。** `cpuid` / SMBIOS / `revpatch` 这类 OpenCore 手段只能改变"报告出来的字符串"。它们不能：

- 造出 Intel 的 VMX MSR（`IA32_VMX_BASIC` 等）——那些 MSR 在 AMD CPU 上不存在，读会得到 `#GP`；
- 让 `VMXON` / `VMCS` 这些指令变得可执行。

也就是说，缺少的是**硬件能力和操作系统实现的组合**，不是"识别问题"。

## 后果

| 依赖 Apple VM 路径的东西 | 状态 |
| --- | --- |
| QEMU + HVF（`-accel hvf`） | 不可用 |
| UTM（`vz` 后端 = Virtualization.framework） | 不可用 |
| Docker Desktop for Mac | 不可用 |
| `Virtualization.framework` 的任何消费者 | 不可用 |
| 纯模拟 QEMU（TCG，`-accel tcg`） | 理论可用，慢到没有实用价值；本机未安装 QEMU |

## 与 AppleVTD 的关系：两个独立的缺口

这一点容易混。它们都表现为"Intel 有、AMD 没有"，但修好任何一个都不会让另一个可用：

| 缺口 | 是什么 | 影响 |
| --- | --- | --- |
| **CPU 虚拟化**（本篇） | 没有 AMD-V/SVM 后端 | 虚拟机跑不起来 |
| **AppleVTD / AMD-Vi**（[06-iommu-applevtd.md](06-iommu-applevtd.md)） | 没有 AMD-Vi 的 I/O DMA 重映射后端 | PCIe passthrough、需要 IOMMU 的设备与驱动路径受限 |

前者是 CPU 指令层，后者是 I/O 页表层。它们共享"Apple 只写了 Intel 那一半"这个原因，但代码路径没有交集。

## 有没有替代路线

按可行性排序：

1. **不在 macOS 里跑虚拟机。** 这台机器是双系统，Windows 侧用 Hyper-V / WSL2，Linux 侧用 KVM，都是原生 AMD 支持。这是唯一零成本的答案。
2. **纯软件模拟**（QEMU TCG）。能跑，但性能约等于没有；而且 macOS 上要拿到一个有网络和显示的 TCG 环境，工作量远大于收益。
3. **写一个 AMD 后端。** 需要实现 SVM 的 VMCB 管理、嵌套页表（NPT）、拦截退出处理，并把它接进 XNU 的 hypervisor 接口。这是操作系统级工程，不是 kext 级工程，不在这个项目的范围内。

## 如果将来要验证结论有没有变

一条命令：

```sh
sysctl kern.hv_support
```

`0` 表示没有。如果哪天 Apple 加了 AMD 后端（没有迹象），这里会变成 `1`，同时 `kern.hv_vmm_present` 在跑虚拟机时会变成 `1`。在那之前，任何声称"用某个 quirk 打开了 AMD 黑苹果的硬件虚拟化"的说法都值得怀疑。
