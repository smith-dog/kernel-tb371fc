#!/usr/bin/env python3
# update-readme-v13.py — point the README's download links at Release v1.3
# (n86 direct-flash image + fixed dlkm package), demote the old
# manager-patch flow to a collapsible legacy section, add DT2W to the
# feature list, note techpack audio/camera/video git exclusion.
import sys

P = "/home/smith/android_kernel_lenovo_paladin/README.md"
c = open(P, encoding="utf-8").read()

# 1) features: add DT2W
old = """- **外设**：指纹（供电轨修复）、WiFi/蓝牙、双扬声器唤醒"""
new = """- **外设**：指纹（供电轨修复）、WiFi/蓝牙、双扬声器唤醒、双击亮屏
  （DT2W：手势武装/退出时序重构 + 唤醒恢复改为面板上电前同步全量固件重刷）"""
assert c.count(old) == 1, "feat: %d" % c.count(old)
c = c.replace(old, new)

old = """panel wake, suspend, KernelSU-as-LKM decoupling. Full patch history in"""
new = """panel wake, suspend, double-tap-to-wake, KernelSU-as-LKM decoupling.
Full patch history in"""
assert c.count(old) == 1, "feat-en: %d" % c.count(old)
c = c.replace(old, new)

# 2) How-to section: v1.3 direct flash primary
old_start = "1. 下载 [Release v1.0]"
old_end = "载荷包（内含 README 一键安装说明）\n"
i0 = c.index(old_start)
i1 = c.index(old_end) + len(old_end)
new_howto = """1. 下载 [Release v1.3](https://github.com/smith-dog/kernel-tb371fc/releases/tag/v1.3)
   中的 [`boot-v27n86-patched-q706.img`](https://github.com/smith-dog/kernel-tb371fc/releases/download/v1.3/boot-v27n86-patched-q706.img)（已含 root，直刷即可）与
   [`tb371fc-dlkm-pkg-fixed.tar.gz`](https://github.com/smith-dog/kernel-tb371fc/releases/download/v1.3/tb371fc-dlkm-pkg-fixed.tar.gz)（vendor 模块载荷包，内含一键安装说明）
2. 刷入并重启：
   ```
   fastboot flash boot boot-v27n86-patched-q706.img
   fastboot set_active a
   fastboot reboot
   ```
3. 开机后 KernelSU 管理器显示"正常/LKM"即 root 就绪；按载荷包内 README
   安装 vendor 模块（WiFi/音频等）
"""
c = c[:i0] + new_howto + c[i1:]

# 3) KSU upgrade line: file path instead of re-patch
old = """**日后升级 KernelSU**：只换新版 `ksu.ko` 重复第 2 步，**内核无需重编**。"""
new = """**日后升级 KernelSU**：只替换 `/data/adb/tb371fc-dlkm/ksu.ko` 并重启，**内核无需重刷**。

<details>
<summary>旧方法（v1.0/v1.1 纯净镜像 + 管理器修补）</summary>

1. 下载 [Release v1.0](https://github.com/smith-dog/kernel-tb371fc/releases/tag/v1.0)
   中的 [`boot-v27n60-pure.img`](https://github.com/smith-dog/kernel-tb371fc/releases/download/v1.0/boot-v27n60-q706.img) 与 [`KernelSU-v2patched-release.apk`](https://github.com/smith-dog/kernel-tb371fc/releases/download/v1.0/KernelSU-v2patched-release.apk)（也可直接用 [Release v1.1](https://github.com/smith-dog/kernel-tb371fc/releases/tag/v1.1) 的 `kernelsu-patched-n61.img` 免修补直刷，已含 Docker 支持）
2. 安装 APK，打开 KernelSU 管理器 → **安装** → **选择并修补一个文件** →
   选 `boot-v27n60-pure.img` → LKM 处选 **"使用本地 LKM 文件"** →
   选 `ksu-32630.ko` → 生成 `kernelsu_patched_*.img`
3. 刷入并重启：
   ```
   fastboot flash boot kernelsu_patched_*.img
   fastboot set_active a
   fastboot reboot
   ```

</details>"""
assert c.count(old) == 1, "ksu-upgrade: %d" % c.count(old)
c = c.replace(old, new)

# 4) provenance: patch range
old = "p1~p130 补丁（见 `tb371fc/scripts/`），全部以上述来源为基础"
new = "p1~p177 补丁（见 `tb371fc/scripts/`），全部以上述来源为基础"
assert c.count(old) == 1, "range: %d" % c.count(old)
c = c.replace(old, new)

# 5) build section: techpack note
old = """LKM 注意：`CONFIG_KSU=m` 时 ksu.ko 需要本树 `drivers/ksu_sym.c`
（48 个非公开符号的 EXPORT 垫片，已内建在树中）。"""
new = """LKM 注意：`CONFIG_KSU=m` 时 ksu.ko 需要本树 `drivers/ksu_sym.c`
（48 个非公开符号的 EXPORT 垫片，已内建在树中）。

techpack 说明：本库仅跟踪 `techpack/display`（显示栈）；`techpack` 下的
audio/camera/video 源码不入库（音频由 dlkm 模块包提供，构建时 Kbuild
会自动跳过不存在的目录，不影响编译）。"""
assert c.count(old) == 1, "build note: %d" % c.count(old)
c = c.replace(old, new)

open(P, "w", encoding="utf-8").write(c)
print("README_V13_DONE")
