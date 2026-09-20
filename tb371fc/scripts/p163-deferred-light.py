#!/usr/bin/env python3
# p163 (TASK-022): the deferred post-wake work must not touch the IC.
# Data (n78): the 400ms deferred resume ran nvt_ts_resume = bootloader reset
# + full fw reflash (188ms) - landing right after the panel lit; on this
# in-cell panel the touch controller lives in the display DDIC, so the reset
# glitched the freshly-lit panel (black with backlight). The IC already
# exited gesture mode and rebooted cleanly in the gesture IRQ work (p160).
# The deferred work now only confirms the fw state, restores the runtime
# config and marks the touch awake - zero IC disturbance.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "drivers/input/touchscreen/nt36532/nt36xxx.c")
BK = "/tmp/p163-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "nt36xxx.c.p161.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

old = """static void nvt_ts_deferred_resume_work(struct work_struct *work)
{
	struct nvt_ts_data *ts =
		container_of(work, struct nvt_ts_data, resume_work.work);

	NVT_LOG("deferred resume start\\n");
	nvt_ts_resume(&ts->client->dev);
}"""
new = """static void nvt_ts_deferred_resume_work(struct work_struct *work)
{
	struct nvt_ts_data *ts =
		container_of(work, struct nvt_ts_data, resume_work.work);

	/* TB371FC p163: NO reset and NO reflash here - the gesture IRQ work
	 * already reset the IC out of gesture mode (p160) and any IC reset
	 * at this point glitches the freshly-lit panel (black with
	 * backlight, TASK-022). Confirm fw state, restore runtime config,
	 * mark the touch awake. */
	NVT_LOG("deferred resume start\\n");
	nvt_check_fw_reset_state(RESET_STATE_REK);
	mutex_lock(&ts->lock);
	bTouchIsAwake = 1;
	mutex_unlock(&ts->lock);
#if NVT_CUST_PROC_CMD
	queue_work(nvt_fwu_wq, &ts_restore_cmd_work);
#endif
	NVT_LOG("deferred resume end\\n");
}"""
assert c.count(old) == 1, "deferred work anchor: %d" % c.count(old)
c = c.replace(old, new)

with open(path, "w", newline="") as f:
    f.write(c)
print("P163_DONE")
