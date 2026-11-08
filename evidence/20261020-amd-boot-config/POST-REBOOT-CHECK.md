# 重启后验收清单（S2 + 蓝牙持久性，2026-10-22 起草）

用途：用户下一次**普通重启**后，5 分钟内跑完，判断 S2 是否跨重启稳定、蓝牙是否持久、
以及有没有复现 HS11 恢复风暴。只读，不改 EFI。

## 0. 重启前（现在就能做）

```bash
sysctl kern.boottime
tail -1 outputs/20261020-amd-usb-ports/candidate-S-20261021/S2-runtime-20261021/snapshots.jsonl
```

记下这两行，作为"重启前基线"。

## 1. 启动是否成功

```bash
sysctl kern.boottime
ls -t /Volumes/OPENCORE/opencore-*.txt | head -1
```

判据：新日志的 `Boot timestamp`（UTC）与 `kern.boottime`（CST，差 8 小时）对得上；
日志里有 `EXITBS:START`。

## 2. 补丁是否仍全部成立

```bash
python3 tools/patchreport.py "$(ls -t /Volumes/OPENCORE/opencore-*.txt | head -1)"
```

判据：`没有 enabled 且 0 替换的补丁`；应用条数应与 10-22 的 17 条一致
（若这次改了 config，以新配置为准）。

## 3. UTBMap 是否仍注入

```bash
rg -a 'Prelinked injection UTBMap|UTBMap.kext v' "$(ls -t /Volumes/OPENCORE/opencore-*.txt | head -1)"
```

判据：`- Success` 且 `UTBMap.kext v1.1`；不能出现 `Invalid Parameter` / `version unavailable`。

## 4. USB 拓扑

```bash
ioreg -rc AppleUSBXHCIPCI -r -l -w0 | rg 'port-count|^ *\+-o (HS|SS)[0-9]+@'
```

判据（与 `candidate-S-20261021/ACCEPTANCE.md` 一致）：

- XH00 七个位置：`0x00100000,0x00200000,…,0x00700000`（硬件 2/3/7/8/15/16/17）
- XHC0/XHC1/XHC2 = 4/4/1
- 无 locationID 冲突

## 5. 蓝牙是否持久

```bash
system_profiler SPBluetoothDataType | rg -i 'State|Address' | head -4
kmutil showloaded --list-only | rg -i 'SimpleMtkBt'
```

判据：`State: On`、`Address: 02:1A:7D:DA:71:2A`、`SimpleMtkBtFw` 已加载。

## 6. 恢复风暴与错误面（关键）

```bash
log show --last 10m --predicate 'process == "kernel" AND subsystem == "com.apple.usb"' \
  | rg -c 'hardwareException|CommandRing::executeCommand.*timeout|createDevice: failed|powerOn: deadline'
```

判据：计数为 **0**。已知噪声 `stopEndpoint ... result code 19` 不算失败，单独记录。

## 7. 探针是否跨重启连续

```bash
tail -3 outputs/20261020-amd-usb-ports/candidate-S-20261021/S2-runtime-20261021/snapshots.jsonl
```

判据：重启后仍有新快照（时间戳在本次 boottime 之后）；端口集合与 §4 一致。

## 8. ESP 是否还是预期内容

```bash
shasum -a 256 /Volumes/OPENCORE/EFI/OC/config.plist \
              /Volumes/OPENCORE/EFI/OC/Kexts/UTBMap.kext/Contents/Info.plist
```

判据：config `bb95cd36…2812`（若期间有新登记改动则以登记表为准）；
UTBMap `998c7fbb…4d0d`。

## 失败怎么办

- 只要 §6 计数 > 0 或 §4 拓扑错乱：**只回滚 UTBMap** 到 `f9b484e8…`，
  用 `private-backups/UTBMap-before-S2-20261021-0643/Info.plist.before-S2`，不叠加第二个变量。
- 启动失败/panic：F11/F12 选救援 U 盘 → 恢复 `EFI/` → 重启。
- 任何回滚都要在 `WINDOW-REGISTER.md` 追加一行。
