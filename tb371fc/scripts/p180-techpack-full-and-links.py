#!/usr/bin/env python3
# p180 (repo completeness): track ALL techpack driver sources (the shipped
# kernel builds display built-in + camera SPECTRA=y + video venus from them;
# audio provides the dlkm module sources) and fix/expand the README
# provenance links (CodeLinaro tag, MiCode kona, scripts dir, tools).
import os, shutil

KV = "/home/smith/android_kernel_lenovo_paladin"
GI = os.path.join(KV, ".gitignore")
RD = os.path.join(KV, "README.md")
BK = "/tmp/p180-backup"
os.makedirs(BK, exist_ok=True)
for f in (GI, RD):
    if not os.path.exists(os.path.join(BK, os.path.basename(f))):
        shutil.copy2(f, os.path.join(BK, os.path.basename(f)))
        print("BACKUP %s" % f)

# --- .gitignore: track whole techpack, artifacts stay ignored globally ---
c = open(GI, encoding="utf-8").read()
old = """# TB371FC: track the display driver we modify (+ stub/Kbuild glue); the
# rest of techpack (audio/camera/video, 170MB+) stays outside git - audio
# ships as dlkm modules instead.
techpack/*
!techpack/Kbuild
!techpack/display
!techpack/stub
*.cmd
*.d
modules.builtin"""
new = """# TB371FC: techpack drivers (display/audio/camera/video) are part of the
# shipped kernel - keep their sources tracked; build artifacts (*.o/*.a/
# *.cmd/*.d/*.order/modules.*) stay ignored via the global rules above.
*.cmd
*.d
modules.builtin"""
assert c.count(old) == 1, "gitignore: %d" % c.count(old)
open(GI, "w", encoding="utf-8").write(c.replace(old, new))
print("GITIGNORE_OK")

# --- README: provenance links + techpack note fix ---
c = open(RD, encoding="utf-8").read()

old = """| 音频核心栈 | CodeLinaro `LA.UM.9.12.r1-18500-SMxx50.QSSI14.0`（techpack/audio） |
| 相机 KMD | 同上 tag（techpack/camera） |
| 视频硬解 | 小米 kona 树 msm_vidc（compatible 完全匹配） |"""
new = """| 显示栈 | CodeLinaro [msm-4.19 @ LA.UM.9.12.r1-18500-SMxx50.QSSI14.0](https://git.codelinaro.org/clo/la/kernel/msm-4.19/-/tree/LA.UM.9.12.r1-18500-SMxx50.QSSI14.0)（vanilla techpack/display；双击唤醒通知钩子 p140/p174 位于 dsi_display.c） |
| 音频核心栈 | 同上 CLO tag 的 [techpack/audio](https://git.codelinaro.org/clo/la/kernel/msm-4.19/-/tree/LA.UM.9.12.r1-18500-SMxx50.QSSI14.0/techpack/audio)（编出 dlkm 音频模块，见模块包） |
| 相机 KMD | 同上 CLO tag 的 [techpack/camera](https://git.codelinaro.org/clo/la/kernel/msm-4.19/-/tree/LA.UM.9.12.r1-18500-SMxx50.QSSI14.0/techpack/camera)（SPECTRA_CAMERA=y，内建） |
| 视频硬解 | [MiCode/Xiaomi_Kernel_OpenSource](https://github.com/MiCode/Xiaomi_Kernel_OpenSource) kona 分支的 msm_vidc（compatible 完全匹配） |"""
assert c.count(old) == 1, "table: %d" % c.count(old)
c = c.replace(old, new)

old = """| 本项目 | p1~p177 补丁（见 `tb371fc/scripts/`），全部以上述来源为基础 |"""
new = """| 本项目 | p1~p177 补丁（[tb371fc/scripts](https://github.com/smith-dog/kernel-tb371fc/tree/main/tb371fc/scripts)），全部以上述来源为基础 |"""
assert c.count(old) == 1, "proj row: %d" % c.count(old)
c = c.replace(old, new)

old = """techpack 说明：本库跟踪 `techpack/display` 与 `techpack/Kbuild`、
`techpack/stub` 胶水；techpack 下的 audio/camera/video 源码未入库且未在
内核配置中启用（音频 .ko 由 dlkm 模块包提供），克隆缺失这些目录不影响
编译与产物功能。"""
new = """techpack 说明：display/audio/camera/video 四个驱动目录的源码已全部入库
（与出货内核一致，相机 KMD 内建、音频编为 dlkm 模块），仅构建产物
（*.o/*.a/*.cmd 等）被忽略。"""
assert c.count(old) == 1, "techpack note: %d" % c.count(old)
c = c.replace(old, new)

old = """LKM 注意：`CONFIG_KSU=m` 时 ksu.ko 需要本树 `drivers/ksu_sym.c`
（48 个非公开符号的 EXPORT 垫片，已内建在树中）。"""
new = """LKM 注意：`CONFIG_KSU=m` 时 ksu.ko 需要本树 `drivers/ksu_sym.c`
（48 个非公开符号的 EXPORT 垫片，已内建在树中）。`tb371fc/tools/` 内含
repack/insmod128/dtbo 等工具与源码（[tools 目录](https://github.com/smith-dog/kernel-tb371fc/tree/main/tb371fc/tools)）。"""
assert c.count(old) == 1, "tools note: %d" % c.count(old)
c = c.replace(old, new)

open(RD, "w", encoding="utf-8").write(c)
print("README_OK")
print("P180_DONE")
