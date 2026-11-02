/*
 * HID over I2C protocol implementation
 *
 * Copyright (c) 2012 Benjamin Tissoires <benjamin.tissoires@gmail.com>
 * Copyright (c) 2012 Ecole Nationale de l'Aviation Civile, France
 * Copyright (c) 2012 Red Hat, Inc
 *
 * This code is partly based on "USB HID support for Linux":
 *
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2005 Vojtech Pavlik <vojtech@suse.cz>
 *  Copyright (c) 2005 Michael Haboustak <mike-@cinci.rr.com> for Concept2, Inc
 *  Copyright (c) 2007-2008 Oliver Neukum
 *  Copyright (c) 2006-2010 Jiri Kosina
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/hid.h>
#include <linux/mutex.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <linux/platform_data/i2c-hid.h>

#include <linux/bootinfo.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "../hid-ids.h"
#include "i2c-hid.h"

/* quirks to control the device */
#define I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV	BIT(0)
#define I2C_HID_QUIRK_NO_IRQ_AFTER_RESET	BIT(1)
#define I2C_HID_QUIRK_NO_RUNTIME_PM		BIT(2)
#define I2C_HID_QUIRK_DELAY_AFTER_SLEEP		BIT(3)
#define I2C_HID_QUIRK_BOGUS_IRQ			BIT(4)
#define I2C_HID_QUIRK_RESET_ON_RESUME		BIT(5)
#define I2C_HID_QUIRK_BAD_INPUT_SIZE		BIT(6)
#define I2C_HID_QUIRK_WAKE_UP_SYS		BIT(7)
#define I2C_HID_QUIRK_SHOULD_SKIP_SET_PWR	BIT(8)
#define I2C_HID_QUIRK_SET_PWR_ON_SHUTDOWN	BIT(9)

/* flags */
#define I2C_HID_STARTED		0
/* bit index 1, i.e. mask 0x2 in stock flags@0x58 */
#define I2C_HID_RESET_PENDING	1
/* bit index 2, i.e. mask 0x4 for data-bearing commands */
#define I2C_HID_COMMAND_IN_PROGRESS	2

_Static_assert(BIT(I2C_HID_RESET_PENDING) == 0x2,
	       "stock flags bit1 is RESET_PENDING");
_Static_assert(BIT(I2C_HID_COMMAND_IN_PROGRESS) == 0x4,
	       "stock data-bearing command uses flags bit2");
_Static_assert(I2C_HID_QUIRK_NO_IRQ_AFTER_RESET == BIT(1),
	       "stock quirks bit1 is NO_IRQ_AFTER_RESET");

#define I2C_HID_PWR_ON		0x00
#define I2C_HID_PWR_SLEEP	0x01
#define I2C_HID_PWR_SHUTDOWN	0x02

/* debug option */
static bool debug = 0;
module_param(debug, bool, 0444);
MODULE_PARM_DESC(debug, "print a lot of debug information");

