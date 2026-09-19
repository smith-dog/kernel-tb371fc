/*
 * as33970.c -- AS33970 ALSA SoC Audio driver
 *
 * Copyright:   (C) 2020 Synaptics Systems, Inc.
 *
 * This is based on Alexander Sverdlin's CS4271 driver code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 ************************************************************************
 *  Modified Date:  2020/11/20
 *  File Version:   1.0.0
 ************************************************************************/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include "as33970.h"

#define DRIVER_VERSION "0.0.1"
/* Global Variables */ 
int probe_status = 0;

#define AS33970_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S32_LE)

#define AS33970_ID(a, b, c, d)  (((a) - 0x20) << 8 | \
				      ((b) - 0x20) << 14| \
				      ((c) - 0x20) << 20| \
				      ((d) - 0x20) << 26)

#define AS33970_ID2CH_A(id)  (((((unsigned int)(id)) >> 8) & 0x3f) + 0x20)
#define AS33970_ID2CH_B(id)  (((((unsigned int)(id)) >> 14) & 0x3f) + 0x20)
#define AS33970_ID2CH_C(id)  (((((unsigned int)(id)) >> 20) & 0x3f) + 0x20)
#define AS33970_ID2CH_D(id)  (((((unsigned int)(id)) >> 26) & 0x3f) + 0x20)

#define AS33970_CONTROL(xname, xinfo, xget, xput, xaccess) { \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = xaccess, .info = xinfo, .get = xget, .put = xput, \
	}

#define AS33970_CMD_GET(item)   ((item) |  0x0100)
#define AS33970_CMD_SET(item)   (item)
#define AS33970_CMD_SIZE 13

#define AS33970_SYS_CMD_GET_PARAMETER_VALUE   \
	AS33970_CMD_GET(SYS_CMD_PARAMETER_VALUE)
#define AS33970_SYS_CMD_SET_PARAMETER_VALUE   \
	AS33970_CMD_SET(SYS_CMD_PARAMETER_VALUE)

#define AS33970_DAI_DSP  1
#define AS33970_READY  0x8badd00d
#define AS33970_ID_ARM        AS33970_ID('M','C','U',' ')
#define AS33970_ID_CPTR       AS33970_ID('<','A','C','>')

#define AS33970_MODE_TRIGGER    AS33970_ID('Z', 'F', 'V', '1')
#define AS33970_MODE_NORMAL     AS33970_ID('Z', 'F', 'V', '0')
#define AS33970_MODE_6CH        AS33970_ID('Z', 'F', 'V', '2')
#define AS33970_MODE_NORMAL_48  AS33970_ID('Z', 'F', 'V', '3')
#define AS33970_FILE_UFVM       AS33970_ID('U', 'F', 'V', 'M')

#define AS33970_AEC_EN_BIT 0x00000004
#define AS33970_SSP_EN_BIT 0x00000008

#define  SYS_CMD_MEM_BULK_WRITE AS33970_CMD_SET(SYS_CMD_MEM_BULK_ACS_DAT)

enum {
	AUDIO_HOT_WORD = 0,
	AUDIO_COMMUNICATION,
	AUDIO_RECOGNITION,
	AUDIO_CAMCORDER,
	AUDIO_MIC,
	AUDIO_6CH_DUMP,
};

/*
 * Defines the command format which is used to communicate with as33970 device.
 */
struct as33970_cmd {
	int	num_32b_words:16;   /* Indicates how many data to be sent.
				     * If operation is successful, this will
				     * be updated with the number of returned
				     * data in word. one word == 4 bytes.
				     */
	u32	command_id:15;
	u32	reply:1;            /* The device will set this flag once
				     * the operation is complete.
				     */
	u32	app_module_id;
	u32	data[AS33970_CMD_SIZE]; /* Used for storing parameters and
					 * receiving the returned data from
					 * device.
					 */
};

/* codec private data*/
struct as33970_priv {
	struct device *dev;
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct regulator_bulk_data supplies[2];
	struct as33970_cmd cmd;
	int cmd_res;

	/*Dai format*/
	unsigned int mclk_rate;
	unsigned int tx_dai_fmt;
	unsigned int rx_dai_fmt;
	unsigned int is_tx_master;
	unsigned int is_rx_master;

	/*Data format*/
	unsigned int sample_rate;
	unsigned int sample_width;
	unsigned int frame_size;

	/*Trigger*/
	unsigned int trigger_model;
	unsigned int is_high_performance_trigger;
	unsigned int is_always_listening;
	unsigned int trigger_source;

	unsigned int mode;
	unsigned int output_signal;
	unsigned int is_aec_enabled;
	unsigned int is_nr_enabled;
	unsigned int is_playback_enabled;
	unsigned int is_record_enabled;
	unsigned int is_screen_on;
	unsigned int is_phone_call_on;
	unsigned int is_load_model;
	unsigned int factory_test_mode;

	/*Firmware Status*/
	unsigned int fw_status;
	unsigned int fw_is_playback_enabled;
	unsigned int fw_is_record_enabled;
	unsigned int fw_is_always_listening;
	unsigned int fw_is_high_performance_trigger;
	unsigned int fw_trigger_model;

	/*GPIO*/
#ifdef AS33970_ENABLE_PIN
	unsigned int enable_gpio;
#endif

	unsigned int avdd13_enable_gpio;
       unsigned int es7210_vdd_enable;
	unsigned int rst_gpio;
       unsigned int irq_gpio;
	int num_supplies;
#ifdef AS33970_NEED_SET_I2C_SPEED
	unsigned int i2c_speed;
#endif
#ifdef AS33970_USE_WORK_QUEUE
	struct work_struct work;
#endif
	struct mutex cmd_mutex;
};

static const char * const as33970_supplies[] = {
	"as33970-vbat",
	"as33970-avdd13",
};
static int as33970_enable_playback(struct as33970_priv *as33970, int enable_playback);
/*
 * This functions takes as33970_cmd structure as input and output parameters
 * to communicate AS33970. If operation is successfully, it returns number of
 * returned data and stored the returned data in "cmd->data" array.
 * Otherwise, it returns the error code.
 */
static int as33970_sendcmd_ex(struct as33970_priv *as33970,
			     struct as33970_cmd *cmd)
{
	int ret = 0;
	int num_32b_words = cmd->num_32b_words;
	unsigned long time_out;
	u32 *i2c_data = (u32 *)cmd;
	int size = num_32b_words + 2;
	dev_info(as33970->dev, "SendCmd, ID = 0x%x, command = %d, data_length = %d, data1=0x%x, data2=0x%x, data3=0x%x, data4=0x%x\n", 
		cmd->app_module_id, cmd->command_id, cmd->num_32b_words, cmd->data[0], cmd->data[1], cmd->data[2], cmd->data[3]);
#ifdef AS33970_NEED_SET_I2C_SPEED
	if (!as33970->i2c_speed) {
		dev_err(as33970->dev, "Please set i2c speed first\n");
		return -EBUSY;
	}
#endif

#ifdef AS33970_BOOT_WITHOUT_FLASH
	if (!as33970->fw_status) {
		dev_err(as33970->dev, "Please set fw status first\n");
		return -ENODEV;
	}
#endif

	/* calculate how many WORD that will be wrote to device*/
	cmd->num_32b_words = cmd->command_id & AS33970_CMD_GET(0) ?
			     AS33970_CMD_SIZE : num_32b_words;


	/* write all command data except for frist 4 bytes*/
	ret = regmap_bulk_write(as33970->regmap, 4, &i2c_data[1], size - 1);
	if (ret < 0) {
		dev_err(as33970->dev, "Failed to write command data\n");
		goto LEAVE;
	}

	/* write first 4 bytes command data*/
	ret = regmap_bulk_write(as33970->regmap, 0, i2c_data, 1);
	if (ret < 0) {
		dev_err(as33970->dev, "Failed to write command\n");
		goto LEAVE;
	}

	/* continuously read the first bytes data from device until
	 * either timeout or the flag 'reply' is set.
	 */
	time_out = msecs_to_jiffies(2000);
	time_out += jiffies;
	do {
		regmap_bulk_read(as33970->regmap, 0, &i2c_data[0], 1);
		if (cmd->reply == 1)
			break;
		mdelay(5);

	} while (!time_after(jiffies, time_out));

