# Release v1.8 — NTsync 内核驱动 + 日志洪水根治

基于 v1.7（#196）的全部功能之上：

## 新增

- **NTsync 内核驱动**（TASK-044）：主线 Linux ntsync（Windows NT 同步原语模拟）
  移植到 4.19。CONFIG_NTSYNC=y 内建，/dev/ntsync 设备节点默认 root-only——
  配合 Winlator/Ludashi 等容器与开启 WINENTSYNC=1 的 Wine/Proton 构建，
  多线程同步可走内核原生 NT 原语（devCheck 实测 Vulkan/游戏进程 ntsync fd 全链生效）。
  默认不开 app 权限，不影响日常安全姿态；需要时配 KSU 模块放行。

## 修复

- **日志洪水根治**（TASK-046 附属）：
  - ZUI vendor HAL 每秒数十条 avc 拒绝刷屏（droidspacesd 网络操作叠加），
    每天写满数百 MB 日志 —— cmdline `audit=0` 源头关闭（SELinux 保持 Enforcing，
    仅关闭记录器；安全拦截行为不变）
  - healthd/QCOM-BATT/binder 心跳循环经 /dev/kmsg 无过滤读通道回灌 ——
    cmdline `printk.devkmsg=off` 关闭
  - 实测：内核日志 ~220,000 行/分钟 → **58 行/分钟**
- **TASK-039 取证插桩泄漏清理**：v1.7 携带的 fzr/fzanon/fzrpl 内核打印
  （冻结回收调试面包屑）全部移除（ddbe98ad）

## 说明

- ntsync 权限模块（KSU）与 Ludashi 汉化版等用户态组件不在本发布范围
- 升级方式：fastboot flash boot boot-v1.8-kspatched-flash.img（与 v1.7 相同流程）
- 回退：刷回 v1.7 镜像即可，无数据迁移

## 校验

- boot-v1.8-kspatched-flash.img 见 Release 附件，内核 #200
