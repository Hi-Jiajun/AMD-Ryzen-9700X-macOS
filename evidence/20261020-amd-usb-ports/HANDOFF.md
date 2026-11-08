# Handoff：amd-usb-ports（B1）研究阶段

更新时间：2026-10-21　状态：**S2 runtime PASS；压测已取消，改为探针观察；未写 ESP、未重启。**

## 结论

- 15 口上限是每控制器 15 个端口对象的硬上限，卡在 `createPorts` 计数与 `setPortLocation`
  位置号两处；端口域 4 bit，16/17 必然进位到控制器字节 `b+1`。
- 本机 17 个对象中只有 7 个接到实际连接器；其余 10 个未引出。
- 推荐（稳定性优先）：**稀疏 map 7 条（硬件 `{2,3,7,8,15,16,17}`，`port-count=17`，补丁全关）**，
  零内核补丁、零位置溢出，覆盖现有全部物理连接器。
- 第二选择（仅受控实验）：阈值补丁创建 17 对象 + 保留字节 1 + 16 号不得挂 Hub；
  须先向 `amd-boot-config` 申请冻结窗口。

## 关键证据

- 机制/候选/评分：`outputs/20261020-amd-usb-ports/2026-10-20-portlimit-mechanism-and-paths.md`
- 社区一手源：`outputs/20261020-amd-usb-ports/community-paths.md`
- 反汇编复核：24H32 KC `cd013c14…`，`createPorts +1344`、`setPortLocation +54/+838`、
  `getPortPropertyTable +231`、`generateBusNumber +26/+49`。

## 阻塞 / 需要协调

1. EFI 写入冻结；R1 真机恢复未做。任何部署/重启需 `amd-boot-config`（thread `01a1579b…`）窗口。
2. 共享目录 `tools/research/usbpatch-check.py` 会把 16/17 判成 `PASS`（根端口模型）；
   独立仓库版判 `topology FAIL`。建议以独立仓库版为准，或把两者合并，避免下游误用。
3. 稀疏 map 候选需从 10-20 备份重新生成（10-17 候选基于同一 map 哈希，但仍应重跑差量校验）。

## 下一步（等协调会话/用户决定）

- 若接受 S：修复版候选已在 `candidate-S-20261021/`（XML，哈希 `998c7fbb…`，
  与 10-17 B 候选逐字节相同），等 `amd-boot-config` 开冻结窗口重试。
- 若坚持 17 对象：先补离线命中审计与拓扑门槛，再谈窗口；不改校验器语义来换 PASS。

## 2026-10-21 S 候选

- 目录：`outputs/20261020-amd-usb-ports/candidate-S-20261021/`
- 文件：`UTBMap.kext/Contents/Info.plist`（7 条：硬件 2/3/7/8/15/16/17，port-count=17）
- 哈希：baseline `f9b484e8…` → **修复候选 `998c7fbb…`（XML）**
- 第一次部署失败：`83f47223…` 是 binary plist（`bplist00`），OpenCore 1.0.7
  `PrelinkedInjectKext` 用 `XmlDocumentParse`（`PrelinkedContext.c:1065-1069`）
  解析 Info.plist，binary 直接 `EFI_INVALID_PARAMETER`；随后日志打印
  `version unavailable`（`OpenCoreKernel.c:584` 的占位文本）。
- 日志括号里的 `V1.0 | NullMoth USB map` 是 config `Kernel->Add` 的 `Comment`，
  不是 kext 版本，也不是旧缓存。
- 修复：生成器改用 `FMT_XML`；新增 `test_injection_plist.py`（3 tests）锁死 XML 格式与 map 形状。
- 离线：`validate.py` ok（7 对象、位置 1..7、含两层 Hub 共 1696 个位置 0 冲突）；
  `usb-port-limit-macos` 全套 **98 tests OK**；config `XhciPortLimit=false`、两条 USB 补丁关闭。
- **S2 runtime PASS（2026-10-21 07:55:46 启动）**：OpenCore 注入 Success / v1.1；
  XH00 7 对象位置 1..7，其他 4/4/1；蓝牙 `State: On`（`02:1A:7D:DA:71:2A`），
  `SimpleMtkBtFw` 已加载；失败签名 0（已知启动期 `stopEndpoint result 19` 噪声单独记录）。
- **压测已取消**（用户决定）：不再做 100 次插拔 / 30 分钟音频；
  改为 mac-watch 探针观察旧补丁症状，见 `USB-PROBE-CHECK.md`。
- 只读监测由用户级 launchd 作业 `com.hiliang.usbmonitor` 承载，输出在
  `candidate-S-20261021/S2-runtime-20261021/`（`usb.log` / `audio.log` / `snapshots.jsonl`）；
  停止用 `launchctl remove com.hiliang.usbmonitor`。
- 音频基线（仅供排查参考，不再是验收项）：07:58 load1≈20 时已有
  `HALS_OverloadMessage` 与 `HALS_IOA1Engine::EndWriting 0xE00002EE`。
- 验收清单与命令：`candidate-S-20261021/ACCEPTANCE.md`。失败只回滚 UTBMap 到 `f9b484e8…`。
