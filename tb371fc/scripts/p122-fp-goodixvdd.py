#!/usr/bin/env python3
# p122-fp-goodixvdd.py
# 修复指纹：驱动 gf_parse_dts 请求的 "fp-gpio-envdd" GPIO 在 DT 中不存在，
# 导致 open(/dev/goodix_fp) 直接 -EINVAL，HAL daemon 永远起不来。
# DT 实际提供的是 goodix_vdd-supply（PMIC 轨）。本补丁让驱动改用该电源轨。
# 台账：TASK-010 追加26 / 对话 2026-09-19
import sys, re

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/drivers/input/fingerprint/platform.c'

src = open(PATH).read()

old_block = '''	printk("gf get vdden_gpio[%d] from dt", gf_dev->vdden_gpio);
	gf_dev->vdden_gpio = of_get_named_gpio(np,"fp-gpio-envdd",0);
	if(gf_dev->vdden_gpio < 0){
	pr_err("failed to get vdden gpio!\\n");
	return gf_dev->vdden_gpio;
	}
	printk("gf get vdden_gpio1111[%d] from dt", gf_dev->vdden_gpio);
	rc = devm_gpio_request(dev, gf_dev->vdden_gpio, "goodix_fpenvdd");
	if(rc){
	pr_err("failed to request vdden gpio, rc = %d\\n", rc);
		goto err_vdden;
	}
	gpio_direction_output(gf_dev->vdden_gpio,1);'''

new_block = '''	/* p122: DT (soc/goodix_fp) has no fp-gpio-envdd GPIO; the sensor is
	 * powered through the goodix_vdd-supply PMIC rail instead. */
	gf_dev->vdd_supply = devm_regulator_get(dev, "goodix_vdd");
	if (IS_ERR(gf_dev->vdd_supply)) {
		pr_err("failed to get goodix_vdd regulator!\\n");
		return PTR_ERR(gf_dev->vdd_supply);
	}
	rc = regulator_enable(gf_dev->vdd_supply);
	if (rc) {
		pr_err("failed to enable goodix_vdd, rc = %d\\n", rc);
		return rc;
	}
	pr_info("p122: goodix_vdd enabled\\n");'''

if old_block not in src:
    print('OLD BLOCK NOT FOUND — aborting'); sys.exit(1)
src = src.replace(old_block, new_block)

# fix error-path labels: vdden gpio no longer requested
old_err = '''err_irq:
	devm_gpio_free(dev, gf_dev->reset_gpio);
err_reset:
        devm_gpio_free(dev, gf_dev->vdden_gpio);
err_vdden:	
	return rc;'''
new_err = '''err_irq:
	devm_gpio_free(dev, gf_dev->reset_gpio);
	return rc;'''
if old_err not in src:
    print('ERROR-PATH BLOCK NOT FOUND — aborting'); sys.exit(1)
src = src.replace(old_err, new_err)

open(PATH, 'w').write(src)
print('p122 applied to', PATH)

# p122b fixup: irq failure path label (err_reset label removed with the vdden block)
import io
p2 = PATH
s2 = io.open(p2).read()
io.open(p2,'w').write(s2.replace('goto err_reset;','goto err_irq;'))
print('p122b applied')
