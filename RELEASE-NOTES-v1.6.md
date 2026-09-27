# v1.6 — 冻结即回收 (Freeze → zram)

## 新增 / What's new
- **冻结即回收**:后台应用冻结后 1 秒内整堆匿名页压入 zram。无条件常开,无需任何配置或钩子
- **修复历史挂死根因**:冻结回收误用 work 队列(普通 work_struct + queue_delayed_work 导致越界定时器)——"打开应用卡死 / 循环重启 / 无故挂死"全部根治
- **治理"游戏一开、后台全灭"**:/proc/vmstat 对 pgscan 计数缩放输出,ZUI 内存清理器(ZuiMemoryCleaner)的激进判定不再误触发;极端内存下保底阀保留
- swappiness=150 / extra_free_kbytes=300000 烤入内核默认
- 诊断三件套(netconsole 模块 / hung_task / softlockup 检测)配置就绪但注释关闭——调试需求见 dev 分支

## 刷机 / Flash
```
fastboot flash boot boot-v1.6-kspatched-flash.img
```
- 需已解锁 BL;root = KernelSU 管理器(32652)配对
- 回退:fastboot 刷回 v1.5 镜像即可

English: background apps are frozen and fully pushed into zram within 1 second
(unconditional, no config). Root cause of the historical freeze-reclaim panics
and reboot loops fixed. Game-launch background massacre tamed via /proc/vmstat
pgscan scaling. Diagnostics off by default — see the dev branch.
