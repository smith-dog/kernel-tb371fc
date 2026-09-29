#!/usr/bin/env python3
# p273 — TASK-044: ntsync 驱动 6.14+→4.19 移植适配 + Kconfig/Makefile/defconfig 接线
# 源文件已从 ~/t44/linux(master a243ede71846, 与 v7.2.8 逐字节一致)拷入目标树。
# 本脚本做 9 处适配,全部字符串精确匹配并断言命中次数,不匹配即中止。
import sys, io

KV = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc"

def patch(path, subs):
    with io.open(path, "r", encoding="utf-8") as f:
        s = f.read()
    for old, new, expect in subs:
        n = s.count(old)
        if n != expect:
            print("FATAL: %s: pattern x%d (expect %d):\n---\n%s\n---" % (path, n, expect, old))
            sys.exit(1)
        s = s.replace(old, new)
    with io.open(path, "w", encoding="utf-8") as f:
        f.write(s)
    print("OK: %s (%d subs)" % (path, len(subs)))

SRC = KV + "/drivers/misc/ntsync.c"

# ---------- 1. 源码适配 ----------
src_subs = [
    # (a) 删 time_namespace include(4.19 无此头,timens 分支一并移除)
    ("#include <linux/time_namespace.h>\n", "", 1),
    # (h-include) compat.h 供 compat_ptr 使用
    ("#include <linux/atomic.h>\n",
     "#include <linux/atomic.h>\n#include <linux/compat.h>\n", 1),
    # (b) kzalloc_obj(obj)
    ("\tobj = kzalloc_obj(*obj);",
     "\tobj = kzalloc(sizeof(*obj), GFP_KERNEL);", 1),
    # (c) FD_PREPARE → 经典 fd 三段式
    ("static int ntsync_obj_get_fd(struct ntsync_obj *obj)\n"
     "{\n"
     "\tFD_PREPARE(fdf, O_CLOEXEC,\n"
     "\t\t   anon_inode_getfile(\"ntsync\", &ntsync_obj_fops, obj, O_RDWR));\n"
     "\tif (fdf.err)\n"
     "\t\treturn fdf.err;\n"
     "\tobj->file = fd_prepare_file(fdf);\n"
     "\treturn fd_publish(fdf);\n"
     "}\n",
     "static int ntsync_obj_get_fd(struct ntsync_obj *obj)\n"
     "{\n"
     "\tstruct file *file;\n"
     "\tint fd;\n"
     "\n"
     "\tfd = get_unused_fd_flags(O_CLOEXEC);\n"
     "\tif (fd < 0)\n"
     "\t\treturn fd;\n"
     "\n"
     "\tfile = anon_inode_getfile(\"ntsync\", &ntsync_obj_fops, obj, O_RDWR);\n"
     "\tif (IS_ERR(file)) {\n"
     "\t\tput_unused_fd(fd);\n"
     "\t\treturn PTR_ERR(file);\n"
     "\t}\n"
     "\n"
     "\tobj->file = file;\n"
     "\tfd_install(fd, file);\n"
     "\treturn fd;\n"
     "}\n", 1),
    # (d) timens 换算 → 恒等直通(4.19 无 time namespace,monotonic 即主机时钟)
    ("\tif (args->flags & NTSYNC_WAIT_REALTIME)\n"
     "\t\tclock = CLOCK_REALTIME;\n"
     "\telse\n"
     "\t\ttimeout = timens_ktime_to_host(clock, timeout);\n",
     "\tif (args->flags & NTSYNC_WAIT_REALTIME)\n"
     "\t\tclock = CLOCK_REALTIME;\n", 1),
    # (e) array_size → 开写乘法(count 上界由下方 sizeof(fds) 校验兜住,64bit 无溢出)
    ("\tsize_t size = array_size(count, sizeof(fds[0]));",
     "\tsize_t size = (size_t)count * sizeof(fds[0]);", 1),
    # (f) kmalloc_flex → 开写尾数组分配(total_count ≤ NTSYNC_MAX_WAIT_COUNT+1)
    ("\tq = kmalloc_flex(*q, entries, total_count);",
     "\tq = kzalloc(sizeof(*q) + total_count * sizeof(q->entries[0]), GFP_KERNEL);", 1),
    # (g) kzalloc_obj(dev)
    ("\tdev = kzalloc_obj(*dev);",
     "\tdev = kzalloc(sizeof(*dev), GFP_KERNEL);", 1),
    # (h) compat_ptr_ioctl(5.0 才有)→ 本地等价封装,插在 obj fops 前
    ("static const struct file_operations ntsync_obj_fops = {",
     "#ifdef CONFIG_COMPAT\n"
     "static long ntsync_compat_ioctl(struct file *file, unsigned int cmd,\n"
     "\t\t\t\tunsigned long arg)\n"
     "{\n"
     "\tif (!file->f_op->unlocked_ioctl)\n"
     "\t\treturn -ENOIOCTLCMD;\n"
     "\treturn file->f_op->unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));\n"
     "}\n"
     "#else\n"
     "#define ntsync_compat_ioctl NULL\n"
     "#endif\n"
     "\n"
     "static const struct file_operations ntsync_obj_fops = {", 1),
    (".compat_ioctl\t= compat_ptr_ioctl,",
     ".compat_ioctl\t= ntsync_compat_ioctl,", 2),
    # (i) module_misc_device(4.19 无此宏)→ 显式 init/exit
    ("module_misc_device(ntsync_misc);\n",
     "static int __init ntsync_init(void)\n"
     "{\n"
     "\treturn misc_register(&ntsync_misc);\n"
     "}\n"
     "\n"
     "static void __exit ntsync_exit(void)\n"
     "{\n"
     "\tmisc_deregister(&ntsync_misc);\n"
     "}\n"
     "\n"
     "module_init(ntsync_init);\n"
     "module_exit(ntsync_exit);\n", 1),
]
patch(SRC, src_subs)

