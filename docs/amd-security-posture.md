# AMD 平台安全姿态现状（A8，2026-10-22）

只读评估：未改 SIP / AMFI / EFI / BIOS。机器：MacPro7,1（无 T2）、macOS 15.8.1 (24H32)。

## 1. 现状

| 项 | 当前值 | 来源 |
|---|---|---|
| SIP | `csr-active-config = 0x00000A43`（Custom Configuration） | `nvram csr-active-config` |
| Kext 签名校验 | **disabled** | `csrutil status` |
| 文件系统保护 | **disabled** | `csrutil status` |
| NVRAM 保护 | **disabled** | `csrutil status` |
| 调试限制 / DTrace 限制 | enabled | `csrutil status` |
| BaseSystem 校验 | enabled | `csrutil status` |
| AMFI | `amfi=0x80` + `-amfipassbeta`（AMFIPass 1.4.1 已加载） | `nvram boot-args` / `kmutil` |
| FileVault | **Off** | `fdesetup status` |
| Gatekeeper（spctl） | assessments enabled | `spctl --status` |
| BIOS Secure Boot | Disabled | 2026-10-21 用户目视 |
| OpenCore `SecureBootModel` | `Disabled`；`Vault = Optional` | ESP config |
| NVRAM 写入 | OpenCore `DisableVariableWrite=true` | ESP config |

### SIP 位展开（0xA43）

| 位 | 状态 | 含义 |
|---|---|---|
| `0x001` CSR_ALLOW_UNTRUSTED_KEXTS | **ON** | 允许加载未签名 kext |
| `0x002` CSR_ALLOW_UNRESTRICTED_FS | **ON** | 允许改系统卷 |
| `0x040` CSR_ALLOW_UNRESTRICTED_NVRAM | **ON** | 允许写 NVRAM 变量 |
| `0x200` CSR_ALLOW_UNAPPROVED_KEXTS | **ON** | 允许未批准 kext |
| `0x800` CSR_ALLOW_UNAUTHENTICATED_ROOT | **ON** | 允许未认证的 root |
| 其余（TASK_FOR_PID / KERNEL_DEBUGGER / APPLE_INTERNAL / DTRACE / DEVICE_CONFIG / RECOVERY_OS / EXEC_POLICY） | off | — |

## 2. 风险分级

| 等级 | 风险 | 说明 |
|---|---|---|
| 高 | 内核完整性基本不设防 | 未签名 kext + AMFI 放宽 + 未认证 root 同时开着：任何拿到 root 的进程都能进内核。这是本机的设计前提（自研驱动必须如此），不是漏洞。 |
| 中 | 物理接触即可读盘 | FileVault Off；挂载外部系统即可读用户数据。 |
| 中 | NVRAM 变量可被改写 | SIP 的 NVRAM 位开着，但 OpenCore `DisableVariableWrite=true` 在更底层拦截；两层策略不一致，排障时容易误判“到底是谁写的”。 |
| 低 | Gatekeeper 仍开启 | 下载的 App 仍受隔离/公证约束，比 SIP 位更严格。 |
| 不可达 | Apple Pay / Secure Enclave / 激活锁 | 无 T2/SEP 硬件，属硬件不存在，不作为可实现目标。 |

## 3. 建议（都不在本次执行）

1. **保持现状**：自研 kext 与 AMFIPass 依赖这些放宽；收紧会直接打断 NVIDIA / CX3 / AirportMtk 链路。
2. 若将来要收紧，从低风险到高风险：
   - 先评估去掉 `CSR_ALLOW_UNAUTHENTICATED_ROOT (0x800)`——多数自签 kext 场景不需要；
   - 再评估 `CSR_ALLOW_UNRESTRICTED_FS (0x002)`；
   - `UNTRUSTED_KEXTS` / `UNAPPROVED_KEXTS` 与 `amfi=0x80` 最后动，且必须配合 AMFIPass 与签名链。
3. **FileVault 单独评估**：黑苹果开 FileVault 会改变启动链（需要 OpenCore 的 `ApECID` / `AuthRestart`
   与 Recovery 配合），风险高于收益，除非机器要离开家里。
4. 不在这台机器上放敏感数据，也不要把它当唯一副本。

## 4. 与计划的关系

- A8 在计划里是 P3、可选；本文件只做“现状 + 分级”，不产生 EFI 变更。
- 将来做 macOS 更新时，SIP 位与 `amfi` 参数要跟 `docs/macos-update-checklist.md` 一起复核。
