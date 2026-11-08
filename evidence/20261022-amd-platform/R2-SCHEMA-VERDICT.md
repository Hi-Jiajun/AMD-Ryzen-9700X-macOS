# R2 判定：DBG-107 与 config 的 schema 不匹配

> 编制：2026-10-23 00:4x CST　状态：**结案（接受为无害噪音，不写 ESP）**
> 输入：只读挂载的 ESP `disk0s1`、运行日志 `opencore-2026-10-22-123428.txt`、本地 OpenCore 源码树

## 1. 结论

1. 那 4 条 `OCS: No schema for HideVerbose` **纯粹是噪音**，没有任何功能影响，也没有启动风险。
2. 根因是**配置比引导器新**：`UEFI/Drivers` 条目里的 `HideVerbose` 是 OpenCore **1.0.8** 才引入的字段，
   而本机跑的是 **1.0.7** 代码（自报 `DBG-107-2026-10-14`）。
3. **「删掉那 4 个键」是错的解法**，已被反证：删掉之后 1.0.9 的 `ocvalidate` 反而报
   `Missing key HideVerbose` ×4。也就是说删键会把一个 1.0.9-合规的 config 变成不合规。
4. 本机 config **已经通过 1.0.9 的 `ocvalidate` 全量校验（No issues found）**，
   即它是一份「1.0.8+ 规格」的配置，只差一个同代的 bootloader。
5. 因此 R2 的正确处置是：**保持现状**；将来若要彻底消掉告警，走「升级 bootloader」而不是「改 config」，
   且单独立窗口。

## 2. 证据

### 2.1 运行日志（权威现场）

`opencore-2026-10-22-123428.txt`（2026-10-22 20:34 那次启动）：

```
10:00:000 00:000 OC: Loaded configuration of 56573 bytes
11:00:000 00:000 OCS: No schema for HideVerbose at 3 index, context <Drivers>!
12:00:000 00:000 OCS: No schema for HideVerbose at 3 index, context <Drivers>!
13:00:000 00:000 OCS: No schema for HideVerbose at 3 index, context <Drivers>!
14:00:000 00:000 OCS: No schema for HideVerbose at 3 index, context <Drivers>!
15:00:000 00:000 OC: Got 4 drivers
```

- 全日志 `OCS:` 计数 = **4**，全部是同一个键、同一个上下文，**没有其它 schema 告警**。
- `context <Drivers>` + `at 3 index` 指的就是 `UEFI/Drivers[0..3]` 这四个字典条目里的第 4 个键。

### 2.2 配置侧

当前 `config.plist`（56,573 B，`bb95cd36…2812`）里 `HideVerbose` 共出现 **5** 次：

| 位置 | 值 | 是否合法 |
|---|---|---|
| `UEFI/APFS/HideVerbose` | `true` | **合法**（该键一直存在，日志未告警） |
| `UEFI/Drivers[0]/HideVerbose`（HfsPlus.efi） | `false` | 1.0.7 不认识 |
| `UEFI/Drivers[1]/HideVerbose`（OpenRuntime.efi） | `false` | 同上 |
| `UEFI/Drivers[2]/HideVerbose`（OpenCanopy.efi） | `false` | 同上 |
| `UEFI/Drivers[3]/HideVerbose`（ResetNvramEntry.efi） | `false` | 同上 |

### 2.3 版本侧

- 本地源码树 `outputs/20261017-audio-current/oc-tools-src/Changelog.md`
  第 3 行 `#### v1.0.8`，第 6 行：`- Added option to hide verbose output from any driver, thx @ilikesn0w`。
  即 **1.0.8 才新增该字段**；第 13 行是 `#### v1.0.7`，其条目里没有这一项。
- 该树的 schema 头 `Include/Acidanthera/Library/OcConfigurationLib.h:624` 确实有
  `HideVerbose` 在 `OC_UEFI_DRIVER_ENTRY_FIELDS` 里（1.0.8+ 规格）。
- 运行中的 `OpenCore.efi`（`7754f6d6…73c8`）字符串表里 `HideVerbose` **只出现 1 次**——
  那是 `UEFI/APFS` 的键；`UEFI/Drivers` 的 schema 里没有它。这与运行时告警完全一致。

### 2.4 反证：删键不可行

用文本级精确删除生成候选（只删 `UEFI/Drivers[0..3]` 的 4 对
`<key>HideVerbose</key><false/>`，`UEFI/APFS` 的 `true` 不动）：