	if (cmd->reply == 1) {
		if (cmd->num_32b_words > 0) {
			regmap_bulk_read(as33970->regmap, 8, &i2c_data[2],
					 cmd->num_32b_words);
			dev_info(as33970->dev, "SendCmd success, ret = %d\n",
				cmd->num_32b_words);
		} else if (cmd->num_32b_words < 0) {
			dev_err(as33970->dev, "SendCmd failed, ID = 0x%x, command = %d, err = %d\n",
				cmd->app_module_id, cmd->command_id, cmd->num_32b_words);
		}

		ret = cmd->num_32b_words;
	} else {
		dev_err(as33970->dev, "SendCmd timeout\n");
		as33970->fw_status = 0;
		as33970->fw_is_always_listening = 0;
		as33970->fw_is_high_performance_trigger = 0;
		as33970->fw_is_playback_enabled = 0;
		as33970->fw_is_record_enabled = 0;
		ret = -EBUSY;
	}

LEAVE:
	return ret;
}

/*
 * as33970_sendcmd: set/get as33970's related configurations
 * @as33970 : pointer variable to struct as33970_priv
 * @cmd : as33970 specified command format
 * @Variable-length argument :
 *         first parameter is command_id
 *         second parameter is app_module_id
 *         third parameter is number of data[] count
 *         following parameters are for data[]
 */
static int as33970_sendcmd(struct as33970_priv *as33970,
			     struct as33970_cmd *cmd, ...)
{
	va_list argp;
	int count, i;
	int ret = 0;

	va_start(argp, cmd);
	cmd->command_id = va_arg(argp, int);
	cmd->app_module_id = va_arg(argp, int);

	count = va_arg(argp, int);
	for (i = 0; i < count; i++)
		cmd->data[i] = va_arg(argp, int);
	va_end(argp);

	cmd->num_32b_words = count;
	cmd->reply = 0;

	ret = as33970_sendcmd_ex(as33970, cmd);
	if (ret < 0)
		dev_err(as33970->dev, "Failed to send command\n");

	return ret;
}

static int as33970_set_mode(struct as33970_priv *as33970, unsigned int mode)
{
	struct as33970_cmd cmd;
	int ret = 0;
	int retry = 0;
	
	if(mode != as33970->mode) {
		for (retry = 0; retry < 3; retry++) {
			ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(SYS_CMD_PM_CMD),
						AS33970_ID_ARM, 3,
						1, 0, 1);
			if (ret < 0) {
				dev_err(as33970->dev, "Failed enter idle, tried %d times, ret =%d\n", retry, ret);
				continue;
			}

			msleep(50);

			ret = as33970_sendcmd(as33970, &cmd, 
						SYS_CMD_EXEC_FILE,
						AS33970_ID_ARM, 1,
						mode);

			if (ret < 0) {
				dev_err(as33970->dev, "Failed exec mode files, tried %d times, ret =%d\n", retry, ret);
				msleep(100);
			} else {
				break;
			}
		}
		as33970->fw_is_playback_enabled = 0;
		as33970->fw_is_record_enabled = 0;
		as33970->fw_is_always_listening = 0;
		as33970->fw_is_high_performance_trigger = 0;
		as33970->fw_trigger_model = 0;
	}

	if (ret < 0) {
		as33970->fw_status = 0;
		dev_err(as33970->dev, "Failed to set mode, ret =%d\n", ret);
	} else {
		dev_dbg(as33970->dev, "Set mode successfully, ret = %d\n", ret);
		as33970->mode = mode;
	}

	return ret;
}

static int as33970_set_always_listening(struct as33970_priv *as33970, 
										unsigned int is_always_listening, 
										unsigned int is_high_performance_trigger, 
										unsigned int trigger_model)
{
	struct as33970_cmd cmd;
	int ret = 0;
	if (is_always_listening) {
		if (AS33970_MODE_TRIGGER != as33970->mode) {
			ret = as33970_set_mode(as33970, AS33970_MODE_TRIGGER);
		}

		if (ret < 0)
			return ret;

		if (!as33970->fw_is_always_listening) {
			ret = as33970_sendcmd(as33970, &cmd,
								AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
								AS33970_ID_ARM, 3,
								EVENT_LP_TRIG_CONTROL, EVENT_PAR_LP_TRIG_STATE, 1);
			msleep(200);
		}
		if (ret < 0)
			return ret;

		as33970->fw_is_always_listening = 1;



		if (is_high_performance_trigger) {
			ret = as33970_sendcmd(as33970, &cmd,
					AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
					AS33970_ID_ARM, 3,
					EVENT_HPM_CONTROL, EVENT_PAR_HPM_STATE, 1);
		} else {
			ret = as33970_sendcmd(as33970, &cmd,
					AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
					AS33970_ID_ARM, 3,
					EVENT_HPM_CONTROL, EVENT_PAR_HPM_STATE, 0);
		}
		
		if (ret < 0)
			return ret;

		as33970->fw_is_high_performance_trigger = is_high_performance_trigger;
		msleep(40);

		if (as33970->fw_is_high_performance_trigger && as33970->is_playback_enabled) {
			ret = as33970_enable_playback(as33970, 1);
		} else if (!as33970->is_record_enabled) {
			ret = as33970_enable_playback(as33970, 0);
		}

		if (ret < 0)
			return ret;
	} else {
		if (AS33970_MODE_TRIGGER == as33970->mode) {
                        ret = as33970_sendcmd(as33970, &cmd,
                                AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
                                AS33970_ID_ARM, 3,
                                EVENT_LP_TRIG_CONTROL, EVENT_PAR_LP_TRIG_STATE, 0);
                       msleep(20);
                 }
		as33970->fw_is_always_listening = 0;
		as33970->fw_is_high_performance_trigger = 0;
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int as33970_set_record_effect(struct as33970_priv *as33970, 
										unsigned int is_aec_enable, 
										unsigned int is_nr_enable)
{
	struct as33970_cmd cmd;
	int ret = 0;
    int record_process_ctrl = (is_aec_enable ? AS33970_AEC_EN_BIT : 0) |
                                (is_nr_enable ? AS33970_SSP_EN_BIT : 0);

    ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(AS33970_SYS_CMD_SET_PARAMETER_VALUE),
						AS33970_ID_ARM, 2,
						EVENT_PAR_USER_FUNC_MODE_SEL1, record_process_ctrl);
    if (ret < 0)
        return ret;
	
    ret = as33970_sendcmd(as33970, &cmd, 
						SYS_CMD_EXEC_FILE,
						AS33970_ID_ARM, 1,
						AS33970_FILE_UFVM);
		
	if (ret < 0)
		return ret;

	as33970->is_aec_enabled = is_aec_enable;
	as33970->is_nr_enabled = is_nr_enable;

	return ret;
}

static int as33970_enable_playback(struct as33970_priv *as33970, int enable_playback)
{
	struct as33970_cmd cmd;
	int ret = 0;

	if (enable_playback) {
		ret = as33970_sendcmd(as33970, &cmd,
					AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
					AS33970_ID_ARM, 3,
					EVENT_USB_PLAYBACK_STARTSTOP, 
					EVENT_PAR_USB_PLAYBACK_STATE, 1);
	} else {
		ret = as33970_sendcmd(as33970, &cmd,
					AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
					AS33970_ID_ARM, 3,
					EVENT_USB_PLAYBACK_STARTSTOP, 
					EVENT_PAR_USB_PLAYBACK_STATE, 0);
	}
		
	if (ret < 0) {
		dev_err(as33970->dev, "set playback status failed\n");
		return ret;
	}

	as33970->fw_is_playback_enabled = enable_playback;
	//msleep(50);

	return ret;
}

static int as33970_enable_record(struct as33970_priv *as33970, int enable_record)
{	
	int ret = 0;
	struct as33970_cmd cmd;
	dev_info(as33970->dev, "%s enter\n", __func__);	
	
#ifdef AS33970_BOOT_WITHOUT_FLASH
	if (!as33970->fw_status) {
		dev_err(as33970->dev, "AS33970 FW is not loaded yet\n");
		return -ENODEV;
	}
#endif

    if (enable_record) {
		/*I2S format: sample rate 48k, sample size 16bit, framesize 64bit*/
		ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(SYS_CMD_PARAMETER_VALUE),
						AS33970_ID_ARM, 2,
						EVENT_PAR_RATE_MAIN_INPUT, as33970->sample_rate);
		if (ret < 0) {
			dev_err(as33970->dev, "Failed to start record, error code %d\n",ret);
			return -EINVAL;
		}

		ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(SYS_CMD_PARAMETER_VALUE),
						AS33970_ID_ARM, 2,
						PAR_INDEX_I2S_TX_WIDTH, as33970->sample_width);
		if (ret < 0) {
			dev_err(as33970->dev, "Failed to start record, error code %d\n",ret);
			return -EINVAL;
		}

		ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(SYS_CMD_PARAMETER_VALUE),
						AS33970_ID_ARM, 2,
						PAR_INDEX_I2S_TX_NUM_OF_BITS, as33970->frame_size);
		if (ret < 0) {
			dev_err(as33970->dev, "Failed to start record, error code %d\n",ret);
			return -EINVAL;
		}

		ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
						AS33970_ID_ARM, 9,
						EVENT_USB_RECORD_STARTSTOP,
						EVENT_PAR_USB_RECORD_STATE, 1,
						EVENT_PAR_RATE_HOST_RECORD, as33970->sample_rate,
						PAR_INDEX_I2S_RX_WIDTH, as33970->sample_width,
						PAR_INDEX_I2S_RX_NUM_OF_BITS, as33970->frame_size);
		
		if (ret < 0) {
			dev_err(as33970->dev, "Failed to start record, error code %d\n",ret);
			return -EINVAL;
		} else {
			dev_info(as33970->dev, "Success to start record\n");
		}
	} else {
		ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
						AS33970_ID_ARM, 3,
						EVENT_USB_RECORD_STARTSTOP, 
						EVENT_PAR_USB_RECORD_STATE, 0);
		if (ret < 0) {
			dev_err(as33970->dev, "Failed to stop record, error code %d\n",ret);
			return -EINVAL;
		} else {
			dev_info(as33970->dev, "Success to stop record\n");
		}
	}

	as33970->fw_is_record_enabled = enable_record;
	//msleep(50);

	return ret;
}

