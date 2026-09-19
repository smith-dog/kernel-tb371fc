// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/extcon.h>
#include <linux/power_supply.h>
#include <linux/usb/ch9.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/ctype.h>

#undef dev_dbg
#define dev_dbg dev_err

#define CHAN_MODE_NUM 6
#define CHANNEL_NUM 4

#define PI2DPX1217_MODE_OFFSET 0x03
#define PI2DPX1217_HPD_OFFSET 0x04
#define PI2DPX1217_EQ_FG_OFFSET 0x05
#define PI2DPX1217_HPD_enable 0x06
#define PI2DPX1217_HPD_disable 0x04
#define PI2DPX1217_DP0_EQ 0x08
#define PI2DPX1217_DP1_EQ 0x08
#define PI2DPX1217_DP2_EQ 0x08
#define PI2DPX1217_DP3_EQ 0x08
#define PI2DPX1217_DP0_FLIP_EQ 0x08
#define PI2DPX1217_DP1_FLIP_EQ 0x08
#define PI2DPX1217_DP2_FLIP_EQ 0x08
#define PI2DPX1217_DP3_FLIP_EQ 0x08
#define PI2DPX1217_USB3TX1_EQ 0x08
#define PI2DPX1217_USB3RX1_EQ 0x08
#define PI2DPX1217_USB3TX2_EQ 0x08
#define PI2DPX1217_USB3RX2_EQ 0x08

/* priority: INT_MAX >= x >= 0 */
#define NOTIFIER_PRIORITY		1

/* Registers Address */
#define VENDOR_REG		0x0
#define CHIP_REG		0x1

#define REDRIVER_REG_MAX		0x12


/* for type c cable */
enum plug_orientation {
	ORIENTATION_NONE,
	ORIENTATION_CC1,
	ORIENTATION_CC2,
};

/*
 * Three Modes of Operations:
 *  - One/Two ports of USB 3.1 Gen1/Gen2 (Default Mode)
 *  - Two lanes of DisplayPort 1.4 + One port of USB 3.1 Gen1/Gen2
 *  - Four lanes of DisplayPort 1.4
 */
enum operation_mode {
	OP_MODE_USB,	/* One/Two ports of USB */
	OP_MODE_DP,		/* DP 4 Lane and DP 2 Lane */
	OP_MODE_USB_AND_DP, /* One port of USB and DP 2 Lane */
};

/*
 * USB redriver channel mode:
 *  - USB mode
 *  - DP mode
 */
enum channel_mode {
	CHAN_MODE_USB,
	CHAN_MODE_DP,
};

/**
 * struct ssusb_redriver - representation of USB re-driver
 * @dev: struct device pointer
 * @regmap: used for I2C communication on accessing registers
 * @client: i2c client structure pointer
 * @config_work: used to configure re-driver
 * @redriver_wq: work queue used for @config_work
 * @usb_psy: structure that holds USB power supply status
 * @host_active: used to indicate USB host mode is enabled or not
 * @vbus_active: used to indicate USB device mode is enabled or not
 * @is_usb3: used to indicate USB3 or not
 * @typec_orientation: used to inditate Type C orientation
 * @op_mode: used to store re-driver operation mode
 * @extcon_usb: external connector used for USB host/device mode
 * @extcon_dp: external connector used for DP
 * @vbus_nb: used for vbus event reception
 * @id_nb: used for id event reception
 * @dp_nb: used for DP event reception
 * @panic_nb: used for panic event reception
 * @chan_mode: used to indicate re-driver's channel mode
 * @eq: equalization register value.
 *      eq[0] - eq[3]: Channel A-D parameter for USB
 *      eq[4] - eq[7]: Channel A-D parameter for DP
 * @output_comp: output compression register value
 *      output_comp[0] - output_comp[3]: Channel A-D parameter for USB
 *      output_comp[4] - output_comp[7]: Channel A-D parameter for DP
 * @loss_match: loss profile matching control register value
 *      loss_match[0] - loss_match[3]: Channel A-D parameter for USB
 *      loss_match[4] - loss_match[7]: Channel A-D parameter for DP
 * @flat_gain: flat gain control register value
 *      flat_gain[0] - flat_gain[3]: Channel A-D parameter for USB
 *      flat_gain[4] - flat_gain[7]: Channel A-D parameter for DP
 * @debug_root: debugfs entry for this context
 */
