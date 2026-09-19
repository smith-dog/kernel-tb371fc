#!/usr/bin/env python3
# p124-usb-online-vbus.py
# 修复充电保护反馈死循环：smblib_get_prop_usb_online() 在 USER_VOTER 投票为 0
# （= input_suspend 挂起）时把 usb ONLINE 报告为 false，导致 Lenovo 电池 HAL
# 读到"充电器离线"而撤销挂起，挂起又让 ONLINE 变 false —— 30ms 自反馈环
# （省电模式疯狂跳变、电池状态闪切、ICL 投票 36 次/s、负载 ~22）。
# 修复：ONLINE 报告 VBUS 物理在场（present 位，与挂起状态解耦）。
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/smith/android_kernel_lenovo_paladin/drivers/power/supply/qcom/smb5-lib.c'

src = open(PATH).read()

old = '''	if (get_client_vote_locked(chg->usb_icl_votable, USER_VOTER) == 0) {
		val->intval = false;
		return rc;
	}

	if (is_client_vote_enabled_locked(chg->usb_icl_votable,
					CHG_TERMINATION_VOTER)) {
		rc = smblib_get_prop_usb_present(chg, val);
		return rc;
	}

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "POWER_PATH_STATUS = 0x%02x\\n",
		   stat);

	val->intval = (stat & USE_USBIN_BIT) &&
		      (stat & VALID_INPUT_POWER_SOURCE_STS_BIT);
	return rc;
}'''

new = '''	/* p124: report VBUS presence for ONLINE. Tying ONLINE to the
	 * input-suspend vote made the Lenovo battery HAL see "charger
	 * gone" during a suspend hold, cancel the suspend, see the
	 * charger again, re-suspend ... in a ~30ms feedback loop
	 * (ICL vote storm + load ~22 + status/saver flapping). */
	return smblib_get_prop_usb_present(chg, val);
}'''

# The target function is smblib_get_prop_usb_online; locate it precisely.
fn = 'int smblib_get_prop_usb_online(struct smb_charger *chg,'
fi = src.find(fn)
if fi < 0:
    print('FUNCTION NOT FOUND'); sys.exit(1)
body_start = src.find('\n', src.find('{', fi))
if old not in src[fi:fi+4000]:
    print('OLD BLOCK NOT FOUND within usb_online — aborting'); sys.exit(1)
src = src[:fi] + src[fi:fi+4000].replace(old, new, 1) + src[fi+4000:]
open(PATH, 'w').write(src)
print('p124 applied: usb ONLINE now reports VBUS presence')
