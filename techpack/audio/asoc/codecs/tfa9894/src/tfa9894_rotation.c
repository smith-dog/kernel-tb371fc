/*
 * tfa9894_rotation.c - TB371FC four-speaker rotation engine (clean-room).
 *
 * Reimplemented from the behaviour spec extracted from the stock
 * audio_tfa98xx.ko (TASK-072 REA).  The selector->slot table below was read
 * byte-for-byte from the stock module's .rodata+0x4b8; the switch sequence
 * mirrors the stock register flow.  See
 * notes/NOTES-tb371fc-tfa9894-new-driver-rea-spec.md.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>

#include "tfa_device.h"
#include "tfa_service.h"
#include "tfa98xx_tfafieldnames.h"
#include "tfa9894.h"

/*
 * selector -> per-device (TDMSPKS, TDMDCS) slot pair.
 * Transcribed from stock audio_tfa98xx.ko .rodata+0x4b8 (16 pairs,
 * indexed [selector][dev_idx]).  Do not "tidy" these values: they encode
 * the physical speaker layout and had been wrong once already.
 */
static const int tfa9894_chan_sel_map[4][4][2] = {
	{   /* selector 0: 0 deg */
		{ 0, 1 }, { 0, 1 }, { 2, 3 }, { 2, 3 },
	},
	{   /* selector 1: 90 deg */
		{ 1, 0 }, { 2, 3 }, { 0, 1 }, { 3, 2 },
	},
	{   /* selector 2: 180 deg */
		{ 3, 2 }, { 3, 2 }, { 1, 0 }, { 1, 0 },
	},
	{   /* selector 3: 270 deg */
		{ 2, 3 }, { 1, 0 }, { 3, 2 }, { 0, 1 },
	},
};

/* Volume step table used to ramp the CF firmware volume in and out. */
static const int tfa9894_vol_table[] = {
	0x00, 0x24, 0x48, 0x6c, 0x90, 0xb4, 0xd8, 0xff,
};

static int g_tfa9894_chan_sel = TFA9894_CHAN_SEL_UNSET;

int tfa9894_get_store_chan_sel(void)
{
	return g_tfa9894_chan_sel;
}

void tfa9894_set_store_chan_sel(int sel)
{
	g_tfa9894_chan_sel = sel;
}

int tfa9894_get_target_chan_sel(int sel, int dev_idx, int *spks, int *dcs)
{
	if ((sel < TFA9894_CHAN_SEL_MIN) || (sel > TFA9894_CHAN_SEL_MAX) ||
	    (dev_idx < 0) || (dev_idx > 3))
		return 2;

	*spks = tfa9894_chan_sel_map[sel][dev_idx][0];
	*dcs = tfa9894_chan_sel_map[sel][dev_idx][1];
	return 0;
}

int tfa9894_change_chan_sel(struct tfa_device *tfa, int spks, int dcs)
{
	int err;

	err = tfa_set_bf(tfa, TFA9894_BF_TDME, 0);
	if (err < 0)
		goto out;
	err = tfa_set_bf(tfa, TFA9894_BF_TDMSPKS, (uint16_t)spks);
	if (err < 0)
		goto out;
	err = tfa_set_bf(tfa, TFA9894_BF_TDMDCS, (uint16_t)dcs);
	if (err < 0)
		goto out;
	err = tfa_set_bf(tfa, TFA9894_BF_TDME, 1);
out:
	pr_debug("tfa9894: %s spks=%d dcs=%d ret=%d\n", __func__, spks, dcs, err);
	return err;
}

int tfa9894_send_chan_sel_to_dsp(struct tfa_device *tfa, const char *name, int sel)
{
	int spks, dcs;
	int cur_spks, cur_dcs;
	int err, rc, i;

	rc = tfa9894_get_target_chan_sel(sel, tfa->dev_idx, &spks, &dcs);
	if (rc)
		return rc;

	/* Already on target: nothing to do (stock short-circuits here too). */
	cur_spks = tfa_get_bf(tfa, TFA9894_BF_TDMSPKS);
	cur_dcs = tfa_get_bf(tfa, TFA9894_BF_TDMDCS);
	if ((cur_spks == spks) && (cur_dcs == dcs))
		return 0;


	/* Fade the firmware volume down before disturbing the routing. */
	for (i = 0; i < ARRAY_SIZE(tfa9894_vol_table); i++) {
		err = tfa_set_bf(tfa, TFA9894_BF_VOL,
				 (uint16_t)tfa9894_vol_table[i]);
		if (err < 0)
			return err;
		msleep(1);
	}

	err = tfa98xx_set_mute(tfa, Tfa98xx_Mute_Amplifier);
	if (err != Tfa98xx_Error_Ok)
		return err;

	err = tfa98xx_powerdown(tfa, 1);
	if (err != Tfa98xx_Error_Ok)
		return err;

	rc = tfa9894_change_chan_sel(tfa, spks, dcs);

	err = tfa98xx_powerdown(tfa, 0);
	if ((err == Tfa98xx_Error_Ok) && (rc == 0))
		err = tfa98xx_set_mute(tfa, Tfa98xx_Mute_Off);
	else if (err == Tfa98xx_Error_Ok)
		err = rc;
	if (err != Tfa98xx_Error_Ok)
		return err;

	/* Fade the firmware volume back up. */
	for (i = ARRAY_SIZE(tfa9894_vol_table) - 1; i >= 0; i--) {
		err = tfa_set_bf(tfa, TFA9894_BF_VOL,
				 (uint16_t)tfa9894_vol_table[i]);
		if (err < 0)
			return err;
		msleep(1);
	}

	cur_spks = tfa_get_bf(tfa, TFA9894_BF_TDMSPKS);
	cur_dcs = tfa_get_bf(tfa, TFA9894_BF_TDMDCS);
	pr_debug("tfa9894: %s readback spks=%d dcs=%d vol=%d\n",
		 __func__, cur_spks, cur_dcs,
		 tfa_get_bf(tfa, TFA9894_BF_VOL));
	return 0;
}
