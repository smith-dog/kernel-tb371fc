/*
 * Copyright (C) 2021 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include "CTN730_i2c.h"
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>

#define MAX_BUFFER_SIZE	512


#define CHIP "CTN730"
#define DRIVER_CARD "CTN730 NFC"
#define DRIVER_DESC "NFC WLC driver for CTN730 "

struct ctn730_dev	{
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct i2c_client *client;
	struct miscdevice ctn730_device;
	int ven_gpio;
	int irq_gpio;
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
};

/**********************************************************
 * Interrupt control and handler
 **********************************************************/
 
static void ctn730_disable_irq(struct ctn730_dev *ctn730_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&ctn730_dev->irq_enabled_lock, flags);
	if (ctn730_dev->irq_enabled) {
		disable_irq_nosync(ctn730_dev->client->irq);
		ctn730_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&ctn730_dev->irq_enabled_lock, flags);
}

static irqreturn_t ctn730_dev_irq_handler(int irq, void *dev_id)
{
	struct ctn730_dev *ctn730_dev = dev_id;

	ctn730_disable_irq(ctn730_dev);

	/* Wake up waiting readers */
	wake_up(&ctn730_dev->read_wq);

	return IRQ_HANDLED;
}

/**********************************************************
 * private functions
 **********************************************************/
 
static int ctn730_enable(struct ctn730_dev *dev)
{
		pr_info("%s power on\n", __func__);
		if (gpio_is_valid(dev->ven_gpio))
			gpio_set_value_cansleep(dev->ven_gpio, 1); 
		msleep(100);


	return 0;

}

static void ctn730_disable(struct ctn730_dev *dev)
{
	/* power off */
	pr_info("%s power off\n", __func__);
	if (gpio_is_valid(dev->ven_gpio))
		gpio_set_value_cansleep(dev->ven_gpio, 0);
	msleep(100);

}

/**********************************************************
 * driver functions
 **********************************************************/
 
 
static ssize_t ctn730_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct ctn730_dev *ctn730_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	pr_debug("%s : reading %zu bytes.\n", __func__, count);

	mutex_lock(&ctn730_dev->read_mutex);

	if (!gpio_get_value(ctn730_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		while (1) {
			ctn730_dev->irq_enabled = true;
			enable_irq(ctn730_dev->client->irq);
			ret = wait_event_interruptible(
					ctn730_dev->read_wq,
					!ctn730_dev->irq_enabled);
			ctn730_disable_irq(ctn730_dev);
			if (ret)
				goto fail;
			if (gpio_get_value(ctn730_dev->irq_gpio))
				break;

			pr_warning("%s: spurious interrupt detected\n", __func__);
		}
	}

	/* Read data */
	ret = i2c_master_recv(ctn730_dev->client, tmp, count);

	mutex_unlock(&ctn730_dev->read_mutex);

	/* ctn730 seems to be slow in handling I2C read requests
	 * so add 1ms delay after recv operation */
	udelay(1000);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&ctn730_dev->read_mutex);
	return ret;
}

static ssize_t ctn730_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct ctn730_dev  *ctn730_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	ctn730_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	pr_debug("%s : writing %zu bytes.\n", __func__, count);
	/* Write data */
	ret = i2c_master_send(ctn730_dev->client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	/* ctn730 seems to be slow in handling I2C write requests
	 * so add 1ms delay after I2C send oparation */
	udelay(1000);

	return ret;
}

