#!/usr/bin/env python3
"""p115 — make stock vendor kernel modules loadable (WiFi QCA6390 + audio_*.ko).

FACTS (2026-09-18 on-device, n45):
- lsmod EMPTY: all 44 /vendor/lib/modules/*.ko fail to load. WiFi dead
  (qca_cld3_qca6390.ko), audio dead (audio_*.ko) share this single root cause.
- insmod wlan -> "Required key not available"; kernel log:
  "PKCS#7 signature not signed with a trusted key". Our kernel has
  CONFIG_MODULE_SIG(_FORCE)=y but was built with an ephemeral key -> the
  Lenovo-signed stock modules are untrusted.
- Module vermagic "4.19.157-perf+ SMP preempt mod_unload modversions aarch64"
  vs our kernel "4.19.157-perf SMP preempt mod_unload aarch64": missing "+" and
  missing "modversions" -> vermagic mismatch as well.
- WiFi HW itself is fine: cnss brings regulators/GPIO/PCIe up, MHI reaches
  MISSION_MODE, firmware loads. Only the host driver module fails.

Fix (kernel side, keep stock modules untouched):
- CONFIG_MODULE_SIG=n / CONFIG_MODULE_SIG_FORCE=n  (accept Lenovo-signed mods)
- CONFIG_MODVERSIONS=y            (vermagic token + CRC checks; paladin tree is
                                   Lenovo's own source so CRCs should match)
- CONFIG_LOCALVERSION="-perf+"    (UTS_RELEASE = "4.19.157-perf+" exact match)
Security note: module signature enforcement is dropped; on this KSU-rooted
personal device the marginal risk is accepted (documented).
"""
import re

P = "/home/smith/android_kernel_lenovo_paladin/.config"

src = open(P).read()

def set_line(text, key, value):
    line = f"{key}={value}" if value is not None else f"# {key} is not set"
    pat = re.compile(rf"^{re.escape(key)}=.*$|^# {re.escape(key)} is not set$",
                     re.M)
    assert pat.search(text), f"key absent: {key}"
    return pat.sub(line.replace("\\", "\\\\"), text, count=1)

src = set_line(src, "CONFIG_MODULE_SIG", None)          # off
src = set_line(src, "CONFIG_MODULE_SIG_FORCE", None)    # off
src = set_line(src, "CONFIG_MODVERSIONS", "y")
src = set_line(src, "CONFIG_LOCALVERSION", '"-perf+"')

open(P, "w").write(src)
print("p115: module-compat config applied "
      "(SIG off, MODVERSIONS=y, LOCALVERSION=-perf+)")
