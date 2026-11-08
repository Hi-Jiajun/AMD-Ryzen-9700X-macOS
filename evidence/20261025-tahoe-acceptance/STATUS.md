# macOS 26 升级后验收状态

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

**用户明确“继续”后的恢复：** 仅恢复USB音频焦点：音频owner关联既存TRB/Start-Stop/deadline原文，USB owner定向核对匹配版本TRB错配判定及字段语义。两任务各自所有权独立，不重复采样，不启动System Trace/录音，不改探针或驱动；其余六域保持暂停。原定期收件仍删除，本轮人工收件推进。关联和语义结果已于10-26收件，见顶部；仅最小下一步候选待交付。

**22:59收件与定时任务收尾：** 音频仍停止、USB闲置，两个cursor及报告哈希均无变化；没有在途工作或已授权采集窗口。为避免继续消耗受限额度，已通过工具删除`hackintosh`每15分钟定期收件。诊断并未完成，证据和暂停断点全部保留，未恢复任何owner或采样；后续须有明确恢复工作指令再安排收件。

**20:44收件修正：** 音频的`REBOOT-TRB-AUDIO-ALIGN`回合中断，最新终态仅确认“已停止、未录音/未System Trace、产物保留”，没有新的关联报告。该任务保持未完成，不将消息投递当作关联验收，不自动重派已停止工作；既有TRB/停流因果仍未知。USB会话与产物无变化，其余六域继续暂停。

**本次启动收件：20:32:19，boot `54893A74-67BA-40BF-B871-AAD88C349078`。** 两个owner已交重启报告：USB见`usb/reboot-203219/REBOOT-USB-PROBE-REPORT.md`，音频见`audio/reboot-focus-20261025-203600/REBOOT-AUDIO-FOCUS.md`。60独立broad/debug记录本boot G2000等时OUT0x02在20:37:37.178/44.192共5条mismatched event TRB，20:37:54.976 abortGated撤销8请求；尚不能区分停流清理与原始完成异常，更不能直接当听感破音次数。音频归档有短时系统/Codex IO（Start37.100、Stop37.753）、4条deadline/3条overload及22.607ms内部调度记录；未见Music直接事件，听感NOT_ASSESSED。已派音频只用两份现有归档做精确身份/时间线关联，不再采样或试录。

60 recorder/alert active，但固定查`.196/.130`，Mac同MAC已从`.101`发beacon；state的UNREACHABLE不能当机器离线，USB观察器仍绑定10-24旧boot，不能据零USB_ERRORS判PASS。独立日志链在20:28:38至20:32:41空窗吻合U盘/Linux阶段，非故障。未修改探针服务/配置。MTKCAP `run-20261025-122955`采集完整、DHCP/上传成功，G2000在08:00.0下1-2/12Mbps；另一路1-11描述符超时不可归给G2000。macOS本次端口表仍6/4/4/1、无硬件16/HS11，G2000控制器8:0:0、硬件port7/location0x00300000；映射通过不等于传输通过，磁盘EFI开关未重新验证。其余六域仍暂停。

**用户最新报告：已再次重启回macOS，期间完成一次U盘启动。** 已安排USB owner只读收取已挂载`/Volumes/MTKCAP`的新采集及本次macOS USB基线，音频owner按新boot重建输出/HAL和当前日志证据。新boot身份与候选是否生效均待owner核实，下方18:07:41及旧session/TID仅为历史，不作为当前事实。Linux探针与macOS证据分开；U盘启动不等于USB音频PASS。不恢复其余六域，无追踪录制/发布/重启窗口。任务回执`reboot-usb-audio-dispatch.json`仅证明投递。

**19:49焦点收件：** 音频与USB最小任务均已终态，未新增采样或派发。USB `AUDIO-KEXT-CAUSAL-BRIEF.md`：已审查USBToolBox1.2.0固定源码没有逐笔音频请求/completion/时钟hook；UTBMap为codeless，属性/ACPI/启动时序的间接影响仍未排除，运行二进制与审查提交的一致性也未证明，不能据此判USB正常。音频 `XCTRACE-MINIMAL-PREFLIGHT.md`：本机文档只支持新输出和2秒限时，当前tailspind仲裁及正常/异常恢复契约仍未证，结论NOT_READY，独立调度NOT_CAPTURED。记录一次标准2秒追踪的具体候选与停止/状态比对条件，尚无明确执行窗口，不自动试录、不重复前置调研。USB短报告登记待下次有效音频任务一并转交，避免只为确认而唤醒owner。其余六域保持暂停，未开放部署/重启。

