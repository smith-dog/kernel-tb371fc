# Lenovo TB371FC (Xiaoxin Pad Pro 12.7 2021, SM8250/kona) custom kernel

Kernel **4.19.198-perf+** for Lenovo TB371FC, built from the Lenovo GPL
source dump with an extensive bring-up patch series. **KernelSU 32630 runs
as an LKM** (`CONFIG_KSU=m` + this repo's symbol-export shim), fully
decoupled from the kernel image — KSU upgrades no longer require kernel
rebuilds.

## Feature status (all verified on device)

| Area | Status |
|---|---|
| Display (incl. 120Hz, wake-from-sleep, brightness) | ✅ |
| Audio (4x TFA9894 speakers via Lenovo machine driver + CLO core) | ✅ |
| Camera photo/video (CLO CamX + venus 855 hard encode) | ✅ |
| WiFi (QCA6390, vendor modules via vermagic-patched copy) | ✅ |
| Bluetooth (HAL-driven UART, QCA6390) | ✅ |
| Fingerprint (Goodix, `goodix_vdd-supply` regulator fix) | ✅ |
| Battery: charge protection 40-60%, maintenance, health | ✅ |
| Suspend-to-RAM (deep) | ✅ |
| VINTF compatible (no boot-time "internal problem" dialog) | ✅ |
| KernelSU 32630 as LKM (backslashxx fork) | ✅ |

## Repo layout

```
<kernel tree>            Lenovo GPL dump + all fixes applied (main branch)
tb371fc/scripts/         p1..p130 patch/build scripts (our working history)
tb371fc/tools/           repack_boot.py, insmod128.c, dtb tools, busybox
tb371fc/ksu_sym.c        48-symbol EXPORT shim required by ksu.ko (LKM)
```

`drivers/ksu_sym.c` in the tree = installed copy of `tb371fc/ksu_sym.c`.
`drivers/Makefile` wires `obj-y += ksu_sym.o`.

## Build (WSL2 Ubuntu, Snapdragon LLVM 10.0.7)

```bash
# toolchain: Snapdragon LLVM 10.0.7 for Android NDK (matches Lenovo banner)
# binutils: aarch64-linux-gnu- 2.46
export PATH=/path/to/snapdragon-llvm-10.0.7/bin:$PATH
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS=-Wno-error Image
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS=-Wno-error drivers/kernelsu/ksu.ko
```

Pack boot image (keeps v2 header, ramdisk & appended DTB tail byte-identical):

```bash
python3 tb371fc/tools/repack_boot.py <apatch_base.img> \
    arch/arm64/boot/Image out/boot.img out/dtbs/tail-new.bin
```

Root note: with `CONFIG_KSU=m` the kernel has **no builtin root**. Use the
KernelSU manager (32642+, must accept boot image **v2**) "select & patch a
file" with your boot image + `ksu.ko`, then flash the patched image. A
manager with boot-image-v2 support is buildable from
`backslashxx/KernelSU` + two one-line patches (see tb371fc/scripts/p12*).

## Key config decisions (VINTF compatible)

- `CONFIG_SYSVIPC=n` + `CONFIG_POSIX_MQUEUE=y` → keeps `IPC_NS` alive
  (Droidspaces) while satisfying FCM5 matrix `SYSVIPC=n`
- `CONFIG_MODVERSIONS=y` + `CONFIG_MODULE_FORCE_LOAD=y` → FCM requirement;
  vendor modules load via `insmod128` (finit_module + IGNORE_MODVERSIONS
  |IGNORE_VERMAGIC flags) — see tb371fc/tools/insmod128.c
- `SUBLEVEL=198` → passes `VintfObject.verifyWithoutAvb()` kernel version
  match (silences the boot-time "internal problem" dialog)
- `CONFIG_ANDROID_PARANOID_NETWORK=n`

## License

GPL-2.0 (inherited from the kernel and Lenovo's GPL release).
