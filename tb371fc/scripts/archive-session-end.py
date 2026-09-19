import io

p = r'D:\work\code-work\checkpoints\TASK-010-20260915-1301.md'
add = """
## 📌 会话收尾状态（2026-09-16 03:1x，上下文极限，交接用）

### 当前确切状态
- 设备：0.13.3→0.13.8 应用回退/更新实验后，运行正常（boot_a = 0.13.8 重新打包的
  内核镜像，uname 4.19.157-perf+ ✓，系统正常 ✓，KPM 文件已在
  /data/adb/ap/kpm/tb371fc-ramoops/ 与 tb371fc-dump/ 两处）
- 我们的 none-elf 构建 tb371fc-ramoops.kpm（5672B）与 tb371fc-dump.kpm（6800B）
  在 0.13.8 首次开机加载时 **rc=-8（ENOEXEC）** —— 尚未定位确切原因
- KP-main 头文件构建的 KPM 在 0.13.8 加载器下 **rc=-8 持续存在**
- 0.13.3 应用回退后 **rc=-2（unknown symbol: __stack_chk_guard/fail）持续存在**

### 已确认的事实
- KP 加载器源码（module.c）的 -ENOEXEC 检查点：
  1. SHN_COMMON 且名为 __gnu_lto*（line 216）
  2. rewrite_section_headers 的文件边界（line 302）
  3. find_sec(".kpm.init"/".kpm.exit") 失败（line 372/379）
- 我们的 none-elf KPM 的节表**完整**（objdump 实测 .kpm.init/exit/info/ctl0 全在）
- 我们的 linux-gnu KPM 的节表**也完整**
- 两者都在 0.13.8 加载器下 rc=-8 → **加载器的节查找/校验逻辑与我们构建的
  ELF 存在更深层的不兼容**，不是简单的节缺失

### 下一步精确动作（新会话执行）
1. 用 **0.13.3 应用对应的 KP 源码版本**（不是 main！）重新构建 KPM：
   - 0.13.3 应用的 KP 版本 = 查 APatch 0.13.3 的 github release 说明
   - 或：git clone KernelPatch 后 checkout 到 0.13.3 发布对应的 commit/tag
   - 用那棵树的 kernel/include 重编 tb371fc-ramoops
2. 或者：**反方向**——把设备 KP 更新到 main 最新版（与我们的头文件匹配）：
   - 需要 kptools + kpimg（KP release 资产）+ 原厂 boot.img 重新打补丁
   - fastboot flash boot 新补丁镜像 → KPM 加载应该 rc=0
3. **注意**：0.13.8 应用的 KPM 存储路径可能已变（0.13.3 的
   /data/adb/ap/kpm/<name>/ 结构在新版下可能不同），部署前先确认
4. 所有脚本已文件化在 scripts/ 目录，构建/部署/检查的完整链路可重放

### 关键教训（本次会话新增）
- KP 加载器的 -ENOEXEC = **节查找失败**（不是 ELF 格式问题）——
  我们的模块节表完整仍被拒 → 加载器的 find_sec 与我们的节名/布局存在
  版本级差异 → 需要用与设备 KP **完全同版本** 的头文件构建
"""

s = io.open(p, encoding='utf-8').read()
s = s.replace('## （原始背景与过程记录，供追溯）', add + '\n\n## （原始背景与过程记录，供追溯）')
io.open(p, 'w', encoding='utf-8').write(s)
print('session-end state archived')