struct ssusb_redriver {
	struct device		*dev;
	struct regmap		*regmap;
	struct i2c_client	*client;

	struct work_struct	config_work;
	struct workqueue_struct *redriver_wq;

	struct power_supply	*usb_psy;
	bool host_active;
	bool vbus_active;
	bool is_usb3;
	enum plug_orientation typec_orientation;
	enum operation_mode op_mode;

	struct extcon_dev	*extcon_usb;
	struct extcon_dev	*extcon_dp;
	struct notifier_block	vbus_nb;
	struct notifier_block	id_nb;
	struct notifier_block	dp_nb;

	struct notifier_block	panic_nb;

	u8	eq[CHAN_MODE_NUM][CHANNEL_NUM];	
	u8	config;
	struct dentry	*debug_root;
	bool debug_state;
	u64	debug_val;
};

static void ssusb_redriver_debugfs_entries(struct ssusb_redriver *redriver);

static int redriver_i2c_reg_get(struct ssusb_redriver *redriver,
		u8 reg, u8 *val)
{
	int ret;
	unsigned int val_tmp;

	ret = regmap_read(redriver->regmap, (unsigned int)reg, &val_tmp);
	if (ret < 0) {
		dev_err(redriver->dev, "reading reg 0x%02x failure\n", reg);
		return ret;
	}

	*val = (u8)val_tmp;

	dev_dbg(redriver->dev, "reading reg 0x%02x=0x%02x\n", reg, *val);

	return 0;
}

static int redriver_i2c_reg_set(struct ssusb_redriver *redriver,
		u8 reg, u8 val)
{
	int ret;

	ret = regmap_write(redriver->regmap, (unsigned int)reg,
			(unsigned int)val);
	if (ret < 0) {
		dev_err(redriver->dev, "writing reg 0x%02x failure\n", reg);
		return ret;
	}

	dev_dbg(redriver->dev, "writing reg 0x%02x=0x%02x\n", reg, val);

	return 0;
}
//Write PI3DPX1207 Byte 3 to set operating mode
//input: confg
// 0000 safe mode
// 0001 safe mode
// 0010 DP mode
// 0011 DP Flip mode
// 0100 USB mode
// 0101 USB Flip mode
// 0110 DP+USB mode
// 0111 DP+USB Flip mode
static int pi2dpx1217_set_operating_mode (struct ssusb_redriver *redriver, u8 confg)
{
	int ret;
	int reg =0;
	reg |= (confg <<4);
	ret = redriver_i2c_reg_set(redriver, PI2DPX1217_MODE_OFFSET, reg);
	if(ret){
		dev_err(redriver->dev,"pi2dpx1217_set_operating_mode failed ret =%d\n",ret);
	}
	return ret;
}

//Write PI2DPX1217 Byte 5 to Byte 8 to set Equalization and Flat gain
//input: confg
static int pi2dpx1217_set_eq_fg(struct ssusb_redriver *redriver, u8 confg)
{
	int ret;
	int i;
	u8 data[4];
	int reg;
	if(confg<2) 
		return -1;
	
	reg = confg-2;
	data[0] = redriver->eq[reg][0];
	data[1] = redriver->eq[reg][1];
	data[2] = redriver->eq[reg][2];
	data[3] = redriver->eq[reg][3];

	if(redriver->debug_state){
		for(i=0; i<4; i++){
			data[i] = (redriver->debug_val >> (i*8)) & 0xff;
		}
	}
	for(i=0; i<4; i++){
		ret = redriver_i2c_reg_set(redriver, PI2DPX1217_EQ_FG_OFFSET + i, data[i]);
		if(ret){
			dev_err(redriver->dev,"pi2dpx1217_set_eq_fg failed,i=%d ret=%d\n",i,ret);
			return ret;
		}
	}
	return ret;
}

