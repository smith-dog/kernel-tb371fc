// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "lenovo_jeita: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_batterydata.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/pmic-voter.h>
#include "lenovo-jeita.h"
#include "smb5-lib.h"

#define JEITA_VOTER		"LENOVO_JEITA_VOTER"
#define BATT_MAINTAINCE_VOTER		"BATT_MAINTAINCE_VOTER"

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

#define JEITA_STEP 8
struct lenovo_jeita_param {
	u32			psy_prop;
	char			*prop_name;
	int			rise_hys;
	int			fall_hys;
};

struct range_data {
	int low_threshold;
	int high_threshold;
	u32 value;
};

struct jeita_fcc_cfg {
	struct lenovo_jeita_param	param;
	struct range_data		fcc_cfg[JEITA_STEP];
	struct range_data		hvdcp3_fcc_cfg[JEITA_STEP];
	struct range_data		default_fcc_cfg[JEITA_STEP];
};

struct jeita_fv_cfg {
	struct lenovo_jeita_param	param;
	struct range_data		fv_cfg[JEITA_STEP];
};
struct lenovo_jeita_info {
	struct device		*dev;
	ktime_t			jeita_last_update_time;
	bool			config_is_read;
	bool			sw_jeita_cfg_valid;
	bool			batt_missing;
	bool			taper_fcc;
	int			jeita_fcc_index;
	int			jeita_fv_index;
	int			get_config_retry_count;
	int			batt_maintaince_fv;
	int			recharge_voltage;
	struct jeita_fcc_cfg	*jeita_fcc_config;
	struct jeita_fv_cfg	*jeita_fv_config;
	struct wakeup_source	*lenovo_jeita_ws;

	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*dc_suspend_votable;
	struct votable		*chg_disable_votable;
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct delayed_work	status_change_work;
	struct delayed_work	fcc_work;
	struct delayed_work	get_config_work;
	struct notifier_block	nb;
};

static struct lenovo_jeita_info *the_chip;

#define STEP_CHG_HYSTERISIS_DELAY_US		5000000 /* 5 secs */

#define BATT_HOT_DECIDEGREE_MAX			600
#define GET_CONFIG_DELAY_MS		2000
#define GET_CONFIG_RETRY_COUNT		50
#define WAIT_BATT_ID_READY_MS		200

static bool is_batt_available(struct lenovo_jeita_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

static bool is_bms_available(struct lenovo_jeita_info *chip)
{
	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (!chip->bms_psy)
		return false;

	return true;
}

static bool is_input_present(struct lenovo_jeita_info *chip)
{
	int rc = 0, input_present = 0;
	union power_supply_propval pval = {0, };

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (chip->usb_psy) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0)
			pr_err("Couldn't read USB Present status, rc=%d\n", rc);
		else
			input_present |= pval.intval;
	}

	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");
	if (chip->dc_psy) {
		rc = power_supply_get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_PRESENT, &pval);
		if (rc < 0)
			pr_err("Couldn't read DC Present status, rc=%d\n", rc);
		else
			input_present |= pval.intval;
	}

	if (input_present)
		return true;

	return false;
}

static int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct range_data *ranges,
		int max_threshold, u32 max_value)
{
	int rc = 0, i, length, per_tuple_length, tuples;

	if (!node || !prop_str || !ranges) {
		pr_err("Invalid parameters passed\n");
		return -EINVAL;
	}

	rc = of_property_count_elems_of_size(node, prop_str, sizeof(u32));
	if (rc < 0) {
		pr_err("Count %s failed, rc=%d\n", prop_str, rc);
		return rc;
	}

	length = rc;

	per_tuple_length = sizeof(struct range_data) / sizeof(u32);

	if (length % per_tuple_length) {
		pr_err("%s length (%d) should be multiple of %d\n",
				prop_str, length, per_tuple_length);
		return -EINVAL;
	}
	tuples = length / per_tuple_length;
	if (tuples > JEITA_STEP) {
		pr_err("too many entries(%d), only %d allowed\n",
				tuples, JEITA_STEP);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_str,
			(u32 *)ranges, length);
	if (rc) {
		pr_err("Read %s failed, rc=%d\n", prop_str, rc);
		return rc;
	}

	for (i = 0; i < tuples; i++) {
		if (ranges[i].low_threshold >
				ranges[i].high_threshold) {
			pr_err("%s thresholds should be in ascendant ranges\n",
						prop_str);
			rc = -EINVAL;
			goto clean;
		}

		if (i != 0) {
			if (ranges[i - 1].high_threshold >
					ranges[i].low_threshold) {
				pr_err("%s thresholds should be in ascendant ranges\n",
							prop_str);
				rc = -EINVAL;
				goto clean;
			}
		}

		if (ranges[i].low_threshold > max_threshold)
			ranges[i].low_threshold = max_threshold;
		if (ranges[i].high_threshold > max_threshold)
			ranges[i].high_threshold = max_threshold;
		if (ranges[i].value > max_value)
			ranges[i].value = max_value;
	}

	return rc;
clean:
	memset(ranges, 0, tuples * sizeof(struct range_data));
	return rc;
}

