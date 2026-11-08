# AMD 平台 macOS 适配项目规划（初步）

> 状态：2026-10-25 已升级至 macOS 26.7.1 / 25G241 / Darwin 25.6.0，已向现有 owner 分派升级验收与相关修复。以本节最新 Tahoe 状态及各 owner 的实际结果为准，Sequoia 成功项不能外推到 Tahoe。每个工作流按共享变更单一写者规则执行。
> 本文件是项目级 spec/roadmap。每个工作流真正启动时，从 §5 抽出该流的 session brief，再按 `superpowers:writing-plans` 生成任务级计划。

## Tahoe 升级后验收（2026-10-25，11:12:19 CST 启动）

**2026-10-26 09:59 用户已明确批准评估后直接安装 NullMoth。** 最新原话“直接让它评估后安装吧”取代此前仅准备/等待安装确认的门槛。已转达安装owner：官方新版、macOS26/RTX5060兼容、备份及同系统回滚核实后直接执行必要系统驱动/用户态/KC更新，无需再请求安装批准；无新版不重装，有实质兼容/恢复风险则具体报阻塞。安装owner已明确接收直接安装授权并在复核现场/恢复门槛；已交官方2.5.5候选，包哈希/26适用性报告见 `nvidia-install/update-2.5.5-20261026-094747/READY-FOR-REVIEW.md`，发布说明无USB/音频修复声明。尚无安装验收结果。禁止自动重启/睡眠/播放录音；如必要涉及共享ESP/config仍由主协调单写者处理，不混改USB映射、端口、采样率或boot-args。缓冲试验保持挂起。必须分开报告磁盘安装、当前boot加载与USB听感，不能把安装完成当破音修复。

**2026-10-26 09:44 用户新指令：通知目标会话更新 NullMoth。** 已通知安装/恢复owner `01a11ccc-49a1-7ff2-8ba9-3698f271045f`，仅恢复本次官方版本核对、适配包/差异与备份回滚、具体更新候选准备；owner已active，尚未升级。实际共享驱动/KC/ESP切换须先收齐候选及同macOS26恢复材料落实具体窗口，不自动重启。既有lastgood指向macOS15的风险必须先处理，不能冒称可回退。其余功能开发及独立NVIDIA自动监控保持暂停。缓冲helper已交付，12项模拟门槛/清理检查通过，真实setter从未执行；当前挂起，避免与驱动升级混改，后续依新基线和播放就绪再安排。源码SHA256 `337ddce0e527efd72637f0b8f185ecc2f0f35c64fa6a5248de8d4d3e3ad2d7ba`，报告 `audio/buffer-discriminator-evidence-20261026/bounded-helper/READY-FOR-REVIEW.md`。通知投递与owner active不等于更新完成，更不等于USB破音修复。

**2026-10-26 09:34 当前结论与已授权下一步（优先于下方历史记录）。** 根因仍 UNKNOWN。G2000 在 XH00→XHC1→原 XH00 均有用户听感破音，Samson 输出也有破音；端口/双输出观察提高共用 CoreAudio、USB 驱动与平台实时期限链的调查优先级，不证明单一共因，更不是提高进程优先级的处方。用户明确 Windows 正常，接受该经验反馈，不要求重做 Windows。A/B/C 均破音说明自定义映射并非 G2000 症状的必要条件；D 禁用活动 NVIDIA 栈仍破音，NullMoth 只能列为可能放大因素，尚未证实贡献或性能不足。当前已恢复完整 A 启动，不再等待恢复重启。HAL safety/TRB 数量不等于可听破音次数，真实调度迟到与 sample→host 期限映射仍未分开。

用户“同意”并“继续”已批准一次 G2000 公开 HAL fsiz 临时原值→请求1024、手动播放60秒、恢复原值并读回。现无setter执行痕迹；音频owner仅准备有界helper供审阅，用户已确认播放器暂停且可开始。原值须现场重读，1024只是请求；若播放实际周期不可归属则因果判别 INCONCLUSIVE。无可比短基线时只记探索结果，不新增播放段。不得因缓冲无改善归罪 NullMoth。不得扩展到录音/trace/IOProc/StartNULL/驱动/EFI/KC/重启；其他六域暂停，持续监工保留。

**NullMoth `-nvoff` 是重要贡献因素，但不是唯一根因（2026-10-26 04:00）。** D boot `0CD28BF7-7DAC-4BA8-B23F-0C09452C45A9` 的 boot-args 含 `-nvoff`，上游语义是让 NVRM 不接管显卡。四个 bundle 仍在 KernelCollection 中，但运行时 `NVRM`、`NVAccel`、`NVRMDisplay` 实例均为 0，只剩 `NVRMBootFB`。A′ 检查窗口记录 20 次真实 kernel TRB mismatch，D 到检查时 0 次；用户主观破音也“稍微好一点”。但 D 的 CoreAudio deadline/overload 仍大量存在，说明还有独立的 CoreAudio/系统调度问题。磁盘已恢复 A，完整 D 现场先备份，待手动重启恢复 NVIDIA 加速。下一步优先用 Windows/第二输出区分残余问题是否 macOS 特有，再拆分 NullMoth 组件或测试 SSDT。

**NullMoth `-nvoff` D 已安全落盘，等待手动重启（2026-10-26 03:50）。** 从已验证 A `89067da0…1468` 出发，只在 boot-args 末尾追加一次 `-nvoff`，config D hash 为 `d907feb8…304d`；USBMap、SSDT、USB 拓扑和音频参数均不变。完整 448 文件 A 备份已逐文件核验；写后全树只有 `OC/config.plist` 一项变化，`plutil` 通过，`ocvalidate` 仍仅既有 4 条 `HideVerbose` 告警，ESP 只读，`fsck_msdos -n` exit 0。下次启动预期 NVRM/NVAccel/NVRMFB/NVRMAGDC 均不加载，显示可能退回基础 framebuffer、降分辨率或黑屏。若破音消失且 TRB/CoreAudio 异常显著减少，NullMoth 路径成为首要根因；若不变，则排除本版 NullMoth 对可听破音的直接作用。禁止自动重启。

**C 完成：自定义 USB 映射已排除，A 已恢复待重启（2026-10-26 03:35）。** 用户在无 USBToolBox、无 UTBMap、无 NativeUSBMap、XH00 原生 15 口的 C 下确认破音仍存在；同一 boot 的 03:32:19 CoreAudio deadline/overload 与 03:32:23 G2000 TRB mismatch 也仍出现。因此当前稀疏映射、Tahoe merge 属性及映射 kext 注入不是本次破音的直接原因。磁盘 config 已恢复 A `89067da0…1468`，完整 C 现场先备份；写后仅 config 变化，`plutil`/`ocvalidate` 等价检查通过，ESP 已只读，`fsck_msdos -n` exit 0。下一步需手动重启回 A，再用 Windows 或第二输出路径区分 G2000 设备行为与 macOS CoreAudio/USB 共用层。此排除不等于已找到根因。

**C 已启动：无自定义映射，底层异常仍复现（2026-10-26 03:33）。** boot session `4F0C10D7-7CE0-47D7-97D8-8A125CA8F508`，启动于 03:31:13。USBToolBox 与 NativeUSBMap 都未加载，XH00 为原生 15 口（总计 15/4/4/1），G2000 仍在硬件7/location `0x00700000`；Samson 和蓝牙按预期消失。日志仍在 03:32:19 出现 CoreAudio deadline/overload，03:32:23 出现 G2000 endpoint `0x02` 的四条 TRB mismatch。因此当前自定义 USB 映射/注入不是这组底层日志异常的直接原因。尚缺一次 C 下总体听感判定；若仍有相同破音，下一层比较 CoreAudio workgroup 调度与 AppleUSBXHCI 等时 completion。

**无自定义映射 C 已安全落盘，等待手动重启（2026-10-26 03:28）。** 当前运行会话仍是 A′；磁盘 `config.plist` 已切换为 C `94f8e379…6ed1`，相比 A 仅关闭 USBToolBox 和 UTBMap 两个 Enabled 项，NativeUSBMap 仍未启用。完整 448 文件 ESP 备份已逐文件核验；写后全树只有 `OC/config.plist` 一项变化，其余 447 文件不变。`plutil` 通过，`ocvalidate` 与 A 一样只有既有 4 条 `HideVerbose` 版本告警，ESP 已恢复只读，`fsck_msdos -n` exit 0（806 files，113,318 KiB free）。下次启动预期由 Apple 原生只建 15 个 XH00 端口：G2000 在硬件 7，仍应存在；硬件 16 蓝牙和硬件 17 Samson 将暂时不可用。若 C 中同一 TRB/CoreAudio 异常仍存在，当前稀疏映射适配不是直接原因；若消失，映射语义成为主嫌疑。禁止自动重启。

**A′ 已启动，拓扑通过且复现同类时序异常（2026-10-26 03:27）。** 新 boot session 为 `AF867CA3-5002-41C4-AF1A-A45B25584BED`，启动时间 03:22:56；`USBToolBox 1.2.0` 已加载，NativeUSBMap 未加载。XH00 仍为硬件 `{2,3,7,8,15,17}`，四控制器 `6/4/4/1`，无硬件16；G2000 仍在硬件7/location `0x00300000`，Samson 在硬件17/location `0x00600000`。ESP 本次未挂载，运行态检查不重写 config；重启前的 A hash 已核验为 `89067da0…1468`。日志在 03:24:11 先出现 CoreAudio deadline，03:24:22/24/59 出现 G2000 endpoint `0x02` TRB mismatch，03:25:02–11 出现 Audio IO Overload；A/B 两种映射实现都落入同一异常家族，但仍需无自定义 map 的 C 对照判断稀疏映射机制本身。

**macOS 15 成功方案与 macOS 26 当前方案已逐键对比（2026-10-26 03:22）。** 15 下 PASS 的 S2 使用 UTBMap `998c7fbb…`：XH00 硬件端口 `{2,3,7,8,15,16,17}` 共 7 个，port-count 17；26 当前 A 使用 `114c35b4…`：仅删除硬件 16，保留 `{2,3,7,8,15,17}` 共 6 个，port-count 仍为 17。USBToolBox 1.2.0 的二进制与 Info 哈希两期完全相同；`XhciPortLimit=false`、两条 15→17 二进制补丁也都保持关闭。真正变化只有移除 HS11、把 MT7922 wake-policy 的 MaxKernel 从 24.99.99 扩到全版本，以及因升级停用 AppleALC/NMHDA/旧 WiFi/BT 驱动。当前蓝牙实际是 Off、地址 NULL，硬件 16 不存在；只有硬件 17 的 Samson 仍通过稀疏 map 可用。G2000 在硬件 7，两次方案的逻辑位置都为 `0x00300000`，因此移除 HS11 没有直接移动 G2000。下一步最有判别力的是关闭全部自定义 USB map，让 Apple 原生只建 15 口；G2000 仍应在，Samson/蓝牙暂不可用。若破音仍存在，当前稀疏映射适配不是直接原因；若消失，映射语义才成为主嫌疑。

**B 启动已有新的时间相关证据（2026-10-26 03:08）。** G2000 在 02:59:53 开始 USB 播放后立即出现 `AppleUSBXHCIIsochronousRequest::update: mismatched event TRB`（endpoint `0x02`）；02:59:57–03:00:17 期间 Music 输出连续触发 `Deadline is prior to Start time` 与 `Audio IO Overload` safety violation，参数为 USB、48 kHz、512 frames；03:00:18 StopIO 撤销 8 个请求。日志时间上两层的异常相邻/重叠，足以说明“USB 全无报错”不成立，但仍不能证明 completion 错配是破音主因，也不能排除 CoreAudio workgroup 先超时导致后续传输清理。00:60 探针本次仍无新采集：recorder 固定查询 `.196/.130`，Mac 已移动到 `.101`，当前 `UNREACHABLE` 是探针配置陈旧；MTKCAP 最新 Linux run 仍是 2026-10-25 12:29:55 UTC，不是本次 B boot。下一轮应先回到 A 收尾，再用受控输出工作负载区分 completion/USB、CoreAudio 调度与 G2000/DAC 三层。

**B 已实际启动，A′ 已放回并等待重启（2026-10-26 03:11）。** B boot session 为 `C1F71EDF-42D3-4185-9CFF-90C7840CB0B0`；当时 config B 中 USBToolBox、UTBDefault、UTBMap 均为 `Enabled=false`，NativeUSBMap 为 `true`，运行态没有加载 USBToolBox，根端口为 6/4/4/1 且硬件16/HS11 未出现。用户报告 B 下仍破音，因此“使用 USBToolBox 映射实现”不能解释全部症状。OCAuxiliaryTools 退出后，救援脚本已把 config 恢复到 A `89067da0…1468`，把 B `45402eb4…0fe4` 另存为证据，二次检查 `ALREADY_A`，ESP 恢复只读，`fsck_msdos -n` exit 0。下次手动重启即 A′；端口和同一条件下的听感验证完成前，不把 B 的结果外推为固定根因。