//Enable or disable the IN_HPD control
//input IN_HPD
// 1 enable HPD
// 0 disable HPD
static int pi2dpx1217_enable_disable_HPD (struct ssusb_redriver *redriver, bool IN_HPD)
{
	int ret;
	int reg=(IN_HPD)	? PI2DPX1217_HPD_enable: PI2DPX1217_HPD_disable;
	ret = redriver_i2c_reg_set(redriver,PI2DPX1217_HPD_OFFSET,reg);
	if(ret){
		dev_err(redriver->dev,"pi2dpx1217_enable_disable_HPD failed ret =%d\n",ret);
	}
	return ret;
}
/**
 * Handle Re-driver chip operation mode and channel settings.
 *
 * Three Modes of Operations:
 *  - One/Two ports of USB 3.1 Gen1/Gen2 (Default Mode)
 *  - Two lanes of DisplayPort 1.4 + One port of USB 3.1 Gen1/Gen2
 *  - Four lanes of DisplayPort 1.4
 *
 * @redriver - contain redriver status
 * @on - re-driver chip enable or not
 */
static void ssusb_redriver_gen_dev_set(
		struct ssusb_redriver *redriver, bool on)
{
	int flip_offset;
	int hpd = 0;
	
	if (redriver->typec_orientation == ORIENTATION_CC1) {
		flip_offset = 0;
	} else if (redriver->typec_orientation 	== ORIENTATION_CC2) {
		flip_offset = 1;
	} else {
		redriver->config = 4;
		dev_err(redriver->dev,"no typec detach,set config as usb mode\n");
		return;
	}
	dev_dbg(redriver->dev,"typec fliped:%d on:%d\n",flip_offset,on);
	switch (redriver->op_mode) {
	case OP_MODE_USB:
		redriver->config = 4+ flip_offset;
		break;
		
	case OP_MODE_DP:
		redriver->config = 2+ flip_offset;
		hpd = 1;
		break;
	case OP_MODE_USB_AND_DP:
		redriver->config = 6+ flip_offset;
		hpd = 1;
		break;
	default:
		dev_err(redriver->dev,
			"Error: op mode: %d, vbus: %d, host: %d.\n",
			redriver->op_mode, redriver->vbus_active,
			redriver->host_active);
		goto err_exit;
	}
	dev_err(redriver->dev,"config:%d\n",redriver->config);
	pi2dpx1217_set_eq_fg(redriver, redriver->config);
	pi2dpx1217_set_operating_mode(redriver, redriver->config);
	
	/* exit/enter deep-sleep power mode */
	pi2dpx1217_enable_disable_HPD(redriver, hpd);
	
	return;

err_exit:
return;
}

