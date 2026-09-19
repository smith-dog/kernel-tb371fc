/*
 * KTZ Semiconductor KTZ8866b LED Driver
 *
 * Copyright (C) 2013 Ideas on board SPRL
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ktz8866ab.h>

#define BL_I2C_ADDRESS			  0x11
#define u8	unsigned char
#define LCD_BL_BIAS_I2C_ID_NAME "ktz8866b"

static DEFINE_MUTEX(read_lock);
static DEFINE_MUTEX(b_store_lock);
int g_ktz8866b_id = 0;
EXPORT_SYMBOL(g_ktz8866b_id);
struct ktz8866_data *ktz8866b_driver_data;
static struct i2c_client *lcd_bl_bias_i2c_client;
static int lcd_bl_bias_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lcd_bl_bias_i2c_remove(struct i2c_client *client);

int lcd_bl_bias_write_byte(unsigned char addr, unsigned char value)
{
	int ret = 0;
	unsigned char write_data[2] = {0};

	write_data[0] = addr;
	write_data[1] = value;

	if (NULL == lcd_bl_bias_i2c_client) {
		dev_warn(&lcd_bl_bias_i2c_client->dev, "[LCD][BL] lcd_bl_bias_i2c_client is null!!\n");
		return -EINVAL;
	}
	ret = i2c_master_send(lcd_bl_bias_i2c_client, write_data, 2);

	if (ret < 0)
		dev_warn(&lcd_bl_bias_i2c_client->dev, "[LCD][BL] i2c write data fail !!\n");

	return ret;
}
EXPORT_SYMBOL(lcd_bl_bias_write_byte);

struct ktz8866_reg ktz8866b_regs[] = {
	{"KTZ8866B_DISP_ID", KTZ8866_DISP_ID},
	{"KTZ8866B_DISP_BC1", KTZ8866_DISP_BC1},
	{"KTZ8866B_DISP_BC2", KTZ8866_DISP_BC2},
	{"KTZ8866B_DISP_BB_LSB", KTZ8866_DISP_BB_LSB},
	{"KTZ8866B_DISP_BB_MSB", KTZ8866_DISP_BB_MSB},
	{"KTZ8866B_DISP_BL_ENABLE", KTZ8866_DISP_BL_ENABLE},
	{"KTZ8866B_DISP_BIAS_CONF1", KTZ8866_DISP_BIAS_CONF1},
	{"KTZ8866B_DISP_BIAS_CONF2", KTZ8866_DISP_BIAS_CONF2},
	{"KTZ8866B_DISP_BIAS_CONF3", KTZ8866_DISP_BIAS_CONF3},
	{"KTZ8866B_DISP_BIAS_BOOST", KTZ8866_DISP_BIAS_BOOST},
	{"KTZ8866B_DISP_BIAS_VPOS", KTZ8866_DISP_BIAS_VPOS},
	{"KTZ8866B_DISP_BIAS_VNEG", KTZ8866_DISP_BIAS_VNEG},
	{"KTZ8866B_DISP_FLAGS", KTZ8866_DISP_FLAGS},
	{"KTZ8866B_DISP_OPTION1", KTZ8866_DISP_OPTION1},
	{"KTZ8866B_DISP_OPTION2", KTZ8866_DISP_OPTION2},
	{"KTZ8866B_DISP_PTD_LSB", KTZ8866_DISP_PTD_LSB},
	{"KTZ8866B_DISP_PTD_MSB", KTZ8866_DISP_PTD_MSB},
	{"KTZ8866B_DISP_DIMMING", KTZ8866_DISP_DIMMING},
	{"KTZ8866B_DISP_FULL_CURRENT", KTZ8866_DISP_FULL_CURRENT},
};

static int lcd_bl_bias_read_byte(u8 regnum)
{
	u8 buffer[1], reg_value[1];
	int res = 0;

	if (NULL == lcd_bl_bias_i2c_client) {
		dev_warn(&lcd_bl_bias_i2c_client->dev, "[LCD][BL] lcd_bl_bias_i2c_client is null!!\n");
		return -EINVAL;
	}

	mutex_lock(&read_lock);

	buffer[0] = regnum;
	res = i2c_master_send(lcd_bl_bias_i2c_client, buffer, 0x1);
	if (res <= 0)	{
	  mutex_unlock(&read_lock);
	  dev_warn(&lcd_bl_bias_i2c_client->dev, "read reg send res = %d\n", res);
	  return res;
	}
	res = i2c_master_recv(lcd_bl_bias_i2c_client, reg_value, 0x1);
	if (res <= 0) {
	  mutex_unlock(&read_lock);
	  dev_warn(&lcd_bl_bias_i2c_client->dev, "read reg recv res = %d\n", res);
	  return res;
	}
	mutex_unlock(&read_lock);

	return reg_value[0];
}

static const struct of_device_id i2c_of_match[] = {
	{ .compatible = "ktz,ktz8866b", },
	{},
};

static const struct i2c_device_id lcd_bl_bias_i2c_id[] = {
	{LCD_BL_BIAS_I2C_ID_NAME, 0},
	{},
};

static struct i2c_driver lcd_bl_bias_i2c_driver = {
/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
	.id_table = lcd_bl_bias_i2c_id,
	.probe = lcd_bl_bias_i2c_probe,
	.remove = lcd_bl_bias_i2c_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = LCD_BL_BIAS_I2C_ID_NAME,
		.of_match_table = i2c_of_match,
	},
};

static int ktz8866b_id_read(void)
{
	int id_b = 0;

	id_b = lcd_bl_bias_read_byte(ktz8866b_regs[0].reg);//read ktz8866a id reg
	if (id_b <= 0)	{
	  dev_warn(&lcd_bl_bias_i2c_client->dev, "read id_b error!!! id_b = 0x%2X\n", id_b);
	  return 0;
	}
	pr_info("%s:read id_b = 0x%2X\n", __func__, id_b);

	return id_b;
}

/*****************************************************************************
 * for ktz8866b to registe
 *****************************************************************************/
