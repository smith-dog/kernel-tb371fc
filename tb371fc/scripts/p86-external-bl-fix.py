#!/usr/bin/env python3
"""p86 — wire the EXTERNAL backlight type to the ktz8866 brightness setter.

TB371FC's active panel (spinel tianma nt36532) uses bl_ctrl_external
(DSI_BACKLIGHT_EXTERNAL), but the case in dsi_panel_set_backlight() is an
empty stub in the public tree: brightness never reaches the ktz8866a I2C
backlight IC (panel0-backlight actual_brightness stuck at 0, wake stays
black). Stock routes it to lcd_bl_set_led_brightness() (exported by
drivers/video/backlight/ktz8866a.c, panel bl range 0..2047 == the IC's
11-bit brightness register pair, so pass through 1:1).
"""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c"
NL = chr(10)
T = chr(9)

OLD = (
    T + "case DSI_BACKLIGHT_EXTERNAL:" + NL +
    T + T + "break;"
)
NEW = (
    T + "case DSI_BACKLIGHT_EXTERNAL:" + NL +
    T + "{" + NL +
    T + T + "extern int lcd_bl_set_led_brightness(int value);" + NL +
    T + T + "/* TB371FC: bl_ctrl_external panel (nt36532 + ktz8866a i2c BL" + NL +
    T + T + " * IC). bl_lvl 0..2047 maps 1:1 onto the IC's 11-bit" + NL +
    T + T + " * brightness register pair. */" + NL +
    T + T + "rc = lcd_bl_set_led_brightness((int)bl_lvl);" + NL +
    T + T + "if (rc)" + NL +
    T + T + T + "DSI_ERR(\"lcd_bl_set_led_brightness failed, rc=%d\\n\", rc);" + NL +
    T + T + "break;" + NL +
    T + "}"
)

src = open(P).read()
if "lcd_bl_set_led_brightness" in src:
    raise SystemExit("already patched")
assert src.count(OLD) == 1, "EXTERNAL case anchor not found"
open(P, "w").write(src.replace(OLD, NEW))
print("patched")
