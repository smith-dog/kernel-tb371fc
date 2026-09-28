# v1.7 — 官方充电保护回归 + 触控增强 + 手写笔三代

## 新增 / What's new
- **官方充电子系统恢复**(CONFIG_SPINEL_CHARGER):40-60% 充电保护与电池养护(浮充电压按循环次数分档)——自社区恢复的官方实现移植。**修复养护模式下 60% 附近的充停抖动**:停充不再挂起输入,电池静置(0A)、适配器直供系统,微循环消失
- **触控拉频默认开启**:触控瞬间小核 1.32GHz / 大核 1.80GHz 低保底 60ms,并联动 sched_boost
- **触控 PSY 事件过滤放宽**:接受 STATUS/PRESENT/ONLINE 事件(社区验证的充电器抖动修复)
- **pen_store 加固**:修复栈未初始化与输入校验缺失(官方 Spinel 原码 bug)
- **手写笔三代(Precision Pen 3)开箱直用**:probe 默认启用新笔协议;配合 persist 属性与 KSU 模块 pen_unlock v1.0 三层固化;蓝牙全功能经 lenovo_pen_status=1
- 诊断三件套保持注释关闭——调试需求见 dev 分支

## 刷机 / Flash
```
fastboot flash boot boot-v1.7-kspatched-flash.img
```
- 需已解锁 BL;root = KernelSU 管理器(32652)配对
- 可选:pen-unlock-v1.0.zip(KSU 管理器安装,手写笔解锁自愈)
- 回退:fastboot 刷回 v1.6 镜像即可

English: official Lenovo charging subsystem restored (40-60% charge
protection + cycle-adaptive float-voltage maintenance), fixing the 60%
charge-suspend jitter (battery now rests while the adapter powers the
system). Touch input boost armed by default. Precision Pen 3 works out
of the box. Diagnostics off by default — see the dev branch.