static ssize_t ktz8866b_registers_show (struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct ktz8866_data *driver_data = i2c_get_clientdata(lcd_bl_bias_i2c_client);
	int reg_count = 0;
	int i, n = 0;
	u8 value = 0;
	struct ktz8866_reg *regs_p;

	reg_count = sizeof(ktz8866b_regs) / sizeof(ktz8866b_regs[0]);
	regs_p = ktz8866b_regs;

	if (atomic_read(&driver_data->suspended)) {
		pr_err("%s: can't read: driver suspended\n", __func__);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			"Unable to read ktz8866b_i2c_client[0] registers: driver suspended\n");
	} else {
		pr_info("%s: read all registers\n", __func__);
		for (i = 0, n = 0; i < reg_count; i++) {
			value = lcd_bl_bias_read_byte(regs_p[i].reg);
			n += scnprintf(buf + n, PAGE_SIZE - n,
				"%-30s (0x%02x) = 0x%02X\n",
					regs_p[i].name, regs_p[i].reg, value);
		}
	}
	return n;
}

static ssize_t ktz8866b_registers_store (struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ktz8866_data *driver_data = i2c_get_clientdata(lcd_bl_bias_i2c_client);
	unsigned reg;
	unsigned value;

	if (atomic_read(&driver_data->suspended)) {
		pr_err("%s: can't write: driver suspended\n", __func__);
		return -ENODEV;
	}
	sscanf(buf, "%2x %2x", &reg, &value);
	if (value > 0xFF)
		return -EINVAL;
	pr_info("%s: writing reg 0x%2x = 0x%2x\n", __func__, reg, value);

	mutex_lock(&b_store_lock);
	lcd_bl_bias_write_byte(reg, (uint8_t)value);
	mutex_unlock(&b_store_lock);

	return size;
}

static DEVICE_ATTR(ktz8866b_reg, 0664, ktz8866b_registers_show, ktz8866b_registers_store);

static int ktz8866b_common_init(struct i2c_client *client)
{
	struct ktz8866_data *driver_data = NULL;
	int ret = 0;

	/* We should be able to read and write byte data */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err ("%s: I2C_FUNC_I2C not supported\n", __func__);
		return -ENOTSUPP;
	}

	driver_data = kzalloc(sizeof(struct ktz8866_data), GFP_KERNEL);
	if (driver_data == NULL) {
		pr_err ("%s: driver_data kzalloc failed\n", __func__);
		return -ENOMEM;
	}
	memset (driver_data, 0, sizeof (*driver_data));

	driver_data->client = client;
	i2c_set_clientdata (client, driver_data);
	atomic_set(&driver_data->suspended, 0);

	driver_data->led_dev.name = LCD_BL_BIAS_I2C_ID_NAME;
	ret = led_classdev_register (&client->dev, &driver_data->led_dev);
	if (ret) {
		pr_err("%s: led_classdev_register %s failed: %d\n", __func__, LCD_BL_BIAS_I2C_ID_NAME, ret);
		goto led_classdev_register_failed;
	}

	pr_info("%s: register ktz8866b ok\n", __func__);
	ktz8866b_driver_data = driver_data;
	ret = device_create_file(ktz8866b_driver_data->led_dev.dev, &dev_attr_ktz8866b_reg);
	if (ret) {
	  pr_err("%s: ktz8866b unable to create suspend device file for %s: %d\n", __func__, LCD_BL_BIAS_I2C_ID_NAME, ret);
	  goto device_create_file_failed;
	}
	dev_set_drvdata (&client->dev, ktz8866b_driver_data);

	g_ktz8866b_id = ktz8866b_id_read();
	return 0;

