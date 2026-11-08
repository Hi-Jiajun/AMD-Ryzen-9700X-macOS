# Zen5 温度通道：重启前阶段

范围：Ryzen 7 9700X（AuthenticAMD / Family 1Ah / Model 44h）；只读现状核对、固定上游依据、离线解码原型与后续接入设计。

顺序与验收：

1. 读取本机 CPU 身份、root powermetrics SMC 结果、最近 MTKCAP 的 hwmon。输出真实证据；不得把占位频率当实时值。
2. 固定 Linux k10temp 与 SMN 实现提交及文件哈希。输出 Model 44h 对应的寄存器和错误语义；不得把 SMN 读取称为零写入。
3. 实现离线温度解码工具。输入为明确标注的合成寄存器快照；输出 JSON，非法身份、读取失败、空值与 PCI 全 1 错误必须拒绝。
4. 验证负温、49°C 调整、CCD valid 位、错误样本及 CLI。所有输出明确为合成输入，不宣称硬件读温成功。
5. 更新主规划的当前状态索引和 power/SMC 交接；保留旧证据。提出下一次硬件窗口的输入、输出、通过条件。

新增文件归属：本目录全部文件。更新文件：`docs/plans/2026-10-19-amd-adaptation-program.md`、`docs/handoff-amd-power-smc.md`。

本阶段不加载 kext、不写 EFI/BIOS、不改当前 MTKCAP 启动包、不访问 PCI/SMN/MSR 寄存器。当前 U 盘先完成 STA reader 修正的真机验收。