static int get_jeita_setting_from_profile(struct lenovo_jeita_info *chip)
{
	struct device_node *batt_node, *profile_node;
	u32 max_fv_uv, max_fcc_ma;
	const char *batt_type_str;
	const __be32 *handle;
	int batt_id_ohms, rc, hysteresis[2] = {0};
	union power_supply_propval prop = {0, };

	handle = of_get_property(chip->dev->of_node,
			"qcom,battery-data", NULL);
	if (!handle) {
		pr_debug("ignore getting sw-jeita/step charging settings from profile\n");
		return 0;
	}

	batt_node = of_find_node_by_phandle(be32_to_cpup(handle));
	if (!batt_node) {
		pr_err("Get battery data node failed\n");
		return -EINVAL;
	}
	
	if (!is_bms_available(chip))
		return -ENODEV;
	
	power_supply_get_property(chip->bms_psy,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &prop);
	batt_id_ohms = prop.intval;

	/* bms_psy has not yet read the batt_id */
	if (batt_id_ohms < 0)
		return -EBUSY;

	profile_node = of_batterydata_get_best_profile(batt_node,
					batt_id_ohms / 1000, NULL);
	if (IS_ERR(profile_node))
		return PTR_ERR(profile_node);

	if (!profile_node) {
		pr_err("Couldn't find profile\n");
		return -ENODATA;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
					&batt_type_str);
	if (rc < 0) {
		pr_err("battery type unavailable, rc:%d\n", rc);
		return rc;
	}
	pr_err("battery: %s detected, getting sw-jeita settings\n",
					batt_type_str);

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
					&max_fv_uv);
	if (rc < 0) {
		pr_err("max-voltage_uv reading failed, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
					&max_fcc_ma);
	if (rc < 0) {
		pr_err("max-fastchg-current-ma reading failed, rc=%d\n", rc);
		return rc;
	}

	chip->taper_fcc = of_property_read_bool(profile_node, "qcom,taper-fcc");

	chip->sw_jeita_cfg_valid = true;
	rc = read_range_data_from_node(profile_node,
			"qcom,jeita-fcc-ranges-hvdcp3",
			chip->jeita_fcc_config->hvdcp3_fcc_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fcc_ma * 1000);

	rc = read_range_data_from_node(profile_node,
			"qcom,jeita-fcc-ranges",
			chip->jeita_fcc_config->default_fcc_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fcc_ma * 1000);
	if (rc < 0) {
		pr_debug("Read qcom,jeita-fcc-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->sw_jeita_cfg_valid = false;
	}

	memcpy(chip->jeita_fcc_config->fcc_cfg, chip->jeita_fcc_config->default_fcc_cfg, sizeof(struct range_data)*JEITA_STEP);

	rc = of_property_read_u32_array(profile_node,
			"qcom,step-jeita-hysteresis", hysteresis, 2);
	if (!rc) {
		chip->jeita_fcc_config->param.rise_hys = hysteresis[0];
		chip->jeita_fcc_config->param.fall_hys = hysteresis[1];
		pr_debug("jeita-fcc-hys: rise_hys=%u, fall_hys=%u\n",
			hysteresis[0], hysteresis[1]);
	}

	rc = read_range_data_from_node(profile_node,
			"qcom,jeita-fv-ranges",
			chip->jeita_fv_config->fv_cfg,
			BATT_HOT_DECIDEGREE_MAX, max_fv_uv);
	if (rc < 0) {
		pr_err("Read qcom,jeita-fv-ranges failed from battery profile, rc=%d\n",
					rc);
		chip->sw_jeita_cfg_valid = false;
	}
	pr_err("sw_jeita_cfg_valid :%d\n",chip->sw_jeita_cfg_valid);
	return rc;
}

static void fcc_work(struct work_struct *work)
{
	struct lenovo_jeita_info *chip = container_of(work,
			struct lenovo_jeita_info, fcc_work.work);
	int rc;
	union power_supply_propval pval = {0, };

	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &pval);
	pr_err("fcc_work : POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT : %d\n",pval.intval);
	if(chip->jeita_fcc_index >= 0){
		if(pval.intval > chip->jeita_fcc_config->fcc_cfg[chip->jeita_fcc_index].value){
			if(pval.intval - 1000000 > chip->jeita_fcc_config->fcc_cfg[chip->jeita_fcc_index].value){
				vote(chip->fcc_votable, JEITA_VOTER, true, pval.intval - 1000000);
				schedule_delayed_work(&chip->fcc_work, msecs_to_jiffies(2000));
				return;
			}
			else{
				vote(chip->fcc_votable, JEITA_VOTER, true, chip->jeita_fcc_config->fcc_cfg[chip->jeita_fcc_index].value);
				return;
			}
		}
		if(pval.intval < chip->jeita_fcc_config->fcc_cfg[chip->jeita_fcc_index].value){
			vote(chip->fcc_votable, JEITA_VOTER, true, chip->jeita_fcc_config->fcc_cfg[chip->jeita_fcc_index].value);
		}
	}
	else{
		return;
	}

}
static void get_config_work(struct work_struct *work)
{
	struct lenovo_jeita_info *chip = container_of(work,
			struct lenovo_jeita_info, get_config_work.work);
	int i, rc;

	chip->config_is_read = false;
	rc = get_jeita_setting_from_profile(chip);

	if (rc < 0) {
		if (rc == -ENODEV || rc == -EBUSY) {
			if (chip->get_config_retry_count++
					< GET_CONFIG_RETRY_COUNT) {
				pr_err("bms_psy is not ready, retry: %d\n",
						chip->get_config_retry_count);
				goto reschedule;
			}
		}
	}

	chip->config_is_read = true;
	for (i = 0; i < JEITA_STEP; i++)
		pr_err("jeita-fcc-cfg: %ddecidegree ~ %ddecidegre, %duA\n",
			chip->jeita_fcc_config->fcc_cfg[i].low_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].high_threshold,
			chip->jeita_fcc_config->fcc_cfg[i].value);
	for (i = 0; i < JEITA_STEP; i++)
		pr_err("jeita-fv-cfg: %ddecidegree ~ %ddecidegre, %duV\n",
			chip->jeita_fv_config->fv_cfg[i].low_threshold,
			chip->jeita_fv_config->fv_cfg[i].high_threshold,
			chip->jeita_fv_config->fv_cfg[i].value);

	return;

