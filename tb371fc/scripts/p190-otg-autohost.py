#!/usr/bin/env python3
# p190-otg-autohost.py — v27n89: remove Lenovo's otg_state gate in
# policy_engine.c start_usb_host() so a Type-C sink attach (keyboard)
# auto-starts host mode via usbpd -> EXTCON_USB_HOST -> dwc3-msm.
# Reversible: restore from .bak-p190.
import sys, shutil

KV = "/home/smith/kernels/tb371fc/android_kernel_lenovo_tb371fc"
F = KV + "/drivers/usb/pd/policy_engine.c"
BAK = F + ".bak-p190"

OLD = """	int ret = 0;

	if (0 != otg_state)
		return;

	val.intval = (cc == ORIENTATION_CC2);"""
NEW = """	int ret = 0;

	/* TB371FC: drop Lenovo's otg_state gate so sink attach auto-hosts */
	val.intval = (cc == ORIENTATION_CC2);"""

src = open(F, encoding="utf-8").read()
if NEW.strip().splitlines()[2] in src and OLD not in src:
    print("ALREADY_PATCHED")
    sys.exit(0)
if OLD not in src:
    print("ANCHOR_NOT_FOUND")
    sys.exit(1)
shutil.copy2(F, BAK)
open(F, "w", encoding="utf-8", newline="").write(src.replace(OLD, NEW, 1))
print("PATCHED_OK")