/***Device Attribute***/
static ssize_t show_reset_dsp(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", gpio_get_value_cansleep(as33970->rst_gpio));
}

static ssize_t store_reset_dsp(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	unsigned int reset_gpio;
	sscanf(buf, "%u", &reset_gpio);
	
	if (reset_gpio) {
		gpio_set_value_cansleep(as33970->rst_gpio, 0);
		dev_info(as33970->dev, "as33970 reset gpio pull down \n");
		msleep(100);
		gpio_set_value_cansleep(as33970->rst_gpio, 1);
		dev_info(as33970->dev, "as33970 reset gpio pull up \n");
	} else {
		dev_info(as33970->dev, "as33970 reset gpio pull down\n");
		gpio_set_value_cansleep(as33970->rst_gpio, 0);
	}
	return count;
}

static DEVICE_ATTR(reset_dsp, 0664, show_reset_dsp, store_reset_dsp);

static ssize_t show_fw_status(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", as33970->fw_status);
}

static ssize_t store_fw_status(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	struct as33970_cmd cmd;
	int ret = 0;
	unsigned int fw_status = 0;

	sscanf(buf, "%u", &fw_status);

	mutex_lock(&as33970->cmd_mutex);
	if(fw_status) {
		as33970->fw_status = 1;
		ret = as33970_sendcmd(as33970, &cmd,
					AS33970_CMD_GET(SYS_CMD_VERSION),
					AS33970_ID_ARM, 0);
		if (ret >= 0) {
			dev_info(as33970->dev, "Firmware version = %d.%d.%d.%d\n",
				 cmd.data[0], cmd.data[1],
				 cmd.data[2], cmd.data[3]);
		} else {
			as33970->fw_status = 0;
		}
	} else {
		as33970->fw_status = 0;
	}
	mutex_unlock(&as33970->cmd_mutex);

	return count;
}

static DEVICE_ATTR(fw_status, 0664, show_fw_status, store_fw_status);

static ssize_t show_fw_version(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	struct as33970_cmd cmd;
	int ret = -1;

	mutex_lock(&as33970->cmd_mutex);
	//if(as33970->fw_status) {
	ret = as33970_sendcmd(as33970, &cmd,
					AS33970_CMD_GET(SYS_CMD_VERSION),
					AS33970_ID_ARM, 0);

	//}
	mutex_unlock(&as33970->cmd_mutex);
	if (ret < 0) {
		return sprintf(buf, "0.0.0.0\n");
	} else {
		return sprintf(buf, "%d.%d.%d.%d\n", 
						cmd.data[0], cmd.data[1],
						cmd.data[2], cmd.data[3]);
	}
}

static DEVICE_ATTR(fw_version, 0444, show_fw_version, NULL);

static ssize_t show_model(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	dev_info(as33970->dev, "show_model is_load_model = %d\n", as33970->is_load_model);
	return sprintf(buf, "%u\n", as33970->is_load_model);
}

static ssize_t store_model(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	struct as33970_cmd cmd;

	int ret = 0, size_word = 0, i = 0;
	unsigned int load_step = 0, mem_type = 0, size_bytes = 0, sum_check = 0;
	unsigned int data_bytes = 0;
	char * str_p;
	memset(&cmd,0,sizeof(struct as33970_cmd));
	dev_info(as33970->dev, "store_model buf %s size = %d\n", buf,size);
	sscanf(buf, "%u %u %u %u %u", &load_step,&mem_type,&size_bytes,&sum_check,&data_bytes);
	mutex_lock(&as33970->cmd_mutex); 

	switch (load_step) {
	case MODEL_START:
		//Step0 : Set the memory type want to access 
		ret = as33970_sendcmd(as33970, &cmd,
							AS33970_CMD_SET(SYS_CMD_MEM_BULK_ACS_CTL),
							AS33970_ID_ARM, 2,
							MEM_BULK_CTL_TYPE,mem_type);
		if (ret < 0) {
			dev_err(as33970->dev, "MODEL_START: MEM_BULK_CTL_TYPE Error.\n");
			mutex_unlock(&as33970->cmd_mutex);
			return -EINVAL;
		}

		//Step 1  Get the valid memory address range can be write 
		ret = as33970_sendcmd(as33970, &cmd,
							AS33970_CMD_GET(SYS_CMD_MEM_BULK_ACS_CTL),
							AS33970_ID_ARM, 1,
							MEM_BULK_CTL_VALID_ADDR);
		if (ret < 0) {
			dev_err(as33970->dev, "MODEL_START: MEM_BULK_CTL_VALID_ADDR Error.\n");
			mutex_unlock(&as33970->cmd_mutex);
			return -EINVAL;
		}
		
		if((cmd.data[1] - cmd.data[0]) < size_bytes) {
			dev_info(as33970->dev, "MODEL_START: Dsp Mem size %d not enough for %d .\n",cmd.data[1] - cmd.data[0],size_bytes);
			mutex_unlock(&as33970->cmd_mutex);
			return -EINVAL;
		}

		if((size_bytes%4) != 0) {
			size_word = (size_bytes/4) + 1;
		} else {
			size_word = (size_bytes/4);
		}
		//Step 2 set the memory access parameters : sub command, start_address , size (UNIT: uint32_t) , target_check_sum 
		ret = as33970_sendcmd(as33970, &cmd,
							AS33970_CMD_SET(SYS_CMD_MEM_BULK_ACS_CTL),
							AS33970_ID_ARM, 4,
							MEM_BULK_CTL_INF,cmd.data[0],size_word,0);
		if (ret < 0) {
			dev_err(as33970->dev, "MODEL_START: MEM_BULK_CTL_INF Error.\n");
			mutex_unlock(&as33970->cmd_mutex);
			return -EINVAL;
		}

		break;
	case MODEL_WRITE:
		
		if((data_bytes%4) != 0) {
			size_word = (data_bytes/4) + 1;
		} else {
			size_word = (data_bytes/4);
		}
		str_p = strstr(buf,":"); // Find start of data
		str_p = str_p + 1;
		//Step3  bulk write data into target memory area with assigned size by SYS_CMD_SET_MEM_BULK_ACS_CTL
		if(size_word == 13) {
			sscanf(str_p, "%x %x %x %x %x %x %x %x %x %x %x %x %x",
				&cmd.data[0],&cmd.data[1],&cmd.data[2],&cmd.data[3],&cmd.data[4],&cmd.data[5],
				&cmd.data[6],&cmd.data[7],&cmd.data[8],&cmd.data[9],&cmd.data[10],&cmd.data[11],&cmd.data[12]);
		} else {
			for(i= 0; i  < size_word; i = i + 1) {
				sscanf(str_p, "%x ",&cmd.data[i]);
				str_p = strstr(str_p," ");
				if(str_p) {
					str_p = str_p + 1;
				} else {
					break;
				}
			}
		}
		
		ret = as33970_sendcmd(as33970, &cmd,
							SYS_CMD_MEM_BULK_WRITE,
							AS33970_ID_ARM, size_word,
							cmd.data[0],cmd.data[1],cmd.data[2],
							cmd.data[3],cmd.data[4],cmd.data[5],
							cmd.data[6],cmd.data[7],cmd.data[8],
							cmd.data[9],cmd.data[10],cmd.data[11],cmd.data[12]);
		if (ret < 0) {
			dev_err(as33970->dev, "MODEL_WRITE: Error.\n");
			mutex_unlock(&as33970->cmd_mutex);
			return -EINVAL;
	    }
		break;
	case MODEL_END:
		
		//Step 4 read back the cur_addr, dat_cnt/size (UNIT: uint32_t), calc_check sum to check 
		
		ret = as33970_sendcmd(as33970, &cmd,
							AS33970_CMD_GET(SYS_CMD_MEM_BULK_ACS_CTL),
							AS33970_ID_ARM, 1,
							MEM_BULK_CTL_INF);
		if (ret < 0) {
			dev_err(as33970->dev, "MODEL_END: Error.\n");
			mutex_unlock(&as33970->cmd_mutex);
			return -EINVAL;
		}

		if(sum_check!=cmd.data[2]) {
			dev_info(as33970->dev, "store_model sum_check error:sum_check = 0x%0x != cmd->data[0]=0x%0x, [1]=0x%0x, [2]=0x%0x\n",sum_check,cmd.data[0],cmd.data[1],cmd.data[2]);
			mutex_unlock(&as33970->cmd_mutex);
			return -1;
		} else {
			dev_info(as33970->dev, "store_model sum_check = 0x%0x = cmd->data[0]=0x%0x, [1]=0x%0x, [2]=0x%0x\n",sum_check,cmd.data[0],cmd.data[1],cmd.data[2]);
			as33970->is_load_model = 1;
			mutex_unlock(&as33970->cmd_mutex);
			return size;
		}
		break;
	default:
		dev_info(as33970->dev, "store_model can't find load_step = %d\n",load_step);
		break;
	}
	mutex_unlock(&as33970->cmd_mutex);
	
	return size;
}

