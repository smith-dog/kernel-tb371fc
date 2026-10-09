# TB371FC Custom Kernel — 4.19.325-perf++

联想小新 Pad Pro 12.7 2021（TB371FC，高通 SM8250/kona）的自定义内核。
基于联想 GPL 公开源码 + 完整 bring-up 补丁集。**v1.5 起 WiFi/音频/加速等
全部驱动内建进内核镜像，功能开箱即用：刷入一个镜像即完成全部安装，无需
任何载荷包或后续步骤。** KernelSU 以 LKM（可加载模块）形态运行，与内核
镜像完全解耦。

**v2.0 是本项目的 stable 基线跳变**：把厂商树的 linux-stable 从
**v4.19.157 完整采纳到 v4.19.325**（168 个 stable 发布的内容，合并进本仓库
`main`，tag `v4.19.157` / `v4.19.325` 已随仓库发布），并在采纳过程中修掉
若干**只有整批吃 stable 才会暴露**的厂商缺陷（USB gadget 生命周期、
driver-core `needs_suppliers` 链表、eMMC 调参 hold、TCP 数据竞争、
early-entropy 初始化等，清单见下）。诊断插桩按发布惯例在本版关闭
（dev 分支常驻，发布件 `strings Image` 已验无残留）。

**A custom kernel for the Lenovo Xiaoxin Pad Pro 12.7 2021 (TB371FC,
Qualcomm SM8250/kona)**, built from the official Lenovo GPL source dump
plus a full bring-up patch series. v2.0 additionally adopts the whole
linux-stable range **v4.19.157 → v4.19.325** into this vendor tree.
KernelSU (backslashxx fork, staging-synced 32651) runs as an **LKM**
(loadable module) — fully decoupled from the kernel image, so KernelSU
upgrades never require a kernel rebuild.


---

## 这是什么 / What is this

本项目旨在**拓展内核能力**——在联想 GPL 公开源码之上，持续为设备带来超出原厂形态的功能支持：

- **音视频**：扬声器全链路（4×TFA9894 功放 + CLO 音频核心栈，v1.5 全内建，
  功放固件烤入内核，冷启动出声更快）、相机录像、venus 硬解码
- **充电**：充电保护（40-60% 电量保持）、电池养护开关、充电状态自反馈死循环修复
- **外设**：指纹（供电轨修复）、WiFi/蓝牙、双扬声器唤醒、双击亮屏
  （DT2W：手势武装/退出时序重构 + 唤醒恢复改为面板上电前同步全量固件重刷）、
  USB-C OTG 自动主机模式（外接键盘/U盘 即插即用，免手动开关）
- **系统兼容**：VINTF 兼容（消除开机"设备内部出现问题"弹窗）、睡眠（deep suspend）
- **内核基线（v2.0）**：linux-stable **v4.19.157 → v4.19.325** 完整采纳（168 个
  stable 发布；相对上一次发布共 121 个提交进 `main`），采纳范围内逐文件按三方比对定案，厂商私有
  机制（`use_out_ep`/`bound` 通知、EOPFEN 拆分、`mb()` 写 TRB 等）保留而非覆盖。
  ⚠ 顺带纠正一处历史误标：**v1.x 横幅上的 `4.19.198` 是早期任务手改 `Makefile SUBLEVEL`
  得到的假版本号，当时树内代码实为 4.19.157**（与 TB371FC stock 同版本）。本版起
  横幅 `4.19.325-perf++` 与 `SUBLEVEL = 325` 与实际采纳内容一致
- **v2.0 顺手修掉的厂商缺陷**（都是"整批吃 stable"才暴露的）：`f_ncm` 第二实例
  错误路径解引用共享 uevent 设备指针（开机 26 s panic）→ 修；driver-core
  `device_links_purge` 裸 `list_del` 让每次正常 `device_del` 都打印
  LIST_POISON 腐败 → `list_del_init`；`mmc_start_request()` 把
  `mmc_retune_hold()` 关在一个**无 Kconfig 条目**的 `#ifdef` 里（守卫恒假）→
  每条 mmc 请求一次 `WARN_ON` → 复位到上游位置；`init/main.c` 合并留下的双份
  early-entropy 块（`boot_init_stack_canary()` 跑两遍）→ 折叠；TZ PRNG 播种路径
  在 .325 下的两处冻结（`dmac_inv_range` / `add_hwgenerator_randomness` 节流）→
  修好并恢复完整 harvest；TCP `sock->sk` 数据竞争标注与原型；perf
  `PERF_FORMAT_LOST` 系列（`read()` 由 32 B 变 40 B 可测）
