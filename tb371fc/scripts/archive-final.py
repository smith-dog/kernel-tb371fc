import io

p = r'D:\work\code-work\checkpoints\TASK-010-20260915-1301.md'
add = """
## 锁定终局实验（2026-09-16 00:0x，决定性闭环）
- reader-v8（dd 直写 logdump，零文件系统依赖版）实测一轮：
  设备卡第一屏 → 用户按键进 fastboot → 恢复备份 → 读 logdump：**全零**
- LOS 20 内核从未执行 /init（连 mknod+dd 都没跑到）——内核空间内死亡，
  与用户空间/ramdisk/文件系统完全无关
- 证据矩阵终版：三内核源 × 两 ramdisk × 两 dtbo（原厂/DFPS-off）× dd 直写 =
  全部 init 前失败；Lenovo 私有二进制 = 完美启动
- **路线终判：无 Lenovo 板级源码 = 不可行（确定性，无剩余假设）**
- 设备最终状态：APatch 备份运行中（已验证）；dtbo_a = DFPS-off 版（stock 内核下
  无不良影响，仅禁用动态帧率；如需还原：fastboot flash dtbo 官方 fw 内原厂 dtbo.img）

## 最终结论补充（2026-09-15 23:1x，logdump 实验）
- vanilla 157 内核在 pstore 初始化之前 panic 循环（logdump 全零 + ABL 兜底 fastboot）
- 所有日志通道物理失效：无串口 / stock 无 pstore / kcore 与 devmem 未编译 /
  ramoops 自导出因无法启动读取内核而死锁

## （原始背景与过程记录，供追溯）"""

s = io.open(p, encoding='utf-8').read()
s = s.replace('## （原始背景与过程记录，供追溯）', add, 1)
io.open(p, 'w', encoding='utf-8').write(s)
print('checkpoint finalized')