#define i2c_hid_dbg(ihid, fmt, arg...)					  \
do {									  \
	if (debug)							  \
		dev_printk(KERN_DEBUG, &(ihid)->client->dev, fmt, ##arg); \
} while (0)

static int kb_connect = 0;
/* TASK-081 D86 (Round3 audit M3): stock keyboard soft power key.
 * Stock keeps a dedicated input_dev at 0xa4f8118 and injects KEY_POWER from
 * i2c_hid_irq when a 0x06 status frame reports online==1 outside of the
 * resume path (stock 0xffffff8008cb3c58-d8: ctx+4 = inbuf[3] bit0,
 * resume flag 0xa4f8168 gate, then bl kb_report_power_key). */
static struct input_dev *kb_input_dev;
/* TASK-081 D91: stock keyboard sysfs layout (Round3 audit M3 closure).
 * Stock creat_kb_status_node (0x8cb1aa8) does kobject_create_and_add
 * (0x937a7f8) named kb_status at the kernel top level plus
 * sysfs_create_group (0x835e970, group 0xa360840) with three attrs:
 * kb_status_value (kb_status_show, ctx+4 single int), kb_hall_value
 * (kb_hall_show, ctx+8), kb_vid_pid (kb_id_show, ctx+0x38/0x3c).
 * The inbuf[3] hall bit1 feeds ctx+8 in stock i2c_hid_irq
 * (0x8cb3c58-90). */
static struct kobject *kb_status_kobj;
static int kb_report_power_key(void);
static int report_special_key(int key, int value);
struct i2c_hid;
static bool handle_special_key_frame(struct i2c_hid *ihid);
static bool spec_key_lockscreen;
static bool spec_key_switchlanguage;
static bool spec_key_micdisable;
static bool spec_key_touchpanelmute;
static bool spec_key_globalsearch;
static bool spec_key_fullscreen;
static bool spec_key_splitscreen;
static bool spec_key_superintcon;
static bool spec_key_customerapp1;
static bool spec_key_customerapp2;
static char kb_status = 0;
extern unsigned int pm_wakeup_irq;
#define VID_KB_LENOVO 0x17ef
#define PID_KB_LENOVO 0x6127
#define PID_KB_LENOVO_V04 0x6175
#define VID_KB_LENOVO_TP 0x04f3
#define PID_KB_LENOVO_TP 0x31a8

/*Spruce code for OSPURCET-1235 by chenzm9 at 2023/2/16 start*/
int mcu_resume_gpio = 0;
int mcu_rst_gpio = 0;
struct regulator *ldo_keyboard;
bool g_already_sleep = false;
static bool is_mcu_init = false;
void kb_i2c_suspend(void);
int kb_i2c_resume(void);
/*Spruce code for OSPURCET-1235 by chenzm9 at 2023/2/16 end*/

/* Spruce code for OSPURCET-780 by sunft3 at 2023/01/18 start */
struct i2c_hid *g_ihid=NULL;
struct i2c_client *g_client=NULL;
struct hid_device *g_phid=NULL;
/* Spruce code for OSPURCET-780 by sunft3 at 2023/01/18 end */

/* Spruce code for OSPURCET-780 by sunft3 at 2023/01/18 start */
extern void hidinput_disconnect(struct hid_device *hid);
extern int hidinput_connect(struct hid_device *hid, unsigned int force);
static unsigned char g_screen_on = 0;
/* Spruce code for OSPURCET-780 by sunft3 at 2023/01/18 end */

struct i2c_hid_desc {
	__le16 wHIDDescLength;
	__le16 bcdVersion;
	__le16 wReportDescLength;
	__le16 wReportDescRegister;
	__le16 wInputRegister;
	__le16 wMaxInputLength;
	__le16 wOutputRegister;
	__le16 wMaxOutputLength;
	__le16 wCommandRegister;
	__le16 wDataRegister;
	__le16 wVendorID;
	__le16 wProductID;
	__le16 wVersionID;
	__le32 reserved;
} __packed;

struct i2c_hid_cmd {
	unsigned int registerIndex;
	__u8 opcode;
	unsigned int length;
	bool wait;
};

union command {
	u8 data[0];
	struct cmd {
		__le16 reg;
		__u8 reportTypeID;
		__u8 opcode;
	} __packed c;
};

#define I2C_HID_CMD(opcode_) \
	.opcode = opcode_, .length = 4, \
	.registerIndex = offsetof(struct i2c_hid_desc, wCommandRegister)

/* fetch HID descriptor */
static const struct i2c_hid_cmd hid_descr_cmd = { .length = 2 };
/* fetch report descriptors */
static const struct i2c_hid_cmd hid_report_descr_cmd = {
		.registerIndex = offsetof(struct i2c_hid_desc,
			wReportDescRegister),
		.opcode = 0x00,
		.length = 2 };
/* commands */
static const struct i2c_hid_cmd hid_reset_cmd =		{ I2C_HID_CMD(0x01),
							  .wait = true };
static const struct i2c_hid_cmd hid_get_report_cmd =	{ I2C_HID_CMD(0x02) };
static const struct i2c_hid_cmd hid_set_report_cmd =	{ I2C_HID_CMD(0x03) };
static const struct i2c_hid_cmd hid_set_power_cmd =	{ I2C_HID_CMD(0x08) };
static const struct i2c_hid_cmd hid_no_cmd =		{ .length = 0 };

/*
 * These definitions are not used here, but are defined by the spec.
 * Keeping them here for documentation purposes.
 *
 * static const struct i2c_hid_cmd hid_get_idle_cmd = { I2C_HID_CMD(0x04) };
 * static const struct i2c_hid_cmd hid_set_idle_cmd = { I2C_HID_CMD(0x05) };
 * static const struct i2c_hid_cmd hid_get_protocol_cmd = { I2C_HID_CMD(0x06) };
 * static const struct i2c_hid_cmd hid_set_protocol_cmd = { I2C_HID_CMD(0x07) };
 */

/* The main device structure */
struct i2c_hid {
	struct i2c_client	*client;	/* i2c client */
	struct hid_device	*hid;	/* pointer to corresponding HID dev */
	union {
		__u8 hdesc_buffer[sizeof(struct i2c_hid_desc)];
		struct i2c_hid_desc hdesc;	/* the HID Descriptor */
	};
	__le16			wHIDDescRegister; /* location of the i2c
						   * register of the HID
						   * descriptor. */
	unsigned int		bufsize;	/* i2c buffer size */
	u8			*inbuf;		/* Input buffer */
	u8			*rawbuf;	/* Raw Input buffer */
	u8			*cmdbuf;	/* Command buffer */
	u8			*argsbuf;	/* Command arguments buffer */

	unsigned long		flags;		/* device flags */
	unsigned long		quirks;		/* Various quirks */

	wait_queue_head_t	wait;		/* For waiting the interrupt */

	struct i2c_hid_platform_data pdata;

	bool			irq_wake_enabled;
	struct mutex		reset_lock;

	unsigned long		sleep_delay;
};
_Static_assert(offsetof(struct i2c_hid, flags) == 0x58,
	       "stock i2c-hid flags offset");
_Static_assert(offsetof(struct i2c_hid, quirks) == 0x60,
	       "stock i2c-hid quirks offset");
_Static_assert(offsetof(struct i2c_hid, wait) == 0x68,
	       "stock i2c-hid waitqueue offset");

/* stock i2c_hid_probe allocates a 0x98-byte object into the exported
 * device_keyboard symbol. Only the fields consumed by this driver are
 * modeled here; the remaining storage preserves the stock object size. */
struct keyboard_device {
	u8 status;
	u8 reserved0[3];
	int online;
	int hall;
	u8 reserved[0x98 - 12];
};
_Static_assert(offsetof(struct keyboard_device, status) == 0x00,
	       "stock keyboard status offset");
_Static_assert(offsetof(struct keyboard_device, online) == 0x04,
	       "stock keyboard online offset");
_Static_assert(offsetof(struct keyboard_device, hall) == 0x08,
	       "stock keyboard hall offset");
struct keyboard_device *device_keyboard;
EXPORT_SYMBOL(device_keyboard);

static const struct i2c_hid_quirks {
	__u16 idVendor;
	__u16 idProduct;
	__u32 quirks;
} i2c_hid_quirks[] = {
	{ USB_VENDOR_ID_WEIDA, USB_DEVICE_ID_WEIDA_8752,
		I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV },
	{ USB_VENDOR_ID_WEIDA, USB_DEVICE_ID_WEIDA_8755,
		I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV },
	/* Stock quirks decision tree (Image-b 0x8cb36e8-0x8cb3774,
	 * Unicorn sweep quirks_sweep6.py): 0x17ef/0x6127 does not
	 * match any stock sub-entry and resolves to 0. */
	{ VID_KB_LENOVO, PID_KB_LENOVO, 0 },
	/* 0x17ef/0x6175 and 0x04f3/0x31a8: stock quirks = 0x7 (bit0|bit1|bit2) */
	{ VID_KB_LENOVO, PID_KB_LENOVO_V04,
		 I2C_HID_QUIRK_NO_IRQ_AFTER_RESET |
		 I2C_HID_QUIRK_NO_RUNTIME_PM |
		 I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV },
	{ VID_KB_LENOVO_TP, PID_KB_LENOVO_TP,
		 I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV |
		 I2C_HID_QUIRK_NO_IRQ_AFTER_RESET |
		 I2C_HID_QUIRK_NO_RUNTIME_PM },
	/* 0x32cd/0x003a: stock quirks = 0x7 (bit0|bit1|bit2) exactly */
	{ 0x32cd, 0x003a,
		 I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV | I2C_HID_QUIRK_NO_IRQ_AFTER_RESET |
		 I2C_HID_QUIRK_NO_RUNTIME_PM },
	{ 0, 0 }
};

/*
 * i2c_hid_lookup_quirk: return any quirks associated with a I2C HID device
 * @idVendor: the 16-bit vendor ID
 * @idProduct: the 16-bit product ID
 *
 * Returns: a u32 quirks value.
 */
static u32 i2c_hid_lookup_quirk(const u16 idVendor, const u16 idProduct)
{
	u32 quirks = 0;
	int n;

	printk("hid-keyboard,%s,1\n", __func__);
	for (n = 0; i2c_hid_quirks[n].idVendor; n++)
		if (i2c_hid_quirks[n].idVendor == idVendor &&
		    (i2c_hid_quirks[n].idProduct == (__u16)HID_ANY_ID ||
		     i2c_hid_quirks[n].idProduct == idProduct))
			quirks = i2c_hid_quirks[n].quirks;
	printk("hid-keyboard,%s,2\n", __func__);

	return quirks;
}

static int i2c_hid_wait_reset(struct i2c_hid *ihid)
{
	struct wait_queue_entry wait;
	long timeout;

	init_wait_entry(&wait, 0);
	prepare_to_wait_event(&ihid->wait, &wait, TASK_UNINTERRUPTIBLE);
	if (!test_bit(I2C_HID_RESET_PENDING, &ihid->flags)) {
		finish_wait(&ihid->wait, &wait);
		return 0;
	}

	for (;;) {
		timeout = schedule_timeout(1250);
		prepare_to_wait_event(&ihid->wait, &wait, TASK_UNINTERRUPTIBLE);
		if (!test_bit(I2C_HID_RESET_PENDING, &ihid->flags)) {
			finish_wait(&ihid->wait, &wait);
			return 0;
		}
		if (!timeout) {
			finish_wait(&ihid->wait, &wait);
			return -ENODATA;
		}
	}
}

static int __i2c_hid_command(struct i2c_client *client,
		const struct i2c_hid_cmd *command, u8 reportID,
		u8 reportType, u8 *args, int args_len,
		unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	union command *cmd = (union command *)ihid->cmdbuf;
	int ret;
	struct i2c_msg msg[2];
	int msg_num = 1;

	int length = command->length;
	bool wait = command->wait;
	unsigned int registerIndex = command->registerIndex;

	/* special case for hid_descr_cmd */
	if (command == &hid_descr_cmd) {
		cmd->c.reg = ihid->wHIDDescRegister;
	} else {
		cmd->data[0] = ihid->hdesc_buffer[registerIndex];
		cmd->data[1] = ihid->hdesc_buffer[registerIndex + 1];
	}

	if (length > 2) {
		cmd->c.opcode = command->opcode;
		cmd->c.reportTypeID = reportID | reportType << 4;
	}

	memcpy(cmd->data + length, args, args_len);
	length += args_len;

	/* Stock cmd dump (Image-b 0x9bed06ac). */
	printk("hid-keyboard,%s,length=%d,cmd=%*ph\n",
	       "i2c_hid_command", length, length, cmd->data);

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = length;
	msg[0].buf = cmd->data;
	if (data_len > 0) {
		msg[1].addr = client->addr;
		msg[1].flags = client->flags & I2C_M_TEN;
		msg[1].flags |= I2C_M_RD;
		msg[1].len = data_len;
		msg[1].buf = buf_recv;
		msg_num = 2;
	}

	/* Stock sets RESET_PENDING for every wait command; data-bearing
	 * commands also hold the bit2 in-progress gate while the transfer runs. */
	if (wait)
		set_bit(I2C_HID_RESET_PENDING, &ihid->flags);

	if (data_len > 0)
		set_bit(I2C_HID_COMMAND_IN_PROGRESS, &ihid->flags);

	ret = i2c_transfer(client->adapter, msg, msg_num);

	if (data_len > 0)
		clear_bit(I2C_HID_COMMAND_IN_PROGRESS, &ihid->flags);

	if (ret != msg_num)
		return ret < 0 ? ret : -EIO;

	ret = 0;

	/*
	 * Stock wait path: an unacknowledged transfer sleeps one 100ms quantum;
	 * an acknowledged transfer uses init_wait_entry/prepare_to_wait_event
	 * with a 1250-jiffy schedule_timeout loop.  The IRQ clears
	 * RESET_PENDING when the reset completion arrives.
	 */
	if (wait) {
		/* Stock wait prints (Image-b 0x9bed09f4/0x9bed06e2/0x9bed06fe). */
		printk("hid-keyboard,%s,waiting...\n", "i2c_hid_command");
		printk("hid-keyboard,%s,1 i2c_wait=%d\n",
		       "i2c_hid_command", wait);
		if (ihid->quirks & I2C_HID_QUIRK_NO_IRQ_AFTER_RESET)
			msleep(100);
		else
			ret = i2c_hid_wait_reset(ihid);
		printk("hid-keyboard,%s,finished.\n", "i2c_hid_command");
		/* Stock wait exit print (Image-b 0x9bed0a3d). */
		printk("hid-keyboard,%s,2 i2c_wait=%d\n",
		       "i2c_hid_command", wait);
	}

	return ret;
}

static int i2c_hid_command(struct i2c_client *client,
		const struct i2c_hid_cmd *command,
		unsigned char *buf_recv, int data_len)
{
	return __i2c_hid_command(client, command, 0, 0, NULL, 0,
				buf_recv, data_len);
}

static int i2c_hid_get_report(struct i2c_client *client, u8 reportType,
		u8 reportID, unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 args[3];
	int ret;
	int args_len = 0;
	u16 readRegister = le16_to_cpu(ihid->hdesc.wDataRegister);

	i2c_hid_dbg(ihid, "%s\n", __func__);
	if (reportID >= 0x0F) {
		args[args_len++] = reportID;
		reportID = 0x0F;
	}

	args[args_len++] = readRegister & 0xFF;
	args[args_len++] = readRegister >> 8;

	ret = __i2c_hid_command(client, &hid_get_report_cmd, reportID,
		reportType, args, args_len, buf_recv, data_len);
	if (ret) {
		printk("hid-keyboard,%s,failed to retrieve report from device.\n",
		       "i2c_hid_get_raw_report");
		return ret;
	}

	return 0;
}

/**
 * i2c_hid_set_or_send_report: forward an incoming report to the device
 * @client: the i2c_client of the device
 * @reportType: 0x03 for HID_FEATURE_REPORT ; 0x02 for HID_OUTPUT_REPORT
 * @reportID: the report ID
 * @buf: the actual data to transfer, without the report ID
 * @len: size of buf
 * @use_data: true: use SET_REPORT HID command, false: send plain OUTPUT report
 */
static int i2c_hid_set_or_send_report(struct i2c_client *client, u8 reportType,
		u8 reportID, unsigned char *buf, size_t data_len, bool use_data)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 *args = ihid->argsbuf;
	const struct i2c_hid_cmd *hidcmd;
	int ret;
	u16 dataRegister = le16_to_cpu(ihid->hdesc.wDataRegister);
	u16 outputRegister = le16_to_cpu(ihid->hdesc.wOutputRegister);
	u16 maxOutputLength = le16_to_cpu(ihid->hdesc.wMaxOutputLength);
	u16 size;
	int args_len;
	int index = 0;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	if (data_len > ihid->bufsize)
		return -EINVAL;

	size =		2			/* size */ +
			(reportID ? 1 : 0)	/* reportID */ +
			data_len		/* buf */;
	args_len =	(reportID >= 0x0F ? 1 : 0) /* optional third byte */ +
			2			/* dataRegister */ +
			size			/* args */;

	if (!use_data && maxOutputLength == 0)
		return -ENOSYS;

	if (reportID >= 0x0F) {
		args[index++] = reportID;
		reportID = 0x0F;
	}

	/*
	 * use the data register for feature reports or if the device does not
	 * support the output register
	 */
	if (use_data) {
		args[index++] = dataRegister & 0xFF;
		args[index++] = dataRegister >> 8;
		hidcmd = &hid_set_report_cmd;
	} else {
		args[index++] = outputRegister & 0xFF;
		args[index++] = outputRegister >> 8;
		hidcmd = &hid_no_cmd;
	}

	args[index++] = size & 0xFF;
	args[index++] = size >> 8;

	if (reportID)
		args[index++] = reportID;

	memcpy(&args[index], buf, data_len);

	ret = __i2c_hid_command(client, hidcmd, reportID,
		reportType, args, args_len, NULL, 0);
	if (ret) {
		printk("hid-keyboard,%s,failed to set a report to device.\n",
		       "i2c_hid_output_raw_report");
		return ret;
	}

	return data_len;
}

