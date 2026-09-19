P = '/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_panel.c'
src = open(P).read()
old = '\t\t\tpr_info("P99: DCS 0x0A read rc=%d\\n", p99_rc);\n\t\t}\n\n\t\t}\n\t}\n'
new = '\t\t\tpr_info("P99: DCS 0x0A read rc=%d\\n", p99_rc);\n\t\t}\n\t}\n'
n = src.count(old)
print('count', n)
if n == 1:
    open(P, 'w').write(src.replace(old, new))
    print('fixed')