**映射对照条件更新（USB owner直接用户指令）：** 用户表示VNC可用并同意相关对照，又要求提供Windows回退办法、允许将救援材料上传GitHub。USB owner正在准备卷外A/B配置及macOS/Windows恢复包；当前回合尚未终态，不重复派发。其报告445文件备份核验通过、ESP仍只读未切换；模拟恢复通过不等于Windows实机恢复通过。主协调已登记条件性授权，仍须收齐具体包、审阅差异/回退条件、确认Windows能访问救援A并指定EFI串行写者；未开放自动重启、录音或其他域恢复。本段仅更新授权与在途进展，不宣称可部署或验收完成。

**映射评估已收齐：根因未定，原生B候选仍不可部署。** 当前实载为USBToolBox 1.2.0 + UTBMap；上游1.2.0标注Tahoe support (WIP)，corpnewt USBMap也已生成Tahoe新旧属性键，无证据证明任一方案在macOS26普遍最优。原生codeless候选0.3.0端口表静态等价通过，但merge残留硬件16、首次createPorts先后、干净启动匹配及独立回A救援路径未验。建议端口参数保持不变的A→B→A整套映射方案对照；ACPI处理/启动时序也随方案变化，不能直接归因单个函数。音频已交实际破音判据：同设备/物理口/曲目/格式/音量与相等有效播放时长，A有/B无/A′有增强关联；无复现或条件变化为INCONCLUSIVE，日志静默不算修复，也无需逐次人工报点。后续先准备ESP外配置、差异白名单/ocvalidate和回A条件，再申请具体串行启动窗口；本轮未部署或重启。NULL Start/Stop继续暂缓，六域继续暂停，15分钟监督保持。报告：`usb/tahoe-mapping-ab-review-20261026/REVIEW.md`、`audio/mapping-ab-verdict-20261026/VERDICT-DESIGN.md`。

**用户新指向：优先检验USB映射方案的因果，而非继续零散日志/计时查询。** 用户强烈怀疑USB相关kext，并问macOS26是否仍以USBMap最优。已派USB owner核对当前实载USBToolBox/UTBMap与USBMap区别、上游Tahoe明确支持依据、既有原生候选的等价A/B/A真实门槛；音频owner只定义实际破音判据与偶发性/控制变量边界。不承诺一次保证定位，不把TRB日志或静态校验当真实破音红/绿闭环。NULL Start/Stop窗口改为暂缓，用户提问不是批准。仅评估/准备，无部署、EFI/KC/BIOS改动、播放录音或重启授权；其余六域暂停。

**01:48监工收件：可执行候选已审阅，等待一次具体窗口批准。** 音频交付 `audio/nullstart-candidate-20261026/READY-FOR-REVIEW.md`、完整源码/单次wrapper；真实版本编译未运行，7项模拟生命周期/flags/watchdog/禁止重复检查通过，主协调逐一核对16项manifest哈希及attempt不存在。已审阅完整UID/boot双门槛、Start成功才配平一次Stop、异常清理不明即禁止重跑。仅请求对G2000执行一次Start(NULL)→最多2秒/100组公开计时读取→Stop(NULL)；不是播放/录音，但会改变设备活动生命周期，可能干扰现有输出，HAL阻塞或Stop失败时不能保证清理配平。准确命令/批准对象哈希/残余风险见state.pendingDecisions。

真实Start/Stop窗口仍关闭、尚未执行。两个owner现均idle：USB受运行态证据缺失阻塞，音频等待本次用户决定；不再扩展离线函数或重复采样、不过度调用Astra/付费模型。15分钟监工保持，未获批准时仅低成本增量检查，不重复催问同一窗口，其余六域暂停。

**01:33监工收件：两项限定离线审查均已到边界。** SDK记载计时服务的受支持生命周期为Start(NULL)→查询→Stop(NULL)，不保证仅凭全局running即可查询，故纯getter路径关闭；具体既有stop错误仍不能排除状态竞态。USB activate/link模型证实会容忍部分已越过段事件，但实际请求/段代次仍缺，不继续扩展函数扫描。报告 `audio/hal-availability-gate-20261026/AVAILABILITY.md`、`usb/activate-segment-20261026/ACTIVATE-MODEL.md`。

USB因缺运行态证据而合理闲置。音频仅准备可编译、可审查的NULL Start/Stop候选及模拟清理验证，真实Start/Stop未授权，禁止执行。候选完成后主协调审查命令/输入输出/退出清理边界，再提交具体窗口给用户决定；不以文档草案就要求批准，也不把失败或被杀后的状态恢复说成已保证。Astra不重复调用；原空日志和getter窗口不复活，其余六域暂停、监工继续。

**01:18监工收件：只读HAL映射路径遇到真实接口阻塞。** 唯一一次GetCurrentTime返回`kAudioHardwareNotRunningError`（stop/flags0），TranslateTime未调用、零映射pair；子进程正常回收，未重试/启动设备。全局running门槛与该调用不是原子快照，不能据此判USB停播或时钟稳定，根因仍UNKNOWN。USB owner已独立确认Astra对象身份反例，并纠正自有报告/校验器：“最终必Busy”撤回，实际Busy或0分支未知。

已分别安排最后一项有明确边界的离线增量：音频核对公开API是否要求调用进程启动IO、给具体最小解锁条件；USB仅查activate及必要段赋值，明确允许接受的段模型。两者均不得继续无界扩展函数/重采或自行启动设备；若仍欠运行态代次/调度证据，须记录到此可安全离线推进已到头及具体执行候选。报告见 `audio/hal-map-one-shot-20261026/RESULT.md` 和已勘误 `usb/error-propagation-20261026/ERROR-PROPAGATION.md`。Astra一次复核已完成不重复，其他六域暂停，监工继续。

**01:03监工收件：Astra已交付，根因仍UNKNOWN。** 固定KC反例纠正前序“mismatch最终必Busy”：队首null探测为0后，扫描后续pending请求发生BadArgument，最终返回仍可能被覆盖0；不能把选中请求与原队首混为一体。请求不正常推进、该分支不直接abort仍成立，运行时走哪支未知，已交USB owner独立复核并更正自有报告。原始音频还有已知tap停止后的目标safety和部分标为silent的Music周期，不能把日志异常都当自然音乐破音。报告 `audio/astra-review-20261026/ROOT-REVIEW.md`。

Astra一次升级已完成，不重复叫用。已交音频owner单次公开HAL映射相位getter：最多2秒/100组、自有子进程3秒watchdog，UID/运行/接口门槛不符即停，不主动播放、不录音、不setter/trace/硬件传输/持久配置、不重试；这是结构化映射观察，不重复空日志。阳性也不证明私有deadline或USB硬件根因，阴性也不排除未复现。其余六域暂停，15分钟监工继续；所有新任务仅为派发，不代表结果或修复通过。

**Astra升级已触发：** 两份增量报告已收件仍未确定根因，已按用户授权在音频owner当前回合结束后显式切换 `gpt-6-astra` high，唯一任务 `ASTRA-USB-AUDIO-ROOT-REVIEW-20261026` 已派。USB上层错误传播：BadArgument→Busy、不推进完成、不直接abort；音频严格配对6周期，10.666ms期限间隔与11.354ms预算不同，HAL自洽不能充作独立调度证据。原始交接在 `coordination/ASTRA-HANDOFF-20261026.md`；USB暂不重复分析，等待Astra一次跨域可证伪机制审查。定期监工继续，未开放新危险采集或发布窗口。

**2026-10-26 主协调监工恢复：** 用户要求主会话持续监工。已恢复每15分钟仅USB/音频的增量收件（`hackintosh-usb`，无实质变化静默），并核实独立NVIDIA监控当时为ACTIVE，现已重新暂停。USB已派固定版本update错误向上层传播/请求推进影响审查；音频已派同线程同周期deadline与时钟映射语义审查。两任务均只读离线、GPT6.1sol medium、无子代理，不重复空日志/症状摘要。用户新增授权：本轮两份报告仍未收窄可验证机制，或15分钟无实质进展，则待当前音频回合结束后仅一次升级 `gpt-6-astra` high 做原始证据跨域根因复核；唯一派发键及触发门槛见state.astraEscalation。其余六域暂停，所有发布/危险采集/重启门槛不变。监工不因一次idle擅自删除，也不为制造进度重复测试。

**用户补充播放路径（2026-10-26）：** 音乐破音在Apple Music及B站网页音乐视频均有察觉；影视主要也在B站网页观看而较少察觉。此为跨应用、同网站不同内容的用户观察，尚非同浏览器/编码/源采样率/音量/时长的受控对照。Apple Music专有路径不足以统一解释；共用系统/USB链路仍保留，用户进一步倾向“异常同样存在、音乐更容易听出”；这只作为主观判断，尚非两场景异常率相同的实测。后续优先共用USB/调度/音频时钟，内容诱发异常率分支降级。音频owner已完成离线假设修正，见 `audio/SYMPTOM-PATH-UPDATE.md`；共用系统/调度/USB链路保留，实际异常率与可察觉程度未分开。不新增采集或改设置，音频/USB当前无在途任务，其余六域暂停。

**2026-10-26 00:13只读窗口已收尾：** 音频owner一次15秒观察按时结束，log子进程已回收、未重试。G2000默认输出、设备nominal/virtual/physical均48kHz、全局运行标志1，但仅捕获一条HAL维护事件，无法确认整窗持续播放或目标Context/TRB异常；结论INCONCLUSIVE、听感NOT_ASSESSED，不能判PASS。用户补充“看影视基本未察觉，听音乐较明显”已登记转交；音乐/影视实际客户端输入格式、重采样和独占状态仍未知，不能凭设备48kHz假定源格式。报告 `audio/passive15-20261026/REPORT.md`。单窗已关闭，两域闲置、其余六域继续暂停，无后台新任务；后续优先核对应用播放元数据差异，不自动重采日志或改设置。

**本轮一次只读窗口：** 已审阅 `audio/resume-trb-audio-align-20261025/NEXT-DISCRIMINATOR.md`，主协调依据用户继续诊断授权，开放一次目标只读核实及至多15秒log stream（256KiB/2000行先到即停）；目标未运行即止，不主动播放、不要求人工报点、不自动重试。录音/System Trace/追踪资源接管/持久日志配置/驱动/EFI/KC及重启窗口仍关闭。仅这项被动观察在途，结果待收件。

**2026-10-26 恢复收件（优先于下方历史状态）：** 音频关联、15秒历史尾窗及USB固定25G241错配语义均已交付并审阅。两轮G2000新流启动后约78/69ms出现完成TRB错配；第二轮Context184/a744明确有Music/G2000 overload/safety记录，54.976 StopIO与abort8请求同毫秒。停止时撤销符合清理时序，不等于8次丢包。首轮实际IO约0.653s；旧22.607ms scheduler_latency属于a4a6/Codex，不能拼成G2000 a482的同周期调度证据。

5条事件为3 Success/2 Stopped，均未匹配驱动当前跟踪的TRB段集合并返回kIOReturnBadArgument；缺event parameter→enqueue/request generation→Stop Endpoint完成/换环链条，仍不能区分活动完成异常与旧代次迟到，也不能归罪USBToolBox或判定听感。根因NOT_IDENTIFIED、听感NOT_ASSESSED。报告：`audio/resume-trb-audio-align-20261025/{REPORT,TAIL-CONTEXT}.md`、`usb/trb-predicate-20261026/TRB-PREDICATE.md`。仅向音频owner转交语义结果并派一次离线最小判别候选任务；USB闲置，六域暂停，自动收件仍删除。未开放采集/资源接管/配置/安装/重启窗口。

**最新用户明确恢复：** “继续”仅恢复USB音频诊断，覆盖下方音频停止状态。已派音频既存日志时间线关联、USB匹配版本TRB语义核对；无新采样/录音/trace、无部署或探针改址窗口，不恢复其他六域，不重建定期自动化。

**22:59协调自动化收尾：** 音频已停止且USB闲置，反复收件无增量或可执行窗口，已删除`hackintosh`每15分钟收件以节省额度。破音诊断与TRB/停流关联仍未完成，保存证据/断点，不恢复其他域；明确恢复工作后再安排收件。

**20:44执行状态修正：** 音频的既存TRB/Start-Stop关联回合已中断，最新owner终态确认停止、未录音/未System Trace，未交付新关联报告。下方“交音频owner”仅表示已派发，不代表完成；保存结果，待明确恢复，不自动重启。USB及其余暂停域不追加工作。