static int i2c_hid_set_power(struct i2c_client *client, int power_state)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret, retry;
	unsigned long now, delay;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	/*
	 * Some devices require to send a command to wakeup before power on.
	 * The call will get a return value (EREMOTEIO) but device will be
	 * triggered and activated. After that, it goes like a normal device.
	 */
	if (power_state == I2C_HID_PWR_ON &&
	    ihid->quirks & I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV) {
		ret = i2c_hid_command(client, &hid_set_power_cmd, NULL, 0);

		/* Device was already activated */
		if (!ret)
			goto set_pwr_exit;
		/* vendor required to sleep 10ms to send next command */
		if (ihid->hid->vendor == VID_KB_LENOVO &&
		    (ihid->hid->product == PID_KB_LENOVO ||
		     ihid->hid->product == PID_KB_LENOVO_V04))
			msleep(10);
	}

	if (ihid->quirks & I2C_HID_QUIRK_DELAY_AFTER_SLEEP &&
	    power_state == I2C_HID_PWR_ON) {
		now = jiffies;
		if (time_after(ihid->sleep_delay, now)) {
			delay = jiffies_to_usecs(ihid->sleep_delay - now);
			usleep_range(delay, delay + 1);
		}
	}

	/*
	 * Stock waits once before the first power command, then allows four
	 * attempts total with a 10 ms retry delay between attempts. The
	 * fourth failure goes directly to the reset recovery below.
	 */
	msleep(10);
	for (retry = 0; retry < 4; retry++) {
		ret = __i2c_hid_command(client, &hid_set_power_cmd, power_state,
			0, NULL, 0, NULL, 0);
		if (!ret)
			break;
		if (retry == 0)
			printk("failed to change power setting,then retry\n");
		if (retry < 3)
			msleep(10);
	}

	if (ret && mcu_rst_gpio) {
		gpiod_set_raw_value(gpio_to_desc(mcu_rst_gpio), 0);
		msleep(10);
		gpiod_set_raw_value(gpio_to_desc(mcu_rst_gpio), 1);
		msleep(10);
		printk("failed to change power setting retry 3 times,set reset pin to 1.\n");
	}

	if (ihid->quirks & I2C_HID_QUIRK_DELAY_AFTER_SLEEP &&
	    power_state == I2C_HID_PWR_SLEEP)
		ihid->sleep_delay = jiffies + msecs_to_jiffies(20);

set_pwr_exit:

	/*
	 * The HID over I2C specification states that if a DEVICE needs time
	 * after the PWR_ON request, it should utilise CLOCK stretching.
	 * However, it has been observered that the Windows driver provides a
	 * 1ms sleep between the PWR_ON and RESET requests.
	 * According to Goodix Windows even waits 60 ms after (other?)
	 * PWR_ON requests. Testing has confirmed that several devices
	 * will not work properly without a delay after a PWR_ON request.
	 */
	if (!ret && power_state == I2C_HID_PWR_ON)
		msleep(60);

	return ret;
}

static int i2c_hid_hwreset(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	/*
	 * This prevents sending feature reports while the device is
	 * being reset. Otherwise we may lose the reset complete
	 * interrupt.
	 */
	mutex_lock(&ihid->reset_lock);

	ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);
	if (ret)
		goto out_unlock;

	i2c_hid_dbg(ihid, "resetting...\n");

	ret = i2c_hid_command(client, &hid_reset_cmd, NULL, 0);
	if (ret) {
		printk("failed to reset device.\n");
		i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);
	}

out_unlock:
	mutex_unlock(&ihid->reset_lock);
	return ret;
}

static void i2c_hid_get_input(struct i2c_hid *ihid)
{
	int ret;
	u32 ret_size;
	int size = le16_to_cpu(ihid->hdesc.wMaxInputLength);
	bool lenovo_kb;
	/* IRQ is registered before hid_allocate_device in this probe. */
	if (!ihid->hid)
		return;

	if (size > ihid->bufsize)
		size = ihid->bufsize;

	ret = i2c_master_recv(ihid->client, ihid->inbuf, size);
	if (ret != size) {
		if (ret < 0)
			return;

		/* Stock prints (Image-b 0x9bed010c for i2c fail,
		 * 0x9bee013a for data length mismatch). */
		printk("hid-keyboard,%s,ret < 0,i2c_master_recv fail\n",
		       "i2c_hid_irq");
		printk("hid-keyboard,%s,hid-keyboard,%s,get %d data instead of %d\n",
		       "i2c_hid_irq", "i2c_hid_irq", ret, size);
		return;
	}

	ret_size = ihid->inbuf[0] | ihid->inbuf[1] << 8;
	/* Stock special-key/hidinput gate covers only the three spinel
	 * V04/P12 combos below; 0x17ef/0x6127 is not in the stock gate. */
	lenovo_kb = (ihid->hid->vendor == 0x17ef &&
		     ihid->hid->product == 0x6175) ||
		    (ihid->hid->vendor == 0x04f3 &&
		     ihid->hid->product == 0x31a8) ||
		    (ihid->hid->vendor == 0x32cd &&
		     ihid->hid->product == 0x003a);

	if (!ret_size) {
		/* host or device initiated RESET completed */
		if (test_and_clear_bit(I2C_HID_RESET_PENDING, &ihid->flags))
			wake_up(&ihid->wait);
		return;
	}

	if (ihid->quirks & I2C_HID_QUIRK_BOGUS_IRQ && ret_size == 0xffff) {
		return;
	}

	if ((ret_size > size) || (ret_size < 2)) {
		if (ihid->quirks & I2C_HID_QUIRK_BAD_INPUT_SIZE) {
			ihid->inbuf[0] = size & 0xff;
			ihid->inbuf[1] = size >> 8;
			ret_size = size;
		} else {
			printk("hid-keyboard,%s,incomplete report (%d/%d)\n",
			       "i2c_hid_irq", size, ret_size);
			return;
		}
	}

	/* Stock input dump (Image-b 0x9bed01d5). */
	printk("hid-keyboard,%s,ret_size=%d,input=%*ph\n",
	       "i2c_hid_irq", ret_size, ret_size, ihid->inbuf);

	/* Stock report type print (Image-b 0x9bed01fd for 0x06 / 0x9bed02e1 for 0x07). */
	if (ihid->inbuf[2] == 0x06) {
		printk("hid-keyboard,%s,0x06\n", "i2c_hid_irq");
		kb_connect = ihid->inbuf[3] & 0x01;
		kb_status = ihid->inbuf[3];
		/* stock i2c_hid_irq 0x8cb3c94/0x8cb3ca8: update device_keyboard
		 * online/hall from inbuf[3] before the power-key gate. */
		if (device_keyboard) {
			device_keyboard->online = ihid->inbuf[3] & 0x01;
			device_keyboard->hall = (ihid->inbuf[3] >> 1) & 0x01;
		}
		/* stock i2c_hid_irq 0xffffff8008cb3cdc-d8: inject the soft power
		 * key when the frame reports online==1 and g_screen_on==0. */
		if (kb_connect == 1 && !g_screen_on) {
			printk("hid-keyboard,%s,kb_report_power_key\n", __func__);
			kb_report_power_key();
		}
		/* stock 0x8cb3d6c..0x8cb3da8: manage hidinput registration
		 * only for 0x06 status frames, from device_keyboard->status
		 * and online, not from a PID-specific global. */
		if (lenovo_kb && device_keyboard) {
			if (device_keyboard->status) {
				if (device_keyboard->online == 0) {
					hidinput_disconnect(ihid->hid);
					device_keyboard->status = 0;
				}
			} else if (device_keyboard->online == 1) {
				if (hidinput_connect(ihid->hid, 0) == 0)
					device_keyboard->status = 1;
			}
		}

		{
			u16 adc_value = ihid->inbuf[7] |
					(ihid->inbuf[8] << 8);

			printk("hid-keyboard,%s,adc_value=0x%x,device_keyboard->kb_connect_status=%d,kb_hall=%d,input_registered=%d\n",
				"i2c_hid_irq", adc_value,
				device_keyboard ? device_keyboard->online : 0,
				device_keyboard ? device_keyboard->hall : 0,
				device_keyboard ? device_keyboard->status : 0);
		}
	}

	/* stock 0x8cb3dd8: a 0x07 frame with no keyboard input device would
	 * execute input_event(NULL, ...). Skip only the invalid event, then keep
	 * the frame's normal identity/report continuation. */
	if (ihid->inbuf[2] == 0x07 && kb_input_dev)
		printk("hid-keyboard,%s,0x07,input_report_key:[%d].\n",
			"i2c_hid_irq", ihid->inbuf[3]);
	if (ihid->inbuf[2] == 0x07 && !kb_input_dev)
		printk("hid-keyboard,%s,kb_input_dev is NULL", __func__);

	if (ihid->inbuf[2] != 0x06 && ihid->inbuf[2] != 0x07) {
		if (lenovo_kb) {
			printk("hid-keyboard,%s,v02 v03 v04 spinel,special key\n",
				"i2c_hid_irq");
			if (ihid->inbuf[2] == 0x05 &&
				test_bit(I2C_HID_STARTED, &ihid->flags) &&
				device_keyboard && device_keyboard->online == 1) {
				if (handle_special_key_frame(ihid))
					return;
			}
		} else {
			printk("hid-keyboard,%s,other FW\n", "i2c_hid_irq");
		}
	}

	if (test_bit(I2C_HID_STARTED, &ihid->flags))
		hid_input_report(ihid->hid, HID_INPUT_REPORT, ihid->inbuf + 2, ret_size - 2, 1);
	return;
}