static void ssusb_redriver_config_work(struct work_struct *w)
{
	struct ssusb_redriver *redriver = container_of(w,
			struct ssusb_redriver, config_work);
	struct extcon_dev *edev = NULL;
	union extcon_property_value val;
	unsigned int extcon_id = EXTCON_NONE;
	int ret = 0;

	dev_dbg(redriver->dev, "%s: USB SS redriver config work\n",
			__func__);

	edev = redriver->extcon_usb;

	if (redriver->vbus_active)
		extcon_id = EXTCON_USB;
	else if (redriver->host_active)
		extcon_id = EXTCON_USB_HOST;

	if (edev && (extcon_id != EXTCON_NONE)
			&& extcon_get_state(edev, extcon_id)) {
		ret = extcon_get_property(edev, extcon_id,
					EXTCON_PROP_USB_SS, &val);
		if (!ret) {
			redriver->is_usb3 = (val.intval != 0);

			dev_dbg(redriver->dev, "SS Lane is used? [%s].\n",
				redriver->is_usb3 ? "true" : "false");
		} else {
			redriver->is_usb3 = true;

			dev_dbg(redriver->dev, "Default true as speed isn't reported.\n");
		}

		if (redriver->is_usb3 || (redriver->op_mode != OP_MODE_USB)) {
			ret = extcon_get_property(edev, extcon_id,
					EXTCON_PROP_USB_TYPEC_POLARITY, &val);
			if (!ret)
				redriver->typec_orientation = val.intval ?
					ORIENTATION_CC2 : ORIENTATION_CC1;
			else if (redriver->op_mode == OP_MODE_USB)
				redriver->typec_orientation = ORIENTATION_NONE;
			else
				dev_err(redriver->dev, "fail to get orientation when has DP.\n");

			ssusb_redriver_gen_dev_set(redriver, true);
		} else {
			dev_dbg(redriver->dev,
				"Disable chip when not in SS USB mode.\n");

			ssusb_redriver_gen_dev_set(redriver, false);
		}

		dev_dbg(redriver->dev, "Type C orientation code is %d.\n",
				redriver->typec_orientation);
	} else if (redriver->op_mode != OP_MODE_USB) {
		/*
		 * USB host stack will be turned off if peer doesn't
		 * support USB communication. PD driver will send
		 * id notification when disable host stack. Update
		 * redriver channel mode when operation mode changed.
		 */
		dev_dbg(redriver->dev,
				"Update redriver operation mode.\n");

		ssusb_redriver_gen_dev_set(redriver, true);
	} else {
		dev_dbg(redriver->dev, "USB Cable is disconnected.\n");

		/* Set back to USB only mode when cable disconnect */
		redriver->op_mode = OP_MODE_USB;

		ssusb_redriver_gen_dev_set(redriver, false);
	}
}

static int ssusb_redriver_dp_notifier(struct notifier_block *nb,
		unsigned long dp_lane, void *ptr)
{
	struct ssusb_redriver *redriver = container_of(nb,
			struct ssusb_redriver, dp_nb);
	enum operation_mode op_mode;
	dev_dbg(redriver->dev,
		"redriver op mode change: %ld event received\n", dp_lane);
	switch (dp_lane) {
	case 0:
		op_mode = OP_MODE_USB;
		break;
	case 2:
		op_mode = OP_MODE_USB_AND_DP;
		break;
	case 4:
		op_mode = OP_MODE_DP;
		break;
	default:
		return 0;
	}

	if (redriver->op_mode == op_mode)
		return 0;

	redriver->op_mode = op_mode;

	queue_work(redriver->redriver_wq, &redriver->config_work);

	return 0;
}

static int ssusb_redriver_vbus_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct ssusb_redriver *redriver = container_of(nb,
			struct ssusb_redriver, vbus_nb);

	dev_dbg(redriver->dev, "vbus:%ld event received\n", event);

	if (redriver->vbus_active == event)
		return NOTIFY_DONE;

	redriver->vbus_active = event;

	queue_work(redriver->redriver_wq, &redriver->config_work);

	return NOTIFY_DONE;
}

static int ssusb_redriver_id_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct ssusb_redriver *redriver = container_of(nb,
			struct ssusb_redriver, id_nb);
	bool host_active = (bool)event;

	dev_dbg(redriver->dev, "host_active:%s event received\n",
			host_active ? "true" : "false");

	if (redriver->host_active == host_active)
		return NOTIFY_DONE;

	redriver->host_active = host_active;

	queue_work(redriver->redriver_wq, &redriver->config_work);

	return NOTIFY_DONE;
}