static DEVICE_ATTR(load_model, 0664, show_model, store_model);


#ifdef AS33970_NEED_SET_I2C_SPEED
static ssize_t show_i2c_speed(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", as33970->i2c_speed);
}

static ssize_t store_i2c_speed(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct as33970_priv *as33970 = dev_get_drvdata(dev);
	struct as33970_cmd cmd;
	int ret = 0;
	unsigned int i2c_speed = 0;

	sscanf(buf, "%u", &i2c_speed);

	if(i2c_speed)
		//TODO: Add call platform API that can set I2C speed
		//ret = change_i2c_speed(i2c_speed);
		//if (ret)
		//	as33970->i2c_speed = i2c_speed;
	return 1;
}

static DEVICE_ATTR(i2c_speed, 0664, show_i2c_speed, store_i2c_speed);
#endif

/***Mixer***/
#if 0
static int cmd_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(struct as33970_cmd);

	return 0;
}

static int cmd_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
#if (KERNEL_VERSION(3, 15, 0) > LINUX_VERSION_CODE)
	//struct snd_soc_component *codec = snd_kcontrol_chip(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	//struct snd_soc_component *codec = snd_soc_component_to_codec(component);
#endif
	memcpy(ucontrol->value.bytes.data, &as33970->cmd,
	       sizeof(as33970->cmd));

	return 0;
}