**20:32:19本次启动已收件：** boot54893A74…；no-HS11端口表6/4/4/1仍生效，MTKCAP采集完整。60独立日志已捕获G2000等时OUT0x02的5条完成TRB错配及随后8请求撤销，优先交音频owner用既存归档对齐短时系统音频Start/Stop和deadline异常，先区分正常停流清理与主因，不新增trace或外推听感PASS。60的固定目标`.196/.130`已与Mac `.101`beacon脱节，USB observer仍旧boot，零计数不能验收；该探针配置问题已登记未改。其余六域暂停继续，所有发布/重启门槛不变。详见STATUS顶部及两个reboot报告。

**最新启动事件：用户再次重启回macOS，并完成一次U盘启动。** USB/音频两个owner接续只读取证：USB收取MTKCAP及当前控制器/映射基线，音频核对新boot输出及有界当前日志；本次boot UUID/设备session待核实，旧18:07现场不再标为当前。其余六域仍暂停，不把用户自行启动视作候选发布或追踪资源接管授权。

**最新用户覆盖（2026-10-25 19:35起）：** GPT五小时额度将到，暂缓其余会话项目开发，只优先USB音频破音。仅音频/USB保持有界诊断，原生map通用适配也暂停；Wi-Fi/BT、CX3、NVIDIA视频/安装、RTL及显示器六域已确认暂停，保留成果，恢复需明确新指令。CX3当前idle、WAVE4未构建。自动收件只看音频/USB，独立NVIDIA监控暂停；不得自动用付费DeepSeek接续其他项目。优先核对现用kext因果边界及独立调度追踪安全前置，根因尚未锁定；共享发布/重启及系统追踪资源接管窗口均未开放。本段优先于下方历史并行任务安排。

**19:49焦点结果：** USB源码审查未见映射kext直接逐笔处理音频，间接拓扑/时序仍未排除；没有运行正常证明。音频2秒标准追踪候选已具体化，但tailspind资源仲裁/异常恢复仍未证，NOT_READY/NOT_CAPTURED保持。两个owner已终态，不重复调查或自动采集；待明确串行窗口及可接受的恢复边界。其余六域暂停不变。

用户已授权根据当前配置与原验收清单向现有会话分派验收及相关修复；当前目录全模型均可协作，包括 GPT 系列与 deepseek-flash。完整任务、归属、共享变更边界和回执在 `outputs/20261025-tahoe-acceptance/PLAN.md`、`tasks/`、`dispatch.json`。

最新结果见 `outputs/20261025-tahoe-acceptance/STATUS.md`：RTL8125短时验收通过；NVIDIA当前安装和显示通过，但自动回退仍含macOS15包，尚未验证为26可用；18:07重启后USB风暴停止，连接短窗口通过，USB音乐仍偶发破音。Wi-Fi、音频、视频和CX3仍在进行，不将分派成功写成验收完成。

**第二轮协调修正（用户当轮指令）：** 主会话只统筹，不再执行各域诊断、采样或实现。已向七个现有owner投递并行开发任务，音频牵头故障诊断、USB提供底层证据及离线替代map，Wi-Fi/CX3/视频/恢复方案同步推进，RTL提供共享网络兼容门槛。不得把持续人工报点或关面板作为诊断前提。具体所有权与交付见`outputs/20261025-tahoe-acceptance/tasks/coordination-wave2.md`；发布/重启另开单变量集成窗口，主会话指定具体执行owner。CPU传感器/早期故障取证保持P2队列，AppleHV/核显保持原门槛。

**协作模型成本规则（同日用户补充）：** GPT可用时，大多数协作默认优先GPT；deepseek-flash仅作必要补充、保守使用。此要求同样适用于所有协作会话及其下级子代理，不以“全模型可用”解释为默认选DeepSeek。不为切换模型重做已运行任务。

**19:13增量收件：** 音频取得83组控制器帧号/host-time短窗证据，仍缺独立调度/端点映射/completion，根因未定；与USB证据接口已闭环转交。原生USB map0.3.0被动匹配通过，但merge旧键/启动时序未证，仍不可部署，owner继续离线审查。Wi-Fi WAVE3对象尺寸/签名迁移使raw import缺口33降至4，虚表和回调仍阻塞；CX3 .139 VLAN/MAC/checksum完整构建及定向简验通过，均未实际加载或硬件验收。显示器用户态修复及615.78.08隔离实验库已交，当前2.5.4配对vendor/KAPI桥未闭合；已安排安装owner纠正DDC寻址契约并提供准确构建资料，显示器继续离线异步状态机。26恢复包未激活，视频源码依赖未解，RTL保持。详细证据与四项派发见STATUS及coordination-state；共享发布窗口未开放。

**19:26依赖更新：** 音频限定清点未找到已有调度trace，新采集资源仲裁/恢复未验证，保持NOT_CAPTURED/NOT_READY，不自动试录。安装已交DDC勘误和KAPI类型/锁契约，但准确配对2.5.4的Darwin615 vendor及构建provenance缺失，已转显示器，clean615仅实验用途。Wi-Fi/CX3的WAVE3均已终态收件，已继续派原生虚表迁移和链路/media/down-up/电源离线切片；USB/显示器当前执行不重复催促。未解除任何实机发布门槛，视频/RTL保持。

**显示器子线纳入与恢复（用户同意）：** “评估 Redmi G Pro 27U macOS 调节支持”`01a14aaa-ae44-77c3-a768-c7c571eb8c2e`正式纳入C3外接显示器控制，P2，解除旧暂缓。既有HTTP/ADB控制面与原生亮度候选分别推进；原生控制中心支持未验，旧2.3.1构建部署步骤不沿用。与安装`01a11ccc…`和视频`01a16744…`按文件/接口协同；只离线候选，不写共享驱动/KC/ESP。任务书：`outputs/20261025-tahoe-acceptance/tasks/monitor-ddc-wave2.md`。本主会话继续唯一统筹，定期主动收取八个owner进展，避免另开重复管理会话。

**18:07重启后续：** no-HS11六口保护表已生效，HS12位置0x00600000，枚举/地址/过流失败计数0，选定外设session稳定；仅PASS_SHORT_WINDOW，不是USB音频PASS。用户报告HECATE偶发破音，换USB3口相同；IORegistry确认已从XHC2换到1022:43fc控制器，两边均Full Speed。换口后仍有Music实时周期超时与safety violation，USB驱动/等时传输与调度时钟尚待区分，未取得听感时间相关性。USB及音频owner继续只读分析，暂缓板载音频候选安装，不改缓冲/采样率。定时器等待延迟仍有波动，不能再全归因USB风暴，也没有证据支持放宽Darwin24旧timer补丁。证据：`outputs/20261025-usb-reboot-180741/`。

**17:54续跑更新：** 当前boot已变为17:40:59。只读探针已补端口计数delta与跨boot识别；HS11风暴复现。完成本地/NAS各444文件核验后，已只替换UTBMap为no-HS11保护候选（`998c7fbb…`→`114c35b4…`），原444文件仅此项变化，config不变、ESP恢复只读；待用户从内置OpenCore重启验6口/外设连续性，内置蓝牙暂不可用。音频board-only临时AuxKC已链接通过、未部署；Wi-Fi V1两处RESERVED合法修补机制已核对，但旧用户态/Skywalk条件仍未解决；视频切换在新boot有界对照未复现，BT709/低延迟仍待修。完整证据在 `outputs/20261025-tahoe-followup-1740/` 及各域FOLLOWUP。

- 真实 ESP config 为 57055 B / `89067da0…1468`；OpenCore 仍为 `7754f6d6…73c8`。协调者已只读挂载 disk0s1，备份 EFI 并逐文件核验 444/444；不使用旧 efi 快照覆盖。
- 本次启动日志与当前 config 交叉核对：16 条适用内核补丁全部 Success 且替换数大于 0，10 条按版本跳过；旧 Darwin24.6 timer 补丁正确跳过。`DisableVariableWrite=true` 保持。
- 首轮定时器短测的实时线程 1000µs 中位为 1.410 倍，未达到旧清单 1.05 倍参考门槛；采样时 USB 风暴与多会话并行工作尚在进行，暂不归因标定错误、不放宽旧补丁版本号。先恢复稳定基线，再复核。

| 工作域 | 当前验收事实 | 已派现有 owner / 目标 |
| --- | --- | --- |
| USB / 蓝牙 USB 前置 | HS11隔离后六口连接短验通过；USB音乐仍破音，跨控制器换口未消除音频时序异常 | `01a123d5…`：检查等时传输/USB共享驱动及补丁路径；完整BT待单独恢复端口和验收 |
| MT7922 Wi-Fi / BT 协议栈 | PCI 存在但无 Wi-Fi 接口；旧 Wi-Fi/BT 依赖 kext 均限制到 Darwin24.99.99 | `01a1671c…`：核对25 ABI及版本门控，保留已有未部署功能成果，形成兼容候选 |
| NVIDIA 安装/显示 | 四个2.5.4已加载，Darwin25变体；Metal3、4K160已有升级后初验 | `01a11ccc…`：payload/AuxKC、bootguard和回滚验收，不重复安装器 |
| NVIDIA 视频 | 旧15上的硬编硬解/色彩/低延迟结果不能外推26 | `01a16744…`：原生VT实际出帧、色彩三方对照、低延迟和可自动执行的播放短检 |
| USB、板载与显示音频 | USB HECATE在26仍偶发破音；旧AppleALC/NMHDA门控到24，板载/GPU无输出；旧15断音结案保持 | `01a14a75…`：先对齐破音听感与时序证据，board-only候选仅临时链接通过、暂缓部署 |
| CX3 | 内核kext 0.44.47已加载，PCI存在；链路/接口尚未启用，不能直接判定固件损坏 | `01a15c2a…`：只读核对25 networking/DMA路径和日志，离线修复；不触发风险初始化 |
| RTL8125 | Lucy1.1.100加载，en0为当前默认出口；AppleEthernetRL已被25.x Block | `01a129b1…`：核对25分支、Skywalk/AppleVTD/真实绑定，保留默认出口 |

各域首轮可并行只读验收、源码修复与离线构建；ESP/USB写入、安装/卸载、重建KC、重启/睡眠及风险硬件操作由协调者安排串行窗口。本轮分派不等于所有功能已通过。CPU传感器、AppleVTD/HV、核显新增实验暂不插入升级回归。

## 升级前 Sequoia 状态索引（2026-10-25 早间历史）

| 工作项 | 已确认状态 | 下一步 / 证据入口 |
| --- | --- | --- |
| M0 启动/回滚、更新维护 | 基线、恢复演练、BIOS 核对、补丁注册表及更新校验工具已交付；R2 schema 告警无害结案；R10 旧日志 323 份已可逆迁出 | `docs/handoff-amd-boot-config.md`；仅在系统/驱动更新时回归，不为消告警升级 OpenCore |
| USB 基础适配 | S2 稀疏 map 跨重启 PASS，MT7922 remote-wake 修复已生效 | 保留正常使用观察；取消过的长压测不恢复 |
| Wi-Fi | 现 owner `01a1671c-cac5-72e2-808f-fb5abcd74269`；Linux <SSID> SAE/PMF、DHCP、HE160 协商成功；不能外推 macOS/6GHz 已通过 | 继续 WTBL/BSS HE 与数据面、扫描/CoreWLAN、AWDL；USB 最新结果已同步 |
| USB 采集工具 | 旧 reader 的 perf RAW 尾填充错误已修复并部署；macOS/Linux 各 8 项 ring 回归通过；旧采集仅保住 16 条 WTBL，缺失 32 条不可恢复 | 下一次计划内 MTKCAP 启动验证完整 STA 和上传 ACK；`outputs/20261025-usb-capture-analysis/RELEASE.md` |
| NVIDIA | 2.5.4 实际重启验收通过：四驱动、Metal 3、4K160 HiDPI、旧 NMHDA 检查 GREEN；用户短时体验 Chrome 闪烁未再出现，转日常观察 | 旧版硬编硬解已有出帧证据；色彩元数据、低延迟及应用播放路径需在 2.5.4 各自复核；`outputs/20261025-nvidia-254/README.md` |
| 音频断音 | 用户已确认解决，关闭独立工作流 | 保留更新后的短回归，不重开 amd-audio |
| 定时器 / NVRAM | 定时器复核未见旧 8 倍异常；普通等待 1.25 倍为合并行为，实时线程接近 1 倍；DisableVariableWrite 保持 true | 不因合并行为改补丁；早期内核 panic/硬复位持久取证仍待解决 |
| CPU 传感器 / 电源 | 原版 SMCAMDProcessor 不支持 Family 1Ah；温度离线解码原型 8 项测试通过；root powermetrics SMC 未给出真实温度；SuperIO 仍未知 | 当前 U 盘自带匹配的 k10temp；先完成 STA 验收，之后单变量取得 Linux Tctl 对照，再解决 macOS 读取通道；`outputs/20261025-amd-sensors/README.md` |
| 蓝牙 / 睡眠 | 控制器、配对、短播放、断开重连已有 PASS；完整的睡眠唤醒持久性未验 | 后续短验收联查蓝牙、USB、显示、网络，不从控制器 On 外推全部通过 |
| CX3 | 新被动 Linux 基线为 PARTIAL：无 mlx4 模块绑定、firmware/PSID 未知；macOS 首个 TX 错误和 DHCP 仍由 owner 收敛 | owner `01a15c2a…` 正在推进；`.129` 保护候选不能等同首错已修；硬件结果以该会话当轮证据为准 |
| AppleVTD / AppleHV / 核显 | AppleVTD 仍缺安全自检与真实消费者验收；AppleHV 未立项；核显 Deferred | 研究线不阻塞日常可用，未因本次继续指令自动开启新硬件实验 |
| 安全姿态 | 只读评估已交付，保持现状 | SIP/AMFI/FileVault 收紧为可选独立任务；Apple Pay/T2 不作为可实现目标 |

