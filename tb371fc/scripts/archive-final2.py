import io

p = r'D:\work\code-work\checkpoints\TASK-010-20260915-1301.md'
add = """
## 🔒 v8-reader 实测定稿（2026-09-16 00:4x → 01:2x）
- DFPS-off dtbo + reader-v8（LOS 20 + dd 直写 logdump 迷你 ramdisk）实测：
  仍卡第一屏、无任何传输通道、logdump 逻辑上必然全零（mini-init 未执行）
- **LOS 20 内核对 mini-ramdisk（纯 busybox，无 Android 复杂性）同样 init 前挂死**
  —— ramdisk/用户空间变量彻底排除，LOS kona 内核 = 内核空间内早期死亡，实锤
- DFPS-off dtbo 实验：reader-v7（vanilla 157 + mini-ramdisk）+ DFPS-off = mini-init
  执行成功（自导航 fastboot 实测）——**vanilla 157 + DFPS-off dtbo = 内核能跑到 init**，
  但其 ZUI ramdisk 版（v11）卡第一屏 —— 卡点 = ZUI ramdisk/用户空间阶段（待日志定界）
- 日志读取的唯一剩余通道：**APatch KPM 物理内存读取模块**
  （libtersafe/KPM-MemReader 已验证 KPM 可读物理内存；改造为 ioremap 直读
  0x27e00000+2MB ramoops 区并落盘）—— 开发量 ~1-3 小时，无实验不确定性
- 设备状态：APatch 备份运行中 ✓（uname 实测）；dtbo_a = DFPS-off 版（无实际影响，可还原）

## （原始背景与过程记录，供追溯）"""

s = io.open(p, encoding='utf-8').read()
s = s.replace('## （原始背景与过程记录，供追溯）', add, 1)
io.open(p, 'w', encoding='utf-8').write(s)
print('checkpoint updated')