static int cmd_put(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
#if (KERNEL_VERSION(3, 15, 0) > LINUX_VERSION_CODE)
	struct snd_soc_component *codec = snd_kcontrol_chip(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(codec);
#else
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	struct snd_soc_component *codec = snd_soc_component_to_codec(component);
#endif
	memcpy(&as33970->cmd, ucontrol->value.bytes.data,
	       sizeof(as33970->cmd));

	as33970->cmd_res = as33970_sendcmd_ex(codec, &as33970->cmd);

	if (as33970->cmd_res < 0)
		dev_err(codec->dev, "Failed to send cmd, ret = %d\n",
			as33970->cmd_res);

	return as33970->cmd_res < 0 ? as33970->cmd_res : 0;
}
#endif

static int playback_status_get(struct snd_kcontrol *kcontrol, 
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ucontrol->value.bytes.data[0] = (unsigned char)as33970->is_playback_enabled;

	return ret;
}

static int playback_status_put(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	mutex_lock(&as33970->cmd_mutex);
	as33970->is_playback_enabled = (unsigned int)ucontrol->value.bytes.data[0];

#ifdef AS33970_ENABLE_SCREEN_STATUS
	if (!as33970->is_screen_on && !as33970->is_playback_enabled) {
		as33970->is_high_performance_trigger = 0;
	} else {
		as33970->is_high_performance_trigger = 1;
	}
#else
	if (as33970->is_playback_enabled)
		as33970->is_high_performance_trigger = 1;
#endif
	if (as33970->fw_is_always_listening) {
		as33970_set_always_listening(as33970,
									as33970->fw_is_always_listening,
									as33970->is_high_performance_trigger,
									as33970->trigger_model);
	}

	dev_info(as33970->dev, "playback_status_put %d, trigger power state = %d",
							as33970->is_playback_enabled,
							as33970->is_high_performance_trigger);
	mutex_unlock(&as33970->cmd_mutex);
	return ret;
}

#ifdef AS33970_ENABLE_SCREEN_STATUS
static int screen_status_get(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ucontrol->value.bytes.data[0] = (unsigned char)as33970->is_screen_on;

	return ret;
}

static int screen_status_put(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	mutex_lock(&as33970->cmd_mutex);
	as33970->is_screen_on = (unsigned int)ucontrol->value.bytes.data[0];

	if (!as33970->is_screen_on && !as33970->is_playback_enabled) {
		as33970->is_high_performance_trigger = 0;
	} else {
		as33970->is_high_performance_trigger = 1;
	}

	if (as33970->fw_is_always_listening)
		as33970_set_always_listening(as33970,
									as33970->fw_is_always_listening,
									as33970->is_high_performance_trigger,
									as33970->trigger_model);

	mutex_unlock(&as33970->cmd_mutex);
	dev_info(as33970->dev, "screen_status_put %d, trigger power state = %d",
							as33970->is_screen_on,
							as33970->is_high_performance_trigger);
	return ret;
}
#endif
#ifdef AS33970_ENABLE_PHONE_CALL_STATUS
static int phone_call_status_get(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ucontrol->value.bytes.data[0] = (unsigned char)as33970->is_phone_call_on;

	return ret;
}

static int phone_call_status_put(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	mutex_lock(&as33970->cmd_mutex);
	as33970->is_phone_call_on = (unsigned int)ucontrol->value.bytes.data[0];
	if (as33970->is_phone_call_on) {
		if (AUDIO_COMMUNICATION != as33970->output_signal && !as33970->fw_is_record_enabled) {
			ret = as33970_set_mode(as33970, AS33970_MODE_NORMAL);
			ret = as33970_set_record_effect(as33970, as33970->is_aec_enabled, as33970->is_nr_enabled);
			as33970->output_signal = AUDIO_COMMUNICATION;
			as33970_enable_record(as33970, 1);
			as33970_enable_playback(as33970, 1);
		}
	} else {
		if (AUDIO_COMMUNICATION == as33970->output_signal) {
			as33970_enable_record(as33970, 0);
			as33970_enable_playback(as33970, 0);
			as33970_set_always_listening(as33970,
							as33970->fw_is_always_listening,
							as33970->fw_is_high_performance_trigger,
							as33970->trigger_model);
		}
	}
	dev_info(as33970->dev, "phone_call_status_put %d", as33970->is_phone_call_on);
	mutex_unlock(&as33970->cmd_mutex); 
	return ret;
}
#endif
/*byte 0: 0 means disable alwasy listening, 1 means enable always listening
 *byte 1: 0 means low power trigger, 1 means high performance trigger
 *byte 2: Trigger word selection when multiple trigger case. Currently ignored.
 */
static int trigger_status_info(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 3;

	return 0;
}

static int trigger_status_get(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ucontrol->value.bytes.data[0] = (unsigned char)(as33970->is_always_listening);
	ucontrol->value.bytes.data[1] = (unsigned char)(as33970->is_high_performance_trigger);
	ucontrol->value.bytes.data[2] = (unsigned char)(as33970->trigger_model);
	
	return ret;
}

static int trigger_status_put(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	unsigned int is_always_listening = (unsigned int)ucontrol->value.bytes.data[0];
	unsigned int is_high_performance_trigger = (unsigned int)(ucontrol->value.bytes.data[1]);
	unsigned int trigger_model = (unsigned int)ucontrol->value.bytes.data[2];

	mutex_lock(&as33970->cmd_mutex);
	dev_info(as33970->dev, "trigger_status_put enter listening %d, power state %d, model %d", is_always_listening, is_high_performance_trigger, trigger_model);
	if (as33970->is_record_enabled && AUDIO_HOT_WORD != as33970->output_signal) {
		as33970->is_high_performance_trigger = is_high_performance_trigger;
		as33970->is_always_listening = is_always_listening;
		as33970->trigger_model = trigger_model;
		dev_dbg(as33970->dev, "Recording in none trigger mode, store the trigger status\n");
		mutex_unlock(&as33970->cmd_mutex);
		return ret;
	}

	if (as33970->is_record_enabled && 
		(is_always_listening != as33970->is_always_listening ||
		trigger_model != as33970->trigger_model)) {
		dev_dbg(as33970->dev, "Recording in trigger mode, record the trigger status\n");
		mutex_unlock(&as33970->cmd_mutex);
		return ret;
	}

	if (is_always_listening  != as33970->is_always_listening ||
		is_high_performance_trigger != as33970->is_high_performance_trigger ||
		trigger_model        != as33970->trigger_model) {
		ret = as33970_set_always_listening(as33970, is_always_listening, 
											is_high_performance_trigger,
											trigger_model);
		if (ret >= 0 && as33970->is_always_listening)
			as33970->output_signal = AUDIO_HOT_WORD;
		as33970->is_high_performance_trigger = is_high_performance_trigger;
		as33970->is_always_listening = is_always_listening;
		as33970->trigger_model = trigger_model;
	}
	dev_info(as33970->dev, "Actual trigger status: listening %d, power state %d, model %d",
								as33970->fw_is_always_listening,
								as33970->fw_is_high_performance_trigger,
								as33970->fw_trigger_model);
	mutex_unlock(&as33970->cmd_mutex);
	return ret;
}

#define GOOGLE_SOURCE (1)
#define CLOVA_SOURCE (2)
static int trigger_source_get(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = as33970->trigger_source;
	return 0;
}

static int trigger_source_put(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	as33970->trigger_source = (unsigned int)ucontrol->value.integer.value[0];
	return 0;
}

static int as33970_get_output_signals(struct snd_kcontrol *kcontrol,
										struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	ucontrol->value.integer.value[0] = as33970->output_signal;
         
	return 0;
}

static int as33970_set_output_signals(struct snd_kcontrol *kcontrol,
										struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0, select = 0;
	struct as33970_cmd cmd;

	if (as33970->fw_is_record_enabled) {
		dev_err(as33970->dev, "Cannot change output signal while recording\n");
		return -EBUSY;
	}

	select = ucontrol->value.integer.value[0];
		 
	if (select == as33970->output_signal)
		return 0;
	
	mutex_lock(&as33970->cmd_mutex);
	
	switch (select) {
	case AUDIO_HOT_WORD:
		if (!as33970->fw_is_always_listening) {
			dev_err(as33970->dev, "Please enable trigger first!!!\n");
			mutex_unlock(&as33970->cmd_mutex); 
			return -EBUSY;
		} else {
			dev_info(as33970->dev, "Trigger power state %d\n", as33970->fw_is_high_performance_trigger);
		}
		break;
	case AUDIO_COMMUNICATION:
		if (as33970->mode != AS33970_MODE_NORMAL) {
			ret = as33970_set_mode(as33970, AS33970_MODE_NORMAL);
		}
        ret = as33970_set_record_effect(as33970, as33970->is_aec_enabled, as33970->is_nr_enabled);

		break;
	case AUDIO_RECOGNITION:
		if (as33970->mode != AS33970_MODE_TRIGGER) {
			ret = as33970_set_mode(as33970, AS33970_MODE_TRIGGER);
		}
		ret = as33970_sendcmd(as33970, &cmd,
							AS33970_CMD_SET(SYS_CMD_EVENT_PARAM),
							AS33970_ID_ARM, 3,
							EVENT_LP_TRIG_CONTROL, EVENT_PAR_LP_TRIG_STATE, 0);
		msleep(20);
		break;
	case AUDIO_CAMCORDER:
		if (as33970->mode != AS33970_MODE_NORMAL) {
			ret = as33970_set_mode(as33970, AS33970_MODE_NORMAL);
		}
        ret = as33970_set_record_effect(as33970, 0, as33970->is_nr_enabled);
		break;
	case AUDIO_MIC:
		if (as33970->mode != AS33970_MODE_NORMAL_48) {
			ret = as33970_set_mode(as33970, AS33970_MODE_NORMAL_48);
		}
		break;
	case AUDIO_6CH_DUMP:
	default:
		if (as33970->mode != AS33970_MODE_6CH) {
			ret = as33970_set_mode(as33970, AS33970_MODE_6CH);
		}
		break;
	}

	if (ret < 0) {
		dev_err(as33970->dev,
			"Failed to set output signal, ret =%d\n", ret);
	} else {
		as33970->output_signal = select;
		if (as33970->output_signal != AUDIO_HOT_WORD) {
			as33970->fw_is_always_listening = 0;
			as33970->fw_is_high_performance_trigger = 0;
		}
		dev_info(as33970->dev,
			"Set output signal(0x%02x) successfully, ret = %d\n",
			select, ret);
	}
	mutex_unlock(&as33970->cmd_mutex);
	return ret;
}

static int as33970_get_aec(struct snd_kcontrol *kcontrol,
							struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = as33970->is_aec_enabled;

	return 0;
}

static int as33970_set_aec(struct snd_kcontrol *kcontrol,
							struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);

	int ret = 0;
	unsigned int aec_enable = 0;

	aec_enable = ucontrol->value.integer.value[0];

    if (AS33970_MODE_NORMAL != as33970->mode) {
        dev_dbg(as33970->dev, "Record processing control will take effect next time!!!");
        as33970->is_aec_enabled = aec_enable;
        return ret;
    }
	mutex_lock(&as33970->cmd_mutex); 
	ret = as33970_set_record_effect(as33970, aec_enable, as33970->is_nr_enabled);
	mutex_unlock(&as33970->cmd_mutex); 

	return ret;
}

static int as33970_get_nr(struct snd_kcontrol *kcontrol,
							struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = as33970->is_nr_enabled;

	return 0;
}
					 
static int as33970_set_nr(struct snd_kcontrol *kcontrol,
							struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;
	unsigned int nr_enable = 0;
	
	nr_enable = ucontrol->value.integer.value[0];

    if (AS33970_MODE_NORMAL != as33970->mode) {
        dev_dbg(as33970->dev, "Record processing control will take effect next time!!!");
        as33970->is_nr_enabled = nr_enable;
        return ret;
    }
	
	mutex_lock(&as33970->cmd_mutex); 
	ret = as33970_set_record_effect(as33970, as33970->is_aec_enabled, nr_enable);
	mutex_unlock(&as33970->cmd_mutex); 

	return ret;
}

enum {
	FAC_RECORD_MUTE = 0,
	FAC_RECORD_MIC1,
	FAC_RECORD_MIC2,
	FAC_RECORD_ECHO_REF1,
	FAC_RECORD_ECHO_REF2,
	FAC_RECORD_ECHO_REF3,
	FAC_RECORD_ECHO_REF4,
	FAC_TRIGGER,
} fac_test_cmd;

static int fac_test_get(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);

	dev_info(as33970->dev,
		"%s fac_test = %d\n", __func__, as33970->factory_test_mode);
	ucontrol->value.integer.value[0] = as33970->factory_test_mode;

	return 0;
}

