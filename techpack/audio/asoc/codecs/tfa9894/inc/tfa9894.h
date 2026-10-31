/*
 * tfa9894.h - TB371FC four-speaker TFA9894 rotation engine (clean-room).
 *
 * This unit owns the parts of the TFA9894 codec behaviour that are specific
 * to the TB371FC four-speaker layout: the rotation selector state, the
 * selector -> TDM slot table, and the DSP slot-switch sequence.  It was
 * reimplemented from the disassembly-derived behaviour spec for the stock
 * audio_tfa98xx.ko module (see notes/NOTES-tb371fc-tfa9894-new-driver-rea-spec.md),
 * not copied from the vendor/NXP source.
 *
 * It depends only on the generic TFA service layer (tfa_set_bf/tfa_get_bf,
 * tfa98xx_set_mute, tfa98xx_powerdown).
 */

#ifndef __TFA9894_H__
#define __TFA9894_H__

#include "tfa_device.h"

/* Rotation selectors written by the audio HAL: 0=0deg 1=270deg 2=180deg 3=90deg
 * (the HAL's angle->selector mapping lives in audio.primary.kona.so; the kernel
 * only consumes the selector value).
 */
#define TFA9894_CHAN_SEL_MIN		0
#define TFA9894_CHAN_SEL_MAX		3
#define TFA9894_CHAN_SEL_UNSET		(-1)

/*
 * Stored selector for the device set.  -1 means "not chosen yet"; the
 * tfa_dev_start hook then leaves the container default in place.
 */
int tfa9894_get_store_chan_sel(void);
void tfa9894_set_store_chan_sel(int sel);

/*
 * Look up the (TDMSPKS, TDMDCS) slot pair for one device under selector
 * @sel.  @dev_idx is the container device index.  Returns 0 on success and
 * -EINVAL when the selector or device index is out of range.
 */
int tfa9894_get_target_chan_sel(int sel, int dev_idx, int *spks, int *dcs);

/*
 * Write a slot pair to one amplifier: TDME off, set TDMSPKS/TDMDCS, TDME on.
 * This is the register-only form used on the cold-start path.
 */
int tfa9894_change_chan_sel(struct tfa_device *tfa, int spks, int dcs);

/*
 * Apply selector @sel to one amplifier with the full no-click sequence
 * (volume fade out, mute, power down, retarget slots, power up, unmute,
 * volume fade in).  @name is only used for diagnostics.  Returns 0 when the
 * amplifier already sits on the target slots.
 */
int tfa9894_send_chan_sel_to_dsp(struct tfa_device *tfa, const char *name, int sel);

#endif /* __TFA9894_H__ */