**当前优先级（2026-10-25 19:35起，用户覆盖此前并行安排）：** 为保留GPT五小时额度，只推进USB音频偶发破音。Wi-Fi/BT、CX3、NVIDIA视频、NVIDIA安装、RTL8125及显示器六域均已确认暂停，保留源码/候选/证据；不得由旧任务或自动化自行恢复。CX3暂停后仅写PAUSE.md，回合已结束为idle，WAVE4尚未构建，未发现CX3构建进程。旧CX3/NVIDIA持续取证日志流保留，不当作开发任务，不擅自终止。

音频owner只核对最多2秒标准System Trace的资源仲裁/恢复前置，尚不录制；USB owner只复用源码审查给现用USBToolBox/UTBMap因果边界及一个最小判别条件，原生map完善暂停。两域均不再开子代理，不以DeepSeek自动续跑来规避GPT限额。现有heartbeat已缩至两域，独立NVIDIA监控已暂停。未改音频参数、引导、驱动或KC，未开放重启/采集窗口。收件与回执见focus-*.json；暂停投递与实际确认分别登记在state。

2026-10-25；现场为 macOS 26.7.1 / 25G241 / Darwin 25.6.0。最新boot为18:07:41，boot UUID DF172B9E-6067-45BA-B3BE-042EE27BD3A2。no-HS11六口保护表已生效；USB枚举短窗口通过，但用户确认USB音乐仍偶发破音，不能判音频功能通过。后续以各域RESULT/FOLLOWUP的证据与时间为准。

## 协作与变更规则