static int fac_test_set(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;
	struct as33970_cmd cmd;

	mutex_lock(&as33970->cmd_mutex); 
	as33970->factory_test_mode = ucontrol->value.integer.value[0];
	dev_info(as33970->dev, "%s fac_test = %d\n", __func__, as33970->factory_test_mode);
	switch(as33970->factory_test_mode) {
	case FAC_RECORD_MUTE:
		/*Actually MUTE MIC Here*/
		ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(7),
						AS33970_ID_ARM | 1, 3,
						5, 1, 0);
		break;
	case FAC_RECORD_MIC1:
		ret = as33970_sendcmd(as33970, &cmd, 
                        AS33970_CMD_SET(CHANNEL_MIXER_CMD_CONFIG),
                        AS33970_ID_CPTR | CPTR_MIXER_ID, 2,
                        0x1c0, 0x1c0);
		ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(7),
						AS33970_ID_ARM | 1, 3,
						5, 0, 0);
		break;
	case FAC_RECORD_MIC2:
        ret = as33970_sendcmd(as33970, &cmd, 
                        AS33970_CMD_SET(CHANNEL_MIXER_CMD_CONFIG),
                        AS33970_ID_CPTR | CPTR_MIXER_ID, 2,
                        0x1c1, 0x1c1);
        ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(7),
						AS33970_ID_ARM | 1, 3,
						5, 0, 0);
		break;
	case FAC_RECORD_ECHO_REF1:
        ret = as33970_sendcmd(as33970, &cmd, 
                        AS33970_CMD_SET(CHANNEL_MIXER_CMD_CONFIG),
                        AS33970_ID_CPTR | CPTR_MIXER_ID, 2,
						0x2c0, 0x2c0);
		break;
	case FAC_RECORD_ECHO_REF2:
		ret = as33970_sendcmd(as33970, &cmd, 
						AS33970_CMD_SET(CHANNEL_MIXER_CMD_CONFIG),
						AS33970_ID_CPTR | CPTR_MIXER_ID, 2,
						0x2c1, 0x2c1);
		break;
	case FAC_RECORD_ECHO_REF3:
		ret = as33970_sendcmd(as33970, &cmd, 
						AS33970_CMD_SET(CHANNEL_MIXER_CMD_CONFIG),
						AS33970_ID_CPTR | CPTR_MIXER_ID, 2,
						0x2c2, 0x2c2);
		break;
	case FAC_RECORD_ECHO_REF4:
		ret = as33970_sendcmd(as33970, &cmd, 
						AS33970_CMD_SET(CHANNEL_MIXER_CMD_CONFIG),
						AS33970_ID_CPTR | CPTR_MIXER_ID, 2,
						0x2c3, 0x2c3);
		break;
	case FAC_TRIGGER:
	    ret = as33970_sendcmd(as33970, &cmd,
						AS33970_CMD_SET(SW_CADENCE_CMD_START),
						AS33970_ID_ARM | FWID_SW, 1,
						SYS_CADENCE_LP_TRIGGER_DET);
		break;
	default:
		dev_err(as33970->dev, "%s Unsupported cmd\n", __func__);
		mutex_unlock(&as33970->cmd_mutex); 
		return -EINVAL;
	}
	if (ret < 0) {
		dev_err(as33970->dev, "Failed set factory test %d\n",ret);
		ret = -EINVAL;
	} else {
		dev_info(as33970->dev, "Success to set factory test\n");
	}		
	mutex_unlock(&as33970->cmd_mutex); 
	return ret;
}

static int as33970_set_dai_fmt(struct snd_soc_dai *dai,
			      unsigned int fmt)
{

	struct snd_soc_component *component = dai->component;
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);

	dev_info(as33970->dev, "%s- %08x, dai->id = %d\n", __func__, fmt, dai->id);
	/* set master/slave */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		if (dai->id == AS33970_DAI_DSP) {
			as33970->is_rx_master = 0;
		} else {
			as33970->is_tx_master = 0;
		}
		break;
	default:
		dev_err(as33970->dev, "Unsupported DAI master mode\n");
		return -EINVAL;
	}

	/* set format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	default:
		dev_err(as33970->dev, "Unsupported DAI format\n");
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		dev_err(as33970->dev, "Unsupported Inersion format\n");
		return -EINVAL;
	}

	if (dai->id == AS33970_DAI_DSP)
		as33970->rx_dai_fmt = fmt;
	else
		as33970->tx_dai_fmt = fmt;

	return 0;
}

#define AS33970_SAMPLE_WIDTH_16BIT (1)
#define AS33970_SAMPLE_WIDTH_24BIT (2)

static int as33970_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	const unsigned int sample_rate = params_rate(params);
	unsigned int sample_width, frame_size;
	int ret = 0;

	dev_info(as33970->dev, "Set hw_params, dai->id = %d\n", dai->id);
	sample_width = snd_pcm_format_width(params_format(params));


	if (sample_width < 0)
		return sample_width;

	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0)
		return frame_size;

	dev_info(as33970->dev, "Sample size %d bits, frame = %d bits, rate = %d Hz\n",
		sample_width, frame_size, sample_rate);

	as33970->sample_rate = sample_rate;
	as33970->frame_size = frame_size;

	switch (sample_width) {
	case 24:
		as33970->sample_width = AS33970_SAMPLE_WIDTH_24BIT;
		break;
	default:
		as33970->sample_width = AS33970_SAMPLE_WIDTH_16BIT;
		break;
	}

	return ret;
}

#ifdef AS33970_USE_WORK_QUEUE
 static void as33970_work_queue(struct work_struct *work)
{
	struct as33970_priv *as33970 = container_of(work, struct as33970_priv, work);

	/*start/stop as33970 here*/
	mutex_lock(&as33970->cmd_mutex);
	
	if (as33970->is_record_enabled) {
		as33970_enable_playback(as33970, 1);
		as33970_enable_record(as33970, 1);
	} else {
		as33970_enable_record(as33970, 0);
		if (!(as33970->fw_is_always_listening && as33970->fw_is_high_performance_trigger && as33970->is_playback_enabled)) {
			as33970_enable_playback(as33970, 0);
		}
	}
	mutex_unlock(&as33970->cmd_mutex);
}
#endif

static int as33970_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
   struct snd_soc_component *component = dai->component;
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;
	
	dev_err(as33970->dev, "%s   enter\n", __func__);
	
#ifdef AS33970_BOOT_WITHOUT_FLASH
	if (!as33970->fw_status) {
		dev_err(as33970->dev, "AS33970 FW not load yet\n");
		return -ENODEV;
	}
#endif

	if (as33970->is_record_enabled) {
		dev_err(as33970->dev, "Record already started\n");
		return 0;
	}
	mutex_lock(&as33970->cmd_mutex);
#ifndef AS33970_USE_WORK_QUEUE
	as33970->is_record_enabled = 1;
	as33970_enable_playback(as33970, 1);
	as33970_enable_record(as33970, 1);
#endif
	mutex_unlock(&as33970->cmd_mutex);
	dev_info(as33970->dev, "%s, driver output signal is %d, as33970 mode is 0x%x", __func__,
					as33970->output_signal, as33970->mode);
	dev_info(as33970->dev, "%s exit, playback = %d, record = %d\n", __func__,
					as33970->is_playback_enabled, as33970->is_record_enabled);
	return ret;
}

static void as33970_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
    struct snd_soc_component *component = dai->component;
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_info(as33970->dev, "%s enter\n", __func__);

#ifdef AS33970_BOOT_WITHOUT_FLASH
	if (!as33970->fw_status) {
		dev_err(as33970->dev, "AS33970 FW not load yet\n");
		return;
	}
#endif

	mutex_lock(&as33970->cmd_mutex); 
#ifndef AS33970_USE_WORK_QUEUE
	as33970->is_record_enabled = 0;
	as33970_enable_record(as33970, 0);
	if (!(as33970->fw_is_always_listening && as33970->fw_is_high_performance_trigger && as33970->is_playback_enabled)) {
		as33970_enable_playback(as33970, 0);
	}
#endif	
	if (as33970->is_always_listening && !as33970->fw_is_always_listening) {
		msleep(50);	
		ret = as33970_set_always_listening(as33970, as33970->is_always_listening,
									as33970->is_high_performance_trigger,
									as33970->trigger_model);
		if (ret >=0 )
			as33970->output_signal = AUDIO_HOT_WORD;
	}
	mutex_unlock(&as33970->cmd_mutex);
	dev_info(as33970->dev, "%s exit, playback = %d, record = %d\n", __func__,
					as33970->is_playback_enabled, as33970->is_record_enabled);
}

static int as33970_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct device *dev = component->dev;
	dev_info(dev, "as33970_hw_free\n");

	return 0;
}

static int as33970_trigger(struct snd_pcm_substream *substream,
			      int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
	int ret = 0;
	dev_info(as33970->dev, "as33970_trigger\n");
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#ifdef AS33970_USE_WORK_QUEUE
		as33970->is_record_enabled = 1;
		schedule_work(&as33970->work);
#endif		
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#ifdef AS33970_USE_WORK_QUEUE
		as33970->is_record_enabled = 0;
		schedule_work(&as33970->work);
#endif
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static const char * const as33970_switch[] = {
									"Off", "On"};
static const struct soc_enum as33970_switch_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(as33970_switch),
						as33970_switch),
};

static const char * const trigger_source_value[] = {"None", "Google", "Clova"};
static const struct soc_enum trigger_source_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(trigger_source_value), trigger_source_value),
};

static const char * const as33970_output_signals[] = {
		"Hot Word", "Communication", "Recognition", "Camcorder", "MIC", "6ch Dump"};
static const struct soc_enum as33970_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(as33970_output_signals),
				as33970_output_signals),
};

