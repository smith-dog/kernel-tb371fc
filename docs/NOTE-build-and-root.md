# ⚠️ v1.5 起本流程已废弃

本文描述的载荷包(tb371fc-dlkm)部署方式仅适用于 v1.4 及更早内核。
v1.5 起全部驱动内建,**刷入单一 boot 镜像即完成安装,无需载荷包**;
刷过载荷包的请用 Release v1.5 的 tb371fc-payload-cleanup.sh 清理。
当前权威流程见 [README.md](../README.md)。以下为历史文档。

# NOTE — TB371FC 内核构建与 Root 流程（smith-dog/kernel-tb371fc）

> 适用：本项目全部 v27n 系列内核（4.19.198-perf+，CONFIG_KSU=m 纯 LKM 形态）。
> 本文件同时存于仓库 README 之外的社区参考文档。

---

## 0. 架构总览（先读这个）

```
┌─ boot 分区 ─────────────────────────────┐
│ 纯净内核 Image（无任何 root 代码）        │
│ + ramdisk（管理器修补时注入              │
│   kernelsu.ko + ksud）                  │
└─────────────────────────────────────────┘
┌─ /data ────────────────────────────────┐
│ /data/adb/tb371fc-dlkm/                │
│   ├── dlkm/*.ko    vendor 模块副本      │
│   ├── insmod128    特殊加载器           │
│   ├── load.sh      开机加载链           │
│   └── fwdbg.sh     固件喂入             │
│ /data/adb/post-fs-data.d/98-dlkm 钩子   │
└─────────────────────────────────────────┘
```

- 内核本体**不含任何 root 代码**（CONFIG_KSU=m）。
- root = KernelSU LKM：管理器修补 boot 镜像时把 `kernelsu.ko` + `ksud`
  注入 ramdisk，init 阶段 insmod（开机 ~1.1s 完成）。
- **升级 KSU = 换新版 ksu.ko 重走管理器修补，内核不用重编。**
- **升级内核 = 重编 Image 后重走管理器修补**（用现有 ksu.ko）。

---

## 1. 内核编译（WSL2 Ubuntu）

### 工具链
- **Snapdragon LLVM 10.0.7 for Android NDK**（与联想原厂编译横幅逐字一致，
  路径示例 `/home/<user>/snapdragon-llvm-10.0.7`）
- **aarch64-linux-gnu binutils ≥ 2.46**（系统包）
- 树内已含全部改动：直接 `make` 即可，无需再打补丁

### 编译内核镜像
```bash
export PATH=/path/to/snapdragon-llvm-10.0.7/bin:$PATH
cd android_kernel_lenovo_paladin
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS=-Wno-error Image
# 产物：arch/arm64/boot/Image
```

### 编译 KernelSU LKM（可选，一般直接用 Release 的 ksu.ko）
```bash
make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang \
     CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as \
     KCFLAGS=-Wno-error drivers/kernelsu/ksu.ko
# 产物：drivers/kernelsu/ksu.ko（vermagic 4.19.198-perf+ 原生正确）
```
注意：ksu.ko 依赖 `drivers/ksu_sym.c`（48 符号 EXPORT 垫片，已在树中
并接入 drivers/Makefile）——**别删它**，否则 modpost 报 40+ undefined。

### 打包 boot 镜像
```bash
python3 tb371fc/tools/repack_boot.py <apatch_base.img> \
    arch/arm64/boot/Image out/boot.img out/dtbs/tail-new.bin
```
- `<apatch_base.img>`：任意一份历史 boot 镜像作模板（脚本只取其头/ramdisk/
  DTB 尾结构，内核 payload 换成我们的），保持 v2 头 + 分区几何
- 产物 `out/boot.img` = **纯净内核**（无 root），交付/Release 用这个

---

## 2. KernelSU 管理器（修补 root 用）

官方管理器**不支持非 GKI**（修补器拒 boot header v2、KMI 列表无 4.19）。
必须用我们构建的版本：