static int ssusb_redriver_extcon_register(struct ssusb_redriver *redriver)
{
	struct device_node *node = redriver->dev->of_node;
	struct extcon_dev *edev;
	int ret = 0;

	if (!of_find_property(node, "extcon", NULL)) {
		dev_err(redriver->dev, "failed to get extcon for redriver\n");
		return 0;
	}

	edev = extcon_get_edev_by_phandle(redriver->dev, 0);
	if (IS_ERR(edev) && PTR_ERR(edev) != -ENODEV) {
		dev_err(redriver->dev, "failed to get phandle for redriver\n");
		return PTR_ERR(edev);
	}

	if (!IS_ERR(edev)) {
		redriver->extcon_usb = edev;

		redriver->vbus_nb.notifier_call = ssusb_redriver_vbus_notifier;
		redriver->vbus_nb.priority = NOTIFIER_PRIORITY;
		ret = extcon_register_notifier(edev, EXTCON_USB,
				&redriver->vbus_nb);
		if (ret < 0) {
			dev_err(redriver->dev,
				"failed to register notifier for redriver\n");
			return ret;
		}

		redriver->id_nb.notifier_call = ssusb_redriver_id_notifier;
		redriver->id_nb.priority = NOTIFIER_PRIORITY;
		ret = extcon_register_notifier(edev, EXTCON_USB_HOST,
				&redriver->id_nb);
		if (ret < 0) {
			dev_err(redriver->dev,
				"failed to register notifier for USB-HOST\n");
			goto err;
		}
	}

	edev = NULL;
	/* Use optional phandle (index 1) for DP lane events */
	if (of_count_phandle_with_args(node, "extcon", NULL) > 1) {
		edev = extcon_get_edev_by_phandle(redriver->dev, 1);
		if (IS_ERR(edev) && PTR_ERR(edev) != -ENODEV) {
			ret = PTR_ERR(edev);
			goto err1;
		}
	}

	if (!IS_ERR_OR_NULL(edev)) {
		redriver->extcon_dp = edev;
		redriver->dp_nb.notifier_call =
				ssusb_redriver_dp_notifier;
		redriver->dp_nb.priority = NOTIFIER_PRIORITY;
		ret = extcon_register_blocking_notifier(edev, EXTCON_DISP_DP,
				&redriver->dp_nb);
		if (ret < 0) {
			dev_err(redriver->dev,
				"failed to register blocking notifier\n");
			goto err1;
		}
	}

	/* Update initial VBUS/ID state from extcon */
	if (extcon_get_state(redriver->extcon_usb, EXTCON_USB))
		ssusb_redriver_vbus_notifier(&redriver->vbus_nb, true,
			redriver->extcon_usb);
	else if (extcon_get_state(redriver->extcon_usb, EXTCON_USB_HOST))
		ssusb_redriver_id_notifier(&redriver->id_nb, true,
				redriver->extcon_usb);

	return 0;

err1:
	if (redriver->extcon_usb)
		extcon_unregister_notifier(redriver->extcon_usb,
			EXTCON_USB_HOST, &redriver->id_nb);
err:
	if (redriver->extcon_usb)
		extcon_unregister_notifier(redriver->extcon_usb,
			EXTCON_USB, &redriver->vbus_nb);
	return ret;
}

static int ssusb_redriver_panic_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct ssusb_redriver *redriver = container_of(this,
			struct ssusb_redriver, panic_nb);

	pr_err("%s: op mode: %d, vbus: %d, host: %d\n", __func__,
		redriver->op_mode, redriver->vbus_active,
		redriver->host_active);

	return NOTIFY_OK;
}

static const struct regmap_config redriver_regmap = {
	.max_register = REDRIVER_REG_MAX,
	.reg_bits = 8,
	.val_bits = 8,
};