static const char * const as33970_fac_test[] = {
		"Mute", "MIC1", "MIC2", "Echo Ref 1", "Echo Ref 2", "Echo Ref 3", "Echo Ref 4", "Trigger"};
static const struct soc_enum fac_test_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(as33970_fac_test),
				as33970_fac_test),
};


static const struct snd_kcontrol_new as33970_snd_controls[] = {
#if 0
	AS33970_CONTROL("SendCmd", cmd_info, cmd_get, cmd_put,
			SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_WRITE |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE),
#endif
	SOC_ENUM_EXT("Playback Status", as33970_switch_enum[0], playback_status_get, playback_status_put),
#ifdef AS33970_ENABLE_SCREEN_STATUS
	SOC_ENUM_EXT("Screen Status", as33970_switch_enum[0], screen_status_get, screen_status_put),
#endif
#ifdef AS33970_ENABLE_PHONE_CALL_STATUS
	SOC_ENUM_EXT("Phone Call Status", as33970_switch_enum[0], phone_call_status_get, phone_call_status_put),
#endif
	AS33970_CONTROL("Trigger Status", trigger_status_info, trigger_status_get, trigger_status_put,
			SNDRV_CTL_ELEM_ACCESS_READ |
			SNDRV_CTL_ELEM_ACCESS_WRITE |
			SNDRV_CTL_ELEM_ACCESS_VOLATILE),
	SOC_ENUM_EXT("Trigger Source", trigger_source_enum[0], trigger_source_get, trigger_source_put),
	SOC_ENUM_EXT("Output Signals", as33970_enum[0],
			as33970_get_output_signals, as33970_set_output_signals),
	SOC_ENUM_EXT("AS33970 AEC", as33970_switch_enum[0],
			as33970_get_aec, as33970_set_aec),
	SOC_ENUM_EXT("AS33970 NR", as33970_switch_enum[0],
			as33970_get_nr, as33970_set_nr),
	SOC_ENUM_EXT("AS33970 Factory Test", fac_test_enum[0],
			fac_test_get, fac_test_set),
};

static const struct snd_soc_dapm_widget as33970_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("Mic AIF", "Capture", 0,
			     SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AEC AIF", NULL, 0,
			     SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_OUTPUT("AEC DATA"),
};

static const struct snd_soc_dapm_route as33970_intercon[] = {
	{"Mic AIF", NULL, "MIC"},
	{"AEC AIF", NULL, "Playback"},
	{"AEC DATA", NULL, "AEC AIF"},
};

static struct snd_soc_dai_ops as33970_dai_ops = {
	.set_fmt = as33970_set_dai_fmt,
	.hw_params = as33970_hw_params,
    .startup = as33970_startup,
    .shutdown = as33970_shutdown,
    .hw_free = as33970_hw_free,
	.trigger = as33970_trigger,
};

static struct snd_soc_dai_driver soc_codec_as33970_dai = {
		.name = "as33970-i2s-codec",
		.playback = {
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_48000,
			.formats	= AS33970_FORMATS,
		},
		.capture = {
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_48000,
			.formats	= AS33970_FORMATS,
		},
		.ops = &as33970_dai_ops,
};
#if 0
static int as33970_reset(struct as33970_priv *as33970)
{
	int ret = 0;
#ifndef AS33970_BOOT_WITHOUT_FLASH
#ifndef AS33970_NEED_SET_I2C_SPEED
	unsigned int val = 0;
	unsigned long time_out;
	struct as33970_cmd cmd;
#endif
#endif

	if (as33970->rst_gpio) {
		gpio_set_value_cansleep(as33970->rst_gpio, 0);
		msleep(20);
		gpio_set_value_cansleep(as33970->rst_gpio, 1);
#ifndef AS33970_BOOT_WITHOUT_FLASH
#ifndef AS33970_NEED_SET_I2C_SPEED
		//TODO: We should not delay the system boot here
		msleep(500);

		/* continuously read the first bytes data from device until
		 * either timeout or the device ready.
		 */
		time_out = msecs_to_jiffies(1000);
		time_out += jiffies;
		do {
			ret = regmap_bulk_read(as33970->regmap, 4, &val, 1);
			if (val == AS33970_READY)
				break;
			msleep(20);

		} while (!time_after(jiffies, time_out));

		if (val != AS33970_READY) {
			dev_err(as33970->dev, "\nFailed to reset as33970!\n");
			return -ENODEV;
		}


		ret = as33970_sendcmd(as33970, &cmd,
					AS33970_CMD_GET(SYS_CMD_VERSION),
					AS33970_ID_ARM, 0);
		if (ret > 0) {
			dev_info(as33970->dev, "Firmware version = %d.%d.%d.%d\n",
				cmd.data[0], cmd.data[1],
				cmd.data[2], cmd.data[3]);
		} else {
			dev_err(as33970->dev, "Failed to get firmware version, ret =%d\n",
				ret);
			return  -EIO;
		}
#endif
#endif
	}
	return ret;
}
#endif

static int as33970_probe(struct snd_soc_component *component)
{
	struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);

	//as33970_reset(as33970);
	as33970->component = component;
	dev_info(as33970->dev, "AS33970 device attached\n");

	return 0;
}

static void as33970_remove(struct snd_soc_component *component)
{
	//struct as33970_priv *as33970 = snd_soc_component_get_drvdata(component);
#ifndef AS33970_BOOT_WITHOUT_FLASH
	if (as33970->rst_gpio) {
		gpio_set_value_cansleep(as33970->rst_gpio, 0);
	}
#endif
#ifdef AS33970_USE_WORK_QUEUE
	flush_work(&as33970->work);
#endif
	return;
}

static const struct snd_soc_component_driver soc_component_driver_as33970 = {
	.probe = as33970_probe,
	.remove = as33970_remove,
	.controls = as33970_snd_controls,
	.num_controls = ARRAY_SIZE(as33970_snd_controls),
	.dapm_widgets = as33970_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(as33970_dapm_widgets),
	.dapm_routes = as33970_intercon,
	.num_dapm_routes = ARRAY_SIZE(as33970_intercon),
};

static bool as33970_volatile_register(struct device *dev, unsigned int reg)
{
	return true; /*all register are volatile*/
}

const struct regmap_config as33970_regmap_config = {
	.reg_bits = 16,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = AS33970_REG_MAX,
	.cache_type = REGCACHE_NONE,
	.volatile_reg = as33970_volatile_register,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};
EXPORT_SYMBOL_GPL(as33970_regmap_config);

static irqreturn_t irq_handler(int irq, void *data){
	struct as33970_priv *as33970 = (struct as33970_priv*)data;
	if (!as33970->is_record_enabled) {
		as33970->trigger_source = GOOGLE_SOURCE;
		dev_info(as33970->dev, "Trigger %d detected\n", GOOGLE_SOURCE);
	} else {
		dev_info(as33970->dev, "Skip Trigger %d while recording\n", GOOGLE_SOURCE);
	}
	return IRQ_HANDLED;
}

int as33970_probe_status(void)
{
	return probe_status;
}
EXPORT_SYMBOL_GPL(as33970_probe_status);