七个现有会话均已成功接收任务并开工；每份 tasks/*.md 都明确：当前可用全模型均可协作，包含 GPT 系列、deepseek-flash，不沿用旧额度限制。dispatch.json 保存派发回执与任务书 SHA256。

各会话可并行只读检查、归属源码修复、离线构建；本协调会话统一维护公共规划及共享ESP/USB。安装卸载、KC重建、EFI/USB写入、重启睡眠及风险硬件操作须串行安排。17:54仅由协调者发布USB端口表；音频集合仍为临时离线构建，未发布live KC，未自动重启。

## 跨会话主动收件（18:54）

用户已同意恢复Redmi G Pro 27U子线，现为八个owner。主会话继续唯一统筹，未另建重复管理会话；已建立本会话每15分钟收件的heartbeat，只有实质进展/失败/阻塞/冲突/待决策才通知，原NVIDIA健康监控保持独立。`coordination-state.json`保存owner、cursor、已收件产物与下一依赖，避免重复派发。工具回执确认自动化`hackintosh`为ACTIVE。

19:09触发、19:13收件更新（以下为owner交付，未由主会话重复领域测试）：

- 音频WAVE3取得83组控制器USB帧号/host time，约1秒未见明显倍率或倒退；非已知破音同期，设备sample/seed、completion和独立runnable→dispatch仍缺，根因未定。已与USB双向转交证据，安排音频先离线核对不接管tailspind资源的追踪消费方案，无窗口不改变系统追踪配置。
- USB原生map0.3.0已生成，四控制器被动匹配、离线检查通过；merge旧键及启动时序仍未证明，保持NOT_DEPLOYABLE。已派owner继续离线审查及准备单变量启动判据，未发布。
- Wi-Fi WAVE3完成原生对象大小及部分签名迁移，raw import缺口33降至4（2个RESERVED、2个旧link-state签名）；Controller/接口46/186槽位不匹配及纯虚回调签名仍阻塞。不是可部署或联网候选；owner仍执行中，不重复催促。
- CX3 WAVE3 .139完整构建及定向简验通过，VLAN/MAC/checksum属DEV_DONE/SMOKE_PASS；实际loaded仍.47，硬件数据面及完整验收未通过。owner终验收尾中，不追加重叠任务。
- 显示器交付用户态修复及615.78.08隔离231对象库；原生亮度仍未实现，无可部署驱动。已把DDC寻址勘误转安装owner，要求准确配对vendor/headers/构建与补丁清单；显示器继续离线异步状态机/mock验证，并复用CX3的CLT21 sanitizer路径作有界对照。
- 安装owner已纠正boot归属，82文件26恢复候选及dry-run仍未激活，lastgood仍为15包。视频实现库源码依赖、RTL已通过基线均无新变化，保持不改。

本轮四项增量任务仅投递成功，待owner结果；回执见`heartbeat-1909-receipts.json`。`coordination-state.json`已按文件保存WAVE产物哈希及收件版本，避免WAVE3反复被判新增。共享发布窗口仍关闭。

19:24触发、19:26增量收件：八域cursor已更新，首次WAVE哈希扫描无变化；仅补读有新交付的音频与安装侧产物。音频`SYSTEM-TRACE-READINESS.md`确认限定目录未找到可消费的既存trace，tailspin默认禁用、kdebug无buffer；独立调度证据仍NOT_CAPTURED，新2秒采集因资源仲裁/恢复未验证而NOT_READY，未执行。安装侧`VENDOR-61578-AUDIT.md`确认准确配对的Darwin615 vendor archive、完整headers/编译链接命令/补丁顺序缺失；DDC协议契约已纠正，`NvKmsKapiDisplay`为dpy ID，须经既有device内KAPI/ioctl锁路径，不能强cast。已转显示器当前执行轮消费，clean615继续unpaired experimental，不能更新产品驱动或恢复候选。显示器主机侧状态机、IOI2C结构适配与codec sanitizer报告通过，仍在执行，未计内核或屏幕实机PASS。USB仍在审查启动merge，不催促。Wi-Fi/CX3已收到WAVE3终态，分别派发下一批虚表迁移与链路/media/down-up/电源离线切片；视频/RTL无变化不派重复任务。回执`heartbeat-1924-receipts.json`只证明两项开发任务及一次接口转交投递成功。无发布、采样、安装或重启。

八个会话快照已核对：六个继续具体任务，视频接口补充与RTL交付完毕，不为保持活跃而重复测试。共享发布窗口关闭；没有由本次收件触发安装、EFI/KC写入、网络切换或重启。

## 首轮与第二轮状态入口

第二轮并行已派给七个现有owner，7/7消息投递成功，详见`tasks/coordination-wave2.md`及`wave2-dispatch.json`；投递成功不等于实现或验收完成。音频owner牵头USB破音自主诊断，USB owner提供总线证据及离线原生map；Wi-Fi、CX3、NVIDIA视频、26恢复方案同步开发；RTL owner提供默认出口兼容门槛。主会话从此只做协调、收件和集成决策，不再代替各域执行诊断或修改实现。共享系统发布窗口尚未开放；CPU传感器/早期取证仍P2排队，不自动启动AppleHV/核显/安全收紧。

| 域 | 当前结论 | 下一步 | 现有会话 |
| --- | --- | --- | --- |
| USB / 蓝牙 USB 前置 | PASS_SHORT_WINDOW仅限连接：六口/HS12位置正确，18:10:58–18:11:29各端口address/enum/overcurrent计数0、选定Hub/U盘/麦克风session不变；音频仍FAIL | 核对USB等时传输/时钟与共享kext路径。临时隔离不恢复蓝牙；不把连接稳定外推音频正常 | 01a123d5-8642-7810-9a09-8d4f19d4f12a |
| MT7922 Wi-Fi / BT 协议栈 | 两版构建完成；V1两个RESERVED引用已由真实父类槽位及OpenCore链接流程解释，不能仅凭nm缺失判ABI失败；Fw/Patcher/BlueTool版本范围已精确核对 | 旧用户态方案与Skywalk/en0条件仍阻塞部署，V2原生路径仍有缺口；不直接放宽门控 | 01a1671c-cac5-72e2-808f-fb5abcd74269 |
| NVIDIA 安装/显示 | 当前运行PASS：四个2.5.4、payload/AuxKC一致、Metal3、4K160；26恢复候选82文件清单复核通过，与当前NVAccel/AuxKC一致 | 候选未激活、未验证回退启动；现有自动回退仍含macOS15包 | 01a11ccc-49a1-7ff2-8ba9-3698f271045f |
| NVIDIA 视频 | 强制硬编硬解短帧PASS；BT.709元数据和低延迟FAIL。新boot原/收紧探针共24播放项均出帧，旧切换异常未复现 | 保留首轮失败；无红绿差异不能确定根因或宣布修复；无可部署视频库候选 | 01a16744-5dd8-7082-880d-b2b636b4fbe7 |
| USB / 板载 / HDMI / DP 音频 | 功能FAIL：USB HECATE偶发破音，跨XHC2→1022:43fc控制器换口后仍有Music实时周期异常；板载/GPU仍无输出。3.0.3临时AuxKC仅PASS_LINK_ONLY，未部署 | 先将破音时间与音频/USB日志对齐，保留USB驱动和调度/时钟嫌疑；板载候选部署暂缓，不改缓冲/采样率 | 01a14a75-0606-7a40-8574-5b52fa2a26bc |
| CX3 | 只读加载/PCI PASS；.137 Tahoe离线构建及普通host简验PASS；sanitizer在main前SIGILL，为环境阻塞；实机网络未验 | 继续原功能队列；不因只读link0判固件坏，不自行初始化硬件 | 01a15c2a-ed65-7f52-a04a-d2b103a4de9b |
| RTL8125 | 短时验收PASS：Lucy1.1.100、2.5G、默认en0、无本次启动panic/mbuf异常；10次网关ping全通过 | 保留当前组合；长稳/吞吐/睡眠未验。Skywalk候选变更后须复验 | 01a129b1-58b1-7312-9079-a3bab0656044 |

七域RESULT.md均已落盘；USB另见boot174059/REPORT.md，Wi-Fi、视频和音频另见各自FOLLOWUP.md，音频临时链接结果为auxkc-preflight/20261025-175300-595260/RESULT.json。音频构建时32个已覆盖输入保持不变，但当时ESP卸载窗口内config未被其脚本采集；config未变结论来自协调者独立读回，不冒充音频构建期间连续采集。安装报告的macOS/Darwin术语已更正。

17:54部署与探针改动的完整证据：../20261025-tahoe-followup-1740/。本地与NAS各444/444备份一致，写后核对444原文件仅UTBMap变化，config哈希保持89067da0…，目标map为114c35b4…，ESP已只读。新增卷上任务备份不参与加载。挂载点重建及diskutil卸载回退过程已如实记录。

只读探针已部署schema2：硬件port/控制器路径/entry ID/boot UUID、统计值与delta、跨boot重置基线、采集失败UNKNOWN。17:50:49实际捕获HS11 address/enum各+39；不依赖限流后的空日志，未再将采集错误写成零故障。

## 协调侧实测

- 内置ESP只读挂载；EFI备份444/444逐文件一致。config SHA256：89067da0ad75b1924af2e45e9df2e46d75528e687ebf5e916c4ae8e488261468。
- 当前启动日志对照当前config：16条适用补丁Success且实际替换大于0，10条版本跳过；旧Darwin24.6 timer补丁正确跳过。
- 18:07启动后定时器实时线程1000µs中位1.456倍，未达旧参考门槛；USB风暴已停止，不能全部归因USB。另一个有界同CPU对照中，busy-spin约1.000倍、实时wait中位1.030倍且有尾部延迟；未见内部固定8倍换算异常，但不是独立硬件绝对时钟校准。保持未收敛，不改旧补丁门控。

## 18:07重启与USB音频回归

证据目录：`../20261025-usb-reboot-180741/`。map哈希114c35b4…已读回，config仍89067da0…；ESP只读。16条适用补丁Success且替换>0，10条正常版本跳过。Samson恢复枚举，BT预期Off。

用户确认偶发破音，继续自然播放。18:11音箱在GP19/XHC2/HS01（0x02100000）；18:20:44快照已在GPP7/1022:43fc/HS02（0x00300000），确实跨控制器，均为Full Speed，后续快照session保持一致。HAL只读结果48kHz/512帧、safety/latency各50帧；20秒property通知为0，但同期日志有异常，因此该通知不能作为破音排除判据。以新控制器快照为下界，18:20:45–18:21:43日志有92条deadline-prior-start、83条IO Overload，38行提及Music，其中30行明确当前应用是Music，其余仅出现在其他客户端字段，不能把全部overload归给HECATE；日志行不是独立破音次数。18:18:50的startIO属于Samson，不能作为音箱换口分界；早先18:19统计不用于换口归属。尚无用户破音时间标记，物理听感相关性待验。USB日志calcError字段不直接等于传输失败。未改任何音频参数、kext或KC；两个owner已收到新任务。

## 执行顺序

18:26:12.973收到用户“刚破音”标记，已捕获18:24:42–18:26:13历史短窗。18:26:11.491 Music出现SafetyViolationOccurred：调度延迟13.732ms、预算11.354ms、回调0.0727ms；HECATE session/entry与18:20相同，USB探针至18:26:02无连接计数异常，同窗未见明确USB传输失败签名。首次取得听感反馈邻域内的实时异常，仍受人为反应/投递延迟限制，不能据此定位根因。证据：`../20261025-usb-reboot-180741/crackle-182612/RESULT.md`。音频与USB owner已同步；等价原生map仅离线准备，不部署。

下一步先收敛当前自然播放的USB破音：听感时间→Music/HECATE时序→USB等时/共享驱动路径。已跨控制器换口，不重复相同对照。取得证据后再选择单变量可恢复实验；当前不再要求重启，不部署板载音频、不改USB保护表。恢复候选激活、BT/Wi-Fi门控及完整睡眠/长稳分别后置。
