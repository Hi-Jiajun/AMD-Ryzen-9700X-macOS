# AMD Ryzen 9700X / B850 — macOS adaptation notes

This is the adaptation record for **one specific machine**: an AMD Ryzen 7 9700X
(AM5, Zen 5) on a MAXSUN MS-Terminator B850BKB WIFI, running macOS 15.8.1 and
then macOS 26.7.1 (Tahoe) through OpenCore. It is not a guide, not a
distribution, and not a promise that your board will behave the same way.

The reason to publish it is that AMD-on-macOS information at this generation is
thin, and most of what does exist is either "it works for me" or a wall of
forum posts. This repository tries to separate three things that usually get
mixed together:

- what **can be fixed** with OpenCore, kernel patches and drivers,
- what **cannot be fixed** because Apple's framework has no AMD backend at all,
- what **needs a driver written from scratch**, and how far that got.

## The machine

| | |
| --- | --- |
| CPU | AMD Ryzen 7 9700X, Family `0x1A` Model `0x44` (Zen 5), 8C/16T |
| Board | MAXSUN MS-Terminator B850BKB WIFI (B850, AM5) |
| Memory | 48 GB |
| GPU | NVIDIA RTX 5060 (NullMoth driver) + AMD iGPU (deferred, not attempted) |
| Storage | internal NVMe, OpenCore on `disk0s1` |
| macOS | 15.8.1 (24H32) → 26.7.1 (25G241), Darwin 24.6.0 → 25.6.0 |
| Bootloader | OpenCore 1.0.9 (`DBG-107-2026-10-14` era), SMBIOS `MacPro7,1` |
| Ethernet | Realtek RTL8125 (Lucy driver) + Mellanox ConnectX-3 Pro (own kext) |
| Wi-Fi / BT | MediaTek MT7922 / RZ616 (own drivers, separate repositories) |

SMBIOS identifiers, real MAC addresses and LAN addresses have been replaced with
documentation placeholders throughout. Nothing here is a working EFI — the
config fragments quoted in `docs/` are excerpts for illustration.

## The short version

**Cannot be fixed, at all:**

- **CPU virtualization.** `kern.hv_support=0`. Apple's Hypervisor.framework is
  Intel-VMX-only: `hv_support_init()` → `vmx_hv_support()` → `vmx_is_available()`
  checks the Intel CPUID VMX bit, and the VMX MSRs and `VMXON`/`VMCS`
  instructions do not exist on AMD. Spoofing the CPUID vendor string cannot
  create them. SVM being enabled in BIOS is irrelevant. Consequence: QEMU-HVF,
  UTM(vz), Docker Desktop and anything else built on Apple's VM path are out;
  only pure-emulation QEMU/TCG remains. See [docs/virtualization.md](docs/virtualization.md).
- **AppleVTD / AMD-Vi IOMMU.** A sibling gap with its own missing backend, not
  a consequence of the one above. Fixing either would not fix the other. See
  [evidence/20261013-applevtd-amdvi/](evidence/20261013-applevtd-amdvi/).

**Fixable, and measured working:**

- **Zen 5 timer.** The `mach_wait_until` deadline was overshooting by a stable
  ~8×. A single `_tsc_init` patch rewriting `_busFCvtn2t` to `0x06666666`
  (25 MHz) removes it: realtime medians dropped to 1.027× / 1.007× / 1.004× at
  100/500/1000 µs. Patch hit confirmed by offline binary diff. Note the patch is
  version-gated to Darwin 24.6.0 and will be skipped after an OS update. See
  [docs/handoff-amd-timing-nvram.md](docs/handoff-amd-timing-nvram.md).
- **USB 15-port limit.** Two parts are required together: the upstream
  `XhciPortLimit` binary patch (raises the two `0xF` thresholds) and a
  `DeviceProperties` bus-number move that frees the `locationID` range the
  overflow ports would collide with. Either alone fails. See
  [docs/handoff-usb-portlimit.md](docs/handoff-usb-portlimit.md).