device_create_file_failed:
	device_remove_file (ktz8866b_driver_data->led_dev.dev, &dev_attr_ktz8866b_reg);
	kfree (ktz8866b_driver_data);
led_classdev_register_failed:
	led_classdev_unregister(&driver_data->led_dev);
	kfree (driver_data);

	return ret;
}

int lcd_set_bias(bool enable)
{
	dev_warn(&lcd_bl_bias_i2c_client->dev, "bias:value = %d", enable);
	if (enable) {
		//write vsp/vsn reg
		lcd_bl_bias_write_byte(KTZ8866_DISP_BC1, 0XFA); /* BL_CFG1；OVP=40V，线性调光，PWM disabled */
		lcd_bl_bias_write_byte(KTZ8866_DISP_BC2, 0xED); /* BL_CFG2；dimming 512ms */
		lcd_bl_bias_write_byte(KTZ8866_DISP_DIMMING, 0xAA); /* Turn ON/OFF RAMP time  512ms */
		lcd_bl_bias_write_byte(KTZ8866_DISP_OPTION2, 0x37); /* BL_OPTION2；电感4.7uH，BL_CURRENT_LIMIT 2.5A；*/
		lcd_bl_bias_write_byte(KTZ8866_DISP_FULL_CURRENT, 0xAD); /* Backlight Full-scale LED Current 22mA/CH；*/
		lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_BOOST, 0x2E); /* LCD_BOOST_CFG */
		lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_VPOS, 0x24); /* OUTP_CFG，OUTP = 5.8V */
		lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_VNEG, 0x24); /* OUTN_CFG，OUTN = -5.8V */
		lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_CONF1, 0x9D);/* OUTP enable, OUTN disable*/
		lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_CONF1, 0x9F);/* OUTP enable, OUTN enable*/
		lcd_bl_bias_read_byte(KTZ8866_DISP_BIAS_CONF1);
	} else {
		lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_CONF1, 0x9D);/* OUTP enable, OUTN disable*/
		lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_CONF1, 0x99);/* OUTP disable, OUTN disable*/
		lcd_bl_bias_read_byte(KTZ8866_DISP_BIAS_CONF1);
	}
	return 0;
}
EXPORT_SYMBOL(lcd_set_bias);

static int lcd_bl_bias_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	if (NULL == client) {
		dev_warn(&lcd_bl_bias_i2c_client->dev, "[LCD][BL] i2c_client is NULL\n");
		return -EINVAL;
	}

	lcd_bl_bias_i2c_client = client;

	dev_warn(&lcd_bl_bias_i2c_client->dev, "backlight, i2c address: %0x\n", client->addr);

	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_BOOST, 0x2E); /* LCD_BOOST_CFG */
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_VPOS, 0x24); /* OUTP_CFG，OUTP = 5.8V */
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_VNEG, 0x24); /* OUTN_CFG，OUTN = -5.8V */
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_BIAS_CONF1, 0x9F);/* OUTP/OUTN enable ,EXT_EN enable*/
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_BC2, 0xED);/* BL_CFG2；dimming 512ms */
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_BC1, 0XFA); /* BL_CFG1；OVP=40V，线性调光，PWM disabled */
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_DIMMING, 0xAA); /* Turn ON/OFF RAMP time  512ms */
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_OPTION2, 0x37); /* BL_OPTION2；电感4.7uH，BL_CURRENT_LIMIT 2.5A；*/
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_FULL_CURRENT, 0xAD); /* Backlight Full-scale LED Current 22mA/CH；*/
	ret = lcd_bl_bias_write_byte(KTZ8866_DISP_BL_ENABLE, 0x7F); /* BL enabled and Current sink 1/2/3/4/5/6 enabled；*/
	if (ret < 0) {
		dev_warn(&lcd_bl_bias_i2c_client->dev, "I2C write reg is fail!");
		return -EINVAL;
	} else {
		dev_warn(&lcd_bl_bias_i2c_client->dev, "I2C write reg is success!");
	}

	ktz8866b_common_init(lcd_bl_bias_i2c_client);
	return 0;
}

static int lcd_bl_bias_i2c_remove(struct i2c_client *client)
{
	struct ktz8866_data *ktz8866b_driver_data = i2c_get_clientdata(lcd_bl_bias_i2c_client);
	struct ktz8866_data *driver_data = i2c_get_clientdata(lcd_bl_bias_i2c_client);

	device_remove_file (ktz8866b_driver_data->led_dev.dev, &dev_attr_ktz8866b_reg);
	kfree (ktz8866b_driver_data);
	led_classdev_unregister(&driver_data->led_dev);
	kfree (driver_data);

	lcd_bl_bias_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

module_i2c_driver(lcd_bl_bias_i2c_driver);

MODULE_AUTHOR("caiyifeng <caiyifeng@huanqin.com>");
MODULE_DESCRIPTION("LCD BL I2C Driver");
MODULE_LICENSE("GPL");