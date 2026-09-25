#!/usr/bin/env python3
"""p210 - TASK-034 P2: build the CLO audio stack into the kernel (=y).

konaauto.conf is the single switch file: techpack/audio/Makefile includes it
(ARCH_KONA branch) and exports the flags; every per-module Kbuild registers
objects as obj-$(CONFIG_X) (verified asoc/dsp Kbuilds), so m->y flips make
the stack resident in vmlinux/Image. konaautoconf.h uses '#define X 1' which
is identical for y and m -- no source change needed.

Keep =m (NOT built in):
- MSM_ULTRASOUND  : hardware/feature absent (dead-weight, removed from payload)
- SND_SOC_KONA    : CLO kona machine driver -- collides with the Lenovo
                    audio_machine_kona.ko prebuilt that provides the real
                    4-speaker TDM routing (reverse target, TASK-034 P4)
- VOICE_MHI / QTI_PP / MSM_AVTIMER / AUXPCM_DISABLE : unused on this device
Idempotent."""

P = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/techpack/audio/config/konaauto.conf"
KEEP = {"MSM_ULTRASOUND", "SND_SOC_KONA", "VOICE_MHI", "QTI_PP", "MSM_AVTIMER", "AUXPCM_DISABLE"}

src = open(P).read()
out, flipped, kept = [], 0, 0
for line in src.splitlines():
    s = line.strip()
    if s.startswith("export CONFIG_") and s.endswith("=m"):
        name = s[len("export CONFIG_"):-len("=m")]
        if name in KEEP:
            kept += 1
            out.append(line)
        else:
            flipped += 1
            out.append(line[: -len("=m")] + "=y")
    else:
        out.append(line)
new = "\n".join(out) + "\n"
if flipped == 0 and "=y" in src:
    print("p210: konaauto.conf ALREADY flipped (kept=%d)" % kept)
else:
    open(P, "w").write(new)
    print("p210: konaauto.conf flipped %d flags to =y, %d kept =m" % (flipped, kept))
print("p210: OK")