- **Bluetooth stability under sustained USB load.** The MT7922's remote wake on
  this platform triggers an XH00/HS11 recovery storm (`createDevice` returning
  `0xe00002bc` hundreds of times, keyboard and microphone dropping out). A
  codeless kext setting `kUSBRemoteWakeOverride=false` — the same quirk Linux
  `btusb` applies to Ryzen/MT7922 — cleared it for 29 minutes of observation.
  Clearing the stuck state needs an AC power cycle, not a warm reboot.

**Needs new code, partially there:**

- **CPU temperature / fan sensors.** AppleSMC runs, but there are zero real
  sensor values: 65 SMC keys enumerate and none of them is a temperature, fan,
  voltage, power or live frequency. `IOPlatformSensor`, `AppleACPIThermalZone`
  and `AppleSMBusController` instances are all zero. The upstream
  `SMCAMDProcessor` supports only CPU families `0x17`/`0x19` and this is `0x1A`;
  its companion kext writes MSRs and P-states and is not read-only. A read-only
  plugin starting from package temperature is the conditional path; the
  temperature base address is shared across families `17h`–`1Ah`. See
  [docs/handoff-amd-power-smc.md](docs/handoff-amd-power-smc.md).
- **NVIDIA RTX 5060**, **Mellanox ConnectX-3**, **MT7922 Wi-Fi**, **RTL8125.**
  Each is its own project with its own repository; the handoff notes here record
  the platform-side constraints they ran into.

## Layout

```
README.md                        the short version
docs/INDEX.md                    navigation for everything below
docs/2026-10-19-amd-adaptation-program.md
                                 the programme: classification, workflows, milestones
docs/virtualization.md           why AppleHV has no AMD backend
docs/handoff-amd-boot-config.md  boot baseline, backup and restore drill
docs/handoff-amd-timing-nvram.md Zen 5 timer/TSC and NVRAM semantics
docs/handoff-amd-power-smc.md    CPU power, SMC sensors, fan control
docs/handoff-usb-portlimit.md    the 15-port limit, both halves
docs/device-driver-feasibility.md per-device feasibility and handoffs
docs/install-runbook.md          the master on-site log
docs/bios/                       decoded BIOS: Setup/CBS tables, IFR dump, variable list
docs/10-实时的全部115个固件变量.txt
                                 every firmware variable the board exposes, live
evidence/                        the measurement records the conclusions come from
```

`evidence/` keeps the raw reports, per-domain, dated. Each directory is one
session's measurement work: baselines, what was run, what came back, what was
rejected. `docs/2026-10-19-amd-adaptation-program.md` is the index of record for
the whole programme and is the longest document here.

## What this cost, and what it is worth

The programme ran as roughly a dozen parallel workstreams with a single writer
for the shared EFI, because two sessions writing `config.plist` on one machine
produces faults nobody can attribute. The rule that mattered: **only one session
touches the ESP at a time; everything else submits candidates and waits for a
window.** Several conclusions in here came from that discipline holding — and a
few sessions were burned when it did not.

The most useful single thing in this repository, if you take one, is the
classification in `docs/01-adaptation-program.md` section 2: which gaps are
kernel-level and unfixable, which are I/O-level and patchable, which are
device-level and need a driver. Everything else follows from getting that
boundary right.

## License

Written material (all of `docs/` and `README.md`) is licensed
**CC BY 4.0** — https://creativecommons.org/licenses/by/4.0/

Code snippets embedded in the documents, and the small tools under `evidence/`,
are licensed **MIT**. See [LICENSE](LICENSE).

## Acknowledgements

- The OpenCore team and Acidanthera (Lilu, VirtualSMC, WhateverGreen).
- The AMD_Vanilla kernel patch set, whose `_tsc_init` / `bscFCvtn2t` work the
  timer section builds on.
- trulyspinach's `SMCAMDProcessor`, for documenting which CPU families the
  sensor path covers (and thereby showing why this one is out of scope).
- The NullMoth NVIDIA project, whose driver runs the RTX 5060 here.