**当前可执行顺序：** 重启前完成传感器离线阶段及文档校准；Wi-Fi/CX3 由各自现有会话推进。下次先验证当前 STA reader 修正版，再单独接入 Linux Tctl 对照，随后安排短睡眠/蓝牙集成验收。当前 U 盘包保持 `67d614c6…1208e`，CPU 传感器候选尚未加入。

---

**Goal:** 把当前 AMD Hackintosh 的适配工作拆成边界清晰、可独立验收的会话工作流；先补齐日常可用性，再处理平台能力与长期研究，避免多个会话同时改 EFI 导致不可归因的故障。

**Architecture:** 分三层：A = Apple 内核/框架层，B = I/O 与总线层，C = 设备驱动层；D 是依赖上述能力的验收功能，不单独立项。每个工作流一个 owner session；跨工作流的 boot 配置由 `amd-boot-config` 统一冻结、备份和发布。

**Tech Stack:** OpenCore、AMD_Vanilla 内核补丁、Lilu/VirtualSMC/AppleALC/WhateverGreen、AirportMtk、NullMoth NVRM/NVMTL/NVAccel、Cx3Mlx4、AVAMDVi、macOS 15.8.1 (24H32)、Ryzen 7 9700X。

**Spec / Context:** `docs/device-driver-feasibility.md`、`docs/install-runbook.md`、`docs/todo-open-items.md`、`outputs/20261017-audio-current/`、`outputs/20261013-applevtd-amdvi/`、`~/Documents/Github/AirportMtk/docs/architecture/upstream-port-plan.md`、`~/Documents/usb-port-limit-macos/verification/`。

---

## 0. 结论先行

- **真正“Apple 侧没有 AMD 后端”的**：AppleHV（CPU 虚拟化）、AppleVTD（IOMMU）、XCPM/P-state 电源管理、SMC 传感器/风扇、NVRAM 写入语义、Zen5 定时器标定。
- **当前尚未完成的主要设备线**：MT7922 WiFi 数据面/HE/AWDL、蓝牙与睡眠持久性、RTX 5060 视频功能完善、Mellanox CX3 稳定性；USB 基础修复和音频断音已完成相应阶段。
- **已有会话在做的**：AppleVTD（`01a13455…`）、USB（`01a123d5…`）、MT7922 WiFi（`01a123d6…`）、NVIDIA 安装/音频/PR（`01a11ccc…`、`01a14a75…`、`01a14a4b…`）。这些会话是 owner，不另起同名流。
- **已拍板（2026-10-19）**：G0 日常优先，研究线保留但不阻塞；G1 研究超过 15 口限制的最优解，不预设全 17/15；核显（C8）列为 Deferred，后续再做。
- **不建议同时开超过 3–4 个会改 boot 配置的会话**；所有 EFI 写入必须走 `amd-boot-config` 的冻结窗口。
- **本会话只做协调/派发**：Wave 0、USB 研究、WiFi/NVIDIA/音频实现都由对应 owner 会话执行，不在本会话里包办。
- 其余决策门（G2–G9）按 §6 的触发时机再定；现有授权内继续离线工作，硬件实验按 owner 的单变量窗口执行。

---

## 1. 历史基线（2026-10-20；当前状态见顶部索引）

> 这是规划用快照，不是交付验收。Wave 0 必须重新冻结“仓库快照 / 实际 ESP / 本次运行态”三者，三者不一致时以实际运行态为准。

| 项 | 当前证据 | 备注 |
| --- | --- | --- |
| 硬件 | Ryzen 7 9700X（8C/16T）、48 GB、MacPro7,1、RTX 5060 8 GB、2 TB NVMe、RTL8125、MT7922、CX3 | 双系统，U 盘/Windows 可作恢复入口 |
| OS | macOS 15.8.1（24H32），裸机 | `kern.hv_vmm_present=0` |
| 启动源/ESP | 内置 NVMe `disk0` 的 ESP `disk0s1`；OpenCore `DBG-107-2026-10-14`；config 56,063 B（`3dad2596…`） | 仓库 `efi/` 是 10-07 旧快照，不能覆盖 ESP；详见 `outputs/20261020-amd-boot-config/BASELINE.md` |
| BIOS | SVM Enabled；IOMMU Enabled；SR-IOV Disabled；XHCI Hand-off 无此选项；Secure Boot/CSM/Fast Boot Disabled | 2026-10-21 用户目视确认，未改值 |
| CPU 虚拟化 | `kern.hv_support=0`；`machdep.cpu.vendor=AuthenticAMD`；`machdep.cpu.features` 无 VMX | AppleHV 无 AMD 后端 |
| IOMMU | `ioreg` 无 AppleVTD 实例；10-20 基线看到 `1022:14d9` IOMMU 在 `00:00.2` 正常枚举；BIOS 目视 **Enabled** | 10-08 的 “Disabled” 是解码误差；以 10-21 目视为准 |
| CPU 电源 | `machdep.xcpm.mode=0`；`hw.cpufrequency=5.14 GHz`；`pmset -g therm` 无 CPU power status | 无 XCPM/P-state；未装 SMCAMDProcessor |
| 定时器 | 音频项目实测 100/500/1000 µs 等待实际中位为请求的 7.93–7.99 倍；Zen5 12-bit FID/FSB 处理可疑 | timer 补丁已部署但未单独验收 |
| WiFi | AirportMtk 0.1.0（`d4c5c5a5…`）；en1 active `192.0.2.130`，5GHz 160MHz；PHY a/b/g/n/ac/ax | 关联/4-way PASS：msg4 `dv=2/sec=1`，Rsn `akms=2`，RxBA agreed=2，RxAgg ampdu=515；短吞吐 75/37 Mbps；扫描 UI 仍 0（airportd 空缓存） |
| 蓝牙 | `State: Off`、`Address: NULL`、`Firmware v0 c0`；BT 在 USB Port 16 | 被 15 端口上限挡住；SimpleMtkBtFw 属实验性 |
| USB | S2 稀疏 map 已生效：XH00 **7 个对象**（硬件 2/3/7/8/15/16/17，位置 1..7），其他控制器 4/4/1；蓝牙已 On | 2 条内核补丁仍关闭；待探针观察旧 USB 补丁症状 |
| GPU | NullMoth 2.3.7 已加载；Metal 3、4K@160 Hz 正常 | `h264/hevc_videotoolbox` 编码失败 `-12908`；`videotoolbox_vld` 解码初始化失败 |
| CX3 | Cx3Mlx4 0.44.36 已加载；mailbox/DMA 已通 | `attachInterface` 冻屏未解决 |
| 音频 | AppleALC + NMHDA；内置输入/输出/HDMI 可用 | 曾有断音；关闭 USB 端口补丁后改善；timer 补丁未单独验收 |
| NVRAM | `DisableVariableWrite` 规避写 NVRAM；早期 panic 无报告/无 unified log | 仓库 10-14 决定不做接管 |
| 已正常 | RTL8125 en0、NVMe TRIM=Yes、USB 键鼠、4K 显示 | 不单独立项，只做回归 |

### 1.1 已有会话（owner 映射）

| 会话 | 标题/职责 | 在计划中的角色 |
| --- | --- | --- |
| `01a13455-ee1f-73b2-a501-793fd020dc51` | AppleVTD 支持 AMD IOMMU（路线 1b） | A2 owner，继续使用，不新建 |
| `01a123d5-8642-7810-9a09-8d4f19d4f12a` | USB 15 端口上限补丁 | B1 owner，继续使用，不新建 |
| `01a1671c-cac5-72e2-808f-fb5abcd74269` | MT7922 驱动核心整改与 AWDL | C1 当前 owner；旧 `01a123d6…` 已交接 |
| `01a11ccc-49a1-7ff2-8ba9-3698f271045f` | NVIDIA 驱动安装（2.3.x） | C3 的安装/打包子线 |
| `01a14a75-0606-7a40-8574-5b52fa2a26bc` | HDMI/DP 音频修复 | C3 的 HDMI 音频子线 |
| `01a14a4b-a3cc-7b61-ae18-331017050bc8` | NVIDIA Chrome 加速 PR #263 | C3 的上游/PR 子线 |
| `01a1482b-452f-7700-bf2b-11cb9beb1e2a` | VNC / USB / 显卡驱动验收 | C5 的关联验收会话，不直接当音频 owner |
| `01a15c2a-ed65-7f52-a04a-d2b103a4de9b` | CX3 冻屏复现与 60 探针取证 | C4 owner，继续使用，不新建 |
| `01a153f1-711c-7e72-a33e-aac4728694dc` | 本会话 | 程序协调/规划；不承担实现，避免既当裁判又当运动员 |

---

## 2. 分类法

### A. Apple 内核/框架层（没有 AMD 后端，或语义不完整）

| ID | 工作项 | 现状 | 影响 |
| --- | --- | --- | --- |
| A1 | AppleHV / Hypervisor.framework（Intel VMX → AMD-V/SVM） | 无后端；`hv_support=0` | 本地 VM、Docker Desktop、UTM(vz)、Lima(vz)、QEMU-HVF 全部不可用 |
| A2 | AppleVTD / IOMMU（Intel VT-d → AMD-Vi） | IOMMU BIOS=Enabled；已有 AVAMDVi 研究；AppleVTD 未装成；无真实消费者 | 无 DMA 隔离；SR-IOV 另受 BIOS Disabled 限制 |
| A3 | CPU 电源管理（XCPM/P-state/C-state/CPPC） | `xcpm.mode=0`；5.14 GHz 为静态展示值 | 缺少可信测量，不能据此证明锁频或功耗/温度偏高；先做传感器 |
| A4 | SMC 传感器/风扇 | 只有 VirtualSMC 假 SMC；无 SMCAMDProcessor | 无 CPU 温度/功耗/风扇传感器；风扇只能 BIOS 控制 |
| A5 | 定时器/TSC 标定（Zen5 FID/FSB/LAPIC） | 10-22 复核旧 8 倍异常未复现；普通等待 1.25 倍为合并行为 | 保持现状，系统更新后重新标定 |
| A6 | NVRAM 写入/早期启动可观测性 | `DisableVariableWrite`；早期 panic 无报告、无 unified log | 变量持久化不可靠；早期故障只能靠屏幕/复位猜 |
| A7 | 启动链/内核补丁维护（AMD_Vanilla、ProvideCurrentCpuInfo、CPUID、KC patches） | 版本绑定；每次 macOS 更新需重推 | 更新即可能失效；多会话并行改 EFI 风险高 |
| A8 | 安全/平台完整性（SIP/AMFI/FileVault/Apple Pay/DRM） | SIP 自定义；Kext Signing/FS Protections/NVRAM Protections 关闭；FileVault Off | 安全姿态与原生 Mac 不同；Apple Pay/T2 相关不可用 |

### B. I/O 与总线层

| ID | 工作项 | 现状 | 影响 |
| --- | --- | --- | --- |
| B1 | USB 15 端口上限 / locationID / port map | **S2 + MT7922 remote-wake 禁用 runtime PASS** | 控制器恢复风暴的触发路径已关闭 |
| B2 | PCIe 枚举 / ReBAR / MMIO / IOMMU | Above 4G + ReBAR 已开；IOMMU Enabled；SR-IOV Disabled | 影响 GPU BAR、直通、AppleVTD 前提；SR-IOV 不可用 |

### C. 设备驱动层