static irqreturn_t i2c_hid_irq(int irq, void *dev_id)
{
	struct i2c_hid *ihid = dev_id;

	/* stock i2c_hid_irq 0x8cb3b2c: drop frames while a command has a
	 * pending response buffer. */
	if (test_bit(I2C_HID_COMMAND_IN_PROGRESS, &ihid->flags))
		return IRQ_HANDLED;

	i2c_hid_get_input(ihid);

	return IRQ_HANDLED;
}

static int i2c_hid_get_report_length(struct hid_report *report)
{
	return ((report->size - 1) >> 3) + 1 +
		report->device->report_enum[report->type].numbered + 2;
}

/*
 * Traverse the supplied list of reports and find the longest
 */
static void i2c_hid_find_max_report(struct hid_device *hid, unsigned int type,
		unsigned int *max)
{
	struct hid_report *report;
	unsigned int size;

	/* We should not rely on wMaxInputLength, as some devices may set it to
	 * a wrong length. */
	list_for_each_entry(report, &hid->report_enum[type].report_list, list) {
		size = i2c_hid_get_report_length(report);
		if (*max < size)
			 *max = size;
	}
}

static void i2c_hid_free_buffers(struct i2c_hid *ihid)
{
	kfree(ihid->inbuf);
	kfree(ihid->rawbuf);
	kfree(ihid->argsbuf);
	kfree(ihid->cmdbuf);
	ihid->inbuf = NULL;
	ihid->rawbuf = NULL;
	ihid->cmdbuf = NULL;
	ihid->argsbuf = NULL;
	ihid->bufsize = 0;
}

static int i2c_hid_alloc_buffers(struct i2c_hid *ihid, size_t report_size)
{
	/* the worst case is computed from the set_report command with a
	 * reportID > 15 and the maximum report length */
	int args_len = sizeof(__u8) + /* ReportID */
		       sizeof(__u8) + /* optional ReportID byte */
		       sizeof(__u16) + /* data register */
		       sizeof(__u16) + /* size of the report */
		       report_size; /* report */

	ihid->inbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->rawbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->argsbuf = kzalloc(args_len, GFP_KERNEL);
	ihid->cmdbuf = kzalloc(sizeof(union command) + args_len, GFP_KERNEL);

	if (!ihid->inbuf || !ihid->rawbuf || !ihid->argsbuf || !ihid->cmdbuf) {
		i2c_hid_free_buffers(ihid);
		return -ENOMEM;
	}

	ihid->bufsize = report_size;

	return 0;
}

static int i2c_hid_get_raw_report(struct hid_device *hid,
		unsigned char report_number, __u8 *buf, size_t count,
		unsigned char report_type)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	size_t ret_count, ask_count;
	int ret;

	if (report_type == HID_OUTPUT_REPORT)
		return -EINVAL;

	/*
	 * In case of unnumbered reports the response from the device will
	 * not have the report ID that the upper layers expect, so we need
	 * to stash it the buffer ourselves and adjust the data size.
	 */
	if (!report_number) {
		buf[0] = 0;
		buf++;
		count--;
	}

	/* +2 bytes to include the size of the reply in the query buffer */
	ask_count = min(count + 2, (size_t)ihid->bufsize);

	ret = i2c_hid_get_report(client,
			report_type == HID_FEATURE_REPORT ? 0x03 : 0x01,
			report_number, ihid->rawbuf, ask_count);

	if (ret < 0)
		return ret;

	ret_count = ihid->rawbuf[0] | (ihid->rawbuf[1] << 8);

	if (ret_count <= 2)
		return 0;

	ret_count = min(ret_count, ask_count);

	/* The query buffer contains the size, dropping it in the reply */
	count = min(count, ret_count - 2);
	memcpy(buf, ihid->rawbuf + 2, count);

	if (!report_number)
		count++;

	return count;
}

static int i2c_hid_output_raw_report(struct hid_device *hid, __u8 *buf,
		size_t count, unsigned char report_type, bool use_data)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int report_id = buf[0];
	int ret;

	if (report_type == HID_INPUT_REPORT)
		return -EINVAL;

	mutex_lock(&ihid->reset_lock);

	if (report_id) {
		buf++;
		count--;
	}

	ret = i2c_hid_set_or_send_report(client,
				report_type == HID_FEATURE_REPORT ? 0x03 : 0x02,
				report_id, buf, count, use_data);

	if (report_id && ret >= 0)
		ret++; /* add report_id to the number of transferred bytes */

	mutex_unlock(&ihid->reset_lock);

	return ret;
}

static int i2c_hid_output_report(struct hid_device *hid, __u8 *buf,
		size_t count)
{
	return i2c_hid_output_raw_report(hid, buf, count, HID_OUTPUT_REPORT,
			false);
}

static int i2c_hid_raw_request(struct hid_device *hid, unsigned char reportnum,
			       __u8 *buf, size_t len, unsigned char rtype,
			       int reqtype)
{
	switch (reqtype) {
	case HID_REQ_GET_REPORT:
		return i2c_hid_get_raw_report(hid, reportnum, buf, len, rtype);
	case HID_REQ_SET_REPORT:
		if (buf[0] != reportnum)
			return -EINVAL;
		return i2c_hid_output_raw_report(hid, buf, len, rtype, true);
	default:
		return -EIO;
	}
}

static int i2c_hid_parse(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct i2c_hid_desc *hdesc = &ihid->hdesc;
	unsigned int rsize;
	char *rdesc;
	int ret;
	int tries = 3;
	char *use_override;

	/* Stock prints unconditionally (Image-b 0x8cb1e98:
	 * "hid-keyboard,=== %s ====", %s = "i2c_hid_parse"). */
	printk("hid-keyboard,=== %s ====\n", "i2c_hid_parse");

	rsize = le16_to_cpu(hdesc->wReportDescLength);
	if (!rsize || rsize > HID_MAX_DESCRIPTOR_SIZE) {
		printk("hid-keyboard,%s,weird size of report descriptor (%u)\n",
			"i2c_hid_parse", rsize);
		return -EINVAL;
	}

	/*
	 * Stock gate (Image-b i2c_hid_parse 0x8cb1f04-0x8cb1fd0): the reset
	 * section runs only while the screen is on; with the screen off it
	 * prints "screen is off didn't reset" and skips the
	 * section.  The gate is re-checked on every retry.  The %s in the
	 * stock prints is the literal "i2c_hid_hwreset" kept from the
	 * original helper (rodata 0x9bed54b).  kb_i2c_resume() still
	 * requires g_already_sleep (returns -1 when awake) and only the
	 * reset-command failure path calls kb_i2c_suspend() (0x8cb1fb0);
	 * the resume failure path unlocks and retries without suspending
	 * (0x8cb1f34).
	 */
	for (;;) {
		printk("hid-keyboard,%s\n", "i2c_hid_hwreset");
		if (!g_screen_on) {
			printk("hid-keyboard,%s,screen is off didn't reset\n",
				"i2c_hid_hwreset");
			break;
		}

		mutex_lock(&ihid->reset_lock);
		ret = kb_i2c_resume();
		if (ret) {
			mutex_unlock(&ihid->reset_lock);
			msleep(1000);
			if (tries-- == 0)
				return ret;
			continue;
		}

		usleep_range(1000, 5000);
		printk("hid-keyboard,%s,resetting...\n", "i2c_hid_hwreset");
		ret = __i2c_hid_command(client, &hid_reset_cmd, 0, 0,
					NULL, 0, NULL, 0);
		if (!ret) {
			mutex_unlock(&ihid->reset_lock);
			break;
		}

		printk("hid-keyboard,%s,failed to reset device.\n",
			"i2c_hid_hwreset");
		kb_i2c_suspend();
		mutex_unlock(&ihid->reset_lock);
		msleep(1000);
		if (tries-- == 0)
			return ret;
	}

	use_override = i2c_hid_get_dmi_hid_report_desc_override(client->name,
								&rsize);

	if (use_override) {
		rdesc = use_override;
		i2c_hid_dbg(ihid, "Using a HID report descriptor override\n");
	} else {
		rdesc = kzalloc(rsize, GFP_KERNEL);

		if (!rdesc) {
			dbg_hid("couldn't allocate rdesc memory\n");
			return -ENOMEM;
		}

		printk("hid-keyboard,%s,asking HID report descriptor\n",
			"i2c_hid_parse");

		ret = i2c_hid_command(client, &hid_report_descr_cmd,
				      rdesc, rsize);
		printk("hid-keyboard,%s,Report Descriptor: %*ph\n",
		       "i2c_hid_parse", rsize, rdesc);
		if (ret) {
			printk("hid-keyboard,%s,reading report descriptor failed\n",
		       "i2c_hid_parse");
			kfree(rdesc);
			return -EIO;
		}
	}




	ret = hid_parse_report(hid, rdesc, rsize);
	if (!use_override)
		kfree(rdesc);

	if (ret) {
		printk("hid-keyboard,%s,parsing report descriptor failed\n",
		       "i2c_hid_parse");
		return ret;
	}

	return 0;
}