reschedule:
	schedule_delayed_work(&chip->get_config_work,
			msecs_to_jiffies(GET_CONFIG_DELAY_MS));

}

static int get_val(struct range_data *range, int rise_hys, int fall_hys,
		int current_index, int threshold, int *new_index, int *val)
{
	int i;

	*new_index = -EINVAL;
	*val = -EINVAL;

	/*
	 * If the threshold is lesser than the minimum allowed range,
	 * return -ENODATA.
	 */
	//if (threshold < range[0].low_threshold)
	//	return -ENODATA;

	/* First try to find the matching index without hysteresis */
	for (i = 0; i < JEITA_STEP; i++) {
		if (range[i].high_threshold !=range[i].low_threshold) {
			if (is_between(range[i].low_threshold,
				range[i].high_threshold, threshold)) {
				*new_index = i;
				*val = range[i].value;
				break;
			}
		}
	}
#if 0
	if (*new_index == current_index + 1) {
		if (threshold <
			(range[*new_index].low_threshold + rise_hys)) {
			/*
			 * Stay in the current index, threshold is not higher
			 * by hysteresis amount
			 */
			*new_index = current_index;
			*val = range[current_index].value;
		}
	} else if (*new_index == current_index - 1) {
		if (threshold >
			range[*new_index].high_threshold - fall_hys) {
			/*
			 * stay in the current index, threshold is not lower
			 * by hysteresis amount
			 */
			*new_index = current_index;
			*val = range[current_index].value;
		}
	}
#endif
	if(*new_index == current_index)
		return -1;
	return 0;
}

