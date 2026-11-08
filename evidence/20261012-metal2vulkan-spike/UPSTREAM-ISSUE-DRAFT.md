# Draft issue for `nullmoth/nvidia-macos-driver`

Status: **drafted, not posted.** The repo's issue channel is open (112 open issues), and
translator work does get filed there (e.g. #112). Note: `nullmoth` has left no comments in the
last 100 issue comments, so a reply is not guaranteed.

Suggested title:

> Translator: metal2vulkan published a full rewrite (0.2.0) — is a rebase planned, and to which revision?

---

## Body

`translator/` vendors a heavily modified copy of `steelbrain/metal2vulkan`. Upstream has just published
a complete rewrite, and I measured what that means for this fork. Two questions at the end.

### What upstream did

`steelbrain/metal2vulkan@13b279df1bf87893d389f955a90c55b6ff3d0f9b` (2026-10-11) squashes 11203 private
commits into one public commit. Its `CHANGELOG.md` says of 0.2.0: *"A full rewrite: nothing of 0.1.0's
API or command line remains."* The tree becomes a 5-crate workspace (`metal2vulkan`,
`metal2vulkan-air`, plus the new `metal2vulkan-runtime` — "the Metal API on Vulkan"), and 0.2.0 is not
on crates.io (crates.io still carries the unrelated 0.1.0), so consumers must pin a git revision.
Licence changes from `LGPL-3.0-or-later` to dual `GPL-2.0-or-later OR LGPL-3.0-or-later`.

### What this fork currently is (measured)

Comparing `translator/translator/src/` at v1.13.0 against upstream `src/` at `43c46ac8` (the 2026-09-06
public state) and at the 0.1.0 tag:

| | files | byte-identical to upstream |
|---|---|---|
| upstream 0.1.0 tag | 234 | — |
| upstream `43c46ac8` | 261 | — |
| this fork (v1.13.0) | 299 | 6 (vs tag) / 11 (vs `43c46ac8`) |

Every one of upstream's 0.1.0 `src/` files is still present (no deletions), and **41 files are added**
— roughly 309 KB of driver-specific lowering:

```
mesh_lower 81K  ray_query 56K  b78_tcg 42K  ift_runtime 35K  byte_words 25K
ldhoist 19K  latch_trampoline 14K  widen_shl 12K  sample_positions 6K
atomicrmw_text 5K  cl_constant 5K
```

Shared files are rewritten, not carried: `passes/mod.rs` 63,657 → 106,924 B and
`reflect/mod.rs` 29,063 → 126,123 B (4.3x). `NOTICE` already records this as a modification by
NullMoth Systems, so this is by design — I mention it only to size the work.

### Pointing the wrapper at 0.2.0 (measured)

Took `translator/wrapper` at v1.13.0 unchanged and re-pointed its `metal2vulkan` dependency from the
vendored 0.1.0 to `13b279df`:

```
cargo check  ->  33 errors: could not compile `nvmtl_translate` (lib)
```

All of them are removed API, in whole modules: `metal2vulkan::passes` (`Stage`,
`TransformOptions`), `metal2vulkan::reflect` (`reflect_sanitized`, `ResourceKind`,
`RuntimeSamplerState`, `SYNTHETIC_BINDING_BASE`), `metal2vulkan::mesh_lower` (`lower_mesh`), and
`translate_native_no_retry{,_with_options,_constructed_with_options}` — plus
`specialize_air_function_constants`, `specialize_runtime_table_module`, `specialize_linked_module`,
`linked_functions::*`, `canonicalize_spirv_bytes`. Two structural changes matter more than the count:

- **Input format.** `nvmtl_translate` takes LLVM text today; 0.2.0's `Library::parse` /
  `from_air` / `from_metallib` take `&[u8]` and its docs state *"LLVM text is not an input"*.
- **Device description.** 0.2.0 compiles against a `DeviceDescription` (Khronos profile JSON, or
  `ash` + `DeviceDescription::query`), which the translator has no notion of today.

### What 0.2.0 *can* do with this driver's shaders (measured, so the port is not speculative)

I ran the driver's own embedded metallib (`plugin/nvconv_lib.h`, 55,322 bytes — the
`nvconv3/4/5_b/_s` + `nvconv_cmp` format-conversion kernels) through both translators. Extracting the
7 AIR blobs with `scripts/mtlb-extract/mtlb_extract.py` from the 0.1.0 tree, then:

- 0.2.0's CLI compiles 6/6 of the kernels (`--profile nvidia-rtx50-linux.json`), self-check
  `spirv-val vulkan1.2, vulkan1.4: PASS` each;
- the installed `libnvmtl_translate.dylib` (0.1.0 lineage) translates 7/7 blobs via
  `nvmtl_translate(ll, "kernel", …)`;
- 0.2.0 translates the same 7 AIR blobs, `spirv-val` PASS.

One migration detail fell out of this and is worth recording: 0.2.0's output for these kernels
requires `uniformBufferStandardLayout` (it places the buffer-address table in `Uniform` with stride 4:
`OpTypeArray %uint %uint_60` / `%uint_36` / `%uint_20`), where 0.1.0's output does not. Under
`--uniform-buffer-standard-layout` all 7 validate. **NVK reports that feature as true** here
(`DeviceDescription::query` against the driver's own ICD: `NVIDIA GeForce RTX 5060 (NVK GB206-A)`,
api 1.4.363, vendor `0x10de`, device `0x2d05`, 127 features incl. `UniformBufferStandardLayout`,
`ScalarBlockLayout`, `TaskShader`, `MeshShader`, `RayQuery`) — so 0.2.0 is not asking for anything
this back end lacks.

### The blocker that makes it a port rather than a bump

The two translators do not agree on binding numbering, so the emitted SPIR-V is not interchangeable
with the plugin's descriptor setup:

```
blob_00 (nvconv3_b)   driver = 0,1,2,23,29,30      0.2.0 = 0,1,2,3,4,5
blob_06 (nvconv_cmp)  driver = 5,6,7,8,9           0.2.0 = 0,1,2,3,4
```

The driver's scheme reserves a high region for its synthetic buffer-address table and shifts real
bindings up; 0.2.0 emits a contiguous plan from its `bind` layer. `nvmtl_reflect`'s JSON is what the
plugin builds its `MTLArgument`s from, and the plugin has hard requirements on several entry points
(e.g. `nvmtl_translate_fc` returning `"rebuild nvmtl_translate"` when absent; `nvmtl_link_externs` /
`nvmtl_link_visible` / `nvmtl_link_runtime` / `nvmtl_lower_mesh2` refusing their features). So a
faithful port has to carry the reflection contract and the address-table lowering across, not just
re-point a dependency.

### The two questions

1. **Is a rebase to `43c46ac8` (2026-09-06) on the table?** That squash contains AIR intrinsic
   lowering, access-chain and pointer handling, constant/composite dedup, storage and descriptor-slot
   fixes, and function-constant folding — which sound closer to the driver's own bug surface than
   0.2.0's redesign does. It keeps the 0.1.0 API, so the 41 fork files and the wrapper should keep
   compiling, making it a far smaller change than the rewrite.
2. **If 0.2.0 is the target, what is the roadmap?** Specifically whether
   `metal2vulkan-runtime`'s Metal-shaped API is of interest for the Vulkan back end, and whether a
   C-ABI veneer over it (preserving today's 13-symbol surface, which the plugin dlopens) is a path
   you would take — or whether the intent is to stay on the 0.1.0 architecture.

I have the repro scripts and the exact byte sizes for everything above and am happy to share them.