static int i2c_hid_start(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;
	unsigned int bufsize = HID_MIN_BUFFER_SIZE;

	i2c_hid_find_max_report(hid, HID_INPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(hid, HID_OUTPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(hid, HID_FEATURE_REPORT, &bufsize);

	if (bufsize > ihid->bufsize) {
		disable_irq(client->irq);
		i2c_hid_free_buffers(ihid);

		ret = i2c_hid_alloc_buffers(ihid, bufsize);
		enable_irq(client->irq);

		if (ret)
			return ret;
	}

	return 0;
}

static void i2c_hid_stop(struct hid_device *hid)
{
	hid->claimed = 0;
}

static int i2c_hid_open(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret = 0;

	ret = pm_runtime_get_sync(&client->dev);
	if (ret < 0)
		return ret;

	set_bit(I2C_HID_STARTED, &ihid->flags);
	return 0;
}

static void i2c_hid_close(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	clear_bit(I2C_HID_STARTED, &ihid->flags);

	/* Save some power */
	pm_runtime_put(&client->dev);
}

static int i2c_hid_power(struct hid_device *hid, int lvl)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	/* Stock power level print (Image-b 0x9bed03c3). */
	printk("hid-keyboard,%s,lvl=%d\n", "i2c_hid_power", lvl);

	switch (lvl) {
	case PM_HINT_FULLON:
		pm_runtime_get_sync(&client->dev);
		break;
	case PM_HINT_NORMAL:
		pm_runtime_put(&client->dev);
		break;
	}
	return 0;
}

struct hid_ll_driver i2c_hid_ll_driver = {
	.parse = i2c_hid_parse,
	.start = i2c_hid_start,
	.stop = i2c_hid_stop,
	.open = i2c_hid_open,
	.close = i2c_hid_close,
	.power = i2c_hid_power,
	.output_report = i2c_hid_output_report,
	.raw_request = i2c_hid_raw_request,
};
EXPORT_SYMBOL_GPL(i2c_hid_ll_driver);

static int i2c_hid_init_irq(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	unsigned long irqflags = 0;
	int ret;

	/* Stock IRQ request print (Image-b 0x9bed0e20). */
	printk("hid-keyboard,%s,Requesting IRQ=%d\n",
	       "i2c_hid_init_irq", client->irq);

	if (!irq_get_trigger_type(client->irq))
		irqflags = IRQF_TRIGGER_LOW;

	ret = request_threaded_irq(client->irq, NULL, i2c_hid_irq,
				   irqflags | IRQF_ONESHOT, client->name, ihid);
	if (ret < 0) {
		printk("hid-keyboard,%s,Could not register for %s interrupt, irq = %d, ret = %d\n",
		       "i2c_hid_init_irq", client->name, client->irq, ret);

		return ret;
	}

	irq_set_irq_wake(client->irq, 1);
	printk("hid-keyboard,%s,wake_up\n", "i2c_hid_init_irq");
	ret = device_init_wakeup(&client->dev, true);
	if (ret != 0) {
		printk("hid-keyboard,%s,no wake_up\n", "i2c_hid_init_irq");
	}

	return 0;
}

static int i2c_hid_fetch_hid_descriptor(struct i2c_hid *ihid)
{
	struct i2c_client *client = ihid->client;
	struct i2c_hid_desc *hdesc = &ihid->hdesc;
	unsigned int dsize;
	int ret;
	int retry;

	/* i2c hid fetch using a fixed descriptor size (30 bytes) */
	if (i2c_hid_get_dmi_i2c_hid_desc_override(client->name)) {
		i2c_hid_dbg(ihid, "Using a HID descriptor override\n");
		ihid->hdesc =
			 *i2c_hid_get_dmi_i2c_hid_desc_override(client->name);
		goto validate;
	}

	/* Stock probe fetches and validates the 30-byte descriptor in a
	 * three-attempt loop with a 15 ms backoff. */
	for (retry = 0; retry < 3; retry++) {
		printk("hid-keyboard,%s,1 i2c_wait=%d\n",
			"i2c_hid_probe", retry);
		ret = i2c_smbus_read_byte(client);
		if (ret < 0)
			printk("hid-keyboard,%s,nothing at this address.\n",
				"i2c_hid_probe");
		printk("hid-keyboard,%s,2 i2c_wait=%d\n",
			"i2c_hid_probe", retry);
		printk("hid-keyboard,%s\n", "i2c_hid_fetch_hid_descriptor");
		printk("hid-keyboard,%s,Fetching the HID descriptor\n",
			"i2c_hid_fetch_hid_descriptor");
		ret = i2c_hid_command(client, &hid_descr_cmd,
					ihid->hdesc_buffer,
					sizeof(struct i2c_hid_desc));
		if (ret) {
			dev_err(&client->dev, "hid_descr_cmd failed\n");
			msleep(15);
			continue;
		}
		if (le16_to_cpu(hdesc->bcdVersion) != 0x0100) {
			printk("hid-keyboard,%s,unexpected HID descriptor bcdVersion (0x%04hx)\n",
				"i2c_hid_fetch_hid_descriptor",
				le16_to_cpu(hdesc->bcdVersion));
			msleep(15);
			continue;
		}
		dsize = le16_to_cpu(hdesc->wHIDDescLength);
		if (dsize != sizeof(struct i2c_hid_desc)) {
			printk("hid-keyboard,%s,weird size of HID descriptor (%u)\n",
				"i2c_hid_fetch_hid_descriptor", dsize);
			msleep(15);
			continue;
		}
		printk("hid-keyboard,%s,dsize=%d,HID Descriptor: %*ph\n",
			"i2c_hid_fetch_hid_descriptor", dsize, dsize,
			ihid->hdesc_buffer);
		printk("hid-keyboard,%s,i2c success,retry count is:%d\n",
			"i2c_hid_probe", retry);
		return 0;
	}
	printk("hid-keyboard,%s,i2c error,retry count is:%d\n",
		"i2c_hid_probe", retry);
	return -ENXIO;

validate:
	if (le16_to_cpu(hdesc->bcdVersion) != 0x0100) {
		printk("hid-keyboard,%s,unexpected HID descriptor bcdVersion (0x%04hx)\n",
			"i2c_hid_fetch_hid_descriptor",
			le16_to_cpu(hdesc->bcdVersion));
		return -ENODEV;
	}
	dsize = le16_to_cpu(hdesc->wHIDDescLength);
	if (dsize != sizeof(struct i2c_hid_desc)) {
		printk("hid-keyboard,%s,weird size of HID descriptor (%u)\n",
			"i2c_hid_fetch_hid_descriptor", dsize);
		return -ENODEV;
	}
	printk("hid-keyboard,%s,dsize=%d,HID Descriptor: %*ph\n",
		"i2c_hid_fetch_hid_descriptor", dsize, dsize,
		ihid->hdesc_buffer);
	return 0;
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id i2c_hid_acpi_blacklist[] = {
	/*
	 * The CHPN0001 ACPI device, which is used to describe the Chipone
	 * ICN8505 controller, has a _CID of PNP0C50 but is not HID compatible.
	 */
	{"CHPN0001", 0 },
	{ },
};

static int i2c_hid_acpi_pdata(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	static guid_t i2c_hid_guid =
		GUID_INIT(0x3CDFF6F7, 0x4267, 0x4555,
			  0xAD, 0x05, 0xB3, 0x0A, 0x3D, 0x89, 0x38, 0xDE);
	union acpi_object *obj;
	struct acpi_device *adev;
	acpi_handle handle;

	handle = ACPI_HANDLE(&client->dev);
	if (!handle || acpi_bus_get_device(handle, &adev)) {
		dev_err(&client->dev, "Error could not get ACPI device\n");
		return -ENODEV;
	}

	if (acpi_match_device_ids(adev, i2c_hid_acpi_blacklist) == 0)
		return -ENODEV;

	obj = acpi_evaluate_dsm_typed(handle, &i2c_hid_guid, 1, 1, NULL,
				      ACPI_TYPE_INTEGER);
	if (!obj) {
		dev_err(&client->dev, "Error _DSM call to get HID descriptor address failed\n");
		return -ENODEV;
	}

	pdata->hid_descriptor_address = obj->integer.value;
	ACPI_FREE(obj);

	return 0;
}

static void i2c_hid_acpi_fix_up_power(struct device *dev)
{
	struct acpi_device *adev;

	adev = ACPI_COMPANION(dev);
	if (adev)
		acpi_device_fix_up_power(adev);
}

static const struct acpi_device_id i2c_hid_acpi_match[] = {
	{"ACPI0C50", 0 },
	{"PNP0C50", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, i2c_hid_acpi_match);
#else
static inline int i2c_hid_acpi_pdata(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	return -ENODEV;
}

static inline void i2c_hid_acpi_fix_up_power(struct device *dev) {}
#endif

#ifdef CONFIG_OF
static int i2c_hid_of_probe(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	struct device *dev = &client->dev;
	u32 val;
	int ret;

	/* TB371FC magnetic keyboard MCU reset/wake pins. */
	ret = of_get_named_gpio_flags(dev->of_node, "mcu_rst_gpio", 0, NULL);
	if (ret < 0) {
		printk("hid-keyboard,%s,Not support MCU HW reset pin\n", __func__);
		return -ENODEV;
	}
	mcu_rst_gpio = ret;
	/* Stock keeps the reset pin in mcu_rst_gpio only. The old
	 * kb,output-gpio consumer is not present in stock and must not
	 * drive this pin from keyboard status frames. */
	pdata->output_gpio = -1;

	ret = of_get_named_gpio_flags(dev->of_node, "mcu_resume_gpio", 0,
				      NULL);
	if (ret < 0) {
		printk("hid-keyboard,%s,Not support MCU wake_sleep pin\n", __func__);
		return -ENODEV;
	}
	mcu_resume_gpio = ret;
	
	ret = of_property_read_u32(dev->of_node, "hid-descr-addr", &val);
	if (ret) {
		printk("HID register address not provided\n");
		return -ENODEV;
	}
	if (val >> 16) {
		printk("hid-keyboard,%s,Bad HID register address: 0x%08x\n",
		       __func__, val);
		return -EINVAL;
	}
	pdata->hid_descriptor_address = val;

	return 0;
}

static const struct of_device_id i2c_hid_of_match[] = {
	{ .compatible = "hid-over-i2c" },
	{},
};
MODULE_DEVICE_TABLE(of, i2c_hid_of_match);
#else
static inline int i2c_hid_of_probe(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static void i2c_hid_fwnode_probe(struct i2c_client *client,
				 struct i2c_hid_platform_data *pdata)
{
	u32 val;
	int ret;

	/* Stock pre-probe delay: of_property_read_variable_u32_array()
	 * at 0x8cb32e0 reads "post-power-on-delay-ms" and stores it at
	 * ihid+184 for the msleep at 0x8cb3044. */
	ret = of_property_read_u32(client->dev.of_node,
				   "post-power-on-delay-ms", &val);
	if (!ret)
		pdata->post_power_delay_ms = val;
}

/* Stock register_kb_wakeup_devices @0xffffff8008cb18c8. */
static int register_kb_wakeup_devices(void)
{
	printk("hid-keyboard,%s\n", __func__);
	kb_input_dev = input_allocate_device();
	if (!kb_input_dev) {
		printk("hid-keyboard,%s,failed to input_allocate_device keyboard device\n",
		       __func__);
		return -1;
	}

	input_set_capability(kb_input_dev, EV_KEY, 0x74);  /* KEY_POWER */
	input_set_capability(kb_input_dev, EV_KEY, 0x28e);
	input_set_capability(kb_input_dev, EV_KEY, 0x28f);
	input_set_capability(kb_input_dev, EV_KEY, 0x280);
	input_set_capability(kb_input_dev, EV_KEY, 0x282);
	input_set_capability(kb_input_dev, EV_KEY, 0x283);
	input_set_capability(kb_input_dev, EV_KEY, 0x284);
	input_set_capability(kb_input_dev, EV_KEY, 0x285);
	input_set_capability(kb_input_dev, EV_KEY, 0x286);
	input_set_capability(kb_input_dev, EV_KEY, 0x287);
	input_set_capability(kb_input_dev, EV_KEY, 0x289);
	input_set_capability(kb_input_dev, EV_KEY, 0x28a);
	input_set_capability(kb_input_dev, EV_KEY, 0x28b);
	kb_input_dev->name = "keyboard_wakeup_devices";
	if (input_register_device(kb_input_dev)) {
		printk("hid-keyboard,%s,failed to register keyboard device\n",
		       __func__);
		return -1;
	}
	return 0;
}

/* stock kb_report_power_key @0xffffff8008cb1828: log; if the input device
 * is not registered log and return -1; otherwise report KEY_POWER press +
 * sync + release + sync and return 0. */
static int kb_report_power_key(void)
{
	int ret;

	printk(KERN_ERR "===%s===\n", __func__);
	if (!kb_input_dev) {
		printk(KERN_ERR "kb_input_dev is NULL");
		return -1;
	}
	input_report_key(kb_input_dev, KEY_POWER, 1);
	input_sync(kb_input_dev);
	input_report_key(kb_input_dev, KEY_POWER, 0);
	input_sync(kb_input_dev);
	ret = 0;
	return ret;
}

/* stock report_special_key @0xffffff8008cb1a18: log, require the wakeup
 * input device, then emit EV_KEY plus EV_SYN. */
static int report_special_key(int key, int value)
{
	printk("hid-keyboard,%s,key_value=%02x event=EV_KEY\n", __func__, key);
	if (!kb_input_dev) {
		printk("hid-keyboard,%s,kb_input_dev is NULL", __func__);
		return -1;
	}
	input_event(kb_input_dev, EV_KEY, key, value & 0xff);
	input_event(kb_input_dev, EV_SYN, 0, 0);
	return 0;
}

static bool update_special_key(int key, bool pressed, bool *state)
{
	if (*state == pressed)
		return false;
	report_special_key(key, pressed ? 1 : 0);
	 *state = pressed;
	return true;
}

/* stock 0x8cb3f24..0x8cb34684: first changed key wins; return true so the
 * caller suppresses the normal HID report for a consumed 0x05 frame. */
static bool handle_special_key_frame(struct i2c_hid *ihid)
{
	u8 b3 = ihid->inbuf[3];
	u8 b4 = ihid->inbuf[4];
	u8 b5 = ihid->inbuf[5];

	if (update_special_key(0x280, !!(b3 & 0x40), &spec_key_lockscreen))
		return true;
	if (update_special_key(0x282, !!(b3 & 0x80), &spec_key_switchlanguage))
		return true;
	if (update_special_key(0x283, !!(b4 & 0x01), &spec_key_micdisable))
		return true;
	if (update_special_key(0x284, !!(b4 & 0x10), &spec_key_touchpanelmute))
		return true;
	if (update_special_key(0x285, !!(b5 & 0x02), &spec_key_globalsearch))
		return true;
	if (update_special_key(0x286, !!(b4 & 0x02), &spec_key_fullscreen))
		return true;
	if (update_special_key(0x287, !!(b4 & 0x04), &spec_key_splitscreen))
		return true;
	if (update_special_key(0x289, !!(b4 & 0x20), &spec_key_superintcon))
		return true;
	if (update_special_key(0x28a, !!(b4 & 0x40), &spec_key_customerapp1))
		return true;
	if (update_special_key(0x28b, !!(b4 & 0x80), &spec_key_customerapp2))
		return true;
	return false;
}

static ssize_t kb_sys_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!device_keyboard)
		return scnprintf(buf, PAGE_SIZE, "0\n");
	return scnprintf(buf, PAGE_SIZE, "%d\n", device_keyboard->online);
}

static ssize_t kb_sys_hall_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!device_keyboard)
		return scnprintf(buf, PAGE_SIZE, "0\n");
	return scnprintf(buf, PAGE_SIZE, "%d\n", device_keyboard->hall);
}