| ID | 工作项 | 现状 | 影响 |
| --- | --- | --- | --- |
| C1 | MT7922 WiFi（AirportMtk） | **关联/4-way/data PASS**；扫描 UI 0、RLM/AWDL 未完成 | 可用但吞吐仍低；airportd 缓存问题独立 |
| C2 | MT7922 蓝牙（SimpleMtkBtFw/BlueToolFixup） | **remote-wake 禁用已生效并 runtime PASS** | 蓝牙保持在线，remote wake 已关闭 |
| C3 | RTX 5060（NVRM/NVMTL/NVAccel/NVHDA） | 2.5.4 安装与重启通过；旧版原生硬编硬解已有实际出帧 | 色彩、低延迟和应用入口待新版复核；Chrome 闪烁转日常观察 |
| C4 | Mellanox CX3（Cx3Mlx4） | **0.44.37 已加载并 probe PASS**；call/phase 取证链已自测 | 等待用户暂停工作后做下一次受控复现 |
| C5 | 音频（AppleALC/NMHDA/CoreAudio 调度） | 用户确认断音已解决 | 只做更新后短回归 |
| C6 | RTL8125 有线 | 工作正常 | 只做回归 |
| C7 | NVMe/TRIM | 工作正常 | 只做回归 |
| C8 | Ryzen 核显 | macOS 无 Raphael 核显驱动；IORegistry 无 1002 设备 | 不可用；**后续再做（Deferred）**，当前不进入日常波次 |

### D. 依赖功能（不单独立项，作为验收场景）

| ID | 功能 | 依赖 |
| --- | --- | --- |
| D1 | 本地 VM / Docker / UTM / Lima | A1 |
| D2 | PCI 直通 / SR-IOV | A2 + B2 + BIOS；SR-IOV 当前 BIOS=Disabled，只能先做传统 PCI 直通评估 |
| D3 | AirDrop / Handoff / Continuity / Universal Control / Sidecar / 连续互通相机 | C1 + C2 + C3（HEVC 编码） |
| D4 | 硬件视频编解码 | C3 |
| D5 | 睡眠/唤醒 | A3 + A6 + B1 |
| D6 | iServices / Apple Pay / DRM | A8 + SMBIOS；Apple Pay 需 T2/Secure Enclave，基本不可达 |

### 2.1 明确不做 / 后续再做

- Apple Pay / Secure Enclave / T2 相关：硬件不存在，不作为可实现目标。
- Thunderbolt / USB4、SATA：本机没有对应硬件。
- 把修改过的 Apple 二进制分发或公开；AppleVTD 路线只做本机自用。
- 通用化到其他 AMD 平台；本计划只针对这台 Ryzen 7 9700X 机器。

**后续再做（Deferred）：**

- **Ryzen 核显（C8）**：macOS 没有 Raphael 后端；后续评估自研/移植驱动、显示输出、硬件编解码和功耗收益，先做 Go/No-Go，不进入当前波次。

---

## 3. 工作流与会话分配

> 新会话名是**建议名**，尚未创建。已有会话优先继续，避免重复立项。

| ID | 工作流 | 类型 | 优先级 | Owner session | 依赖 | 当前状态 |
| --- | --- | --- | --- | --- | --- | --- |
| P0 | 程序协调与基线 | 协调 | P0 | 本协调会话（用户本轮指定） | — | 只维护规划、分派、依赖、收件与集成决策，不代替各域实现 |
| A7/B2 | boot 配置、备份、补丁注册表、更新回归 | 维护 | P0 | 发布执行owner由主会话按窗口指定 | — | M0历史交付保持；本轮没有共享EFI/KC发布窗口，主会话不亲自实施 |
| B1 | USB 15 端口上限 / port map | 连接短验通过，音频仍FAIL | P1 | `01a123d5…`（已有） | A7 | 26上no-HS11六口保护生效，蓝牙临时隔离；推进总线证据与原生map离线候选 |
| C1 | MT7922 WiFi / AirportMtk | 实现 | P1 | `01a1671c…` | — | 26上尚无接口；RESERVED槽位已解释，继续用户态/V2兼容路线，保留HE/TX/扫描/AWDL成果 |
| C3 | RTX 5060 驱动 + HDMI 音频 + VideoToolbox | 修复与验收 | P1 | 安装`01a11ccc…`、视频`01a16744…`、音频`01a14a75…` | A7 | 26上2.5.4显示/硬编解码短验通过；BT709/低延迟FAIL，26回退候选未激活，板载/GPU音频未恢复 |
| C3显示器 | Redmi G Pro 27U外接显示器控制 | 恢复开发 | P2 | `01a14aaa…` | C3安装/打包 | 保留桥接App/CLI；26回归及原生亮度离线候选，与NVIDIA共享接口统一集成 |
| C5 | 音频断音 / CoreAudio 调度 | 旧15结案；26回归FAIL | P1（26回归） | 音频`01a14a75…`牵头，USB`01a123d5…`协作 | A5、B1 | 26用户破音反馈邻域有Music超时；从日志自主诊断，不依赖持续人工报点，不新建重复会话 |
| A5/A6 | 定时器/TSC + NVRAM + 早期启动日志 | 平台 | P2，音频相关证据随P1 | 平台研究队列；当前时序诊断由音频owner牵头 | A7 | 26仍有等待尾延迟，未见固定8倍异常；旧24.6补丁不外推；NVRAM保持，早期取证待解决 |
| A3/A4 | CPU 电源管理 + SMC 传感器/风扇 | 研究 | P2 | `amd-power-smc` | A7 | 基线/可行性/离线温度解码完成；真实温度与 macOS 读取层待验 |
| C2 | MT7922 蓝牙 | 阻塞 | P2 | Wi-Fi/BT `01a1671c…` | B1 | 26上端口被有意隔离，Off为预期；旧15基础播放/重连PASS不外推26，完整睡眠待验 |
| A2 | AppleVTD / AMD-Vi | 研究 | P2 | `01a13455…`（已有） | B2、BIOS | 路线 1b，稳定透传档 |
| C4 | CX3 驱动 / attachInterface 冻屏 | 实现 | P1并行开发 | `01a15c2a…`（已有） | A7 | 26实际0.44.47只读加载；开发树已前进至独立候选，继续MTU/DMA/生命周期最小批及host环境修复，不启用实机网络 |
| A1 | AppleHV / AMD-V 后端 | 研究 | P3 | **`amd-hv-cpu`（新）** | A7 | 未开始；工作量大 |
| A8 | 安全姿态 / SIP / FileVault / Apple Pay / DRM | 已评估 | P3 | `amd-security-posture` | A7 | 评估已交付，保持现状；收紧为可选独立任务 |
| C8 | Ryzen 核显 | Deferred | — | **`amd-igpu`（未来，未创建）** | — | macOS 无驱动；后续做 Go/No-Go，不进入 Wave 1–4 |

**并发上限建议：** 同一时间最多 3–4 个活跃实现会话；其中最多 1 个持有 EFI 写权限（`amd-boot-config`）。其余会话只产出候选补丁/诊断，不直接写 ESP。

**工作量量级（相对）：** S = 几天；M = 1–2 周；L = 3–6 周；XL = 2 个月以上。Wave 1 的 USB/NVIDIA/audio 为 M–L，WiFi 为 XL；Wave 2 的 timing 为 M、power/SMC 为 L、BT 为 S–M；Wave 3 的 AppleVTD/CX3 为 L–XL，AppleHV 为 XL+；security 为 S；iGPU 为 Deferred，不计入当前估算。

### 3.1 隔离/协作模型

**角色：**

| 角色 | 谁 | 职责 | 不做什么 |
| --- | --- | --- | --- |
| 程序协调 | 本会话 | 维护本计划、优先级、依赖图、跨流决策；审阅证据；在波次边界做集成判断 | 不写驱动、不改 EFI、不替 owner 做实现 |
| 工作流 owner | 每个工作流一个会话 | 拥有该流的仓库/目录、基线、实现、证据、handoff | 不碰其他 owner 的目录；不直接写 ESP |
| EFI 单点写者 | `amd-boot-config` | 唯一允许写 ESP 的会话；应用候选、维护补丁注册表、冻结窗口、恢复演练 | 不实现设备驱动 |
| 集成/验收 | 程序协调 + 对应 owner | 波次边界验收、回归、更新兼容性 | 不在未验收时宣布“完成” |

**隔离边界：**

- **代码隔离**：`AirportMtk`、`usb-port-limit-macos`、`nvidia-macos-driver`、`tools/cx3-mlx4` 等各自归 owner；跨目录只读。
- **产物隔离**：每个流只写自己的 `outputs/<date>-<workstream>/`；共享结论写 `docs/`，但先经协调会话确认。
- **EFI 隔离**：除 `amd-boot-config` 外，任何会话不得直接写 ESP；候选以“补丁 + 目标文件 + 哈希 + apply/rollback + 测试计划”交付。
- **实验隔离**：一次重启只改一个变量；需要重启的流先在协调会话登记冻结窗口，避免和 WiFi/NVIDIA/USB 的改动叠加。
- **上下文隔离**：每个 owner 只读本计划 §5 的对应 brief + 自己的 handoff；不继承其他流的现场假设。

**协作契约：**

- **单一事实源**：本文件负责 scope/优先级/依赖；owner 的 handoff 负责该流的最新状态。
- **状态格式**：`状态 / 本次证据 / 阻塞 / 下一步 / 需要协调`，每次阶段结论必须带可复现命令或哈希。
- **候选格式**：候选补丁必须带目标文件、应用前后哈希、命中位置/冲突数、回滚命令、测试计划。
- **依赖协议**：owner 声明 blocking edges；协调会话更新波次 frontier；blocker 未完成不启动下游 ticket。
- **跨流请求**：需要别的流配合时，发消息给协调会话，由它排冻结窗口/优先级，不直接改别人的目录或 EFI。
- **升级路径**：启动失败/panic/EFI 损坏 → 立即通知 `amd-boot-config` + 协调会话，先回滚再分析；不在坏状态上继续叠加变量。

### 3.2 派发批次

| 批次 | 会话 | 启动条件 | 备注 |
| --- | --- | --- | --- |
| Batch 0 | `amd-boot-config` | 已完成 | 备份、离线恢复、R1 写回+重启、R6 BIOS 目视均通过；R2 无害结案、R10 可逆迁移已执行 |
| Batch 1 | `amd-usb-ports`（已有 `01a123d5…`） | Batch 0 完成 | S2 已部署；重启后做 7 口/蓝牙 + 探针观察，不做人工压测 |
| Batch 1 | `amd-wifi-mt7922`（已有 `01a123d6…`） | Batch 0 条件通过（只读/离线可开工） | 数据面/HE/RA；AWDL 后置 |
| Batch 1 | `amd-nvidia-5060`（umbrella） | Batch 0 条件通过（只读/离线可开工） | 复用已有安装/HDMI/PR 子线；VideoToolbox 新子线 |
| Batch 2 | `amd-audio`（新） | USB + timing 基线有结论 | 断音归因；不先于 USB/timer |
| Batch 2 | `amd-timing-nvram`（新） | Batch 1 有稳定组合 | timer 独立 A/B + NVRAM/早期日志 |
| Batch 2 | `amd-power-smc`（新） | Batch 0 完成 | 先测量，再决定是否调频 |
| Batch 2 | `amd-bt-mt7922`（新） | USB Port 16 可枚举 | 否则只做离线固件/协议准备 |
| Batch 3 | `amd-vtd-iommu`（已有 `01a13455…`） | Batch 0 完成 | 已有流继续；不新建 |
| Batch 3 | `amd-cx3-mlx4`（新/续） | Batch 0 完成 | 冻屏复现优先 |
| Batch 3 | `amd-hv-cpu`（新） | G6 确认有 VM/Docker 需求 | 先做 Go/No-Go |
| Batch 4 | `amd-security-posture`（可选） | M4 前 | 非 AMD 专属 |
| 后续 | `amd-igpu`（未来） | Wave 4 后或你点名 | 只做 Go/No-Go，不进入日常波次 |

**派发规则：** 每个批次最多 3–4 个活跃 owner；Batch 0 必须先于任何会改 EFI 的批次。派发时由协调会话给每个 owner 一份 session brief（§5 对应小节 + handoff 路径 + 验收 + 回滚），owner 不得自行扩大 scope。

---

## 4. 波次与依赖

```text
Wave 0  基线
  amd-boot-config ──> 冻结仓库/ESP/运行态，备份，补丁注册表，恢复演练

Wave 1  日常可用性
  amd-usb-ports ──> amd-bt-mt7922（Port 16 解锁后）
  amd-wifi-mt7922 ───> D3（AWDL/AirDrop，数据面达标后）
  amd-nvidia-5060 ──> D4（VideoToolbox）

Wave 2  平台稳定性
  amd-timing-nvram ──> amd-audio
  amd-power-smc
  amd-bt-mt7922（依赖 Wave 1 的 USB）

Wave 3  平台能力 / 研究
  amd-vtd-iommu ──> D2（PCI 直通/SR-IOV）
  amd-cx3-mlx4
  amd-hv-cpu ──> D1（VM/Docker）

Wave 4  集成验收
  D3 / D4 / D5 / D6
  安全姿态与 macOS 更新回归
```

