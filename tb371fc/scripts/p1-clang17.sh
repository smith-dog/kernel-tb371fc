#!/bin/bash
# P1: install a 4.19-capable clang (prefer 17; fallback to repo versions)
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-clang17c.log 2>&1
echo START
rm -f /etc/apt/sources.list.d/llvm17.list
. /etc/os-release
DIST=$VERSION_CODENAME
echo "codename=$DIST"
KEY=/etc/apt/trusted.gpg.d/llvm.asc
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor --yes -o "$KEY" && echo gpg-ok
echo "deb [signed-by=$KEY] http://apt.llvm.org/$DIST/ llvm-toolchain-$DIST-17 main" > /etc/apt/sources.list.d/llvm17.list
apt-get update 2>&1 | grep -E "^E:" | head -5
apt-get install -y clang-17 lld-17 2>&1 | tail -3
if ! command -v clang-17 >/dev/null; then
  echo "llvm.org route failed; trying ubuntu repo versions"
  rm -f /etc/apt/sources.list.d/llvm17.list
  apt-get update 2>&1 | tail -1
  apt-cache search -n '^clang-1[0-9]$' | sort
  for v in 18 16 15; do
    apt-get install -y clang-$v lld-$v 2>&1 | tail -1
    command -v clang-$v && break
  done
fi
echo ===
command -v clang-17 && clang-17 --version | head -1
echo SCRIPT_DONE