static int get_batt_maintaince_fv(struct lenovo_jeita_info *chip){
	int rc = 0;
	struct power_supply *exfg_psy;
	union power_supply_propval exfgpval = {0, };
	int batt_maintaince_fv = 0;
	int recharge_voltage = 0;

	exfg_psy = power_supply_get_by_name("bq27541-0");
	if (exfg_psy)
		rc = power_supply_get_property(exfg_psy,
				POWER_SUPPLY_PROP_GAUGE_VOLTAGE, &exfgpval);
	if (rc < 0) {
		pr_err("Couldn't configure POWER_SUPPLY_PROP_GAUGE_VOLTAGE rc=%d\n",
			rc);
	} else {
		switch (exfgpval.intval) {
			case EXFG_STEP3_VOLTAGE_MV:
				batt_maintaince_fv = EXFG_STEP3_FLOAT_VOLTAGE_MV;
				recharge_voltage = EXFG_STEP3_RECHARGE_VOLTAGE_MV;
				break;
			case EXFG_STEP2_VOLTAGE_MV:
				batt_maintaince_fv  = EXFG_STEP2_FLOAT_VOLTAGE_MV;
				recharge_voltage = EXFG_STEP2_RECHARGE_VOLTAGE_MV;
				break;
			case EXFG_STEP1_VOLTAGE_MV:
				batt_maintaince_fv  = EXFG_STEP1_FLOAT_VOLTAGE_MV;
				recharge_voltage = EXFG_STEP1_RECHARGE_VOLTAGE_MV;
				break;
			case EXFG_STEP0_VOLTAGE_MV:
				batt_maintaince_fv  = EXFG_STEP0_FLOAT_VOLTAGE_MV;
				recharge_voltage = EXFG_STEP0_RECHARGE_VOLTAGE_MV;
				break;
			default:
				break;
		}
		pr_err("current gauge voltage = %d fv= %d recharge_voltage = %d\n",exfgpval.intval,batt_maintaince_fv, recharge_voltage);

		if(batt_maintaince_fv != chip->batt_maintaince_fv){
			chip->batt_maintaince_fv = batt_maintaince_fv;
			chip->recharge_voltage= recharge_voltage;

			rc = 0;
		}
	}
	return rc;
}
#define JEITA_SUSPEND_HYST_UV		0
static int handle_jeita(struct lenovo_jeita_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0, fv_uv = 0, temp = 0;
	u64 elapsed_us;

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable){
		pr_err("%s : no fcc_votable\n", __FUNCTION__);
		goto update_time;
	}
	if (!chip->fv_votable)
		chip->fv_votable = find_votable("FV");
	if (!chip->fv_votable){
		pr_err("%s : no fv_votable\n", __FUNCTION__);
		goto update_time;
	}
	if (!chip->usb_icl_votable)
		chip->usb_icl_votable = find_votable("USB_ICL");
	if (!chip->usb_icl_votable){
		pr_err("%s : no usb_icl_votable\n", __FUNCTION__);
		goto update_time;
	}
	if (!chip->dc_suspend_votable)
		chip->dc_suspend_votable = find_votable("DC_SUSPEND");
	if (!chip->dc_suspend_votable){
		pr_err("%s : no dc_suspend_votable\n", __FUNCTION__);
		goto update_time;
	}
	if (!chip->chg_disable_votable)
		chip->chg_disable_votable = find_votable("CHG_DISABLE");
	if (!chip->chg_disable_votable){
		pr_err("%s : no chg_disable_votable\n", __FUNCTION__);
		goto update_time;
	}

	power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &pval);
	pr_debug("%s : POWER_SUPPLY_PROP_REAL_TYPE : %d\n", __FUNCTION__, pval.intval);
	if (pval.intval == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
		memcpy(chip->jeita_fcc_config->fcc_cfg, chip->jeita_fcc_config->hvdcp3_fcc_cfg, sizeof(struct range_data)*JEITA_STEP);
	} else {
		memcpy(chip->jeita_fcc_config->fcc_cfg, chip->jeita_fcc_config->default_fcc_cfg, sizeof(struct range_data)*JEITA_STEP);
	}

	if (!chip->sw_jeita_cfg_valid) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, JEITA_VOTER, false, 0);
		if (chip->fv_votable)
			vote(chip->fv_votable, JEITA_VOTER, false, 0);
		if (chip->usb_icl_votable)
			vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
		if (chip->dc_suspend_votable)
			vote(chip->dc_suspend_votable, JEITA_VOTER, false, 0);
		if (chip->chg_disable_votable)
			vote(chip->chg_disable_votable, JEITA_VOTER, false, 0);
		
		return 0;
	}

	elapsed_us = ktime_us_delta(ktime_get(), chip->jeita_last_update_time);
	/* skip processing, event too early */
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		return 0;

	rc = power_supply_get_property(chip->batt_psy,
			chip->jeita_fcc_config->param.psy_prop, &pval);

	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
				chip->jeita_fcc_config->param.prop_name, rc);
		return rc;
	}
	temp = pval.intval;
	rc = get_val(chip->jeita_fcc_config->fcc_cfg,
			chip->jeita_fcc_config->param.rise_hys,
			chip->jeita_fcc_config->param.fall_hys,
			chip->jeita_fcc_index,
			temp,
			&chip->jeita_fcc_index,
			&fcc_ua);
	pr_err("rc:%d,temp:%d,index:%d,fcc:%d\n",rc,temp,chip->jeita_fcc_index,fcc_ua);

	if (!rc){
		if(chip->jeita_fcc_index < 0){
			pr_err("battery temp too low or too high,disable charge\n");
			vote(chip->chg_disable_votable, JEITA_VOTER, true, fcc_ua ? false : true);
		}
		else{
			vote(chip->chg_disable_votable, JEITA_VOTER, false, false);
		}
		rc = power_supply_get_property(chip->bms_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if(chip->jeita_fcc_index == 0){
			if(pval.intval > 4200000){
				fcc_ua = 1500000;
			}
			pval.intval = -1200;
			rc = power_supply_set_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &pval);
		}
		else{
			pval.intval = -1700;
			rc = power_supply_set_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &pval);
		}
	}
	if(fcc_ua > 0)
		schedule_delayed_work(&chip->fcc_work, msecs_to_jiffies(0));

	rc = get_batt_maintaince_fv(chip);
	if(!rc){
		vote(chip->fv_votable, BATT_MAINTAINCE_VOTER, true, chip->batt_maintaince_fv);
		pval.intval = chip->recharge_voltage;
		rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_RECHARGE_MV, &pval);
	}
	rc = get_val(chip->jeita_fv_config->fv_cfg,
			chip->jeita_fv_config->param.rise_hys,
			chip->jeita_fv_config->param.fall_hys,
			chip->jeita_fv_index,
			temp,
			&chip->jeita_fv_index,
			&fv_uv);
	if (!rc){
		if (fv_uv > 0) {

			rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
			if (!rc && (pval.intval > fv_uv)){
				vote(chip->usb_icl_votable, JEITA_VOTER, true, 0);
				vote(chip->dc_suspend_votable, JEITA_VOTER, true, 1);
			}
			else if (pval.intval < (fv_uv - JEITA_SUSPEND_HYST_UV)){
				vote(chip->usb_icl_votable, JEITA_VOTER, false, 0);
				vote(chip->dc_suspend_votable, JEITA_VOTER, false, 0);
			}

		}
	}
	/*
	 * If JEITA float voltage is same as max-vfloat of battery then
	 * skip any further VBAT specific checks.
	 */

	/*
	 * Suspend USB input path if battery voltage is above
	 * JEITA VFLOAT threshold.
	 */

	vote(chip->fv_votable, JEITA_VOTER, true, fv_uv);

