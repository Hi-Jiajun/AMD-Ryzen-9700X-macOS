# 2026-10-21 AMD power/SMC 只读基线

本目录只包含第一轮只读测量与方案评估，不含 EFI/BIOS/kext 修改。

- `BASELINE.md`：CPU/XCPM、pmset、内核、IORegistry 现状。
- `SMC-KEYS.md`：VirtualSMC 65 键全量枚举与真假传感器分类。
- `OPTIONS.md`：SMCAMDProcessor、自研只读插件、维持现状三方案对比。
- `smcdump.c`：只读 SMC 枚举源码。
- `smcdump`：本次实际执行的 x86_64 工具。

跨会话交接见 `docs/handoff-amd-power-smc.md`。
