#!/usr/bin/env python3
# p179 (docs fix): 1) remove the over-warning slot note from README/docs
# (the corrected flash flow never touches another slot); 2) correct the
# techpack note: tracked = display + Kbuild/stub glue; audio ships via the
# dlkm package; camera/video are not enabled in the kernel config at all
# (verified: no CONFIG_CAMERA_*, modules.builtin empty) so a fresh clone
# builds a functionally identical kernel.
P = "/home/smith/android_kernel_lenovo_paladin/README.md"
D = "/home/smith/android_kernel_lenovo_paladin/docs/NOTE-build-and-root.md"

note = "   > 说明：`fastboot flash boot` 刷入的是**当前活动槽**；不确定可先 `fastboot getvar current-slot` 查看。刷完切勿 `set_active` 切到另一槽——那会启动旧内核。\n"

c = open(P, encoding="utf-8").read()
assert c.count(note) == 1, "readme note: %d" % c.count(note)
c = c.replace(note, "")

old = """techpack 说明：本库仅跟踪 `techpack/display`（显示栈）；`techpack` 下的
audio/camera/video 源码不入库（音频由 dlkm 模块包提供，构建时 Kbuild
会自动跳过不存在的目录，不影响编译）。"""
new = """techpack 说明：本库跟踪 `techpack/display` 与 `techpack/Kbuild`、
`techpack/stub` 胶水；techpack 下的 audio/camera/video 源码未入库且未在
内核配置中启用（音频 .ko 由 dlkm 模块包提供），克隆缺失这些目录不影响
编译与产物功能。"""
assert c.count(old) == 1, "techpack note: %d" % c.count(old)
c = c.replace(old, new)
open(P, "w", encoding="utf-8").write(c)
print("README_FIXED")

c = open(D, encoding="utf-8").read()
assert c.count(note) == 1, "doc note: %d" % c.count(note)
c = c.replace(note, "")
open(D, "w", encoding="utf-8").write(c)
print("DOC_FIXED")