**启动规则：**

1. Wave 0 未完成前，不启动任何会改 boot 配置的实现会话。
2. Wave 1 先做“对日常影响最大、且不依赖其他研究”的三条：USB、WiFi、NVIDIA。
3. `amd-audio` 在 `amd-usb-ports` 和 `amd-timing-nvram` 都有稳定基线后再开始，否则无法归因。
4. `amd-bt-mt7922` 必须等 USB Port 16 能被枚举；否则只能做离线固件/协议准备。
5. `amd-hv-cpu` 放到最后：先确认是否真有 VM/Docker 需求；没有就只保留研究文档，不投入实现。

---

## 5. 每个工作流的交付与验收

### 5.1 `amd-boot-config`（P0）

**Scope:** ESP/config.plist/Kexts/内核补丁/备份/恢复演练/更新回归/BIOS 关键项只读核对与变更申请；不实现设备驱动。

**Deliverables:**
- 一份“实际启动源”证据：磁盘/GPT/卷/`EFI/OC` 路径，不依赖卷名。
- 全量 EFI 备份 + 逐文件 SHA256 + 卷外副本。
- 补丁注册表：每条 Kernel/Booter patch 的用途、版本限制、命中位置、回滚值。
- 冻结窗口协议：谁在何时可以写 ESP，其他会话如何提交候选。
- macOS 小版本更新检查清单（timer/USB/AMD_Vanilla/AMFIPass）。
- BIOS 关键项快照：SVM、IOMMU、SR-IOV、Above 4G、ReBAR、XHCI Hand-off；与仓库 10-08 快照做差异表。

**Acceptance:**
- 从备份恢复一次，机器可正常启动（恢复演练）。
- 仓库快照、ESP 文件、运行态 `nvram boot-args`/`kmutil showloaded` 三者有明确差异表。
- 任何候选补丁都有“应用前/后哈希 + 反汇编命中 + 冲突数”记录。
- IOMMU 口径冲突（10-08 快照 Disabled vs AppleVTD 会话 Enabled）已解释并以实际 BIOS 为准。

**当前状态（2026-10-20）：** 备份 771/771、离线恢复 436/436 通过；真机恢复待批；M0 条件通过。详见 §6.2。

### 5.2 `amd-usb-ports`（B1，研究→实现，已有 `01a123d5…`）

**Scope:** 研究并实现“超过 macOS 每控制器 15 口限制”的最优解；不预设“全 17 口”或“稳定 15 口”。范围包括 AppleUSBHostPort/AppleUSBXHCI 的检查、locationID 4-bit 编码、port-count、控制器基址、ACPI port 来源、UTBMap 重排、控制器拆分/重映射，以及与音频/USB 时序的交互；不改蓝牙协议栈。

**当前基线（2026-10-20）：** XHC0 硬件 17 口，但 2 条 USB 15→17 补丁 `Enabled=false`，实际只建 15 口，HS11/HS12 不存在。研究从“15 口可用”开始，不要假设 17 口已开。

**研究结论（2026-10-20）：** XH00 的 17 个对象里只有 **7 个接到实际连接器**（硬件 `{2,3,7,8,15,16,17}`）。推荐 **S（稀疏 map）**：`port-count` 仍写 17，map 只声明这 7 个，`XhciPortLimit=false`，两条自定义补丁继续关闭；预期 7 个对象、逻辑位置 1..7、零内核补丁、零位置溢出。P（创建 17 个对象）只作为冻结窗口里的受控实验，原因是 16/17 会越过 4-bit 端口域，且 16 号下挂 Hub 会与根 17 冲突。

**S 部署状态（2026-10-21）：** S1 `83f47223…` 是 binary plist，OpenCore prelink 只认 XML，日志报 `Invalid Parameter`，已回滚 baseline。S2 改为 XML `998c7fbb…`（与 10-17 已实测候选逐字节一致），06:43 写回；07:55 重启后 OpenCore 日志为 `Prelinked injection UTBMap.kext ... - Success` / `UTBMap.kext v1.1`。首次启动时 XH00 7 个对象、位置 1..7，其他控制器 4/4/1，蓝牙 On，短时验收通过。

**S2 回归（2026-10-21 10:10 boot）：** CX3 `netupstop(2)` 冻屏并重启后，XH00/HS11（硬件 port 16，逻辑 `0x00600000`）从 10:10:52 起持续出现 `createDevice(0xe00002bc)`、命令超时、`0x00000040` 和控制器 recovery；截至 10:22 已计数 160 次创建设备失败/160 次 recovery。Bluetooth 当时为 `State: Off / Address: NULL`，USB 树无 `Wireless_Device`；同控制器 HS10 上的 MCHOSE 键盘与 HS12 上的麦克风在 recovery 时一起被断电并短暂移出 HID Event System。

**冷启动复验（2026-10-21 10:25:54 boot）：** 用户关机并拔掉交流电 30 秒，BIOS 设置未改。上电后 Bluetooth 恢复 `On`、Address `02:1A:7D:DA:71:2A`、Firmware `v9735 c9236`，`Wireless_Device@0x00600000` 与 MCHOSE HID service 在线。到 10:33:33（跨过上一轮持续故障起点），HS11 create failure 0、controller recovery 0、MCHOSE terminate 0。判定为“冷启动后临时 PASS”；普通重启与睡眠/唤醒仍未验收。证据在 `outputs/20261021-freeze-keyboard/post-cold-boot-20261021-102554/`。

**Remote-wake 修复（2026-10-21 10:50 部署，11:23 验收）：** 根据 Linux `btusb` 对 Ryzen + MT7922 的 quirk，新增 codeless `MT7922USBWakePolicy.kext`，精确匹配 `locationID=0x00600000` 并设置 `kUSBRemoteWakeOverride=false`。重启后 MT7922 节点实测 `No`，Bluetooth On、MCHOSE 在线；到 boot 后 29 分钟，HS11 create failure、XH00 recovery、MCHOSE terminate 均为 0。证据：`outputs/20261021-mt7922-wake-policy/VERIFY-PASS-20261021-112305.txt`。

**研究顺序（先研究，后实现）：**

1. 复盘现有补丁与冲突模拟器，确认 15 口限制到底卡在哪几层。
2. 枚举候选路径，逐条做离线冲突模拟、稳定性/可维护性/成本评分。
3. 选出最优解与第二选择；先做非关键端口、只读或可秒回滚的验证。
4. 只有评分、回滚和探针观察都通过，才进入 EFI 写入。

**Deliverables:**
- 机制报告：15 口限制到底卡在哪几层（数量检查、locationID 宽度、端口号分配、hub 半字节、控制器基址），每层给出符号/偏移/证据。
- 候选路径清单，至少覆盖：
  - 放宽 `AppleUSBHostPort::setPortLocation` / `AppleUSBXHCI::createPorts` 的检查与掩码；
  - 压缩/重排 UTBMap，把 16/17 映射到逻辑 ≤15 的端口号；
  - 改 ACPI `_UPC`/port 号，从源头避免 >15；
  - 拆分/重映射控制器基址（XHC0/XHC1/XHC2 的 locationID 划分）；
  - 社区已有方案（corpnewt/USBMap issue #63 等）的适用性复核。
- 评分矩阵与推荐：稳定性、无 locationID 冲突、音频/USB 时序、macOS 更新可维护性、实现成本；给出最优解和第二选择。
- 每版 macOS 可复用的补丁校验器/冲突模拟器（输入候选补丁，输出命中位置/冲突数）。
- 选定方案的实现与探针观察/重启记录（不做人工 100 次插拔/30 分钟音频压测）。

**Acceptance:**
- 至少 2 条可行路径的对比和评分，明确“最优解”及理由；不以“全 17”或“15 口”作为预设答案。
- 选定方案下：XHC0 目标端口全部创建且无 locationID 冲突；或明确记录为何某条路径不可行。
- 探针在正常使用窗口内未发现与旧 USB 补丁相同的症状：`hardwareException`、`CommandRing::executeCommand`、`createDevice: failed`、`powerOn: deadline`、HS11/port16 错误或控制器恢复风暴；不做人工 100 次插拔/30 分钟音频压测。
- 不引入新的 panic/复位；回滚只需恢复 UTBMap/config 的单个文件。

### 5.3 `amd-wifi-mt7922`（C1，已有 `01a123d6…`）

**Scope:** AirportMtk 数据面（TXWI、STA/BSS records、RX reorder）、HE/RA、运行时开关；AWDL 单独排期。

**当前状态（2026-10-21 09:18）：** AKM/dv 一致性修复已生效（build `d4c5c5a5…`）：`en1` active `192.0.2.130`，system_profiler Connected / Channel 36 160MHz；Eapol2 最后为 msg4 `dv=2/sec=1/kdlen=0`；`AirportMtkRsn akms=0x2`；`RxBa agreed=2`、`RxAgg ampdu=515`；ping 网关 6.3ms，短吞吐 75/37 Mbps。`networksetup -getairportnetwork` 仍报未关联、`cwscan` 仍 0，是 airportd 空缓存/UI 报告问题，数据面已可用。`BssRlm sent=0` 因开关默认 off；RLM/RA/AWDL 后续再评估。

**线程状态（2026-10-21 09:20）：** owner 线程 `01a123d6…` 的**用户 UI turn 仍可执行**；失败的是 `send_message_to_thread` 的**自动投递 turn**。投递内容会留在信箱里，用户随后发“继续”可把它带进正常 turn 执行。**该 WiFi 线由用户自己继续旧线程，协调会话不再投递/接管**；需要程序化连续驱动时才新建会话。

**历史根因（2026-10-21 09:07）：** `AirportMtkRsn` 曾为 `akms=0x8 supp_akms=0xa`，msg2 却写 RSN IE **AKM 2**、发 `dv=3`；AP 按 AKM 2 期望 `dv=2`，静默丢弃。修复后 `akms=0x2`、msg4 `dv=2`，4-way 已通过。

**Deliverables:**
- 以 Linux 7.2.11 为蓝本的差异表和逐项移植记录。
- 吞吐/延迟/丢包基线：Linux 对照 vs macOS 当前 vs 目标。
- HE 开机与运行时开关的稳定性矩阵。

**Acceptance:**
- 上行/下行达到 Linux 对照的 90%（当前记录：Linux 上行 842 Mbps；macOS 下行 113 / 上行 33）。
- 连续 60 分钟 iperf/大文件传输无断流、无 panic。
- AWDL/AirDrop 作为独立里程碑，不在数据面达标前承诺。

### 5.4 `amd-nvidia-5060`（C3 umbrella）

**Scope:** NVRM/NVMTL/NVAccel/NVHDA 的安装、Metal/显示、HDMI/DP 音频、VideoToolbox、上游 PR；不改 macOS 系统二进制。

**Sub-tracks / existing sessions:**
- 安装/打包：`01a11ccc…`
- HDMI/DP 音频：`01a14a75…`
- Chrome/上游 PR：`01a14a4b…`
- VideoToolbox：新子线（可由 umbrella 会话直接做）

**Acceptance:**
- `h264_videotoolbox`/`hevc_videotoolbox` 在 `-allow_sw 0` 下编码成功；`videotoolbox_vld` 解码初始化成功。
- 4K@160 Hz、Metal 3、HDMI 音频、睡眠/唤醒回归通过。
- 驱动版本/补丁可回滚；`config.plist` 哈希不变或有明确差异。

### 5.5 `amd-audio`（C5）

**Sequoia旧问题已结案；2026-10-25 Tahoe回归重新开放。** 用户先前确认15上的断音已解决；26升级后USB偶发破音由既有音频owner牵头、USB协作，按顶部最新验收状态推进。以下为旧范围历史，不新开重复会话、不要求持续人工报点或长压测。

**Scope:** CoreAudio 调度、AppleALC/NMHDA、USB 端口补丁与 timer 的交互；不改 WiFi/GPU 驱动。

**当前状态（2026-10-21）：** 用户在 timer + USB S2 组合下做了短音频检查，**没有断音**；按 `amd-audio` 回归 PASS 处理。不做强制 60 分钟/30 分钟压测，保留日常观察和短回归。

**Deliverables:**
- 断音复现脚本与最小判据（512/4096 帧、60 分钟）。
- timer 补丁、USB 补丁、NVIDIA HDMI 音频三者的 A/B 矩阵。
- 可日常使用的稳定组合（不是“自建播放器能跑”）。

**Acceptance:**
- 日常短检/正常使用无可听断音；观察窗口内 HALS 无 `kIOReturnIsoTooOld`（不做强制长压测）。
- 512 帧缓冲下 `mach_wait_until` 中位 < 1.1× 请求值。
- 回滚组合有记录，且不依赖重新编译。

### 5.6 `amd-timing-nvram`（A5/A6）

