#!/usr/bin/env python3
"""p86 v2 — wire EXTERNAL backlight type to ktz8866 (first stub only)."""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
NL = chr(10)
T = chr(9)
Q = chr(34)      # double quote
BS = chr(92)     # backslash
SQ = chr(39)     # single quote

OLD = (T + "case DSI_BACKLIGHT_EXTERNAL:" + NL + T + T + "break;")

NEW = (
    T + "case DSI_BACKLIGHT_EXTERNAL:" + NL +
    T + "{" + NL +
    T + T + "extern int lcd_bl_set_led_brightness(int value);" + NL +
    T + T + "/* TB371FC: bl_ctrl_external panel (nt36532 + ktz8866a i2c" + NL +
    T + T + " * backlight IC). bl_lvl 0..2047 maps 1:1 onto the IC" + NL +
    T + T + " * 11-bit brightness register pair. */" + NL +
    T + T + "rc = lcd_bl_set_led_brightness((int)bl_lvl);" + NL +
    T + T + "if (rc)" + NL +
    T + T + T + "DSI_ERR(" + Q + "lcd_bl_set_led_brightness failed, rc=%d" + BS + "n" + Q + ", rc);" + NL +
    T + T + "break;" + NL +
    T + "}"
)

src = open(P).read()
assert SQ not in NEW or True
if "lcd_bl_set_led_brightness" in src:
    raise SystemExit("already patched")
cnt = src.count(OLD)
assert cnt >= 1, "anchor missing"
src = src.replace(OLD, NEW, 1)
open(P, "w").write(src)
print("patched (first of", cnt, "stubs)")