static ssize_t kb_sys_vid_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* D105 (G4): stock kb_id_show (0x8cb2908) reads ctx+0x38/0x3c,
	 * filled in i2c_hid_probe (0x8cb34b4/0x8cb34c4) from the embedded
	 * HID descriptor wVendorID (ihid+36) / wProductID (ihid+38) - the
	 * same source as hid->vendor/product. Replace the D91 i2c-addr
	 * placeholder. */
	struct hid_device *hid = g_phid;

	if (!hid)
		return scnprintf(buf, PAGE_SIZE, "0 0\n");
	return scnprintf(buf, PAGE_SIZE, "%d %d\n",
			hid->vendor, hid->product);
}

static const struct device_attribute kb_dev_attr_status_value =
	__ATTR(kb_status_value, 0444, kb_sys_status_show, NULL);
static const struct device_attribute kb_dev_attr_hall_value =
	__ATTR(kb_hall_value, 0444, kb_sys_hall_show, NULL);
static const struct device_attribute kb_dev_attr_vid_pid =
	__ATTR(kb_vid_pid, 0444, kb_sys_vid_pid_show, NULL);

static const struct attribute *kb_sys_attrs[] = {
	&kb_dev_attr_status_value.attr,
	&kb_dev_attr_hall_value.attr,
	&kb_dev_attr_vid_pid.attr,
	NULL,
};

static const struct attribute_group kb_sys_attr_group = {
	.attrs = (struct attribute **)kb_sys_attrs,
};

static int i2c_enable_mcu(struct i2c_client *client, struct i2c_hid *ihid)
{
	int ret;

	if (is_mcu_init)
		return 0;

	msleep(100);

	ret = gpio_request(mcu_resume_gpio, "mcu_resume_gpio");
	if (ret) {
		printk("%s: request mcu_resume_gpio failed\n", "i2c_enable_mcu");
		return ret;
	}

	ret = gpio_request(mcu_rst_gpio, "mcu_rst_gpio");
	if (ret) {
		printk("hid-keyboard,%s,request mcu_rst_gpio failed\n",
		       "i2c_enable_mcu");
		gpio_free(mcu_resume_gpio);
		return ret;
	}

	ret = gpiod_direction_output_raw(gpio_to_desc(mcu_rst_gpio), 1);
	if (ret) {
		printk("hid-keyboard,%s,set mcu_rst_gpio failed\n",
		       "i2c_enable_mcu");
		gpio_free(mcu_rst_gpio);
		gpio_free(mcu_resume_gpio);
		return ret;
	}

	is_mcu_init = true;
	{
		int rst_value = gpiod_get_raw_value(gpio_to_desc(mcu_rst_gpio));
		int resume_value = gpiod_get_raw_value(gpio_to_desc(mcu_resume_gpio));

		printk("hid-keyboard,%s,after setting: MCU_HWRESET_PIN=%d,MCU_WAKEUP_PIN=%d\n",
			"i2c_enable_mcu", rst_value, resume_value);
		printk("hid-keyboard,%s,mcu_rst_gpio=%d,mcu_resume_gpio=%d,post_power_delay_ms=%d\n",
			"i2c_enable_mcu", mcu_rst_gpio, mcu_resume_gpio,
			ihid->pdata.post_power_delay_ms);
	}
	return 0;
}

