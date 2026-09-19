import io

p = r'D:\work\code-work\checkpoints\TASK-010-20260915-1301.md'
add = """
## 🔒 终局定稿（2026-09-16 03:1x，全部日志通道穷尽）
- logdump 分区实测**全零**：联想 ABL 不初始化 SMEM minidump 表 → QC minidump
  postmortem 通道不存在。至此全部日志通道物理性穷尽：
  1. pstore/ramoops：能写入但读取需要能启动的 pstore 内核（不存在）
  2. QC minidump postmortem：ABL 不初始化表（logdump 实测全零）
  3. /proc/kcore、/dev/mem：stock 内核未编译
  4. UART：消费级平板不暴露
  5. init 级文件转储：内核在 init 前死亡
- **终审判定：TB371FC 自定义内核路线在"无日志"与"无板级源码"的双重死锁下，
  以现有资源不可行。** 唯一解锁条件：Lenovo 公开 4.19.157 源码（gplcc 路径）。
- 三源交叉实验、全部工具链与修复集、原厂镜像（boot/dtbo/firmware 4.4GB）均已
  归档于本项目目录，源码公开之日即可一日复用完成移植。
- 设备终态：APatch 备份运行 ✓（4.19.157-perf+ / ZUI 16.0.474 / root ✓）；
  dtbo_a = DFPS-off 版（无实际影响，可还原：fastboot flash dtbo 固件包内原厂 dtbo.img）

## （原始背景与过程记录，供追溯）"""

s = io.open(p, encoding='utf-8').read()
s = s.replace('## （原始背景与过程记录，供追溯）', add, 1)
io.open(p, 'w', encoding='utf-8').write(s)
print('final verdict archived')
