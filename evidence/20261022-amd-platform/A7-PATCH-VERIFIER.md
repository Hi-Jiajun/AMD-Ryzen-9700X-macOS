# A7 补丁校验：两个工具 + 当版结论（2026-10-22）

目标：把“系统更新后哪些补丁还成立”从人工比对变成一条命令。

## 1. 两个工具，职责不同

| 工具 | 何时用 | 口径 | 局限 |
|---|---|---|---|
| `tools/patchreport.py <opencore 日志>` | 启动后（有日志） | **权威**：直接读 OpenCore 自己的 `replace count - N` 与 `patcher result` | 需要一次启动日志 |
| `tools/patchcheck-all.py --config <plist>` | 更新前预检（新系统还没启动） | 启发式：按 `Mask`（位掩码）+ `Limit` + 版本门控在 KC 副本上找 `Find` | kext 补丁与 `Limit == len(Find)` 的补丁会误判，见 §4 |

两个都只读：KC 先算 sha256、复制到临时目录再扫，结束复核原件哈希未变。

## 2. 当版结论（Darwin 24.6.0 / 24H32）

日志 `opencore-2026-10-22-031825.txt`：

- **17 条实际应用**，`replace count` 与配置里的 `Count` 全部吻合；
- **9 条被版本门控跳过**（00/01/02/09/11/13/18/22/23），属正常；
- **0 条“执行成功但替换 0 处”**，即没有静默失效的补丁。

值得记住的几条：

| 补丁 | 实际替换 |
|---|---|
| [03] Force cpuid_cores_per_package 13.3+ | 1 |
| [15] _i386_init Remove 3 rdmsr | 3 |
| [20] thread_quantum_expire/unblock/invoke | 3 |
| [21] thread_invoke/thread_dispatch | 2 |
| [25] PAT（Algrey/Zormeister 15.0+） | 2 |
| [17] IOPCIFamily probeBusGated 10-bit tags | 1（prelinked） |
| [29] 9700X LAPIC 25MHz 标定 | 1 |

## 3. 用法

```
python3 tools/patchreport.py /Volumes/OPENCORE/opencore-<最新>.txt --json out.json
python3 tools/patchcheck-all.py --config <备份>/EFI/OC/config.plist --json out.json
```

`patchreport.py` 退出码 1 表示存在 enabled 且 0 替换的补丁。
`patchcheck-all.py` 退出码 1 表示存在 enabled 且 MISS 的补丁。

## 4. 已知局限（实测确认，不要反复调）

1. **kext 补丁**（`Identifier` 为 `com.apple.*`，走 Prelinked patcher）：KC 里 kext 的
   符号/段布局与 `nm -a` 的平铺视图不一致，字节扫描会假阴性。
   实测 [17] 早期版本扫描报 MISS，而日志明确 `replace count - 1` + `Success`。
   现已把这类标成 `kext（以日志为准）`，不再计入 MISS。
2. **`Limit` 语义**：`Limit` 是“只在该符号前 N 字节内搜索”。当 `Limit == len(Find)`
   （如 [29]）时，OpenCore 内部搜索起点与 `nm` 符号地址不一致，扫描会假阳性（报 8 个候选），
   日志实际是 1 处。
3. **多处命中**：[03]/[15]/[25] 这类 pattern 在同一函数里出现多次，OpenCore 按 `Count`
   取前几处；扫描报“多于 Count”只是提示，不是失败。

当版输出：`适用 20 / 字节扫描 16（OK 9，需复核 7）/ kext 补丁 4 / MISS 0`。

结论：**更新前**用 `patchcheck-all.py` 看内核补丁有没有整条消失，**启动后**一律以
`patchreport.py` 为准。