static int redriver_parse_dt(struct ssusb_redriver *redriver){
	struct device_node *node = redriver->dev->of_node;
	int ret = 0;
	
	u8 temp[CHAN_MODE_NUM][CHANNEL_NUM] = {
		{PI2DPX1217_DP0_EQ, PI2DPX1217_DP1_EQ, PI2DPX1217_DP2_EQ, PI2DPX1217_DP3_EQ}, // DP mode
		{PI2DPX1217_DP3_FLIP_EQ, PI2DPX1217_DP2_FLIP_EQ, PI2DPX1217_DP1_FLIP_EQ, PI2DPX1217_DP0_FLIP_EQ}, // DP flip
		{PI2DPX1217_USB3RX2_EQ, PI2DPX1217_USB3TX2_EQ, PI2DPX1217_USB3TX1_EQ, PI2DPX1217_USB3RX1_EQ },// USB mode
		{PI2DPX1217_USB3RX2_EQ, PI2DPX1217_USB3TX2_EQ, PI2DPX1217_USB3TX1_EQ, PI2DPX1217_USB3RX1_EQ}, // USB mode flip
		{PI2DPX1217_DP0_EQ, PI2DPX1217_DP1_EQ, PI2DPX1217_USB3TX1_EQ, PI2DPX1217_USB3RX1_EQ },// DP+USB mode
		{PI2DPX1217_USB3RX2_EQ, PI2DPX1217_USB3TX2_EQ, PI2DPX1217_DP1_FLIP_EQ, PI2DPX1217_DP0_FLIP_EQ}, // DP+USB flip
	};
	memcpy(redriver->eq[0],temp[0],sizeof(temp));
	if (of_find_property(node, "eq", NULL)) {
		ret = of_property_read_u8_array(node, "eq",
				redriver->eq[0], sizeof(redriver->eq));
		if (ret){
			dev_err(redriver->dev,"redriver_parse_dt failed\n");
			return ret;
		}
	}else{
		dev_err(redriver->dev,"redriver_parse_dt no eq node\n");
	}
	return ret;
}
static int redriver_i2c_probe(struct i2c_client *client,
			       const struct i2c_device_id *dev_id)
{
	struct ssusb_redriver *redriver;
	union power_supply_propval pval = {0};
	int ret;
	u8 val;

	redriver = devm_kzalloc(&client->dev, sizeof(struct ssusb_redriver),
			GFP_KERNEL);
	if (!redriver)
		return -ENOMEM;

	INIT_WORK(&redriver->config_work, ssusb_redriver_config_work);

	redriver->redriver_wq = alloc_ordered_workqueue("redriver_wq",
			WQ_HIGHPRI);
	if (!redriver->redriver_wq) {
		dev_err(&client->dev,
			"%s: Unable to create workqueue redriver_wq\n",
			__func__);
		return -ENOMEM;
	}

	redriver->dev = &client->dev;

	redriver->regmap = devm_regmap_init_i2c(client, &redriver_regmap);
	if (IS_ERR(redriver->regmap)) {
		ret = PTR_ERR(redriver->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		goto destroy_wq;
	}

	redriver->client = client;
	i2c_set_clientdata(client, redriver);

	/* Set default parameters for A/B/C/D channels. */
	ret = redriver_i2c_reg_get(redriver, VENDOR_REG, &val);
	if (ret < 0)
		goto destroy_wq;
	dev_err(&client->dev,	"redriver vendor: %x\n", val);
	ret = redriver_i2c_reg_get(redriver, CHIP_REG, &val);
	if (ret < 0)
		goto destroy_wq;
	dev_err(&client->dev,	"redriver chip: %x\n", val);
	
	/* Set id_state as float by default*/
	redriver->host_active = false;

	/* Set to USB by default */
	redriver->op_mode = OP_MODE_USB;

	redriver->usb_psy = power_supply_get_by_name("usb");
	if (!redriver->usb_psy) {
		dev_warn(&client->dev, "Could not get usb power_supply\n");
		pval.intval = -EINVAL;
	} else {
		power_supply_get_property(redriver->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);

		/* USB cable is not connected */
		if (!pval.intval)
			ssusb_redriver_gen_dev_set(redriver, false);
	}

	ret = ssusb_redriver_extcon_register(redriver);
	if (ret)
		goto put_psy;

	redriver->panic_nb.notifier_call = ssusb_redriver_panic_notifier;
	atomic_notifier_chain_register(&panic_notifier_list,
			&redriver->panic_nb);
	redriver_parse_dt(redriver);
	ssusb_redriver_debugfs_entries(redriver);
	dev_err(&client->dev, "USB 3.1 Gen1/Gen2 Re-Driver Probed.\n");

	return 0;

put_psy:
	if (redriver->usb_psy)
		power_supply_put(redriver->usb_psy);

destroy_wq:
	dev_err(&client->dev, "USB 3.1 Gen1/Gen2 Re-Driver Probe failed.\n");
	destroy_workqueue(redriver->redriver_wq);

	return ret;
}

static int redriver_i2c_remove(struct i2c_client *client)
{
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	debugfs_remove(redriver->debug_root);
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&redriver->panic_nb);

	if (redriver->usb_psy)
		power_supply_put(redriver->usb_psy);

	destroy_workqueue(redriver->redriver_wq);

	return 0;
}