static int as33970_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct as33970_priv *as33970;
	int ret, i;
	struct device *dev = &i2c->dev;
	struct regmap *regmap;
	int ret_device_file = 0;
	struct device_node *np = i2c->dev.of_node;

	dev_info(dev, "%s: enter.\n", __func__);
       
	regmap = devm_regmap_init_i2c(i2c, &as33970_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	as33970 = devm_kzalloc(dev, sizeof(*as33970), GFP_KERNEL);
	if (!as33970)
		return -ENOMEM;

	dev_set_drvdata(dev, as33970);
	as33970->regmap = regmap;
	as33970->dev = dev;
	as33970->sample_rate = 48000;
	as33970->sample_width = AS33970_SAMPLE_WIDTH_16BIT;
	as33970->frame_size = 32;  //64;
	as33970->is_rx_master = 0;
	as33970->is_tx_master = 0;
	as33970->is_always_listening = 0;
	as33970->trigger_model = 0;
	as33970->is_high_performance_trigger  = 0;
	as33970->is_playback_enabled = 0;
	as33970->is_record_enabled = 0;
	as33970->is_screen_on = 0;
	as33970->is_phone_call_on = 0;
        as33970->is_aec_enabled = 1;
        as33970->is_nr_enabled = 1;
        as33970->output_signal = AUDIO_RECOGNITION;

#ifdef AS33970_BOOT_WITHOUT_FLASH
	as33970->fw_status = 0;
#else
	as33970->fw_status = 1;
#endif

	as33970->fw_is_playback_enabled = 0;
	as33970->fw_is_record_enabled = 0;
	as33970->fw_is_always_listening = 0;
	as33970->fw_is_high_performance_trigger = 0;
	as33970->fw_trigger_model = 0;

	for (i = 0; i < ARRAY_SIZE(as33970_supplies); i++)
               as33970->supplies[i].supply = as33970_supplies[i];

       as33970->num_supplies = ARRAY_SIZE(as33970_supplies);

       ret = devm_regulator_bulk_get(dev, as33970->num_supplies,
                                       as33970->supplies);
	if (ret != 0) {
		dev_err(dev,
			"Failed to request as33970 supplies: %d\n",
			ret);
		return ret;
	}

	ret = regulator_bulk_enable(as33970->num_supplies, as33970->supplies);
	
	if (ret != 0) {
		dev_err(dev,
			"Failed to enable as33970 supplies: %d\n", ret);
		return ret;
	}

        ret=regulator_set_load(as33970->supplies[0].consumer, 100000);
        if (ret != 0) {
		dev_err(dev,
			"Failed to set vbat3v3 load %d\n", ret);
		return ret;
	}
        
        ret=regulator_set_load(as33970->supplies[1].consumer, 300000);
        if (ret != 0) {
		dev_err(dev,
			"Failed to set avdd13 load %d\n", ret);
		return ret;
	}

	as33970->rst_gpio = of_get_named_gpio(np, "reset-gpios", 0);
       if (as33970->rst_gpio < 0)
               dev_err(dev, "as33970 get reset-gpios failed\n");
       
       if (gpio_is_valid(as33970->rst_gpio)) {
               ret = devm_gpio_request_one(&i2c->dev, as33970->rst_gpio,
                       GPIOF_OUT_INIT_LOW, "AS33970_RST_GPIO");
               if (ret < 0) {
                       dev_err(dev, "%s: error requesting as33970 reset gpio\n",
                               __func__);
                       goto err_gpio_free;
               }
               gpio_set_value_cansleep(as33970->rst_gpio, 0);
       }
       dev_info(dev, "%s: rst_gpio = %d.\n", __func__, as33970->rst_gpio);
       
       msleep(20);
 #ifdef CONFIG_OF
       as33970->avdd13_enable_gpio = of_get_named_gpio(np, "avdd13-enable-gpios", 0);
 #endif
       if (!gpio_is_valid(as33970->avdd13_enable_gpio)) {
               dev_info(as33970->dev, "%s: avdd13_enabel gpio not specified\n", __func__);
               as33970->avdd13_enable_gpio = -1;
       } else {
               ret = gpio_request(as33970->avdd13_enable_gpio, "AS33970_ENABLE_GPIO");
                     
               if (ret < 0) {
                       dev_err(dev, "%s: error requesting avdd13_enable gpio\n",
                               __func__);
                       goto err_gpio_free;
               }
               gpio_set_value_cansleep(as33970->avdd13_enable_gpio, 1);
       }
 
       msleep(30);
       gpio_set_value_cansleep(as33970->rst_gpio, 1);

#ifdef AS33970_NEED_SET_I2C_SPEED
	as33970->is_i2c_speed_set = 0;
#endif

#ifdef CONFIG_OF
	as33970->es7210_vdd_enable = of_get_named_gpio(np, "es7210-vdd-enable", 0);
#endif
        
	if (!gpio_is_valid(as33970->es7210_vdd_enable)) {
		dev_info(as33970->dev, "%s: es7210_vdd_enable gpio not specified\n", __func__);
		as33970->es7210_vdd_enable = -1;
	} else {
	        dev_info(dev, "%s ----", __func__);
		ret = gpio_request(as33970->es7210_vdd_enable, "ES7210_ENABLE_GPIO");
		
		if (ret < 0) {
			dev_err(dev, "%s: error requesting es7210_vdd_enable gpio\n",
				__func__);
			goto err_gpio_free;
		}
		
		gpio_set_value_cansleep(as33970->es7210_vdd_enable, 1);
	}
        dev_info(dev, "%s: es7210_vdd_enable=%d\n", __func__, as33970->es7210_vdd_enable);

        as33970->irq_gpio = of_get_named_gpio(np, "wakeup-gpio", 0);
	if (gpio_is_valid(as33970->irq_gpio)) {
		ret = gpio_request(as33970->irq_gpio, "AS33970_IRQ_GPIO");
		if (ret < 0) {
			dev_err(dev, "AS33970_IRQ_GPIO Request gpio. Fail![%d]\n", ret);
			return ret;
		}
		ret = gpio_direction_input(as33970->irq_gpio);
		if (ret < 0) {
			dev_err(dev, "AS33970 Set gpio direction. Fail![%d]\n", ret);
			return ret;
		}
		gpio_set_debounce(as33970->irq_gpio, 256);/*Hold high for 500ms to indicate trigger*/
		as33970->trigger_source = 0;
		ret = request_irq(gpio_to_irq(as33970->irq_gpio), irq_handler, IRQF_TRIGGER_RISING, "as33970_irq", as33970);
		if (!ret) {
			enable_irq_wake(gpio_to_irq(as33970->irq_gpio));
			dev_info(dev, "AS33970 enable irq successful");
		}
	} else {
		dev_err(dev, "AS33970 Invalid irq gpio num.(init)\n");
	}

	mutex_init(&as33970->cmd_mutex);	
	ret_device_file = device_create_file(dev, &dev_attr_reset_dsp);
	ret_device_file = device_create_file(dev, &dev_attr_fw_status);
	ret_device_file = device_create_file(dev, &dev_attr_fw_version);
	ret_device_file = device_create_file(dev, &dev_attr_load_model);
        #ifdef AS33970_NEED_SET_I2C_SPEED
	ret_device_file = device_create_file(dev, &dev_attr_i2c_speed);
        #endif
        #ifdef AS33970_USE_WORK_QUEUE
        INIT_WORK(&as33970->work, as33970_work_queue);
        #endif
	#ifdef SND_SOC_COMPONENT
	/* register the component */
	ret = snd_soc_register_component(as33970->dev,
				     &soc_component_driver_as33970,
				     &soc_codec_as33970_dai,
				     1);
        #else
	/* register the codec */
	ret = snd_soc_register_codec(as33970->dev, &soc_codec_driver_as33970,
				     soc_codec_as33970_dai,
				     ARRAY_SIZE(soc_codec_as33970_dai));

        #endif

	if (ret < 0)
		goto err;

        probe_status = 1;
        dev_info(dev, "%s: exit\n", __func__);
        return 0;

err_gpio_free:
	if (as33970->avdd13_enable_gpio >= 0)
		gpio_free(as33970->avdd13_enable_gpio);
	if (as33970->rst_gpio >= 0)
		gpio_free(as33970->rst_gpio);
	if (as33970->es7210_vdd_enable >= 0)
		gpio_free(as33970->es7210_vdd_enable);
	
err: 
	dev_err(dev, "Failed to register sound codec(as33970): %d\n", ret);
	return ret;
	
}

static int as33970_i2c_remove(struct i2c_client *client)
{
        #ifdef SND_SOC_COMPONENT
	snd_soc_unregister_component(&client->dev);
	#else
	snd_soc_unregister_codec(&client->dev);
	#endif
	return 0;
}

static void as33970_i2c_shutdown(struct i2c_client *client)
{
        struct as33970_priv *as33970 = i2c_get_clientdata(client);
        
	if (as33970->rst_gpio)
		gpio_set_value_cansleep(as33970->rst_gpio, 0);
         
	msleep(10);
       
	if (as33970->avdd13_enable_gpio)
	         gpio_set_value_cansleep(as33970->avdd13_enable_gpio, 0);

	msleep(10);
        
	regulator_bulk_disable(as33970->num_supplies, as33970->supplies);	
}

const struct of_device_id as33970_dt_ids[] = {
	{ .compatible = "syna,as33970", },
	{ }
};
MODULE_DEVICE_TABLE(of, as33970_dt_ids);

static const struct i2c_device_id as33970_i2c_id[] = {
	{"as33970", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, as33970_i2c_id);

static struct i2c_driver as33970_i2c_driver = {
	.driver = {
		.name = "as33970",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(as33970_dt_ids),
	},
	.id_table = as33970_i2c_id,
	.probe = as33970_i2c_probe,
	.remove = as33970_i2c_remove,
	.shutdown = as33970_i2c_shutdown,
};

module_i2c_driver(as33970_i2c_driver);

MODULE_DESCRIPTION("ASoC AS33970 ALSA SoC Driver");
MODULE_AUTHOR("Rui Qiao <Rui.Qiao@Synaptics.com>");
MODULE_LICENSE("GPL");