static int ctn730_dev_open(struct inode *inode, struct file *filp)
{
	struct ctn730_dev *ctn730_dev = container_of(filp->private_data,
											   struct ctn730_dev,
											   ctn730_device);

	filp->private_data = ctn730_dev;

	pr_info("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static int ctn730_dev_release(struct inode *inode, struct file *filp)
{
	pr_info("%s : closing %d,%d\n", __func__, imajor(inode), iminor(inode));
	return 0;
}

// manually reset device via GPIO
static long  ctn730_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct ctn730_dev *ctn730_dev = filp->private_data;

	pr_info("%s, cmd=%d, arg=%lu\n", __func__, cmd, arg);
	switch (cmd) {
	case ctn730_SET_PWR:

		if (arg == 1) {
			/* power on */
			ctn730_enable(ctn730_dev);
		} else  if (arg == 0) {
			/* power off */
			ctn730_disable(ctn730_dev);
		} else {
			pr_err("%s bad SET_PWR arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations ctn730_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= ctn730_dev_read,
	.write	= ctn730_dev_write,
	.open	= ctn730_dev_open,
	.release  = ctn730_dev_release,
	.unlocked_ioctl  = ctn730_dev_ioctl,
};


/*
 * Handlers for alternative sources of platform_data
 */
/*
 * Translate OpenFirmware node properties into platform_data
 */
 
 // Remaps device tree gpio definition with the
static int ctn730_get_pdata(struct device *dev,
							struct ctn730_i2c_platform_data *pdata)
{
	struct device_node *node;
	u32 flags;
	int val;

	/* make sure there is actually a device tree node */
	node = dev->of_node;
	if (!node)
		return -ENODEV;

	memset(pdata, 0, sizeof(*pdata));

	/* read the dev tree data */

	/* ven pin - enable's power to the chip - REQUIRED */
	val = of_get_named_gpio_flags(node, "enable-gpios", 0, &flags);
	if (val >= 0) {
		pdata->ven_gpio = val;
	}
	else {
		dev_err(dev, "VEN GPIO error getting from OF node\n");
		return val;
	}


	/* irq pin - data available irq - REQUIRED */
	val = of_get_named_gpio_flags(node, "interrupt-gpios", 0, &flags);
	if (val >= 0) {
		pdata->irq_gpio = val;
	}
	else {
		dev_err(dev, "IRQ GPIO error getting from OF node\n");
		return val;
	}


	return 0;
}

/*
 * ctn730_probe
 */

static int ctn730_probe(struct i2c_client *client,
		const struct i2c_device_id *id)

{
	int ret;
	struct ctn730_i2c_platform_data *pdata; // gpio values, from board file or DT
	struct ctn730_i2c_platform_data tmp_pdata;
	struct ctn730_dev *ctn730_dev; // internal device specific data

	pr_err("%s\n", __func__);

	/* ---- retrieve the platform data ---- */
	/* If the dev.platform_data is NULL, then */
	/* attempt to read from the device tree */
	if(!client->dev.platform_data)
	{
		ret = ctn730_get_pdata(&(client->dev), &tmp_pdata);
		if(ret){
			return ret;
		}

		pdata = &tmp_pdata;
	}
	else
	{
		pdata = client->dev.platform_data;
	}

	if (pdata == NULL) {
		pr_err("%s : nfc probe fail\n", __func__);
		return  -ENODEV;
	}

	/* validate the the adapter has basic I2C functionality */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	/* reserve the GPIO pins */
	pr_info("%s: request irq_gpio %d\n", __func__, pdata->irq_gpio);
	ret = gpio_request(pdata->irq_gpio, "nfc_int");
	if (ret){
		pr_err("%s :not able to get GPIO irq_gpio\n", __func__);
		return  -ENODEV;
	}
	ret = gpio_to_irq(pdata->irq_gpio);
	if (ret < 0){
		pr_err("%s :not able to map GPIO irq_gpio to an IRQ\n", __func__);
		goto err_ven;
	}
	else{
		client->irq = ret;
	}

	pr_info("%s: request ven_gpio %d\n", __func__, pdata->ven_gpio);
	ret = gpio_request(pdata->ven_gpio, "nfc_ven");
	if (ret){
		pr_err("%s :not able to get GPIO ven_gpio\n", __func__);
		goto err_ven;
	}

	/* allocate the ctn730 driver information structure */
	ctn730_dev = kzalloc(sizeof(*ctn730_dev), GFP_KERNEL);
	if (ctn730_dev == NULL) {
		dev_err(&client->dev, "failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	/* store the platform data in the driver info struct */
	ctn730_dev->irq_gpio = pdata->irq_gpio;
	ctn730_dev->ven_gpio = pdata->ven_gpio;

	ctn730_dev->client = client;

	/* finish configuring the I/O */
	ret = gpio_direction_input(ctn730_dev->irq_gpio);
	if (ret < 0) {
		pr_err("%s :not able to set irq_gpio as input\n", __func__);
		goto err_exit;
	}

	ret = gpio_direction_output(ctn730_dev->ven_gpio, 0);
	if (ret < 0) {
		pr_err("%s : not able to set ven_gpio as output\n", __func__);
		goto err_exit;
	}


	/* init mutex and queues */
	init_waitqueue_head(&ctn730_dev->read_wq);
	mutex_init(&ctn730_dev->read_mutex);
	spin_lock_init(&ctn730_dev->irq_enabled_lock);

	/* register as a misc device - character based with one entry point */
	ctn730_dev->ctn730_device.minor = MISC_DYNAMIC_MINOR;
	ctn730_dev->ctn730_device.name = CHIP;
	ctn730_dev->ctn730_device.fops = &ctn730_dev_fops;
	ret = misc_register(&ctn730_dev->ctn730_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	ctn730_dev->irq_enabled = true;
	ret = request_irq(client->irq, ctn730_dev_irq_handler,
				IRQF_TRIGGER_HIGH, client->name, ctn730_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
	ctn730_disable_irq(ctn730_dev);

	i2c_set_clientdata(client, ctn730_dev);

	pr_err("%s : success\n", __func__);
	return 0;

err_request_irq_failed:
	misc_deregister(&ctn730_dev->ctn730_device);
err_misc_register:
err_exit:
err_ven:
	gpio_free(pdata->irq_gpio);
	return ret;
}


static int ctn730_remove(struct i2c_client *client)

{
	struct ctn730_dev *ctn730_dev;

	pr_info("%s\n", __func__);

	ctn730_dev = i2c_get_clientdata(client);
	free_irq(client->irq, ctn730_dev);
	misc_deregister(&ctn730_dev->ctn730_device);
	mutex_destroy(&ctn730_dev->read_mutex);
	gpio_free(ctn730_dev->irq_gpio);
	gpio_free(ctn730_dev->ven_gpio);


	kfree(ctn730_dev);

	return 0;
}

/*
 *
 */

static struct of_device_id ctn730_dt_match[] = {
	{ .compatible = "nxp,ctn730", },
	{},
};
MODULE_DEVICE_TABLE(of, ctn730_dt_match);


static const struct i2c_device_id ctn730_id[] = {
	{ "ctn730", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ctn730_id);

static struct i2c_driver ctn730_driver = {
	.id_table	= ctn730_id,
	.probe		= ctn730_probe,
	.remove		= ctn730_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ctn730",
		.of_match_table = ctn730_dt_match,
	},
};

/*
 * module load/unload record keeping
 */

static int __init ctn730_dev_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&ctn730_driver);
}

static void __exit ctn730_dev_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&ctn730_driver);
}

module_init(ctn730_dev_init);
module_exit(ctn730_dev_exit);

MODULE_AUTHOR("Michael Lee");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