# ---------- 2. Kconfig 接线 ----------
KCFG = KV + "/drivers/misc/Kconfig"
NTSYNC_KCONFIG = (
    "config NTSYNC\n"
    "\ttristate \"NT synchronization primitive emulation\"\n"
    "\thelp\n"
    "\t  This module provides kernel support for emulation of Windows NT\n"
    "\t  synchronization primitives. It is not a hardware driver.\n"
    "\n"
    "\t  To compile this driver as a module, choose M here: the\n"
    "\t  module will be called ntsync.\n"
    "\n"
    "\t  If unsure, say N.\n"
)
with io.open(KCFG, "r", encoding="utf-8") as f:
    k = f.read()
if "config NTSYNC" in k:
    print("SKIP: Kconfig already has NTSYNC")
else:
    if not k.endswith("\n"):
        k += "\n"
    with io.open(KCFG, "w", encoding="utf-8") as f:
        f.write(k + NTSYNC_KCONFIG)
    print("OK: Kconfig wired")

# ---------- 3. Makefile 接线 ----------
MKF = KV + "/drivers/misc/Makefile"
with io.open(MKF, "r", encoding="utf-8") as f:
    m = f.read()
if "ntsync" in m:
    print("SKIP: Makefile already has ntsync")
else:
    if not m.endswith("\n"):
        m += "\n"
    with io.open(MKF, "w", encoding="utf-8") as f:
        f.write(m + "obj-$(CONFIG_NTSYNC)\t\t+= ntsync.o\n")
    print("OK: Makefile wired")

# ---------- 4. defconfig ----------
DF = KV + "/arch/arm64/configs/defconfig"
with io.open(DF, "r", encoding="utf-8") as f:
    d = f.read()
if "CONFIG_NTSYNC" in d:
    print("SKIP: defconfig already has NTSYNC")
else:
    if not d.endswith("\n"):
        d += "\n"
    with io.open(DF, "w", encoding="utf-8") as f:
        f.write(d +
                "# p273: TASK-044 - ntsync (Windows NT sync primitive emulation, Wine/Proton userspace)\n"
                "CONFIG_NTSYNC=y\n")
    print("OK: defconfig wired")

print("=== P273 PORT DONE ===")
