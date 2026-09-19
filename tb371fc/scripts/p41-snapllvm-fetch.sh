#!/bin/bash
# p41 v3 — 从用户手动下载的 zip 部署 Snapdragon LLVM ARM Compiler 10.0.7
# 输入: D:\work\code-work\project\tb371fc-kernel\Snapdragon-LLVM-ARM-Compiler-10.0.7-for-Android-NDK-main.zip
# 输出: /home/smith/snapdragon-llvm-10.0.7  (横幅校验通过)
# 运行: wsl -u root -e bash <本文件>
exec > /mnt/d/work/code-work/project/tb371fc-kernel/logs/p41-snapllvm.log 2>&1
echo "=== P41 v3 START $(date) ==="
SRCZIP="/mnt/d/work/code-work/project/tb371fc-kernel/Snapdragon-LLVM-ARM-Compiler-10.0.7-for-Android-NDK-main.zip"
ZIP=/home/smith/snapllvm-main.zip
DST=/home/smith/snapdragon-llvm-10.0.7

[ -f "$SRCZIP" ] || { echo SRC_ZIP_MISSING; exit 1; }
echo "--- source zip: $(stat -c %s "$SRCZIP") bytes ---"

if [ ! -x $DST/bin/clang ]; then
  [ -f $ZIP ] || cp "$SRCZIP" $ZIP || { echo COPY_FAIL; exit 1; }
  echo "--- zip in WSL: $(stat -c %s $ZIP) bytes, verifying integrity ---"
  python3 - "$ZIP" <<'PYEOF'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
bad = z.testzip()
print("zip integrity:", "BAD member: "+bad if bad else "OK")
print("entries:", len(z.namelist()))
sys.exit(1 if bad else 0)
PYEOF
  [ $? -eq 0 ] || { echo ZIP_CORRUPT; exit 1; }

  if ! which unzip >/dev/null 2>&1; then
    apt-get install -y unzip >/dev/null 2>&1 && echo "unzip installed" || echo "apt unzip failed, python fallback"
  fi
  rm -rf $DST
  if which unzip >/dev/null 2>&1; then
    unzip -q -o $ZIP -d /home/smith/ || { echo UNZIP_FAIL; exit 1; }
  else
    python3 -m zipfile -e $ZIP /home/smith/ || { echo UNZIP_FAIL_PY; exit 1; }
  fi
  # zip 根目录 = Snapdragon-LLVM-ARM-Compiler-10.0.7-for-Android-NDK-main/
  ROOT=$(find /home/smith -maxdepth 1 -type d -name "Snapdragon-LLVM-ARM-Compiler-*-main" | head -1)
  [ -n "$ROOT" ] || { echo NO_ROOT_DIR; exit 1; }
  mv "$ROOT" $DST
  chmod -R a+rx $DST/bin $DST/libexec $DST/aarch64-linux-android/bin 2>/dev/null
fi
[ -x $DST/bin/clang ] || { echo NO_CLANG_BIN; exit 1; }

echo "=== clang banner (期望逐字含: clang version 10.0.7 for Android NDK) ==="
$DST/bin/clang --version || { echo CLANG_RUN_FAIL; exit 1; }
$DST/bin/clang --version | grep -q "clang version 10.0.7 for Android NDK" \
  || { echo WRONG_VERSION; exit 1; }

echo "=== bundle layout ==="
ls $DST | head -12
echo "--- aarch64 target binutils ---"
ls $DST/aarch64-linux-android/bin/ 2>/dev/null | head -24
LD=$DST/aarch64-linux-android/bin/aarch64-linux-android-ld
[ -x $LD ] || LD=$(ls $DST/aarch64-linux-android/bin/*ld 2>/dev/null | head -1)
[ -n "$LD" ] && $LD --version | head -2 || echo NO_TARGET_LD
echo "--- clang resource headers ---"
ls -d $DST/lib/clang/*/include 2>/dev/null || ls $DST/lib | head
echo "=== P41 DONE $(date) ==="
