# Ryzen 9700X 温度通道：离线阶段完成

2026-10-25。结论：**已有可验证的温度解码原型和 Linux 对照入口；尚未取得本机真实 CPU 温度，未构建或加载 macOS 传感器 kext。**

## 当前实测

- `macos-cpu.txt`：AuthenticAMD / Family 26 / Model 68 / stepping 0；XCPM mode 0，`hw.cpufrequency=5140000000` 仍是静态值，不能证明 CPU 锁频。
- 05:57 CST 以 root 执行 `/usr/bin/powermetrics --samplers smc -n 1 -i 1000`，退出码 0；输出多次 `Invalid key detected!`，`**** SMC sensors ****` 后没有温度/风扇数据。该结论只排除本次 SMC sampler 能提供读数，不能外推所有 sampler 无效。
- MTKCAP `out/superio-20261024-213411/probe.log`：只有 `hwmon0=nvme`；0x2E/0x4E 因 PCI Bus 占用跳过，芯片 ID 仍未知。未加载 k10temp，不能由此判定它不支持 CPU。
- 离线解开现有 `boot/modloop-lts`：自带 `k10temp.ko`，vermagic 为 `6.6.134-0-lts SMP preempt mod_unload modversions`，支持 `1022:14e3`；本机 ioreg 也枚举该 ID。证据 `k10temp-module.txt`、`amd-pci-identity.json`。模块存在和 ID 匹配不等于硬件读取成功。

## 固定依据与解码

Linux 当前文件提交 `451b1c19dc7cbbad194a6e717b6a732c443d7dd5`：

- [k10temp.c](https://github.com/torvalds/linux/blob/451b1c19dc7cbbad194a6e717b6a732c443d7dd5/drivers/hwmon/k10temp.c#L168-L192)：Tctl 的 SMN 地址 `0x59800`，bit31:21 为温度编码，每步 125 m°C。bit19 为 1 或 bit17:16 同为 1 时减 49000 m°C；9700X 的 Linux hwmon 接口将负 Tctl 截为 0。
- [Family 1Ah 分支](https://github.com/torvalds/linux/blob/451b1c19dc7cbbad194a6e717b6a732c443d7dd5/drivers/hwmon/k10temp.c#L523-L534)：Model 40h–4Fh 的 CCD 基址偏移 `0x308`；slot 0 是 `0x59B08`，最多探测 8 个 slot，每步 4 字节。**8 个 slot 不是本机有 8 个 CCD。** 仅 valid bit11 置位才接受；温度为 `(raw & 0x7FF)*125-49000` m°C。
- 当前 Linux 对 9700X 不单独注册 Tdie（历史 Tctl 偏移表只覆盖部分 Family 17h）。原型只命名 Tctl/Tccd，不虚构一个独立 Tdie 传感器。
- [amd_node.c](https://github.com/torvalds/linux/blob/451b1c19dc7cbbad194a6e717b6a732c443d7dd5/arch/x86/kernel/amd_node.c#L85-L130)：SMN read 在锁内写 PCI config `0x60` 选择地址，再读 `0x64`。因此“不改变电源/风扇设置”与“零硬件写入”必须区分。Linux 还保留 PCI config 区域，直接用户态 setpci 不能替代内核接口。
- [与 U 盘版本对应的 stable k10temp](https://github.com/gregkh/linux/blob/v6.6.134/drivers/hwmon/k10temp.c#L503-L507)：Family 1Ah 有 Tctl 路径，**没有该 Family 的 CCD 枚举分支**。因此 stock 6.6.134 的首轮目标仅为 Tctl，不能要求它输出 Tccd 后才判通过。Alpine 的实际构建仍可能有补丁，最终以真机 sysfs 为准。

以上是源码与模块静态核对；未把 AMD PPR 57896 的全文当成本轮已经读过的证据。源码副本及哈希见 `sources/` 和 `SHA256SUMS`。失败的旧路径请求保留为 `sources/amd_smn.c.http404.json`，不作为源码引用。

## 离线工具与验证

```sh
/usr/bin/python3 outputs/20261025-amd-sensors/test_temp_decode.py
/usr/bin/python3 outputs/20261025-amd-sensors/zen5_temp_decode.py outputs/20261025-amd-sensors/synthetic-snapshot.json
```

结果：8 项测试通过；涵盖黄金向量、2048 级温度编码、49°C 偏移两种条件、负温策略、CCD valid/slot、身份门控、transport 失败、全零/全 1 与格式错误、CLI 退出码。完整输出 `tests.txt`。

工具只解码显式输入的 JSON，不读取寄存器。例子中的 51°C / 50°C 是**合成向量**，不是本机测量。JSON 明确输出 `hardware_access=false`、`measurement_verified=false`。

原型比 Linux 原始 Tctl 回退更保守：transport 失败、全零和 `0xFFFFFFFF` 一律拒绝，避免把读取失败显示成 0°C。解码成功只证明位域可解析，不证明样本真实、传感器准确或温度在运行范围内；未来 SMC 发布层另做数据有效性和新鲜度门控。

## 下一次硬件阶段的输入、输出、通过条件

**前置**：当前 USB 的 STA reader 修正版先完成真机验收。此次未改变 USB 包，其 SHA256 仍应为 `67d614c6d68406c03f33a4e659c49bf4b22c941480ac0dd37467d62c36f1208e`。

**输入**：同一 MTKCAP 内核和现有 k10temp 模块；单独登记“通过 Linux 自带驱动读取 Tctl”这个变量。仅走内核驱动与 sysfs，不使用 setpci、/dev/mem 或自制 SMN 写路径，不与 CX3 驱动实验叠加。

**动作候选**：由后续启动钩子在 CPU 身份与内核匹配后执行 `modprobe k10temp`，记录返回码和 dmesg，再从 `/sys/class/hwmon/` 中名称为 k10temp 的设备读取 label/input。候选尚未接入 U 盘，本轮未加载模块。

**输出**：原始 label/input、多次短采样的时间戳、PCI driver 绑定、内核日志；若失败，保存明确错误，不用默认 0 填充。只在空闲和正常轻载观察，不主动满载压测。

**通过**：真实 Tctl 连续可读、量纲为 m°C、没有读取错误或新增内核故障；与同机 BIOS/Windows 的另一次读数仅作交叉合理性检查，不声称不同时刻数值应相等。无需 stock 内核提供 CCD 才通过。

**之后的 macOS 阶段**：先解决 SMN 根设备身份、访问所有权、跨消费者串行化与失败返回，再接 VirtualSMC；仅有一个插件内部 mutex 不代表系统全局访问已串行化。初期仅输出 Tctl，不写 HWCR/P-state/CPPC/风扇设置。该传输层与 kext 仍未实现，不能把本目录称为可安装驱动。