**2026-10-25 增量：** 定时器复核见 `outputs/20261022-amd-platform/A5-TIMER-RECHECK.md`；普通等待约 1.25 倍来自 timer coalescing，busy-spin 1.000 倍、实时线程 1.001–1.004 倍。不将该现象列为缺陷，不为此安排旧 timer-only A/B。当前剩余项为早期内核故障持久取证；DisableVariableWrite 保持 true。下方 10-21 的待办是历史记录。

**Scope:** Zen5 TSC/FID/FSB/LAPIC 标定、timer 补丁、NVRAM 写入语义、早期启动 panic 可观测性。

**当前状态（2026-10-21 09:35）：** timer 补丁已命中（`_tsc_init` 的 `busFCvtn2t` = 25 MHz 换算常量）；100/500/1000µs 三轮中位 1.027/1.007/1.004×，历史 7.933/7.982/7.987× 未复现。当前是复合启动（timer + USB S2 + NVIDIA），只能算组合 PASS；timer-only A/B 仍缺。NVRAM 继续 `DisableVariableWrite=true`，14 变量/795B 稳定；早期 panic 落盘通道仍未解决。未写 EFI/未重启。

**Deliverables:**
- 本机 timer 标定的可复现测量（不只看绝对值，记录倍率/上界）。
- timer 补丁的独立 A/B 验收（排除 USB/音频同时变化）。
- NVRAM 写入风险矩阵；panic 日志落盘方案或“明确不做”的结论。

**Acceptance:**
- 100/500/1000 µs 等待中位与上界都有记录；补丁前后可对照。
- 早期启动故障至少有一个可落盘的证据通道（或明确记录不可达原因）。
- 所有补丁带 Darwin 版本限制和回滚说明。

### 5.7 `amd-power-smc`（A3/A4）

**2026-10-25 增量：** 温度解码离线原型已通过 8 项回归；root SMC 采样无有效传感器值；SuperIO 识别被端口占用保护跳过。现有 MTKCAP 自带匹配内核的 k10temp，可在 STA 验收后单独验证真实 Tctl；macOS 硬件读取层与 kext 仍未实现。阶段设计见 `outputs/20261025-amd-sensors/README.md`，不以合成温度作为本机读数。

**Scope:** CPU P-state/C-state/CPPC、SMC 温度/风扇/电压键；不碰 GPU 电源。

**当前状态（2026-10-21 09:45）：** `machdep.xcpm.mode=0`、vectors/ratio changes 均为 0；`hw.cpufrequency*` 都是 5.14GHz 静态占位值；65 个 SMC 键里真实温度/风扇/电压/功耗/频率传感器为 0（`FNum=0`）。没有 SMCAMDProcessor/SMCProcessor/SMCSuperIO。原版 SMCAMDProcessor 对 Zen5（Family 1Ah）No-Go；自研只读 SMC 插件 Conditional Go（温度原型约 3–5 天，完整监控 1–2 周）；维持现状 + BIOS Smart Fan 是当前最低风险 Go。

**Deliverables:**
- 现状测量：空闲/负载频率、功耗、温度、风扇（能从 BIOS/外部工具拿到的先记录）。
- 可行方案评估：SMCAMDProcessor/自定义 SMC kext/接受固定 P-state。
- 若实现：传感器与调频的最小可用集 + 回滚。

**Acceptance:**
- 至少能看到 CPU 温度/频率；或明确记录“当前平台不可达”的证据。
- 空闲功耗/温度有前后对比；不引入新的 panic/睡眠问题。
- 若把睡眠/唤醒纳入本轮，需有 10 次循环记录；否则明确写“不在本轮范围”。

### 5.8 `amd-bt-mt7922`（C2）

**Scope:** SimpleMtkBtFw/BlueToolFixup/SimpleMtkBtPatcher；依赖 USB Port 16 可枚举。

**Deliverables:**
- 控制器枚举证据（`ioreg`/`system_profiler` 的 Address/Firmware/State）。
- 固件装载与 HCI 初始化日志。
- 与 USB 端口补丁的依赖矩阵；失败时回滚到“无蓝牙”稳定态。

**Acceptance:**
- 蓝牙控制器 Address 非 NULL、State On、可配对键鼠/耳机。
- 重启/睡眠后仍可用；不影响 WiFi 与 USB。

**当前状态（2026-10-21 09:30，runtime PASS / 本轮范围）：** 本次启动 09:16:51；
XH00 的 HS11（硬件 16）在 `locationID=0x00600000` 枚举，控制器 `State: On`、
Address `02:1A:7D:DA:71:2A`。AirPods Pro 已连接，HFP/AVRCP/A2DP/AACP/GATT/ACL
服务可用，并同时成为默认输入和默认输出。本地 Aiff 的播放/暂停/恢复状态为
`true → false → true`；09:30:07 主动断开、09:30:34 重连后 A2DP 再次流式工作。
该窗口无 panic、无蓝牙控制器 reset，USB 四个硬失败签名计数均为 0。
重启/睡眠持久性仍待后续验收。证据见 `docs/handoff-bt-mt7922.md` 与
`outputs/20261021-bt-mt7922/RUNTIME-ACCEPTANCE.md`。

### 5.9 `amd-vtd-iommu`（A2，已有 `01a13455…`）

**Scope:** AppleVTD 上层 + AMD-Vi 后端；不碰 CPU 虚拟化。

**Deliverables:**
- 路线 1b 的阶段性证据：DTE/页表/失效/故障可见性。
- 真实消费者清单；没有消费者时明确“研究性”定位。
- 安全回滚：U 盘启动、ESP 零改动、boot-args 分级。

**Acceptance:**
- AppleVTD 实例存在且 `gSystem` 生效，设备 DMA 正常，无 panic。
- 或明确判定“当前不可行/无消费者”，保留证据并停止投入。

### 5.10 `amd-cx3-mlx4`（C4）

**Scope:** CX3 驱动、attachInterface 冻屏、mailbox/DMA/网络路径；不改其他驱动。

**复现结果（2026-10-21 10:06）：** 用户放行后，最小 `netupstop(2)` 复现整机冻屏。`cx3freeze` 最后一条驱动日志为 10:06:53.366 的 `queryPort` 成功与 MAC 暂存；`cx3watch` 收到 `pre selector 46 netupstop 2`，没有对应 post，也没有后续 `step/hook`。此前 reset、queryfw、fwload、querydevcap、m2dprofile、m2dbringup、queryPort 均成功。结论：卡点落在 selector 46 的 attach 入口附近，mailbox/ICM 不是本轮阻塞点。

**下一次前置条件：** 必须先恢复 USB 干净基线（冷启动后确认 Bluetooth On、`Wireless_Device` 存在、HS11 错误为 0，MCHOSE HID 不间歇消失）。没有新的代码路径或更细 breadcrumb 时，不再运行 `netupstop(2)`。

**0.44.37 取证增强（2026-10-21 11:25）：** USB 基线已连续为 0 错误。上一轮 `cx3freeze` 没有任何 `enter selector 46` 或 netup marker，因此新增 `cx3ClientSelector` / `cx3ClientPhase`，由 100 ms watcher 同时推送 `call / phase / step / hook`。阶段可区分 sweep、transaction lock 等待/获得、dispatch 进入/返回和释放。已构建 PASS、安装到 `/Library/Extensions/Cx3Mlx4.kext`，binary `952e9515…85cf5`，`kmutil load rc=28` 等待重启；0.44.36 完整 bundle 已备份。

**重启后自测（2026-10-21 11:29）：** `0.44.37` 已加载，只读 `cx3ctl probe` PASS；registry 写出 `cx3ClientSelector=0`、`cx3ClientPhase=8`。同时发现旧 watcher 的 `ioreg -n … -d1 -w0` 在 24H6 返回 0 字节并把缺失值伪装成 0；已修为 `ioreg -r -n … -d1 -l -w0`，并增加 `read=ok/read=EMPTY`，commit `e546439`。修正后 watcher 实测 `read=ok call=0 phase=8 step=0 hook=0`，USB 基线仍全绿。

**第二次最小复现（2026-10-21 11:31）：** USB 基线全绿后执行最小 `netupstop(2)`。prefix 全部成功；11:32:07 发出 `pre netupstop 2 attach` / `pre selector 46 netupstop 2`，60 侧 11:32:08.922 记录 Mac DEAD。60 最后 watcher 样本为 11:32:04 `read=ok call=24 phase=8 step=0 hook=0`，这是旧 shell watcher 落后约 3 秒；`cx3freeze` 最后驱动行仍是 selector 24 后 `user client closed`，未出现 `enter selector 46`。selector 46 入口后的 phase/hook 仍未知。

**0.44.38 审查候选（2026-10-21 11:39）：** 已构建但**未安装**：`outputs/20261021-cx3-attach-context/build/Cx3Mlx4.kext`，binary `efeae121…`。方案为把 `attachInterface` 移到独立 `thread_call` worker，user-client 有界等待 10 秒，`stop()` 增加 `thread_call_cancel_wait/free`。新增常驻 10 ms IOKit sampler：`tools/cx3-mlx4/tools/cx3-phase-watch.c`、`netup-watch.sh`；构建使用 `/usr/bin/clang -isysroot "$(xcrun --sdk macosx --show-sdk-path)"`。均未提交，已交 C4 owner 独立审查。

**Deliverables:**
- 冻屏最小复现与调用栈/日志。
- 修复或规避方案；1 小时压力测试。

**Acceptance:**
- 1 小时 mailbox/网络压力无冻屏；重启/睡眠后仍可用。
- 回滚到“不加载 CX3”的稳定态只需移除 kext/恢复 config。

### 5.11 `amd-hv-cpu`（A1）

**Scope:** AppleHV 的 AMD-V 后端可行性；先做决策，不默认写代码。

**Deliverables:**
- 消费者清单（Docker/UTM/Lima/自研 VM）与收益评估。
- XNU `hv_support`/VMX 依赖清单；AMD-V 后端工作量拆解。
- Go/No-Go 建议：研究文档或正式立项。

**Acceptance:**
- 明确回答“值不值得做”；若 Go，给出分阶段原型（hv_vm_create → 最小 vCPU → Linux guest）。
- 若 No-Go，记录原因和替代方案（外部 Proxmox/双系统）。

### 5.12 `amd-security-posture`（A8，可选）

**Scope:** SIP/AMFI/FileVault/Apple Pay/DRM 的现状与风险；非 AMD 专属。

**Deliverables:**
- 安全姿态清单与风险分级。
- FileVault 可行性/风险；Apple Pay/T2 的不可达结论。

**Acceptance:**
- 不改变当前可用性的前提下，给出“保持现状/收紧/放弃”的建议。

### 5.13 `amd-igpu`（C8，Deferred，后续再做）

**Scope:** Ryzen Raphael 核显（1002:13C0）在 macOS 下的驱动可行性；不进入当前 Wave 1–4。

**Deliverables:**
- 现状证据：IORegistry 无 `pci1002` 设备；macOS 无现成 Raphael 驱动。
- 可行路径评估：自研 kext / 移植 Linux amdgpu / 仅做显示输出 / 仅做硬件编解码，分别列出工作量与收益。
- 收益清单：显示输出、硬件 H.264/HEVC、功耗、与 RTX 5060 的分工。
- Go/No-Go 文档；若 Go，再单独立项并给分阶段原型。

**Acceptance:**
- 形成一份有证据的 Go/No-Go 结论；不把“没有现成驱动”直接等同于“永远不做”。
- 若 Go，给出最小原型范围（枚举/寄存器访问/显示或编解码之一）；若 No-Go，记录原因。

**Trigger:** Wave 4 完成后，或你明确点名“现在做核显”。在此之前不创建会话、不投入实现。

---

## 6. 决策门（需要你拍板）