- **现成 APK**：仓库 Release v1.0 的
  [`KernelSU-v2patched-release.apk`](https://github.com/smith-dog/kernel-tb371fc/releases/download/v1.0/KernelSU-v2patched-release.apk)
- **自己构建**（依赖仓库变动时）：
  1. fork `smith-dog/KernelSU`（分支 `allow-bootimage-v2`，已含两类修复：
     ① ksud 门禁 `ver < 3 → ver < 1`（boot v1/v2 放行）；
     ② 5 个 git 依赖重映射社区续命镜像——原 `Kernel-SU` org 已被 GitHub
     下架，Actions/本地均拉不到）
  2. GitHub Actions → `Build Manager` workflow → 手动 dispatch（选分支）
  3. 产物 `manager` artifact 内含 release APK

管理器版本要求：与 ksu.ko（32630）同代即可配对（32642 实测正常）。

---

## 3. Root 流程（每次新内核刷入后必做）

**前置**：设备已有任意可用 boot（首装用官方 stock 亦可——修补不需要 root）。

1. 把两个文件推进设备（无 root 时 adb push 到 `/sdcard/Download/`）：
   - 纯净内核 `boot.img`
   - `ksu.ko`（Release v1.0/v1.1 有）
2. 管理器 → **安装** → **选择并修补一个文件** → 选 boot.img
3. LKM 选择处选 **"使用本地 LKM 文件"** → 选 ksu.ko
4. 产物生成于 `/sdcard/Download/kernelsu_patched_*.img`
5. 刷入：
   ```bash
   adb reboot bootloader
   fastboot flash boot kernelsu_patched_*.img
   fastboot reboot
   ```
6. 开机后验证：管理器显示 **LKM + 正常**；`adb shell su -c id` 返回
   `uid=0(root) ... context=u:r:ksu:s0`

> ⚠️ **绝对不要直接刷纯净内核**（CONFIG_KSU=m 下无任何 root，su 不存在，
> 且 post-fs-data.d 钩子体系依赖 ksud→root 自举，会整机失去 root，只能
> fastboot 重刷修补版救援）。

---

## 4. 开机自动化（root 后一次性部署）

root 起来后，vendor 模块（WiFi/音频/功放等 38 个）不会自动加载，需要
一次性部署载荷包：

```bash
adb push tb371fc-dlkm-pkg.tar.gz /data/local/tmp/
adb shell "su -c 'tar xzf /data/local/tmp/tb371fc-dlkm-pkg.tar.gz -C /data/adb && sh /data/adb/tb371fc-dlkm/install.sh'"
adb reboot
```

内容与机制（全在 /data，恢复出厂后重装即可）：
- `/data/adb/tb371fc-dlkm/dlkm/` — 40 个 vermagic 补丁版 vendor 模块
  （4.19.198-perf+ 对齐）+ `insmod128`（finit_module +
  IGNORE_MODVERSIONS|IGNORE_VERMAGIC，因为内核 MODVERSIONS=y 而模块无 CRC 表）
  + `load_ordered.sh`（depmod 拓扑序）+ `hq_shim.ko`（联想私有符号垫片）
- `/data/adb/post-fs-data.d/98-dlkm` — 薄启动器（ksud 执行，调 load.sh）
- `/data/adb/tb371fc-dlkm/fwdbg.sh` — sysfs 固件自动喂入
  （tfa98xx_QS.cnt 等 uevent 丢失兜底）+ dmesg 取证
- ⚠️ `dlkm/` 里**勿混入** Lenovo `audio_q6_notifier.ko`（PDR 死路）

---

## 5. 常见坑（全部真实踩过）

| 坑 | 后果 | 规避 |
|---|---|---|
| 直刷纯净内核 | 整机无 root（bootstrap 死锁） | 永远走管理器修补 |
| 删 drivers/ksu_sym.c | ksu.ko 编译失败 40+ undefined | 垫片是树的一部分 |
| 模块 vermagic 不匹配 | insmod 拒载 | 用 insmod128 或重打 vermagic |
| Lenovo audio_q6_notifier.ko | 音频 PDR 死路无声 | 从 dlkm 剔除 |
| tfa98xx .cnt 固件不加载 | 无声（uevent 丢失） | fwdbg.sh 自动喂 |
| pkill -f 含自身命令行 | 自杀 | 用精确 pgrep |
| WSL→GitHub 推送被污染 | 解析回 127.0.0.1 | /etc/wsl.conf `generateHosts=false` + hosts 写死 IP，或 git bundle 走 Windows 侧 push |

---

## 6. 版本历史

- v1.0（2026-09-19）：首个公开版，全功能 bring-up + KSU LKM 解耦
- v1.1（2026-09-19）：+xt_addrtype，Docker bridge 网络就绪
- 运行内核演进史（n1~n61）见 `tb371fc/scripts/p*.py|sh` 脚本名即补丁号
