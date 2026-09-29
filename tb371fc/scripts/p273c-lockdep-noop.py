#!/usr/bin/env python3
# p273c — TASK-044: 本树 lockdep.h !CONFIG_LOCKDEP 分支缺 lockdep_assert/lockdep_is_held
# (vendor 修剪),本地补上游语义的兜底定义;#ifndef 保证与 CONFIG_LOCKDEP=y 共存。
import sys, io

SRC = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc/drivers/misc/ntsync.c"

with io.open(SRC, "r", encoding="utf-8") as f:
    s = f.read()

old = "#include <uapi/linux/ntsync.h>\n\n#define NTSYNC_NAME\t\"ntsync\"\n"
new = ("#include <uapi/linux/ntsync.h>\n"
       "\n"
       "#define NTSYNC_NAME\t\"ntsync\"\n"
       "\n"
       "/*\n"
       " * 4.19 backport: this tree's lockdep.h (vendor 4.19) defines neither\n"
       " * lockdep_assert() nor a !CONFIG_LOCKDEP fallback for lockdep_is_held().\n"
       " * Provide the upstream semantics locally; the #ifndef guards keep them\n"
       " * compatible if CONFIG_LOCKDEP is ever enabled.\n"
       " */\n"
       "#ifndef lockdep_assert\n"
       "#define lockdep_assert(c) do { } while (0)\n"
       "#endif\n"
       "\n"
       "#ifndef lockdep_is_held\n"
       "#define lockdep_is_held(lock) (1)\n"
       "#endif\n")

n = s.count(old)
if n != 1:
    print("FATAL: anchor x%d" % n)
    sys.exit(1)
s = s.replace(old, new)

with io.open(SRC, "w", encoding="utf-8") as f:
    f.write(s)
print("OK: p273c applied")
