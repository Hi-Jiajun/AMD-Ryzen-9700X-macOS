# metal2vulkan 0.2.0 spike —— 原始证据

结论与判读写在 `docs/upstream-watch.md` §22.7 / §22.8。这里只放**可复核的原始产物**。

日期：2026-10-12 02:0x（本机 macOS 15.8.1 / iMacPro1,1 / RTX 5060）
上游提交：`steelbrain/metal2vulkan` `13b279df1bf87893d389f955a90c55b6ff3d0f9b`
输入：驱动的格式转换内核 —— 从 `plugin/nvconv_lib.h` 还原的 `nvconv.metallib`（55,322 B，见 `input/`）

## 目录

| 目录/文件 | 是什么 |
| --- | --- |
| `input/nvconv.metallib` | 驱动插件内嵌的那份 metallib（`nvconv3/4/5_b/_s` + `nvconv_cmp` 共 6 个内核） |
| `driver-0.1.0/*.spv` | 本机**装着的驱动翻译器**（`/Library/GPUBundles/NVMTLDriver.bundle/Contents/MacOS/libnvmtl_translate.dylib`）经 `nvmtl_translate(ll,"kernel",…)` 产出的 SPIR-V |
| `new-0.2.0/*.spv` | 0.2.0 CLI（`cargo build -p metal2vulkan --features cli`）产物 |
| `new-0.2.0/*.json` | 0.2.0 的 `--emit-meta`（接口 / layout / 依赖 / 阶段） |
| `sources/probe.c` | 调驱动 dylib 的探针（dlopen + `nvmtl_translate_set_caps(3)` + `nvmtl_translate`） |
| `sources/nvkfeat*` | 查真实设备特性的小 Rust 探针（`DeviceDescription::query` 直查 NVK ICD） |

## 复现命令

```sh
# 1) 还原 metallib（头文件里是 C 数组，长度声明 55322）
sed -n '8,4618p' plugin/nvconv_lib.h | tr -d ' \n\t' | sed 's/0x//g; s/,//g' | xxd -r -p > nvconv.metallib

# 2) 切 AIR blob（用 0.1.0 仓库的脚本）
python3 scripts/mtlb-extract/mtlb_extract.py nvconv.metallib out/air      # -> 7 个 blob

# 3) AIR → LLVM 文本（= 驱动 nvmtl_air_to_text 那一步）
/Library/GPUBundles/nvmtl/air-opt -S blob.air -o blob.ll

# 4) 驱动侧（本机装着的翻译器）
./probe /Library/GPUBundles/NVMTLDriver.bundle/Contents/MacOS/libnvmtl_translate.dylib blob.ll kernel out.spv

# 5) 0.2.0 侧
metal2vulkan blob.air out.spv --profile <…>/profiles/nvidia-rtx50-linux.json --function nvconv3_b

# 6) 两边独立复验（注意 0.2.0 需要设备特性）
spirv-val --target-env vulkan1.2 out_driver.spv
spirv-val --target-env vulkan1.2 --uniform-buffer-standard-layout out_new.spv
```

## 结果

| blob | 函数 | 驱动 0.1.0 | 0.2.0 | 驱动 Op | 0.2.0 Op |
| --- | --- | --- | --- | --- | --- |
| blob_00 | nvconv3_b | 14,484 B | 19,668 B | 271 | 361 |
| blob_01 | nvconv3_s | 14,016 B | 18,696 B | 245 | 312 |
| blob_02 | nvconv4_b | 14,776 B | 21,252 B | 278 | 378 |
| blob_03 | nvconv4_s | 14,308 B | 20,296 B | 252 | 329 |
| blob_04 | nvconv5_b | 14,972 B | 22,192 B | 284 | 389 |
| blob_05 | nvconv5_s | 14,516 B | 21,236 B | 258 | 340 |
| blob_06 | nvconv_cmp | 2,408 B | 4,468 B | 64 | 61 |

双方 7/7 都翻译成功；0.2.0 产出在 `uniformBufferStandardLayout` 下 `spirv-val` 全过，
而 NVK（本机真实设备）报该特性为 true ⇒ 合法用法。

**未做**：真机语义等价对拍（两边的 SPIR-V 只在 NVK 上跑同一 fixture 才能判等价，本文不声称等价）。