| ID | 问题 | 选项 | 建议 | 触发时机 |
| --- | --- | --- | --- | --- |
| G0 | 项目目标优先“日常可用”还是“平台完整性/研究”？ | 日常优先 / 研究优先 / 双线 | **已拍板（2026-10-19）：日常优先；研究线保留但不阻塞** | 已确认 |
| G1 | USB 如何突破 15 口限制、什么是最优解？ | S 稀疏 map / P 17 对象 / 稳定 15 | **研究完成：S 稀疏 map 最优；P 17 对象仅受控实验；建议 S** | Wave 1（`amd-usb-ports`） |
| G2 | WiFi 继续重写 AirportMtk，还是换 Broadcom/Intel 卡？ | 继续重写 / 换卡 / 双轨 | 先继续重写；若 4 周内数据面无 2× 提升，再评估换卡 | Wave 1 结束 |
| G3 | 蓝牙修 Port 16，还是用 USB 蓝牙 dongle？ | 修端口 / dongle / 暂不做 | 先修端口；dongle 作为低成本兜底 | B1 有结论后 |
| G4 | VideoToolbox 等上游，还是自己补？ | 等上游 / 自己实现 / 接受软件编解码 | 先确认上游路线；无路线再评估自研 | Wave 1 |
| G5 | AppleVTD 继续投入，还是停在研究档？ | 继续 / 停 / 找消费者 | 没有真实消费者就停在研究档 | A2 下一里程碑 |
| G6 | AppleHV 是否立项？ | 立项 / 仅研究 / 放弃 | 先确认 VM/Docker 真实需求；无需求仅研究 | Wave 3 |
| G7 | CPU 电源管理投入，还是接受固定 P-state？ | 投入 / 接受 / 只做传感器 | 先做传感器与测量，再决定是否调频 | Wave 2 |
| G8 | CX3 继续修，还是换网卡？ | 继续 / 换卡 / 停 | 先拿 1 小时复现；无稳定路径再换 | Wave 3 |
| G9 | iGPU（C8）是否立项？ | Go / No-Go / 继续观察 | 先做可行性与收益评估，不预设 | Wave 4 后或你点名 |
| G10 | USB 最终交付选 S 还是 P？ | S 稀疏 7 / P 17 对象 / 双轨 | **默认 S**；P 只在冻结窗口做单变量实验 | 生成候选前 |

### 6.1 已拍板（2026-10-19）

| 决策 | 结果 | 影响 |
| --- | --- | --- |
| G0 | 日常优先；研究线保留但不阻塞 | Wave 1 先做 USB/WiFi/NVIDIA/音频；AppleVTD/AppleHV/CX3 放到后续波次 |
| G1 | 研究完成：S（稀疏 map，7 个真实连接器）最优；P（17 对象）只做受控实验 | USB 会话基于 10-20 基线生成 S 候选；P 不进入默认交付 |

**G1 最优解判定标准（按优先级）：** 稳定性 > 无 locationID 冲突 > 音频/USB 时序不受影响 > macOS 更新可维护性 > 实现成本。任何方案必须通过探针观察（旧 USB 补丁症状为 0）；不做人工 100 次插拔/30 分钟音频压测。

**G1 研究结果（2026-10-20）：** XH00 的 17 个对象只有 **7 个接了实际连接器**（硬件 `{2,3,7,8,15,16,17}`）。S 方案用这 7 个、`port-count=17`、补丁全关，逻辑位置压在 1..7；P 方案创建 17 对象，但 16/17 越过 4-bit 端口域，16 号下挂 Hub 会与根 17 冲突，且每版 macOS 要重推字节。默认交付取 S。

### 6.2 Batch 0 结论（2026-10-20）

| 项 | 结果 | 对计划的影响 |
| --- | --- | --- |
| 实际启动源 | 内置 NVMe `disk0` 的 ESP `disk0s1`；OpenCore `DBG-107-2026-10-14`；`config.plist` 56,063 B | 以后以这份 ESP 为准，不再用仓库 `efi/` 旧快照 |
| 备份 | 771 文件；`MANIFEST.sha256` 771/771 OK；Kexts 64/64 OK；整分区 UDRO 镜像 | 已具备离线回滚材料 |
| 离线恢复 | 临时 FAT32 镜像写入预演 436/436 OK；R1 已把 baseline EFI 写回实时 ESP，436/436 校验通过、0 删除 | 写回链路可信；2026-10-21 重启验收通过 |
| 三位一体 | 仓库 `efi/`（10-07）≠ 实际 ESP（10-20）≠ 运行态（有 5 个 AuxKC 驱动不在 ESP） | 仓库快照不能覆盖 ESP；运行态需单独记录 |
| USB | S2 稀疏 map 生效：XH00 7 个真实对象（位置 1..7），其他控制器 4/4/1；蓝牙 On | 内核补丁仍关闭；S 方案已通过启动/拓扑验收 |
| IOMMU | `1022:14d9` 在 `00:00.2` 正常枚举；BIOS 目视 **Enabled**（2026-10-21） | A2/B2 可按 Enabled 准备；AppleVTD 前提满足 |
| BIOS | IOMMU Enabled；SR-IOV Disabled；XHCI Hand-off 无此选项；Secure Boot/CSM/Fast Boot Disabled | R6 已关闭；SR-IOV 关闭影响 D2 的 VF 路线 |
| 补丁 | Kernel/Patch 30 条：27 条基础 + 2 条 USB（关）+ 1 条 timer（开，仅 Darwin 24.6.0） | USB owner 注意补丁是关的；更新前先校验 |
| 原风险的最新处置 | R2 schema 差异已判无害；R10 旧日志 323 份已迁到卷外 | **M0 通过；R2 结案、R10 已执行**，迁移后 ESP 占用 41% 为 10-23 时点记录 |

**M0 状态：通过。** 备份、离线恢复、R1 写回、重启验收和 R6 BIOS 目视均通过；R2 已判无害结案、R10 已可逆迁移，均不再阻塞后续。EFI 写实验仍须遵守“一次一变量 + 先备份”。

### 6.3 R1 重启验收（2026-10-21）

| 项 | 结果 |
| --- | --- |
| 启动时间 | `kern.boottime` = 2026-10-21 03:55:25 CST；最新日志 `opencore-2026-10-20-195500.txt`（19:55:00 UTC） |
| 启动源 | 内置 NVMe `disk0s1`；日志 `boot-path = .../NVMe(...)/HD(1,GPT,6D351E57-...)/EFI\OC\OpenCore.efi` |
| 配置 | 56,063 B；OpenCore `DBG-107-2026-10-14`；加载配置字节数与日志一致 |
| EFI 校验 | 436/436 与 baseline 0 差异；config/OpenCore/BOOT 哈希全部一致 |
| 运行态 | USB 15 个 USB20 + 9 个 USB30；NVIDIA 2.4.3、CX3 0.44.36 已加载；无 panic 报告 |
| 备注 | 03:54:27 有一条 `shutdown_stall`，属于重启前关闭阶段记录，不是本次启动 panic |

### 6.4 R6 BIOS 目视（2026-10-21）

| 项 | 当前值 | 对计划的影响 |
| --- | --- | --- |
| IOMMU | **Enabled** | AppleVTD/AMD-Vi 的前提满足；保持开启 |
| SR-IOV | **Disabled** | D2 的 SR-IOV VF 路线不可用；只能先评估传统 PCI 直通 |
| XHCI Hand-off | **BIOS 没有该选项** | 不适用；USB 映射按当前控制器行为继续 |
| Secure Boot | **Disabled** | 保持关闭，符合当前 OpenCore/黑苹果配置 |
| CSM | **Disabled** | 纯 UEFI，符合要求 |
| Fast Boot | **Disabled** | 保持关闭，便于故障排查 |

---

## 7. 跨工作流规则

1. **单一 EFI 写者**：只有 `amd-boot-config` 能写 ESP；其他会话产出候选文件/补丁，由它统一应用。
   Wave 0 完成后，已有会话的 ESP 直写权限收回；Wave 0 之前 USB/WiFi 等会话的临时直写必须登记并纳入基线。
2. **一次只改一个变量**：每次重启只允许一个 boot 配置变量变化；记录 `config.plist`/Kexts 的 SHA256。
3. **先备份再动手**：全量 EFI 备份 + 卷外副本 + 恢复演练；U 盘/Windows 恢复入口必须可用。
4. **证据格式**：命令、时间、输出、哈希、before/after；结论写进 `outputs/<date>-<workstream>/`，长期结论进 `docs/`。
5. **不碰别人的目录**：`AirportMtk`、`usb-port-limit-macos`、`nvidia-macos-driver`、`tools/cx3-mlx4` 各自归 owner；跨目录只读。
6. **不重复立项**：已有会话是 owner；新会话启动前先读本文件 §1.1 和对应 handoff。
7. **更新回归**：macOS 小版本更新后，先跑 `amd-boot-config` 的补丁校验器；timer 补丁只允许 Darwin 24.6.0，USB 补丁按 KC 重推。
8. **不分发修改过的 Apple 二进制**：本机自用；上游 PR 只提交自己写的代码。

---

## 8. 里程碑与验收

| 里程碑 | 内容 | 验收 |
| --- | --- | --- |
| M0 基线 | 冻结仓库/ESP/运行态，备份，离线恢复，补丁注册表 | 已通过；R2 无害结案、R10 可逆迁移已执行 |
| M1 日常可用 | USB、WiFi、BT、NVIDIA、音频均有稳定组合 | USB 基础修复、音频、NVIDIA 2.5.4 启动已完成相应阶段；仍待 Wi-Fi 数据面/扫描和蓝牙/睡眠综合验收；视频功能分项复核 |
| M2 平台稳定 | timer/NVRAM、CPU 电源/SMC | timer 复核完成、NVRAM 策略保持；温度/频率与早期故障持久取证仍未完成 |
| M3 平台能力 | AppleVTD、CX3、AppleHV | AppleVTD 有消费者或明确停；CX3 1 小时稳定；AppleHV Go/No-Go |
| M4 集成验收 | AirDrop/Handoff/Sidecar、VM、直通、安全姿态 | 依赖功能逐项通过或明确不可达 |
| M5 后续评估 | iGPU（C8） | 有证据的 Go/No-Go；Go 才单独立项 |

---

## 9. 风险与回滚

| 风险 | 影响 | 缓解 |
| --- | --- | --- |
| 多会话同时改 EFI | 故障不可归因、启动失败 | 单一写者 + 冻结窗口 + 一次一变量 |
| 修改 Apple 二进制/签名 | 启动链/更新失败 | 仅本机、保留原版、AMFIPass/备份、恢复演练 |
| USB 补丁引发音频/USB 时序故障 | 断音、设备掉线 | 先建立音频基线；补丁 A/B；保留 15 口稳定档 |
| timer 补丁只对一个 Darwin 版本有效 | 更新后失效/panic | 版本限制 + 更新后先校验 |
| WiFi 重写范围大 | 长期无稳定结果 | 以 Linux 7.2.11 为蓝本、分 TX/RX 两条、先仪器化 |
| NVIDIA 驱动是第三方 | 更新/兼容性风险 | 固定版本、自动回退、PR 上游 |
| 硬件极限（MT7922 AWDL） | 功能不可达 | 决策门提前判定，不无限投入 |
| iGPU 驱动工作量/收益不确定 | 长期投入无回报 | 先做 Go/No-Go，不进入日常波次 |

---

## 10. 下一步（2026-10-25）

1. 重启前：完成 CPU 传感器离线阶段，固定温度地址/位域/错误语义；本轮已完成，见 `outputs/20261025-amd-sensors/README.md`。原版 SMCAMDProcessor 仍不可直接安装。
2. 下次计划内 MTKCAP 启动：先验证已部署的 STA reader 修正版，要求 `seen=emitted=records>0`、无丢失/坏帧、STA/TLV 覆盖满足 Wi-Fi owner 需求，以及上传 ACK。旧采集的 16 条 WTBL 不算完整 STA。
3. STA 验收后：单独登记 Linux k10temp 的 Tctl 对照窗口；当前包尚未接入该模块加载。先取得真实温度，再做 macOS SMN/VirtualSMC 传输层；频率、功耗、调频和风扇控制分阶段后置。
4. Wi-Fi、CX3 继续由各自现有 owner 执行；共享 USB 由协调会话统一维护。CX3 在硬件验证前对齐观察器目标地址，并以 owner 最新当轮结果为准。
5. 后续短睡眠/唤醒验收：联查蓝牙、USB、显示与网络；基础连接 PASS 不等于睡眠持久性 PASS。不恢复取消过的长压测。
6. NVIDIA 先复核 2.5.4 上的色彩、低延迟和应用视频入口；Chrome 闪烁按用户反馈转正常使用观察，复发再定位。
7. M0、R2、R10、音频断音、timer 复核与安全评估不重复立项。OpenCore 升级、安全策略收紧为有实际需求才启动的独立任务。
8. AppleVTD/AppleHV 为长期研究线；核显继续 Deferred。所有 EFI/驱动写入仍遵守单一写者、备份、哈希、单变量与回滚；本次继续指令未触发自动重启或新硬件实验。

---

## 11. Session brief 模板（给后续会话）

```text
你是 <工作流名> 的 owner。工作目录：/Users/you/Documents/Hackintosh-MacLocal。

先读：
- docs/plans/2026-10-19-amd-adaptation-program.md 的 §5.<n>
- <该流已有的 handoff/文档/仓库路径>

目标：<一句话，可验收>
非目标：<明确不做什么>
起点：<已有产物/基线/会话 ID>
约束：一次一变量；改 EFI 前先备份；不碰其他会话目录；证据落盘。
第一 actions：只读检查 → 建立基线 → 给出候选方案 → 等我确认后再改。
交付：<文件/补丁/测试记录/结论>
```