static int eq_get(void *data, u64 *val)
{
	struct ssusb_redriver *redriver = data;

	*val = redriver->debug_val;
	return 0;
}
static int eq_set(void *data, u64 val)
{
	struct ssusb_redriver *redriver = data;

	redriver->debug_val = val;
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(eq_ops, eq_get,	eq_set, "0x%08llx\n");

static int debug_get(void *data, u64 *val)
{
	struct ssusb_redriver *redriver = data;
	*val = redriver->debug_state;
	return 0;
}
static int debug_set(void *data, u64 val)
{
	struct ssusb_redriver *redriver = data;

	redriver->debug_state = val;
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(debug_ops, debug_get,	debug_set, "%d\n");


static void ssusb_redriver_debugfs_entries(
		struct ssusb_redriver *redriver)
{
	struct dentry *ent;

	redriver->debug_root = debugfs_create_dir("pi2dpx_redriver", NULL);
	if (!redriver->debug_root) {
		dev_warn(redriver->dev, "Couldn't create debug dir\n");
		return;
	}

	ent = debugfs_create_file("eq", 0600,
			redriver->debug_root, redriver, &eq_ops);
	if (IS_ERR_OR_NULL(ent))
		dev_warn(redriver->dev, "Couldn't create eq file\n");

	ent = debugfs_create_file("debug", 0600,
			redriver->debug_root, redriver, &debug_ops);
	if (IS_ERR_OR_NULL(ent))
		dev_warn(redriver->dev, "Couldn't create debug file\n");

}

static int __maybe_unused redriver_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	dev_dbg(redriver->dev, "%s: SS USB redriver suspend.\n",
			__func__);

	/* Disable redriver chip when USB cable disconnected */
	if (!redriver->vbus_active && !redriver->host_active &&
	    redriver->op_mode != OP_MODE_DP)
		ssusb_redriver_gen_dev_set(redriver, false);

	flush_workqueue(redriver->redriver_wq);

	return 0;
}

static int __maybe_unused redriver_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	dev_dbg(redriver->dev, "%s: SS USB redriver resume.\n",
			__func__);

	flush_workqueue(redriver->redriver_wq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(redriver_i2c_pm, redriver_i2c_suspend,
			 redriver_i2c_resume);

static void redriver_i2c_shutdown(struct i2c_client *client)
{
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);
	int ret;

	/* Set back to USB mode with four channel enabled */
	ret = pi2dpx1217_set_operating_mode(redriver, 0);
	if (ret < 0)
		dev_err(&client->dev,
			"%s: fail to set USB mode with 4 channel enabled.\n",
			__func__);
	else
		dev_dbg(&client->dev,
			"%s: successfully set back to USB mode.\n",
			__func__);
}

static const struct of_device_id redriver_match_table[] = {
	{ .compatible = "diodes,pi2dpxredriver",},
	{ },
};

static const struct i2c_device_id redriver_i2c_id[] = {
	{ "diodes redriver", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, redriver_i2c_id);

static struct i2c_driver redriver_i2c_driver = {
	.driver = {
		.name	= "diodes redriver",
		.of_match_table	= redriver_match_table,
		.pm	= &redriver_i2c_pm,
	},

	.probe		= redriver_i2c_probe,
	.remove		= redriver_i2c_remove,

	.shutdown	= redriver_i2c_shutdown,

	.id_table	= redriver_i2c_id,
};

module_i2c_driver(redriver_i2c_driver);

MODULE_DESCRIPTION("USB Super Speed Linear Re-Driver Driver");
MODULE_LICENSE("GPL v2");
