# TB371FC Custom Kernel — 4.19.198-perf+

联想小新 Pad Pro 12.7 2021（TB371FC，高通 SM8250/kona）的自定义内核。
基于联想 GPL 公开源码 + 完整 bring-up 补丁集，KernelSU 以 LKM（可加载模块）
形态运行，与内核镜像完全解耦。

**A custom kernel for the Lenovo Xiaoxin Pad Pro 12.7 2021 (TB371FC,
Qualcomm SM8250/kona)**, built from the official Lenovo GPL source dump
plus a full bring-up patch series. KernelSU 32630 runs as an **LKM**
(loadable module) — fully decoupled from the kernel image, so KernelSU
upgrades never require a kernel rebuild.

---

## 这是什么 / What is this

原厂内核存在大量功能缺陷且停止维护。本项目在其 GPL 源码上完成了：

- **音视频**：扬声器全链路（4×TFA9894 功放 + CLO 音频核心栈）、相机录像、
  venus 硬解码
- **充电**：充电保护（40-60% 电量保持）、电池养护开关、充电状态自反馈死循环修复
- **外设**：指纹（供电轨修复）、WiFi/蓝牙、双扬声器唤醒
- **系统兼容**：VINTF 兼容（消除开机"设备内部出现问题"弹窗）、睡眠（deep suspend）
- **Root**：KernelSU 32630（backslashxx fork）以 LKM 运行，管理器一键修补升级
- **Docker**：iptables 全套 + （docker0 网络初始化规则依赖）已齐

**Fixes over stock**: speaker audio chain, camera video recording, fingerprint
power rail, charge-protection feedback loop (36/s kernel vote storm → 0),
boot-time "internal problem" dialog (VINTF kernel-version/config match),
panel wake, suspend, KernelSU-as-LKM decoupling. Full patch history in
`tb371fc/scripts/` (p1–p130).

---

## 怎么用 / How to use

> 前提：Bootloader 已解锁（`fastboot flashing unlock`）。

1. 下载 [Release v1.0](https://github.com/smith-dog/kernel-tb371fc/releases/tag/v1.0)
   中的 `boot-v27n60-pure.img` 与 `KernelSU-v2patched-release.apk`
2. 安装 APK，打开 KernelSU 管理器 → **安装** → **选择并修补一个文件** →
   选 `boot-v27n60-pure.img` → LKM 处选 **"使用本地 LKM 文件"** →
   选 `ksu-32630.ko` → 生成 `kernelsu_patched_*.img`
3. 刷入并重启：
   ```
   fastboot flash boot kernelsu_patched_*.img
   fastboot set_active a
   fastboot reboot
   ```
4. 开机后管理器显示"正常/LKM"即成功；vendor 模块（WiFi/音频等）
   需要 [tb371fc-dlkm-pkg.tar.gz](https://github.com/smith-dog/kernel-tb371fc/releases/tag/v1.0)
   载荷包（内含 README 一键安装说明）

**日后升级 KernelSU**：只换新版 `ksu.ko` 重复第 2 步，**内核无需重编**。

---

## 自己编译 / Build from source

环境：WSL2 Ubuntu + [Snapdragon LLVM 10.0.7 for Android NDK]
（与联想原厂编译横幅一致）+ aarch64-linux-gnu binutils ≥ 2.46。

```bash
export PATH=/path/to/snapdragon-llvm-10.0.7/bin:$PATH
# 内核镜像
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS=-Wno-error Image
# KernelSU LKM 模块
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS=-Wno-error drivers/kernelsu/ksu.ko
# 打包 boot 镜像（保留 v2 头、ramdisk、DTB 尾）
python3 tb371fc/tools/repack_boot.py <apatch_base.img> \
    arch/arm64/boot/Image out/boot.img out/dtbs/tail-new.bin
```

LKM 注意：`CONFIG_KSU=m` 时 ksu.ko 需要本树 `drivers/ksu_sym.c`
（48 个非公开符号的 EXPORT 垫片，已内建在树中）。

---

## 源代码来自哪里 / Provenance

| 组成 | 来源 |
|---|---|
| 内核基线 | [lss4/android_kernel_lenovo_paladin](https://github.com/lss4/android_kernel_lenovo_paladin)（分支 11）——社区开发者整理开源的联想官方 GPL 包（TB-Q706F/Z，代号 paladin，4.19.157 与 TB371FC stock 同版本，含联想板级代码） |
| 音频核心栈 | CodeLinaro `LA.UM.9.12.r1-18500-SMxx50.QSSI14.0`（techpack/audio） |
| 相机 KMD | 同上 tag（techpack/camera） |
| 视频硬解 | 小米 kona 树 msm_vidc（compatible 完全匹配） |
| 触摸/背光驱动 | [tem423/android_kernel_lenovo_tb371fc](https://github.com/tem423/android_kernel_lenovo_tb371fc)（TB371FC 社区内核；本树合入其 nt36532 SPI 触摸驱动与 ktz8866a/b 双芯片背光驱动） |
| KernelSU | [backslashxx/KernelSU](https://github.com/backslashxx/KernelSU) tag 32630（管理器 APK = 本项目 fork 构建：[smith-dog/KernelSU](https://github.com/smith-dog/KernelSU) 分支 allow-bootimage-v2，含 boot-v2 支持 + 依赖镜像修复） |
| 本项目 | p1~p130 补丁（见 `tb371fc/scripts/`），全部以上述来源为基础 |

联想未随 GPL dump 公开的部分（如 144Hz 显示驱动、部分面板参数）不在本树，
对应功能保持原厂形态。

## License

GPL-2.0（继承内核及联想 GPL 发布）。