| 文件 | 字节 | sha256 |
|---|---|---|
| 现状 `config.plist` | 56,573 | `bb95cd36c24f89bcfff5bc4f51d414ad169f43f486d86f9f629bcf20fd222812` |
| 候选（已否决） | 56,413 | `e8006e23bdf62b269ba3fb6da0eaab4fe1d5cb63e0729e86b4a491addb877e9a` |

结构化 diff 只有 4 条 `REMOVED /UEFI/Drivers[i]/HideVerbose`，其余零变化。校验结果：

| 校验 | 现状 | 删键候选 |
|---|---|---|
| `plutil -lint` | OK | OK |
| `ocvalidate`（1.0.9） | **No issues found** | **4 issues**：`Missing key HideVerbose, context <Drivers>!` ×4 |

结论：删键能在 1.0.7 上消掉告警，但会把配置变成 1.0.9 不合规；**收益为零、代价是把问题推给下一次引导器升级**。

## 3. 影响评估

- `HideVerbose` 的语义只是「隐藏某个 driver 的 verbose 输出」。1.0.7 忽略它 → 该 driver 的
  verbose 信息照常打印。对启动、USB、GPU、音频均无影响。
- `OcSerializeLib` 对未知键的处理是「告警并跳过」，不是解析失败；`Loaded configuration of 56573 bytes`
  与 `Got 4 drivers` 都正常，说明四个 driver 全部被识别。
- 2026-10-22 20:34 那次启动后 S2 跨重启验收全 PASS，进一步说明这 4 条告警与实际行为无关。

## 4. 处置

**现在**：不动。R2 记为「已知无害告警」，不再是开放阻塞项。

**将来（可选，需单独窗口）**：把 bootloader 升到 1.0.8+ 同代版本。注意本机的
`tools/backup-oc-debug-20261014-230155/stage-OC-1.0.9-master-DEBUG/OC/OpenCore.efi`
虽然目录名叫 `1.0.9`，但与 ESP 上的 `7754f6d6…73c8` **逐字节相同**、行为是 1.0.7（不认该键），
**目录名不可信**。真要升级必须重新构建并按 §2.3 的方式验版本，不能拿这个目录直接当 1.0.9 用。

升级路径（供将来窗口参考，本次未执行）：

1. 从 `oc-tools-src` 构建 DEBUG/RELEASE 引导器，确认自报版本号 ≥ 1.0.8；
2. 用同代 `ocvalidate` 校验当前 config（预期 No issues）；
3. 登记冻结窗口 → 只替换 `EFI/OC/OpenCore.efi` 与 `EFI/BOOT/BOOTx64.efi`；
4. 重启后验收：`OCS:` 计数为 0、`Got 4 drivers`、UTBMap Success/v1.1、16 口拓扑、蓝牙 On；
5. 回滚：换回 `7754f6d6…73c8` 与 `d3a4854c…10f3`。

## 5. 复现命令

```sh
# 只读挂载 ESP
esp=$(mktemp -d /tmp/esp_ro.XXXXXX)
sudo mount -t msdos -o ro /dev/disk0s1 "$esp"

# 告警计数与上下文
newest=$(ls -t "$esp"/opencore-*.txt | head -1)
grep -c "OCS:" "$newest"; grep "No schema" "$newest"

# config 里的 5 处 HideVerbose
python3 - "$esp/EFI/OC/config.plist" <<'PY'
import plistlib,sys
d=plistlib.load(open(sys.argv[1],'rb'))
def walk(o,p=''):
    if isinstance(o,dict):
        for k,v in o.items():
            if 'HideVerbose' in k: print(f'{p}/{k} = {v}')
            walk(v,f'{p}/{k}')
    elif isinstance(o,list):
        for i,v in enumerate(o): walk(v,f'{p}[{i}]')
walk(d)
PY

# 双向 ocvalidate
ocv=outputs/20261017-audio-current/oc-tools-src/Utilities/ocvalidate/ocvalidate
"$ocv" "$esp/EFI/OC/config.plist"                                   # 现状：No issues found
"$ocv" outputs/20261022-amd-platform/r2/config.plist.r2-nohideverbose # 删键候选：4 issues

# 卸载
sudo umount "$esp" && rmdir "$esp"
```

## 6. 产物

- 本文：`outputs/20261022-amd-platform/R2-SCHEMA-VERDICT.md`
- 被否决的候选与 diff（留作反证，**不要部署**）：
  - `outputs/20261022-amd-platform/r2/config.plist.r2-nohideverbose`
  - `outputs/20261022-amd-platform/r2/r2-diff.txt`
