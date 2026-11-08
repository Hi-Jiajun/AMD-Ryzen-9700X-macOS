# EFI 冻结窗口协议（Wave 0）

生效时间：2026-10-20 12:39 CST（本基线采样开始）。
适用范围：内置 NVMe `disk0` 的 ESP `disk0s1`（`/Volumes/OPENCORE` 挂载点）。

## 1. 角色与写权限

| 角色 | 谁 | 能做什么 | 不能做什么 |
|---|---|---|---|
| EFI 单点写者 | `amd-boot-config`（本会话） | 应用候选、整份备份、哈希、回滚、恢复演练 | 不实现设备驱动 |
| 候选 owner | USB / WiFi / NVIDIA / 音频 / AppleVTD 等会话 | 在自己目录做实验、产出候选文件与测试计划 | **不直接写 ESP**；不改别人的候选 |
| 协调会话 | `01a153f1…` | 排窗口、定优先级、验收 | 不替 owner 实现、不直接写 ESP |

**Wave 0 期间（本文件生效后）：除 `amd-boot-config` 外，任何会话不得写 `disk0s1`。**
只读诊断（`ioreg`、`kmutil`、`nvram`、读 ESP 文件）不受限制。

## 2. 候选怎么提交

候选统一放到：

```
outputs/20261020-amd-boot-config/inbox/<owner>-<YYYYMMDD>-<slug>/
```

必须包含 `CANDIDATE.md`，字段如下（缺一项直接退回）：

1. **Owner / session id**：谁负责，出问题找谁；
2. **目标文件**：ESP 内绝对路径（如 `EFI/OC/config.plist`、`EFI/OC/Kexts/X.kext`）；
3. **变更前哈希**：目标文件当前 sha256（写候选时实测）；
4. **候选文件或补丁**：完整文件，或 unified diff；二进制给 base64/文件；
5. **变更后哈希**：应用后的预期 sha256；
6. **应用命令**：可复制的 `cp/ditto/plutil` 步骤；
7. **回滚命令**：恢复到变更前哈希的步骤；
8. **测试计划**：重启几次、每次只改哪个变量、成功/失败判据；
9. **风险**：影响哪些启动阶段/设备，最坏情况；
10. **证据**：离线校验、反汇编命中、冲突数（如适用）。

候选只放 `inbox/`，**不要直接改 `EFI/`**；`amd-boot-config` 审核后才复制到 ESP。

## 3. 冻结窗口流程

1. **申请**：owner 在协调会话登记，说明候选目录、期望时间、需要几次重启；
2. **排期**：同一时间只允许一个窗口；窗口内只允许一个 boot 变量变化；
3. **预检**（amd-boot-config 执行）：
   - 重新读 ESP，确认 `config.plist`/`OpenCore.efi` 哈希与候选的“变更前”一致；
   - 全量备份当前 ESP 到 `private-backups/esp-<timestamp>/`，逐文件 SHA256；
   - `plutil -lint` / `ocvalidate`（若版本匹配）；
   - 救援 U 盘在位且可启动；
   - 在 `WINDOW-REGISTER.md` 写一行（见 §4）；
4. **应用**：只改候选列出的文件；改完立刻算哈希，写入登记表；
5. **重启验证**：保留救援盘；启动失败/panic 立即按回滚命令恢复，不叠加第二个变量；
6. **收尾**：记录 boot log、`nvram boot-args`、`kmutil showloaded`、关键哈希；
   成功则释放窗口，失败则回滚并标记失败原因。

## 4. 重启前登记表

每次窗口在 `WINDOW-REGISTER.md` 追加一行：

| 时间 | Owner | 变量 | 目标文件 | 变更前 sha256 | 变更后 sha256 | 重启次数 | 结果 | 回滚 |
|---|---|---|---|---|---|---|---|---|

没有登记行的 ESP 写入一律视为违规；发现后按“未登记写入”处理：先备份现场，再回滚到上一个已登记哈希。

## 5. 一次只改一个变量

以下每一项都算独立变量，不能同一轮重启一起改：

- `config.plist` 的 boot-args / Quirks / Kernel Patch / Kernel Add / DeviceProperties；
- `OpenCore.efi` 或 `BOOTx64.efi` 版本；
- 任一 kext 的增删/替换；
- ACPI SSDT 的增删；
- BIOS 项（本协议不覆盖 BIOS；BIOS 改动另立窗口，且需要用户手动操作）。

## 6. 紧急升级路径

启动失败 / panic / 卡 `STOP 0x16` / 卡 `EXITBS` 之后：

1. 立即停止后续写入；
2. 用救援 U 盘启动，或从 `EFI/OC-RESCUE-20261014-230948/` 恢复引导器；
3. 恢复最近一份 `private-backups/esp-*` 的 `EFI/`；
4. 把 boot log、屏幕照片、`nvram -p`、回滚哈希发给协调会话与 `amd-boot-config`；
5. 在坏状态上**不要**继续试第二个补丁。

## 7. 当前状态

- 本会话只做了只读采样 + 卷外备份 + 离线恢复演练；
- **没有写 ESP、没有改 BIOS、没有重启、没有加载/卸载 kext**；
- 在用户确认 Batch 0 验收前，冻结保持。