- **Root**：KernelSU 32651（backslashxx fork，已同步上游 staging）以 LKM 运行，管理器一键修补升级
- **Docker**：iptables 全套 + xt_addrtype（docker0 网络初始化规则依赖）已齐
- **零依赖（v1.5）**：WiFi（2022 世代 qcacld 三仓）、音频全栈（Lenovo machine
  驱动源码化重建 + tfa98xx codec）、rmnet 加速、gspca 全部内建；功放固件烤入；
  WiFi 开机由内核自触发（约 15 秒内自动连网）。`lsmod` 仅有 KernelSU 一行。

**Fixes over stock**: speaker audio chain, camera video recording, fingerprint
power rail, charge-protection feedback loop (36/s kernel vote storm → 0),
boot-time "internal problem" dialog (VINTF kernel-version/config match),
panel wake, suspend, double-tap-to-wake, USB-C OTG auto-host,
KernelSU-as-LKM decoupling.
Full patch history in
`tb371fc/scripts/` (p1–p371, 350 个入库).


---

## 怎么用 / How to use

> 前提：Bootloader 已解锁（`fastboot flashing unlock`）。

**一枚内核、两种 root 形态，各一个镜像即完成全部安装（v1.5 起的形态，v2.0 沿用）。**

1. 从 [Release v2.0](https://github.com/smith-dog/kernel-tb371fc/releases/tag/v2.0)
   下载下列其一——两个镜像内嵌的 Image **字节相同**（同一枚 `#312`），只差 ramdisk 里
   有没有 KernelSU：
   - [`boot-v2.0-kspatched-flash.img`](https://github.com/smith-dog/kernel-tb371fc/releases/download/v2.0/boot-v2.0-kspatched-flash.img)
     （md5 `b7f5b9284a14c8faabb0a6f6c7d379f6`）——KernelSU LKM 已内置，直刷即有 root
   - [`boot-v2.0-pure-flash.img`](https://github.com/smith-dog/kernel-tb371fc/releases/download/v2.0/boot-v2.0-pure-flash.img)
     （md5 `9ec79818758d2b7a753e8dea381b64ab`）——纯净版：原厂 v27n89 ramdisk，
     不含 KernelSU/ksud（给 APatch 等自带 root 方案，或不需要 root 的用法）
2. 刷入并重启（`fastboot` 走 USB，刷前核对 `fastboot getvar current-slot`）：
   ```
   adb reboot bootloader
   fastboot flash boot boot-v2.0-kspatched-flash.img   # 或 boot-v2.0-pure-flash.img
   fastboot reboot
   ```
3. 完成。开机后 `/proc/version` 应为 `Linux version 4.19.325-perf++ ... #312`。
   WiFi 开机约 15 秒内由内核自动触发连网，扬声器/录音/触摸/144Hz 开箱即用。

**⚠️ 刷过旧载荷包（tb371fc-dlkm-pkg-fixed.tar.gz，v1.4 及更早教程装过）的，
刷完新内核后先跑一遍清理脚本**——旧载荷包在新内核上已无用，且每次开机
仍会尝试加载 7 个失效模块。下载 Release v1.5 中的
[`tb371fc-payload-cleanup.sh`](https://github.com/smith-dog/kernel-tb371fc/releases/download/v1.5/tb371fc-payload-cleanup.sh)：

```
adb push tb371fc-payload-cleanup.sh /data/local/tmp/
adb shell "su -c 'sh /data/local/tmp/tb371fc-payload-cleanup.sh'"
```

脚本只做移动（全部文件进 `/sdcard/Download/tb371fc-payload-removed-<时间>/`，
可随时移回），不删除任何东西；在旧内核上运行会被拒绝（旧内核仍依赖载荷包）。
清理完成后重启即可。

**日后升级 KernelSU**：直接在管理器内升级；或换刷新版 kspatched 镜像，内核无需重刷。

<details>
<summary>备选方法（要换 root 方案 / 想自己打镜像：用本仓库 tools 重打包）</summary>

v2.0 除 kspatched 镜像外也直接发布纯净镜像 `boot-v2.0-pure-flash.img`（同一枚 `#312`
Image + 原厂 v27n89 无 KSU ramdisk）。若要把它打进你自己的基底镜像（换 ramdisk /
dtbo 组合），不必依赖任何旧版纯净包：

1. 按下面「自己编译」构建出 `arch/arm64/boot/Image`（或直接取 Release 内任一镜像
   拆出的 Image，两者字节相同），
2. 用 [`tb371fc/tools/repack_boot.py`](tb371fc/tools/repack_boot.py) 保留 v2 头 /
   ramdisk / DTB 尾打进你的基底镜像（原厂 `dtb_size` quirk 已在工具内修正），
3. `fastboot flash boot <out.img>` + `fastboot reboot`。

   只想复刻本次出货的那枚 pure 镜像，直接跑
   [`tb371fc/scripts/repack-pure-boot.sh`](tb371fc/scripts/repack-pure-boot.sh)
   `<纯基底.img> <Image> <out.img>`——它先断言基底 ramdisk 内没有 `kernelsu.ko`，
   再从**输出字节**读回内嵌 Image 的 md5 与 ramdisk 清单（Release v2.0 的 pure 资产即由它产出）。

</details>

---

## 自己编译 / Build from source

环境：WSL2 Ubuntu + **Ubuntu clang 21.1.8** + aarch64-linux-gnu binutils 2.46。
出货 v2.0 的编译横幅即为此组合（`Linux version 4.19.325-perf++ (smith@PC)
(Ubuntu clang version 21.1.8 (6ubuntu1), GNU ld (GNU Binutils for Ubuntu) 2.46) #312`）；
v1.9 及更早用 Snapdragon LLVM 10.0.7，改用发行版 clang 后 `-perf+` 后缀仍与原厂一致。

内核配置：`cp tb371fc/config-v2.0.txt .config`（= v2.0 出货配置，与设备
`/proc/config.gz` 排序后逐行相等）。dev 分支常驻的诊断插桩（netconsole、
kmsg 记录仪、hung/softlockup detector 等）按发布惯例在发布件里关闭，
清单见 `tb371fc/config-dev-fragment.txt`。

```bash
KV=$(pwd)
FLAGS="ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=clang CLANG_TRIPLE=aarch64-linux-gnu- AS=aarch64-linux-gnu-as"
KC="KCFLAGS=-Wno-error -Wno-error=strict-prototypes -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -Wno-error=date-time -include $KV/drivers/staging/fw-api/fw/p226_compat.h"
# 内核镜像（DYNAMIC_SINGLE_CHIP 决定 qcacld 取 qca6390 变体）
make -j6 $FLAGS $KC DYNAMIC_SINGLE_CHIP=qca6390 Image
# 模块链 + KernelSU LKM（同一棵树同一份 .config，别混用旧产物）
make -j6 $FLAGS $KC DYNAMIC_SINGLE_CHIP=qca6390 modules
make -j6 $FLAGS $KC DYNAMIC_SINGLE_CHIP=qca6390 M=drivers/kernelsu modules
# 打包 boot 镜像（保留 v2 头、ramdisk、DTB 尾；ramdisk 里 kernelsu.ko 换成上面编出的）
python3 tb371fc/tools/repack_boot.py <base.img> arch/arm64/boot/Image out/boot.img "" <ramdisk-new.gz>
```

**发布件自检**（这几条是本仓库构建脚本的门禁，自己编完值得跑一遍）：
- `strings arch/arm64/boot/Image | grep -iE "t58|hidg ep0|recorder thread started"` 必须为空（诊断件确实没跟进来）；
- `llvm-nm drivers/mmc/core/core.o | grep "U mmc_retune_hold"` 必须命中——这句在厂商原树里被一个
  **无 Kconfig 条目**的 `#ifdef` 守卫吃掉，编没编进去只有对象级检查看得出来；
- `grep -E "^CONFIG_VSERVICES_SERIAL=" .config` 应为 `=y`：v1.9 起出厂开启的 vservices 串口驱动曾被
  .325 合并连带删掉 Kconfig 条目（源码与 `obj-` 行都在，所以零报错零警告地消失），v2.0 已恢复。

**v1.5 构建注意**：WiFi 驱动为 2022 世代 qcacld 三仓（`drivers/staging/` 下
qcacld-3.0/qca-wifi-host-cmn/fw-api，体积原因不入 git），完整重建请用
`tb371fc/scripts/p224-build.sh`（自动换装+补丁+构建+打包 boot 镜像），
外部源检出说明见脚本头注释。

LKM 注意：`CONFIG_KSU=m` 时 ksu.ko 需要本树 `drivers/ksu_sym.c`
（48 个非公开符号的 EXPORT 垫片，已内建在树中）。`tb371fc/tools/` 内含
repack/insmod128/dtbo 等工具与源码（[tools 目录](https://github.com/smith-dog/kernel-tb371fc/tree/main/tb371fc/tools)）。

techpack 说明：display/audio/camera/video 四个驱动目录的源码已全部入库
（与出货内核一致，相机 KMD 内建；v1.5 起音频栈整体 =y 内建），仅构建产物
（*.o/*.a/*.cmd 等）被忽略。

---

## 源代码来自哪里 / Provenance

| 组成 | 来源 |
|---|---|
| 内核基线 | [lss4/android_kernel_lenovo_paladin](https://github.com/lss4/android_kernel_lenovo_paladin)（分支 11）——社区开发者整理开源的联想官方 GPL 包（TB-Q706F/Z，代号 paladin，4.19.157 与 TB371FC stock 同版本，含联想板级代码） |
| stable 采纳（v2.0） | [gregkh/linux](https://github.com/gregkh/linux) 分支 `linux-4.19.y`：本树把 **v4.19.157 → v4.19.325** 的上游内容整体合并进 `main`（168 个 stable 发布）。仓库内 tag `v4.19.157` / `v4.19.325` 指向上游**原提交**（Greg KH 署名，非本地合成快照），完整祖先链随本仓库发布 |
| 显示栈 | CodeLinaro [msm-4.19 @ LA.UM.9.12.r1-18500-SMxx50.QSSI14.0](https://git.codelinaro.org/clo/la/kernel/msm-4.19/-/tree/LA.UM.9.12.r1-18500-SMxx50.QSSI14.0)（vanilla techpack/display；双击唤醒通知钩子 p140/p174 位于 dsi_display.c） |
| 音频核心栈 | 同上 CLO tag 的 [techpack/audio](https://git.codelinaro.org/clo/la/kernel/msm-4.19/-/tree/LA.UM.9.12.r1-18500-SMxx50.QSSI14.0/techpack/audio)（v1.4 编为 dlkm 模块；v1.5 起整体 =y 内建） |
| 相机 KMD | 同上 CLO tag 的 [techpack/camera](https://git.codelinaro.org/clo/la/kernel/msm-4.19/-/tree/LA.UM.9.12.r1-18500-SMxx50.QSSI14.0/techpack/camera)（SPECTRA_CAMERA=y，内建） |
| 视频硬解 | [MiCode/Xiaomi_Kernel_OpenSource](https://github.com/MiCode/Xiaomi_Kernel_OpenSource) kona 分支的 msm_vidc（compatible 完全匹配） |
| USB 相机 | 内核主线 drivers/media/usb/gspca（v1.5 由模块转内建） |
| 触摸/背光驱动 | [tem423/android_kernel_lenovo_tb371fc](https://github.com/tem423/android_kernel_lenovo_tb371fc)（TB371FC 社区内核；本树合入其 nt36532 SPI 触摸驱动与 ktz8866a/b 双芯片背光驱动） |
| WiFi 驱动 | 2022 世代 qcacld 三仓（驱动 v5.2.0.190I）：qcacld-3.0 与 [arter97-mirror/caf_qca-wifi-host-cmn](https://github.com/arter97-mirror/caf_qca-wifi-host-cmn)（2022-09-29 检出）、fw-api 取 [sonyxperiadev](https://github.com/sonyxperiadev) 镜像 324cb3d（2022-10-07，与设备固件 WLAN.HST.1.0.1.r1-01596 同日）；v1.5 内建 + 内核侧 boot_wlan 自触发（p267） |
| 功放 codec | [InfiniR_kernel_alioth](https://github.com/raystef66/InfiniR_kernel_alioth) 的 tfa98xx codec（适配 4.19 与本机 DT，v1.5 内建） |
| 音频 machine | 本树 kona.c 按联想 `audio_machine_kona.ko` 逆向重建（ELF 表级对齐，v1.5 内建） |
| 功放固件 | 联想原厂 `tfa98xx_QS.cnt`（提取自 stock /vendor/firmware），经 CONFIG_EXTRA_FIRMWARE 烤入内核（v1.5） |
| rmnet 加速 | [MiCode/vendor_qcom_opensource_data-kernel](https://github.com/MiCode/vendor_qcom_opensource_data-kernel) alioth-r-oss 分支 drivers/rmnet/{perf,shs}（v1.5 内建） |
| KernelSU | [backslashxx/KernelSU](https://github.com/backslashxx/KernelSU) staging 同步（驱动 32651；管理器 APK = 本项目 fork 构建：[smith-dog/KernelSU](https://github.com/smith-dog/KernelSU) master，含 boot v1/v2 修补支持 1099b137） |
| 本项目 | p1~p371 补丁（[tb371fc/scripts](https://github.com/smith-dog/kernel-tb371fc/tree/main/tb371fc/scripts)），全部以上述来源为基础 |

联想未随 GPL dump 公开的部分（如 144Hz 显示驱动、部分面板参数）不在本树，
对应功能保持原厂形态。

## 📖 详细构建与 Root 流程

见 [docs/NOTE-build-and-root.md](docs/NOTE-build-and-root.md)——含管理器修补
分步操作、root 自举原理（为什么不能直刷纯净内核）、开机模块自动化部署、
以及全部踩坑表。

## License

GPL-2.0（继承内核及联想 GPL 发布）。