static void i2c_disable_mcu(void)
{
	if (mcu_rst_gpio) {
		gpio_free(mcu_rst_gpio);
		mcu_rst_gpio = 0;
	}
	if (mcu_resume_gpio) {
		gpio_free(mcu_resume_gpio);
		mcu_resume_gpio = 0;
	}
	is_mcu_init = false;
}

static int i2c_hid_probe(struct i2c_client *client,
			 const struct i2c_device_id *dev_id)
{
	int ret;
	struct i2c_hid *ihid;
	struct hid_device *hid;
	__u16 hidRegister;
	struct i2c_hid_platform_data *platform_data = client->dev.platform_data;

	/* Stock entry print (Image-b 0x8cb2eb0-0x8cb2eb8, 0x9bed08cc). */
	printk("hid-keyboard,%s,HID probe called for i2c 0x%02x\n",
	       "i2c_hid_probe", client->addr);

	if (!client->irq) {
		printk("hid-keyboard,%s,HID over i2c has not been provided an Int IRQ\n", "i2c_hid_probe");
		return -EINVAL;
	}

	if (client->irq < 0) {
		if (client->irq != -EPROBE_DEFER)
			printk("hid-keyboard,%s,HID over i2c doesn't have a valid IRQ\n", "i2c_hid_probe");
		return client->irq;
	}

	ihid = devm_kzalloc(&client->dev, sizeof(*ihid), GFP_KERNEL);
	if (!ihid)
		return -ENOMEM;

	device_keyboard = kzalloc(sizeof(*device_keyboard), GFP_KERNEL);
	if (!device_keyboard)
		return -ENOMEM;

	if (client->dev.of_node) {
		ret = i2c_hid_of_probe(client, &ihid->pdata);
		if (ret)
			return ret;
	} else if (!platform_data) {
		ret = i2c_hid_acpi_pdata(client, &ihid->pdata);
		if (ret)
			return ret;
	} else {
		ihid->pdata = *platform_data;
	}

	/* Parse platform agnostic common properties from ACPI / device tree */
	i2c_hid_fwnode_probe(client, &ihid->pdata);

	ret = i2c_enable_mcu(client, ihid);
	if (ret)
		return ret;

	if (ihid->pdata.post_power_delay_ms)
		msleep(ihid->pdata.post_power_delay_ms);

	i2c_set_clientdata(client, ihid);

	ihid->client = client;

	hidRegister = ihid->pdata.hid_descriptor_address;
	if (!hidRegister)
		printk("hid-keyboard,%s,HID register address not provided\n", "i2c_hid_probe");
	ihid->wHIDDescRegister = cpu_to_le16(hidRegister);

	init_waitqueue_head(&ihid->wait);
	mutex_init(&ihid->reset_lock);

	/* we need to allocate the command buffer without knowing the maximum
	 * size of the reports. Let's use HID_MIN_BUFFER_SIZE, then we do the
	 * real computation later. */
	ret = i2c_hid_alloc_buffers(ihid, HID_MIN_BUFFER_SIZE);
	if (ret < 0)
		goto err_regulator;

	i2c_hid_acpi_fix_up_power(&client->dev);

	pm_runtime_get_noresume(&client->dev);
	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	device_enable_async_suspend(&client->dev);

	ret = i2c_hid_fetch_hid_descriptor(ihid);
	if (ret < 0)
		goto err_pm;

	if (client->dev.of_node && mcu_resume_gpio >= 0 &&
	    mcu_rst_gpio >= 0) {
		if (!kb_status_kobj) {
			kb_status_kobj = kobject_create_and_add("kb_status", NULL);
			printk("hid-keyboard,%s,kobject_status=%d\n",
				"creat_kb_status_node",
				(int)(unsigned long)kb_status_kobj);
			if (!kb_status_kobj) {
				printk("hid-keyboard,%s,failed to create kb_status\n",
					"creat_kb_status_node");
			} else if (sysfs_create_group(kb_status_kobj,
					&kb_sys_attr_group)) {
				printk("hid-keyboard,%s,failed to create kb_status\n",
					"creat_kb_status_node");
			}
		}
	}

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		ret = PTR_ERR(hid);
		goto err_irq;
	}

	ihid->hid = hid;
	g_phid = hid;

	hid->driver_data = client;
	hid->ll_driver = &i2c_hid_ll_driver;
	hid->dev.parent = &client->dev;
	hid->bus = BUS_I2C;
	hid->version = le16_to_cpu(ihid->hdesc.bcdVersion);
	hid->vendor = le16_to_cpu(ihid->hdesc.wVendorID);
	hid->product = le16_to_cpu(ihid->hdesc.wProductID);

	/* Stock FW version checks (Image-b 0x8cb34d4..0x8cb3918). */
	printk("hid-keyboard,%s,VID:[0x%x],PID:[0x%x]\n",
	       "i2c_hid_probe", hid->vendor, hid->product);

	/* Old-firmware rejects are keyed by product only in stock. */
	if (hid->product == 0x6103) {
		(void)0;
		ret = -ENODEV;
		goto err_irq;
	}
	if (hid->product == 0x3164) {
		(void)0;
		ret = -ENODEV;
		goto err_irq;
	}

	if (hid->vendor == VID_KB_LENOVO &&
	    hid->product == PID_KB_LENOVO_V04) {
		printk("hid-keyboard,%s,V04 FW\n", "i2c_hid_probe");
		strlcpy(hid->name, "Lenovo Keyboard Pack for Spinel",
			sizeof(hid->name));
	} else if ((hid->vendor == VID_KB_LENOVO_TP &&
		    hid->product == PID_KB_LENOVO_TP) ||
		   (hid->vendor == 0x32cd && hid->product == 0x003a)) {
		printk("hid-keyboard,%s,V02 or V03 FW\n", "i2c_hid_probe");
		snprintf(hid->name, sizeof(hid->name),
			 "%s %04hX:%04hX Spinel keyboard",
			 client->name, (u16)hid->vendor, (u16)hid->product);
	} else {
		printk("hid-keyboard,%s,others devices\n", "i2c_hid_probe");
		snprintf(hid->name, sizeof(hid->name),
			 "%s %04hX:%04hX",
			 client->name, (u16)hid->vendor, (u16)hid->product);
	}
	(void)0;

	if ((hid->vendor == VID_KB_LENOVO &&
	     (hid->product == PID_KB_LENOVO || hid->product == PID_KB_LENOVO_V04)) ||
	    (hid->vendor == VID_KB_LENOVO_TP && hid->product == PID_KB_LENOVO_TP) ||
	    (hid->vendor == 0x32cd && hid->product == 0x003a))
		g_client = client;

	strlcpy(hid->phys, dev_name(&client->dev), sizeof(hid->phys));

	ihid->quirks = i2c_hid_lookup_quirk(hid->vendor, hid->product);

	/* Stock VID/PID prints (Image-b 0x9bed0cf + 0x9bed0ab8). */
	printk("hid-keyboard,%s,get VID=0x%x,get PID=0x%x\n",
	       "i2c_hid_probe", hid->vendor, hid->product);
	printk("hid-keyboard,%s,VID:[0x%x],PID:[0x%x]\n",
	       "i2c_hid_probe", hid->vendor, hid->product);

	if (!kb_input_dev)
		register_kb_wakeup_devices();

	/* Stock hid_add_device print (Image-b 0x9bed0c3a). */
	printk("hid-keyboard,%s,hid_add_device,end,start i2c_hid_init_irq\n",
	       "i2c_hid_probe");

	ret = hid_add_device(hid);
	if (ret) {
		if (ret != -ENODEV)
			printk("hid-keyboard,%s,can't add hid device: %d\n", "i2c_hid_probe", ret);
		goto err_mem_free;
	}

	ret = i2c_hid_init_irq(client);
	if (ret)
		goto err_mem_free;

	if (device_keyboard)
		device_keyboard->status = 1;

	if (!(ihid->quirks & I2C_HID_QUIRK_NO_RUNTIME_PM))
		pm_runtime_put(&client->dev);

	/* Stock keeps the keyboard LDO as pm8150a_l10 and only force-disables
	 * it in charger mode. */
	ldo_keyboard = regulator_get(&hid->dev, "pm8150a_l10");
	if (IS_ERR(ldo_keyboard)) {
		printk(KERN_ERR "hid-keyboard,%s,could not get pm8150a_l10, rc = %ld\n",
		       "i2c_hid_probe", PTR_ERR(ldo_keyboard));
		ret = -ENODEV;
		goto err_irq;
	}
	if (strstr(saved_command_line, "androidboot.mode=charger") &&
	    regulator_is_enabled(ldo_keyboard)) {
		regulator_force_disable(ldo_keyboard);
		(void)0;
	}
	regulator_put(ldo_keyboard);

	/* Stock probe success print (Image-b 0x9bed0d02). */
	printk("hid-keyboard,%s,probe,success\n", "i2c_hid_probe");

	return 0;

err_irq:
	/* Stock probe-fail print (Image-b 0x9bed0981). */
	printk("hid-keyboard,%s,HID probe failed\n", "i2c_hid_probe");
	free_irq(client->irq, ihid);

err_mem_free:
	hid_destroy_device(hid);

err_pm:
	pm_runtime_put_noidle(&client->dev);
	pm_runtime_disable(&client->dev);

