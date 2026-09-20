#!/usr/bin/env python3
# p152 (TASK-022): strip the sleep-in (0x10) entry from the panel OFF
# command set. With the wake gesture permanently armed (p133), the panel
# must NOT enter sleep mode at blank: waking the DDIC from sleep-in within
# seconds of blank fails (panel black with backlight) - only wakes after
# the panel state settles work. Display-off (0x28) alone keeps the DDIC in
# normal mode with dark pixels; the gesture wake (0x29 + video) is then
# instant and reliable.
import os, shutil, sys

KV = "/home/smith/android_kernel_lenovo_paladin"
path = os.path.join(KV, "techpack/display/msm/dsi/dsi_panel.c")
BK = "/tmp/p152-backup"
os.makedirs(BK, exist_ok=True)
dst = os.path.join(BK, "dsi_panel.c.orig")
if not os.path.exists(dst):
    shutil.copy2(path, dst)
    print("BACKUP -> %s" % dst)

with open(path, newline="") as f:
    c = f.read()

anchor = """	for (i = DSI_CMD_SET_PRE_ON; i < DSI_CMD_SET_MAX; i++) {
		set = &priv_info->cmd_sets[i];
		set->type = i;
		set->count = 0;
"""
assert c.count(anchor) == 1, "parse loop anchor: %d" % c.count(anchor)

# insert the strip pass right after the parse loop completes (before rc = 0)
old_tail = """	}

	rc = 0;
	return rc;
}

static int dsi_panel_parse_reset_sequence(struct dsi_panel *panel)"""
new_tail = """	}

	/* TB371FC p152: strip the sleep-in (0x10) entry from the OFF set.
	 * With the wake gesture permanently armed (nt36532 touch, p133),
	 * the DDIC must stay out of sleep at blank - waking it from
	 * sleep-in within seconds of blank fails (panel black with
	 * backlight). Display-off (0x28) alone keeps it wake-ready. */
	{
		struct dsi_panel_cmd_set *off_set =
			&priv_info->cmd_sets[DSI_CMD_SET_OFF];
		u32 j, kept = 0;
		for (j = 0; j < off_set->count; j++) {
			struct dsi_cmd_desc *cmd = &off_set->cmds[j];
			const u8 *tx = cmd->msg.tx_buf;
			if (tx && tx[0] == 0x10 && cmd->msg.tx_len >= 1)
				continue;
			if (kept != j)
				off_set->cmds[kept] = *cmd;
			kept++;
		}
		if (kept != off_set->count) {
			DSI_INFO("TB371FC p152: OFF set %u -> %u cmds (sleep-in stripped)\\n",
				 off_set->count, kept);
			off_set->count = kept;
		}
	}

	rc = 0;
	return rc;
}

static int dsi_panel_parse_reset_sequence(struct dsi_panel *panel)"""
assert c.count(old_tail) == 1, "tail anchor: %d" % c.count(old_tail)
c = c.replace(old_tail, new_tail)

with open(path, "w", newline="") as f:
    f.write(c)
print("P152_DONE")