update_time:
	chip->jeita_last_update_time = ktime_get();

	return 0;
}

static void status_change_work(struct work_struct *work)
{
	struct lenovo_jeita_info*chip = container_of(work,
			struct lenovo_jeita_info, status_change_work.work);
	int rc = 0;

	if (!is_batt_available(chip)|| !is_bms_available(chip)) 
		goto exit_work;
	

	/* skip elapsed_us debounce for handling battery temperature */
	rc = handle_jeita(chip);
	if (rc < 0)
		pr_err("Couldn't handle sw jeita rc = %d\n", rc);


	/* Remove stale votes on USB removal */

	if (! is_input_present(chip)) {
		if (chip->usb_icl_votable)
			vote(chip->usb_icl_votable, JEITA_VOTER,
					false, 0);

	}

exit_work:
	__pm_relax(chip->lenovo_jeita_ws);
}

static int jeita_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct lenovo_jeita_info*chip = container_of(nb, struct lenovo_jeita_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
			|| (strcmp(psy->desc->name, "usb") == 0)) {
		__pm_stay_awake(chip->lenovo_jeita_ws);
		schedule_delayed_work(&chip->status_change_work, 0);
	}

	return NOTIFY_OK;
}

static int jeita_register_notifier(struct lenovo_jeita_info *chip)
{
	int rc;

	chip->nb.notifier_call = jeita_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

int lenovo_jeita_init(struct device *dev, bool lenovo_jeita_en)
{
	int rc;
	struct lenovo_jeita_info* chip;

	if(!lenovo_jeita_en){
		pr_err("lenovo_jeita_en:%d,return\n",lenovo_jeita_en);
		return 0;
	}
	if (the_chip) {
		pr_err("Already initialized\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->lenovo_jeita_ws = wakeup_source_register(dev, "lenovo_jeita");
	if (!chip->lenovo_jeita_ws)
		return -EINVAL;

	chip->dev = dev;
	chip->jeita_fcc_index = -EINVAL;
	chip->jeita_fv_index = -EINVAL;

	chip->jeita_fcc_config = devm_kzalloc(dev,
			sizeof(struct jeita_fcc_cfg), GFP_KERNEL);
	chip->jeita_fv_config = devm_kzalloc(dev,
			sizeof(struct jeita_fv_cfg), GFP_KERNEL);
	if (!chip->jeita_fcc_config || !chip->jeita_fv_config)
		return -ENOMEM;

	chip->jeita_fcc_config->param.psy_prop = POWER_SUPPLY_PROP_TEMP;
	chip->jeita_fcc_config->param.prop_name = "BATT_TEMP";
	chip->jeita_fcc_config->param.rise_hys = 0;
	chip->jeita_fcc_config->param.fall_hys = 0;
	chip->jeita_fv_config->param.psy_prop = POWER_SUPPLY_PROP_TEMP;
	chip->jeita_fv_config->param.prop_name = "BATT_TEMP";
	chip->jeita_fv_config->param.rise_hys = 0;
	chip->jeita_fv_config->param.fall_hys = 0;

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);
	INIT_DELAYED_WORK(&chip->get_config_work, get_config_work);
	INIT_DELAYED_WORK(&chip->fcc_work, fcc_work);

	rc = jeita_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto release_wakeup_source;
	}

	schedule_delayed_work(&chip->get_config_work,
			msecs_to_jiffies(GET_CONFIG_DELAY_MS));

	the_chip = chip;
	pr_err("lenovo_jeita_init success\n");
	
	return 0;

release_wakeup_source:
	wakeup_source_unregister(chip->lenovo_jeita_ws);
	return rc;
}

void lenovo_jeita_deinit(void)
{
	struct lenovo_jeita_info *chip = the_chip;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	cancel_delayed_work_sync(&chip->get_config_work);
	power_supply_unreg_notifier(&chip->nb);
	wakeup_source_unregister(chip->lenovo_jeita_ws);
	the_chip = NULL;
}
