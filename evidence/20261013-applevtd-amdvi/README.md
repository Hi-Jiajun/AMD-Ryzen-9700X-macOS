# 20261013-applevtd-amdvi — AppleVTD 的 AMD-Vi 后端（路线 ①b）

本目录只属于本任务；其它会话的产物不在这里，也未被改动。

## 交付物

| 文件 | 说明 |
| --- | --- |
| `appleVTD-reverse-map.md` | **第一交付物：AppleVTD 逆向地图**（函数清单与地址、调用链、硬件模型、切割面、未决项） |
| `appleVTD-source-notes.md` | **AppleVTD 是开源的**：本机对应 tag `IOPCIFamily-681.140.3`（断言行号指纹），结构体/字段级真相 + 二进制符号↔源码行号 |
| `amd-vi-port-map.md` | **VT-d ↔ AMD-Vi 移植对照表**：寄存器/命令/表结构/页表/故障逐项对照（左列 Apple 源码行号，右列 Linux 常量） |
| `synthetic-dmar-and-injection.md` | 合成 DMAR 入口设计 + Lilu 内存补丁注入机制 + 接管顺序 + 回滚 |
| `raw/` | 从 boot KC 抽出的 IOPCIFamily 条目、符号表、`__cstring` 全表 |
| `disasm/` | AppleVTD 区间反汇编 + 按函数拆分（381 个）+ 索引 |
| `refs/` | Apple 公开源码（tag 681.140.3）：AppleVTD.cpp/.h、dmar.h、balloc.c、rballoc.c、IOPCMessagedInterruptController.cpp、IOPCIBridge.cpp、IOPCIConfigurator.cpp |
| `scripts/` | 4 支可复现脚本（抽符号 / 修 KC 切片偏移 / 拆函数 / 带地址打印 cstring） |
| `impl/` | **Lilu 插件骨架（stage 0）**：能编译出 `AVAMDVi.kext`、带 `-amdvtdoff` 总开关、默认零行为变化 |

## 状态

* 只读取证：未改 ESP / config.plist / boot-args / 系统文件。
* 待用户审核地图与设计后再进入实作（U 盘 OC 配置先行）。
* 关键前提：macOS 15.8.1（24H32）锁版；手术对象 = boot KC 里 `com.apple.iokit.IOPCIFamily` 的 `AppleVTD`/`AppleVTDDeviceMapper`。
* 2026-10-13 追加：路线 ①b 已从"二进制替换"升级为**源码级移植**（AppleVTD 源码公开且版本已对上）。
* 参考源 tag 与 sha256 见 `refs/SOURCE-TAG.txt` 与 `appleVTD-source-notes.md`。