err_regulator:
	i2c_disable_mcu();
	i2c_hid_free_buffers(ihid);
	return ret;
}

static int i2c_hid_remove(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct hid_device *hid = ihid->hid;

	/* Stock remove entry print (Image-b 0x8cb39b0; %s literal from
	 * rodata 0x9bee358). */
	printk("hid-keyboard,%s\n", "i2c_hid_remove");

	if (!(ihid->quirks & I2C_HID_QUIRK_NO_RUNTIME_PM))
		pm_runtime_get_sync(&client->dev);
	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	pm_runtime_put_noidle(&client->dev);

	hid_destroy_device(hid);

	gpio_free(mcu_rst_gpio);
	gpio_free(mcu_resume_gpio);

	free_irq(client->irq, ihid);

	if (ihid->bufsize) {
		i2c_hid_free_buffers(ihid);
		/* Stock teardown print (Image-b 0x8cb3a88), verbatim: fmt
		 * "hid-keyboard,%s" (no newline), argument string
		 * "ard,=== %s ===\n" (0x9bee370) with an unsubstituted %s. */
		printk("hid-keyboard,%s", "ard,=== %s ===\n");
	}

	return 0;
}

static void i2c_hid_shutdown(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	/* Stock shutdown entry print (Image-b 0x8cb3ac0). */
	printk("hid-keyboard,=== %s ===\n", "i2c_hid_shutdown");

	kb_i2c_suspend();
	free_irq(client->irq, ihid);
}

#ifdef CONFIG_PM_SLEEP
static int i2c_hid_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct hid_device *hid = ihid->hid;
	int ret;
	int wake_status;

	if (hid->driver && hid->driver->suspend) {
		/*
		 * Wake up the device so that IO issues in
		 * HID driver's suspend code can succeed.
		 */
		ret = pm_runtime_resume(dev);
		if (ret < 0)
			return ret;

		ret = hid->driver->suspend(hid, PMSG_SUSPEND);
		if (ret < 0)
			return ret;
	}

	if (!pm_runtime_suspended(dev)) {
		/* Save some power */
		if (!(ihid->quirks & I2C_HID_QUIRK_SHOULD_SKIP_SET_PWR))
			i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);

		if (!(ihid->quirks & I2C_HID_QUIRK_WAKE_UP_SYS)) {
			disable_irq(client->irq);
		}
	}

	if (device_may_wakeup(&client->dev)) {
		wake_status = enable_irq_wake(client->irq);
		if (!wake_status)
			ihid->irq_wake_enabled = true;
	}

	return 0;
}

static int i2c_hid_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct hid_device *hid = ihid->hid;
	int wake_status;

	if (!device_may_wakeup(&client->dev)) {
		if (ihid->pdata.post_power_delay_ms)
			msleep(ihid->pdata.post_power_delay_ms);
	} else if (ihid->irq_wake_enabled) {
		wake_status = disable_irq_wake(client->irq);
		if (!wake_status)
			ihid->irq_wake_enabled = false;
	}

	/* We'll resume to full power */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	if (!(ihid->quirks & I2C_HID_QUIRK_WAKE_UP_SYS)) {
		enable_irq(client->irq);
	}

	if ((ihid->quirks & I2C_HID_QUIRK_WAKE_UP_SYS) && (pm_wakeup_irq == client->irq)) {
		i2c_hid_get_input(ihid);
		pm_wakeup_event(&client->dev, 2000);
	}

	/* Instead of resetting device, simply powers the device on. This
	 * solves "incomplete reports" on Raydium devices 2386:3118 and
	 * 2386:4B33 and fixes various SIS touchscreens no longer sending
	 * data after a suspend/resume.
	 *
	 * However some ALPS touchpads generate IRQ storm without reset, so
	 * let's still reset them here.
	 */
	if (ihid->quirks & I2C_HID_QUIRK_RESET_ON_RESUME)
		ret = i2c_hid_hwreset(client);
	else {
		if (!(ihid->quirks & I2C_HID_QUIRK_SHOULD_SKIP_SET_PWR))
			ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);
	}

	if (ret)
		return ret;

	if (hid->driver && hid->driver->reset_resume) {
		ret = hid->driver->reset_resume(hid);
		return ret;
	}

	return 0;
}
#endif

#ifdef CONFIG_PM
static int i2c_hid_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);
	disable_irq(client->irq);
	return 0;
}

static int i2c_hid_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	enable_irq(client->irq);
	i2c_hid_set_power(client, I2C_HID_PWR_ON);
	return 0;
}

/*Spruce code for OSPURCET-1235 by chenzm9 at 2023/2/16 start*/
noinline int kb_i2c_resume(void)
{
	int ret = 0;

	printk("hid-keyboard,=== %s === start\n", __func__);
	if (!g_already_sleep) {
		printk("hid-keyboard,%s,Device was already activated\n",
		       "kb_i2c_resume");
		return -1;
	}
	printk("hid-keyboard,%s,send out one cmd to wakeup\n",
	       "kb_i2c_resume");

	ret = gpiod_direction_output_raw(gpio_to_desc(mcu_resume_gpio), 1);
		if(ret) {
				return ret;
		}

	ret = i2c_hid_set_power(g_client, I2C_HID_PWR_ON);
	if (ret) {
		printk("hid-keyboard,%s,i2c set power on fail\n", __func__);
		return ret;
	}

	g_already_sleep = false;
	printk("hid-keyboard,=== %s === end\n", __func__);

	return 0;
}

noinline void kb_i2c_suspend(void)
{
	int ret;

	printk("hid-keyboard,=== %s === start\n", __func__);
	if (g_already_sleep)
		return;
	/*
	 * TASK-091 (review-approved robustness deviation): kb_hid_suspend/
	 * kb_hid_resume already guard on g_client; kb_i2c_suspend was the
	 * only Spruce entry missing the guard, and an abnormal shutdown
	 * before the kb probe completes (g_client still NULL, vendor
	 * i2c-hid-core.c:143) dereferenced it in i2c_hid_set_power
	 * (NULL+0xd8, panic captured by kmsg_tb371_log seq 713049).
	 */
	if (!g_client) {
		printk("hid-keyboard,%s,g_client not ready, skip\n",
		       __func__);
		return;
	}

	ret = gpiod_direction_output_raw(gpio_to_desc(mcu_resume_gpio), 0);
	if (ret) {
		printk("hid-keyboard,%s,set mcu_resume_gpio failed\n",
		       __func__);
		return;
	}

	ret = i2c_hid_set_power(g_client, I2C_HID_PWR_SLEEP);
	if (ret) {
		printk("hid-keyboard,%s,i2c set sleep fail\n", __func__);
		return;
	}

	g_already_sleep = true;
	printk("hid-keyboard,=== %s === end\n", __func__);
}
/*Spruce code for OSPURCET-1235 by chenzm9 at 2023/2/16 end*/

void kb_hid_suspend(void)
{
int ret = 0;
	int args_len = 6;
	u8 args[6] = {07,00,04,00,00,05};

	if (!g_client)
		return;

	printk("hid-keyboard,=== %s === start\n", "kb_hid_suspend");

	ret = __i2c_hid_command(g_client, &hid_set_report_cmd, 2, 2, args, args_len,
				NULL, 0);
	if (ret)
		printk("hid-keyboard,%s,report screen off cmd failed\n",
		       "kb_hid_suspend");
	else
		g_screen_on = 0;

	/*Spruce code for OSPURCET-1235 by chenzm9 at 2023/2/16 start*/
	kb_i2c_suspend();
	/*Spruce code for OSPURCET-1235 by chenzm9 at 2023/2/16 end*/

	printk("hid-keyboard,=== %s === end\n", "kb_hid_suspend");
	return;
}
void kb_hid_resume(void)
{
	int ret = 0;
	int args_len = 6;
	u8 args[6] = {07,00,04,00,01,05};
	
	if (!g_client)
		return;

	printk("hid-keyboard,=== %s === start\n", "kb_hid_resume");
	/*Spruce code for OSPURCET-1235 by chenzm9 at 2023/2/16 start*/
	kb_i2c_resume();
	/*Spruce code for OSPURCET-1235 by chenzm9 at 2023/2/16 end*/

	ret = __i2c_hid_command(g_client, &hid_set_report_cmd, 2, 2, args, args_len,
				NULL, 0);
	if (ret)
		printk("hid-keyboard,%s,report screen on cmd failed\n",
		       "kb_hid_resume");
	else
		g_screen_on = 1;
	/* Spruce code for OSPURCET-780 by sunft3 at 2023/01/18 end */
	printk("hid-keyboard,=== %s === end\n", "kb_hid_resume");
	return;
}
#endif

static const struct dev_pm_ops i2c_hid_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(i2c_hid_suspend, i2c_hid_resume)
	SET_RUNTIME_PM_OPS(i2c_hid_runtime_suspend, i2c_hid_runtime_resume,
			   NULL)
};

static const struct i2c_device_id i2c_hid_id_table[] = {
	{ "hid", 0 },
	{ "hid-over-i2c", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, i2c_hid_id_table);

static struct i2c_driver i2c_hid_driver = {
	.driver = {
		.name	= "i2c_hid",
		.pm	= &i2c_hid_pm,
		.acpi_match_table = ACPI_PTR(i2c_hid_acpi_match),
		.of_match_table = of_match_ptr(i2c_hid_of_match),
	},

	.probe		= i2c_hid_probe,
	.remove		= i2c_hid_remove,
	.shutdown	= i2c_hid_shutdown,
	.id_table	= i2c_hid_id_table,
};

module_i2c_driver(i2c_hid_driver);

MODULE_DESCRIPTION("HID over I2C core driver");
MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@gmail.com>");
MODULE_LICENSE("GPL");
