import io

p = r'D:\work\code-work\checkpoints\TASK-010-20260915-1301.md'
add = """
## ⚡ v9 实验结果与因果隔离（2026-09-16 00:4x）
- v9（vanilla 157 + 显示栈 + **QCOM_MINIDUMP=y** + 60s 取证 panic + KDMSG 注册）：
  刷入后 **10 秒内即回 fastboot**（远快于 v7 的"卡 logo 冻结"）→ 头号新嫌疑 =
  **QCOM_MINIDUMP 驱动早期崩溃**（c25 的 minidump 代码假设 QC 原版 ABL 初始化了
  SMEM minidump 表；联想 ABL 大概率没有 → 早期空指针 panic）
- 因果隔离实验待做：v9 去掉 QCOM_MINIDUMP 重编 → 若回到"卡 logo 冻结"则确认
- **用户决策已锁定：自定义内核路线不改**（已入档）
- pstore 持续记录特性确认：v7+ 的 ramoops console 实时记录 printk（非仅 panic 时）——
  冻结时环形缓冲里就有完整日志，唯一问题是导出

## 🧰 突破口方案（已定型，下一工作包）：APatch KPM 物理内存读取器
原理链：v9-无minidump（或 v7）挂死时日志实时写入 ramoops 区（0x27e00000+2MB，
已被 DTB no-map 保护）→ 暖重启不丢 → 刷入"APatch 内核 + no-map DTB"正常启动
（stock 内核永不触碰该区）→ APatch 加载自制 KPM → KPM 在内核态 ioremap 该区
→ 写出 /data/local/tmp/ramoops.bin → 完整内核日志到手
- 需开发：APatch KPM（~100 行 C：ioremap + filp_open/kernel_write），
  参考 github.com/bmax121/KernelPatch 的 KPM API 与示例；先搜索现成 KPM dumper
- DTB 手术能力已有（tools/ + dtc），APatch 镜像尾部可注入 no-map 保留节点
- 该方案同时是"无串口设备取早期内核日志"的通用方法

## （原始背景与过程记录，供追溯）"""

s = io.open(p, encoding='utf-8').read()
s = s.replace('## （原始背景与过程记录，供追溯）', add, 1)
io.open(p, 'w', encoding='utf-8').write(s)
print('checkpoint updated')
