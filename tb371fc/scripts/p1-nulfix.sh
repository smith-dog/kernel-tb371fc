#!/bin/bash
# find and restore NUL-contaminated source files, then verify
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p1-nulfix.log 2>&1
KDIR=/home/smith/android_kernel_xiaomi_sm8250
cd $KDIR || exit 1
git config --global --add safe.directory $KDIR
echo "=== NUL-containing tracked files (working tree) ==="
find . -path ./.git -prune -o -type f -print0 | xargs -0 grep -lP '\x00' 2>/dev/null | head -20
echo "=== restore them from HEAD ==="
find . -path ./.git -prune -o -type f -print0 | xargs -0 grep -lP '\x00' 2>/dev/null > /tmp/nufiles.txt
wc -l /tmp/nufiles.txt
xargs -a /tmp/nufiles.txt git checkout -- 2>&1 | head -3
echo "=== verify HEAD blobs are clean ==="
while read f; do printf "%s : %s\n" "$f" "$(git show HEAD:$f 2>/dev/null | grep -cP '\x00')"; done < /tmp/nufiles.txt | head -20
echo "=== remaining NUL files after restore ==="
find . -path ./.git -prune -o -type f -print0 | xargs -0 grep -lP '\x00' 2>/dev/null | wc -l
echo NULFIX_DONE
