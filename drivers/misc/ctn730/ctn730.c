
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
#include "ctn730.h"
#include "crc32.h"
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/firmware.h>

#ifdef CONFIG_DRM
#include <drm/drm_panel.h>
struct drm_panel *ctn730_panel;
#endif

#include <linux/bitops.h>

#define STATUS_OK 0x00
#define STATUS_PARAMETER_ERROR 0x01
#define STATUS_STATE_ERROR 0x02
#define STATUS_VALUE_ERROR 0x03
#define STATUS_REJECTED 0x04
#define STATUS_RESOURCE_ERROR 0x10
#define STATUS_TXLDO_ERROR 0x11
#define STATUS_ANTENNA_SELECTION_ERROR 0x12
#define STATUS_BIST_FAILED 0x20
#define STATUS_BIST_NO_WLC_CAP 0x21
#define STATUS_BIST_TXLDO_CURRENT_OVERFLOW 0x22
#define STATUS_BIST_TXLDO_CURRENT_UNDERFLOW 0x23
#define STATUS_FW_VERSION_ERROR 0x30
#define STATUS_FW_VERIFICATION_ERROR 0x31
#define STATUS_NTAG_BLOCK_PARAMETER_ERROR 0x32
#define STATUS_NTAG_READ_ERROR 0x33

#define MT_MASK		GENMASK(7, 6)
#define INSTRUCTION_MASK		GENMASK(5, 0)
#define RSP_BIT		BIT(6)
#define EVENT_BIT		BIT(7)



enum message_type{
	unkown = 0,
	Command,
	Response,
	Event,
};

enum instruction{
	HOST_RESET = 0,
	HOST_CTRL_WRITE_PARAMETER = 0x01,	
	HOST_CTRL_READ_PARAMETER = 0x02,
	HOST_CTRL_DL_OPEN_SESSION = 0x03,
	HOST_CTRL_DL_COMMIT_SESSION = 0x04,
	HOST_CTRL_DL_WRITE_FLASH = 0x05,
	HOST_CTRL_BIST = 0x06,
	HOST_CTRL_WRITE_SESSION_PARAMETER = 0x07,	
	HOST_CTRL_READ_SESSION_PARAMETER = 0x08,	
	CHG_ENABLE = 0x10,
	CHG_DISABLE = 0x11,
	CHG_DEVICE_STATE = 0x12,
	CHG_CHARGING_STATE = 0x14,
	CHG_CHARGING_INFO = 0x15,
	APP_CTRL_CHN_ENABLE = 0x20,
	APP_CTRL_CHN_DISABLE = 0x21,
	APP_CTRL_CHN_STATE = 0x22,
	APP_CTRL_CHN_SEND_DATA = 0x23,
	APP_CTRL_CHN_RECIVE_DATA = 0x24,
};
enum ctn_status{
	status_unkown = 0,
	download_mode,
	initialized,
	detecting,
	detected,
	deactived,
	lost,
	version_missmatch,
	host_request_dischg,
	rf_on,
	rf_off,
	mac_getted,
	app_chn_enabled,
	app_chn_disabled,
	app_chn_connected,
	app_chn_reciced,
	need_to_app_chmn_enable,
	need_to_host_ctrl_enable_send_charging_info,
	need_to_host_ctrl_disable_send_charging_info,
	need_to_rf_on,
	need_to_rf_off,
	dl_open_session_ok,
	dl_flash_ok,
	dl_commit_ok,
	update_firmware_err,
	need_dowload_firmware,
	bist_err,
};
enum charge_status{
	charge_unkown=0,
	charging,
	full,
	not_charging,
};

enum bist_id{
	BIST_RF_ON = 0x01,
	BIST_RF_OFF = 0x02,
	BIST_ACTIVATION=0x04,
	BIST_READ_NTAG_BLOCKS = 0x09,
};

enum ext_cmd{
	ext_cmd_rf_on = 0x01,
	ext_cmd_update_firmware = 0x02,
};


#define ctn730_err(fmt, ...)		\
	pr_err("ctn730: %s: " fmt, 	\
		__func__, ##__VA_ARGS__)	\
		
#define ctn730_dbg(fmt, ...)		\
	pr_debug("ctn730: %s: " fmt, 	\
		__func__, ##__VA_ARGS__)	\

#define MAX_BUFFER_SIZE	256
#define DEV_DETC_INTERVAL 500
#define PEN_PARAM_LEN 32
#define CMD_DELAY_TIME 0
#define MAC_DELAY_TIME 50
#define STATIC_CHECK_DELAY_TIME 500
#define DETECT_CHECK_DELAY_TIME 2000
struct info {
	int level;
	int attached;
	int charge_state;
	char mac[6];
	char pen_uid[8];

};

struct firmware_info {
	char * data;
	u32 user_ee_size;
	u32 user_app_size;
	u32 size;
	int index;
	u32 user_app_addr;
	u32 user_ee_addr;
	u32 crc;
	char version[2];
};

struct ctn730_dev	{
	struct device * dev;
	struct i2c_client *client;
	struct miscdevice ctn730_device;
	struct info pen_info;
	struct firmware_info firmware;
	struct blocking_notifier_head nh;
	
	int ext_cmd;
	int ven_gpio;
	int rst_gpio;
	int irq_gpio;
	int irq;
	bool irq_enabled;
	bool have_mac;
	bool try_to_cmd;
	bool host_ctrl_flag;
	bool enabled;
	bool hardwareinfo;
	bool suspend;
	bool initialized;
	bool level_changed;
	int app_data_ok;
	spinlock_t irq_enabled_lock;
	struct delayed_work set_work;
	struct delayed_work update_firmware_work;
	struct delayed_work rf_on_work;
	struct delayed_work check_static_charge_work;
#ifdef CONFIG_DRM
	struct notifier_block drm_notifier;
#endif
	struct mutex bist_mutex;
	struct mutex report_info_mutex;
	int status;
	int bist_id;
	int report_info_id;
	bool report_info;
	bool need_report_info;
	char pen_param[PEN_PARAM_LEN];
	int pen_param_count;
	char version[2];

};

static const char * const charge_state_text[] = {
	"Unknown", 
	"Charging", 
	"Full", 
	"Notcharging",
};

static const char * const status_text[] = {
	"status_unkown",
	"download_mode",
	"initialized",
	"detecting",
	"detected",
	"deactived",
	"lost",
	"version_missmatch",
	"host_request_dischg",
	"rf_on",
	"rf_off",
	"mac_getted",
	"app_chn_enabled",
	"app_chn_disabled",
	"app_chn_connected",
	"app_chn_reciced",
	"need_to_app_chmn_enable",
	"need_to_host_ctrl_enable_send_charging_info",
	"need_to_host_ctrl_disable_send_charging_info",
	"need_to_rf_on",
	"need_to_rf_off",
	"dl_open_session_ok",
	"dl_flash_ok",
	"dl_commit_ok",
	"update_firmware_err",
	"need_dowload_firmware",
	"bist_err",
};

static int ctn730_parse_message(struct ctn730_dev *ctn730_dev, char cmd, char * data, int count);

struct ctn730_dev *ctn730;

int is_stylus_attached(void){
	return ctn730->pen_info.attached;
}
EXPORT_SYMBOL_GPL(is_stylus_attached);
int ctn730_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ctn730->nh, nb);
}
EXPORT_SYMBOL_GPL(ctn730_notifier_register);

int ctn730_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ctn730->nh, nb);
}
EXPORT_SYMBOL_GPL(ctn730_notifier_unregister);

int ctn730_notifier_call_chain(struct ctn730_dev *ctn730_dev,
	unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&ctn730_dev->nh, val, v);
}

/**********************************************************
 * Interrupt control and handler
 **********************************************************/
static irqreturn_t ctn730_dev_irq_handler(int irq, void *dev_id)
{	
	struct ctn730_dev *ctn730_dev = dev_id;
	char cmd[2];
	int ret;
	char data[MAX_BUFFER_SIZE];
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return IRQ_HANDLED;
	}
	if(ctn730_dev->suspend){
		ctn730_err("suspended, wakeup event!\n");
		pm_wakeup_event(ctn730_dev->ctn730_device.this_device, 10000);
		ctn730_dev->suspend = 0;
		msleep(50);

	}
	ctn730_dbg("\n");
	memset(cmd, 0, 2);
	memset(data, 0, MAX_BUFFER_SIZE);
	if (gpio_get_value(ctn730_dev->irq_gpio)) {
		ret = i2c_master_recv(ctn730_dev->client, cmd, 2);
		if(ret < 0){
			ctn730_err("i2c_master_recv returned %d\n", ret);
			msleep(2);
			return IRQ_HANDLED;
		}

	}
	else{
		ctn730_err("irq_gpio low do not read i2c\n");
		return IRQ_HANDLED;
	}
	if(cmd[1] >0){
		ret = i2c_master_recv(ctn730_dev->client, data, cmd[1]);
	}
	
	ctn730_err("recieve %*ph %*ph\n", 2, cmd,cmd[1],data);
	ctn730_parse_message(ctn730_dev, cmd[0], data, cmd[1]);
	msleep(5);
	return IRQ_HANDLED;
}

/**********************************************************
 * private functions
 **********************************************************/
static int ctn730_i2c_send(struct ctn730_dev * ctn730_dev, char * data,int count){
	int i;
	int ret;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	ctn730_err("%*ph\n", count, data);
	if(ctn730_dev->enabled){
		ctn730_dbg("%s : gpio_int: %d\n", __func__, gpio_get_value(ctn730_dev->irq_gpio));
		for(i=0;i<count;i++){
			ctn730_dbg("ctn730 send data: %02x\n",data[i]);
		}
		ret = i2c_master_send(ctn730_dev->client, data, count);
		if (ret != count) {
			ctn730_err("%s : i2c_master_send returned %d\n", __func__, ret);
			if(ret == -ENOTCONN){
				msleep(1);
				ret = i2c_master_send(ctn730_dev->client, data, count);
				if (ret != count) {
					ret = -EIO;
					switch(ctn730_dev->ext_cmd){
						case ext_cmd_rf_on:
							schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(1));
							break;

						case ext_cmd_update_firmware:
							schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(1));
							break;
						default:
							schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(1));
						break;
							
					}

				}else{
					ctn730_err("%s : i2c_master_send retried ok\n", __func__);
				}
			}
			else{
				ret = -EIO;
				switch(ctn730_dev->ext_cmd){
					case ext_cmd_rf_on:
						schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(100));
						break;

					case ext_cmd_update_firmware:
						schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(100));
						break;
					default:
						schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(100));
						break;
							
				}
}
		}
	}
	return ret;
}

static void ctn730_hard_reset(struct ctn730_dev *ctn730_dev)
{
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}

	ctn730_err("\n");
	if (gpio_is_valid(ctn730_dev->rst_gpio)){
		gpio_set_value_cansleep(ctn730_dev->rst_gpio, 0); 
		msleep(5);
		gpio_set_value_cansleep(ctn730_dev->rst_gpio, 1); 
		ctn730_dev->initialized = 0;

	}

}

static void ctn730_enable(struct ctn730_dev *ctn730_dev)
{
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}

	ctn730_err("\n");
	if(ctn730_dev->enabled)
		return;
	if (gpio_is_valid(ctn730_dev->ven_gpio))
		gpio_set_value_cansleep(ctn730_dev->ven_gpio, 1); 
	ctn730_dev->enabled = 1;
	enable_irq(ctn730_dev->irq);
	
	ctn730_dev->pen_info.attached = 1;
	schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
	ctn730_notifier_call_chain(ctn730_dev, ctn730_dev->pen_info.attached, NULL);
	kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
}

static void ctn730_disable(struct ctn730_dev *ctn730_dev)
{
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_err("\n");
	if(!ctn730_dev->enabled)
		return;

	disable_irq(ctn730_dev->irq);
	if (gpio_is_valid(ctn730_dev->ven_gpio)){
		gpio_set_value(ctn730_dev->ven_gpio, 0);
		ctn730_dev->initialized = 0;
	}
	else{
		ctn730_err("invalid gpio ven_gpio,disable failed\n");
	}
	ctn730_dev->enabled = 0;
	ctn730_dev->pen_info.level = -1;
	ctn730_dev->pen_info.charge_state = charge_unkown;
	ctn730_dev->have_mac = 0;

	memset(ctn730_dev->pen_info.mac, 0, 6);
	memset(ctn730_dev->pen_info.pen_uid, 0, 8);
	ctn730_notifier_call_chain(ctn730_dev, ctn730_dev->pen_info.attached, NULL);
	kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
}
#if 0
static int ctn730_reset_download(struct ctn730_dev *ctn730_dev)
{
	int ret; 
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return ENODEV;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_RESET;
	cmd[2] = 0x01;
	len++;
	cmd[1] = len;
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	return ret;
}
#endif

static void ctn730_reset_init(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_err("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_RESET;
	cmd[2] = 0x00;
	len++;
	cmd[1] = len;
	if(ctn730_i2c_send(ctn730_dev, cmd, len+2)>=0){
		ctn730_dev->initialized = 1;
	}
	
}

static void ctn730_get_mac(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	int ret;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_BIST;
	cmd[2] = BIST_READ_NTAG_BLOCKS;
	len++;
	cmd[3] = 0x10;
	len++;
	cmd[4] = 1;
	len++;
	cmd[1] = len;
	ctn730_err("%*ph\n", len+2, cmd);
	mutex_lock(&ctn730_dev->bist_mutex);
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	if(ret >=0){
		ctn730_dev->bist_id = BIST_READ_NTAG_BLOCKS;
	}
	mutex_unlock(&ctn730_dev->bist_mutex);
	
}

static void ctn730_enable_chg(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = CHG_ENABLE;
	cmd[3] = (char)(DEV_DETC_INTERVAL >> 8);
	len++;
	cmd[2] = (char)(DEV_DETC_INTERVAL);
	len++;
	cmd[1] = len;
	ctn730_i2c_send(ctn730_dev, cmd, len+2);
}

static void ctn730_disable_chg(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = CHG_DISABLE;
	cmd[1] = len;
	ctn730_i2c_send(ctn730_dev, cmd, len+2);
}

static void ctn730_enable_app_channel(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = APP_CTRL_CHN_ENABLE;
	cmd[1] = len;
	ctn730_i2c_send(ctn730_dev, cmd, len+2);
	
}

static void ctn730_disable_app_channel(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = APP_CTRL_CHN_DISABLE;
	cmd[1] = len;
	ctn730_i2c_send(ctn730_dev, cmd, len+2);
	
}

static void ctn730_enable_charging_info(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	int ret;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_WRITE_SESSION_PARAMETER;
	cmd[2] = 0x00;
	len++;
	cmd[3] = 0x01;
	len++;
	cmd[1] = len;
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	if(ret >= 0){
		mutex_lock(&ctn730_dev->report_info_mutex);
		ctn730_dev->report_info_id = 1;
		mutex_unlock(&ctn730_dev->report_info_mutex);
	}
	
}

static void ctn730_disable_charging_info(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	int ret;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_WRITE_SESSION_PARAMETER;
	cmd[2] = 0x00;
	len++;
	cmd[3] = 0x00;
	len++;
	cmd[1] = len;
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	if(ret >= 0){
		mutex_lock(&ctn730_dev->report_info_mutex);
		ctn730_dev->report_info_id = 0;
		mutex_unlock(&ctn730_dev->report_info_mutex);
	}
}

static void ctn730_app_set_param(struct ctn730_dev *ctn730_dev)
{
	char cmd[260];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 260);
	cmd[0] = APP_CTRL_CHN_SEND_DATA;
	cmd[2] = 0x01;
	len++;
	memcpy(cmd+3, ctn730_dev->pen_param, ctn730_dev->pen_param_count);
	len+= ctn730_dev->pen_param_count;
	cmd[1] = len;
	ctn730_i2c_send(ctn730_dev, cmd, len+2);
	
}

static int ctn730_rf_on(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	int ret;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_BIST;
	cmd[2] = BIST_RF_ON;
	len++;
	cmd[1] = len;
	mutex_lock(&ctn730_dev->bist_mutex);
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	if(ret >=0){
		ctn730_dev->bist_id = BIST_RF_ON;
	}
	mutex_unlock(&ctn730_dev->bist_mutex);
	return ret;
}

static int ctn730_rf_off(struct ctn730_dev *ctn730_dev)
{
	char cmd[10];
	int len = 0;
	int ret;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_BIST;
	cmd[2] = BIST_RF_OFF;
	len++;
	cmd[1] = len;
	mutex_lock(&ctn730_dev->bist_mutex);
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	if(ret >=0){
		ctn730_dev->bist_id = BIST_RF_OFF;
	}
	mutex_unlock(&ctn730_dev->bist_mutex);
	return ret;
}

static int ctn730_dl_open(struct ctn730_dev *ctn730_dev)
{
	int ret;
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_DL_OPEN_SESSION;
	cmd[2] = ctn730_dev->firmware.version[0];
	len++;
	cmd[3] = ctn730_dev->firmware.version[1];
	len++;
	cmd[1] = len;
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	return ret;
}

static int ctn730_dl_commit(struct ctn730_dev *ctn730_dev, uint32_t crc)
{
	int ret;
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_DL_COMMIT_SESSION;
	cmd[5] = (crc &0xff000000) >> 24;
	len++;
	cmd[4] = (crc &0xff0000) >> 16;
	len++;
	cmd[3] = (crc &0xff00) >> 8;
	len++;
	cmd[2] = crc &0xff;
	len++;
	cmd[1] = len;
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	return ret;
}

static int ctn730_dl_flash(struct ctn730_dev *ctn730_dev, char * data, int addr)
{
	int ret;
	char cmd[150];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_DL_WRITE_FLASH;
	cmd[2] = addr & 0xff;
	len++;
	cmd[3] = (addr &0xff00) >> 8;
	len++;
	cmd[4] = (addr &0xff0000) >> 16;
	len++;
	memcpy(cmd + 5, data, 128);
	len += 128;
	cmd[1] = len;
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	return ret;
}

static int ctn730_cn_test(struct ctn730_dev *ctn730_dev)
{
	int ret;
	char cmd[10];
	int len = 0;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	ctn730_dbg("\n");
	memset(cmd, 0, 10);
	cmd[0] = HOST_CTRL_BIST;
	cmd[2] = BIST_ACTIVATION;
	len++;
	cmd[1] = len;
	mutex_lock(&ctn730_dev->bist_mutex);
	ret = ctn730_i2c_send(ctn730_dev, cmd, len+2);
	if(ret >=0){
		ctn730_dev->bist_id = BIST_ACTIVATION;
	}
	mutex_unlock(&ctn730_dev->bist_mutex);
	return ret;
}

static int ctn730_parse_message(struct ctn730_dev *ctn730_dev, char cmd, char * data, int count){
	int mt, instruction;
	int charge_state, level;
	char version[6];
	int ret;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	if(data == NULL){
		ctn730_err("no data\n");
		return -ENODATA;
	}
	ctn730_dbg("\n");
	if((cmd & MT_MASK) == RSP_BIT){
		mt = Response;
	}
	else if((cmd & MT_MASK) == EVENT_BIT){
		mt = Event;
	}
	else if((cmd & MT_MASK) == 0){
		mt = Command;
	}
	else{
		mt = unkown;
	}
	ctn730_dbg("mt: %d\n", mt);
	instruction = (cmd & INSTRUCTION_MASK);
	ctn730_dbg("instruction: %d\n", instruction);

	switch(mt){
		case Event:
			switch(instruction){
				case HOST_RESET:
					if(data[0] == 0){
						ctn730_dev->status = initialized;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						ctn730_dev->version[0] = data[1];
						ctn730_dev->version[1] = data[2];
						ctn730_dev->pen_info.attached = 0;
						ctn730_dev->pen_info.level = -1;
						ctn730_dev->pen_info.charge_state = charge_unkown;
						ctn730_dev->have_mac = 0;
						
						memset(ctn730_dev->pen_info.mac, 0, 6);
						memset(ctn730_dev->pen_info.pen_uid, 0, 8);
						if(!ctn730_dev->hardwareinfo){
							sprintf(version,"%02x%02x",ctn730_dev->version[0], ctn730_dev->version[1]);
							ctn730_dev->hardwareinfo = 1;
						}
						//modify by:wangkai41 2022.2.10 desc:fix tp bug
						ctn730_notifier_call_chain(ctn730_dev, ctn730_dev->pen_info.attached, NULL);
						
						kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
						ctn730_err("version %02x%02x\n", ctn730_dev->version[0],ctn730_dev->version[1]);
					}
					
					if(data[0] == 1){
						ctn730_dev->status = download_mode;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						if(ctn730_dev->initialized){
							ctn730_dev->status = need_dowload_firmware;
							ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						}

					}
					switch(ctn730_dev->ext_cmd){
						case ext_cmd_rf_on:
							schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;

						case ext_cmd_update_firmware:
							schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;
						default:
							schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
						break;
							
					}

					break;
				case CHG_DEVICE_STATE:
					
					if(data[0] == 0){
						ctn730_dev->status = detected;
						ret = cancel_delayed_work_sync(&ctn730_dev->check_static_charge_work);
						ctn730_dev->pen_info.attached = 1;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						memcpy(ctn730_dev->pen_info.pen_uid, data+1, 7);
						ctn730_err("pen_uid:%*ph\n", (int)sizeof(ctn730_dev->pen_info.pen_uid), ctn730_dev->pen_info.pen_uid);
						switch(ctn730_dev->ext_cmd){
							case ext_cmd_rf_on:
								schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;

							case ext_cmd_update_firmware:
								schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;
							default:
								schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;
								
						}
						ctn730_notifier_call_chain(ctn730_dev, ctn730_dev->pen_info.attached, NULL);
						kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
					}
					if(data[0] == 1){
						ctn730_dev->status = deactived;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
					}
					if(data[0] == 2){
						ctn730_dev->status = lost;
						ctn730_dev->pen_info.attached = 0;
						ctn730_dev->pen_info.level = -1;
						ctn730_dev->pen_info.charge_state = charge_unkown;
						ctn730_dev->have_mac = 0;
						
						memset(ctn730_dev->pen_info.mac, 0, 6);
						memset(ctn730_dev->pen_info.pen_uid, 0, 8);
						ctn730_notifier_call_chain(ctn730_dev, ctn730_dev->pen_info.attached, NULL);
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
					}
					if(data[0] == 3){
						ctn730_dev->status = version_missmatch;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
					}
					if(data[0] == 4 ||data[0] == 5){
						if(!ctn730_dev->pen_info.attached){
							ctn730_dev->pen_info.attached = 1;
							ctn730_notifier_call_chain(ctn730_dev, ctn730_dev->pen_info.attached, NULL);
							kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
						}
						ret = cancel_delayed_work_sync(&ctn730_dev->check_static_charge_work);
						ctn730_err("ret=%d\n",ret);
						schedule_delayed_work(&ctn730_dev->check_static_charge_work, msecs_to_jiffies(STATIC_CHECK_DELAY_TIME));
					}
					break;
					
				case CHG_CHARGING_STATE:
					
					if(data[0] == 0){
						charge_state = charging;
						ctn730_err("charge_state %s\n",charge_state_text[charge_state]);
						mutex_lock(&ctn730_dev->report_info_mutex);
						if(ctn730_dev->need_report_info && !ctn730_dev->report_info && ctn730_dev->have_mac){
							ctn730_dev->status = need_to_host_ctrl_enable_send_charging_info;
							schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
						}
						mutex_unlock(&ctn730_dev->report_info_mutex);

					}
					if(data[0] == 1){
						charge_state = full;
						ctn730_err("charge_state %s\n",charge_state_text[charge_state]);
					}
					if(data[0] == 2){
						if(ctn730_dev->host_ctrl_flag == 0){
							charge_state = not_charging;
							ctn730_err("charge_state %s\n",charge_state_text[charge_state]);
						}
					}
					if(charge_state != ctn730_dev->pen_info.charge_state){
						ctn730_dev->pen_info.charge_state = charge_state;
						kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);

					}
					break;

				case CHG_CHARGING_INFO:
					level = data[0];
					if(level != ctn730_dev->pen_info.level){
						if(level ==0xff){
							level = -1;
						}
						ctn730_dev->pen_info.level = level;
						ctn730_dev->level_changed = true;
						ctn730_err("level : %d\n", level);
						kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
					}
					break;
				case CHG_DISABLE:
					if(data[0] == 0){
						ctn730_dev->status = host_request_dischg;
						ctn730_dev->try_to_cmd = 1;
						schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(MAC_DELAY_TIME));
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
					}
					if(data[0] == 1){
						ctn730_err("disable_chg due to overtemp\n");
					}
					break;

				case APP_CTRL_CHN_STATE:
					if(data[0] == 0){
						ctn730_dev->status = app_chn_connected;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
					}
					if(data[0] == 1){
						ctn730_dev->status = detected;
						ctn730_err("app_chn_disconnected ,treat as status %s\n",status_text[ctn730_dev->status]);
					}
					break;

				case APP_CTRL_CHN_SEND_DATA:
					ctn730_err("APP_CTRL_CHN_SEND_DATA evt\n");
					break;
					
				case APP_CTRL_CHN_RECIVE_DATA:
					if(data[0] == 1){
						ctn730_err("APP_CTRL_CHN_RECIVE_DATA last data\n");
						ctn730_dev->status = app_chn_reciced;
						schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
						if(data[1] == 0xaa && data[2] == 0x55 && data[3] == 0xcc && data[4] == 0x33){
							ctn730_dev->app_data_ok = 2;
						}
						else{
							ctn730_dev->app_data_ok = 1;
						}
						kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
					}
					if(data[0] == 0){
						ctn730_err("APP_CTRL_CHN_RECIVE_DATA more data\n");
					}
					break;
					
				default:
					ctn730_err("recieve %02x %02x\n", cmd ,count);
					ctn730_err("recieve %*ph\n", count, data);
					break;
			}

			break;

		case Response:
			switch(instruction){
				case CHG_ENABLE:
					if(data[0] == STATUS_OK){
						ctn730_dev->status = detecting;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						schedule_delayed_work(&ctn730_dev->check_static_charge_work, msecs_to_jiffies(DETECT_CHECK_DELAY_TIME));
					}
					else{
						ctn730_err("CHG_ENABLE  Response error: %d\n", data[0]);
					}

					break;
				case CHG_DISABLE:
					if(data[0] == STATUS_OK){
						ctn730_err("CHG_DISABLE OK\n");
					}
					else{
						ctn730_err("CHG_DISABLE  Response error: %d\n", data[0]);
					}

					break;

				case HOST_CTRL_BIST:
					if(data[0] == STATUS_OK){
						mutex_lock(&ctn730_dev->bist_mutex);
						if(ctn730_dev->bist_id == BIST_RF_ON){
							ctn730_dev->status = rf_on;
							ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						}
						if(ctn730_dev->bist_id == BIST_RF_OFF){
							ctn730_dev->status = rf_off;
							ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						}
						if(ctn730_dev->bist_id == BIST_READ_NTAG_BLOCKS){
							memcpy(ctn730_dev->pen_info.mac, data+1, 6);
							ctn730_dev->status = mac_getted;
							ctn730_dev->have_mac = 1;
							ctn730_dev->level_changed = 1;
							kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);

							ctn730_err("status %s\n",status_text[ctn730_dev->status]);
							ctn730_err("pen_mac: %02X:%02X:%02X:%02X:%02X:%02X\n", ctn730_dev->pen_info.mac[5], ctn730_dev->pen_info.mac[4], ctn730_dev->pen_info.mac[3], ctn730_dev->pen_info.mac[2], ctn730_dev->pen_info.mac[1], ctn730_dev->pen_info.mac[0]);
							ctn730_err("recieve %02x %02x\n", cmd ,count);
							ctn730_err("recieve %*ph\n", count, data);
						}
						if(ctn730_dev->bist_id == BIST_ACTIVATION){
							ctn730_err("BIST_ACTIVATION:%*ph\n", count, data);
						}
						mutex_unlock(&ctn730_dev->bist_mutex);
						switch(ctn730_dev->ext_cmd){
							case ext_cmd_rf_on:
								break;

							case ext_cmd_update_firmware:
								break;

							default:
								schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;
								
						}

					}
					else{
						ctn730_dev->status = bist_err;
						schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
						ctn730_err("HOST_CTRL_BIST  Response error: %d\n", data[0]);
					}

					break;
				case APP_CTRL_CHN_ENABLE:
					if(data[0] == STATUS_OK){
						ctn730_dev->status = app_chn_enabled;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
					}

					break;

				case APP_CTRL_CHN_SEND_DATA:
					if(data[0] == STATUS_OK){
						ctn730_dev->status = detected;
						ctn730_err("APP_CTRL_CHN_SEND_DATA rsp ok, treat as detected\n");
					}else{
						ctn730_dev->app_data_ok = 1;
						kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
					}
					break;

				case APP_CTRL_CHN_DISABLE:
					if(data[0] == STATUS_OK){
						ctn730_dev->status = app_chn_disabled;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
					}

					break;

				case HOST_CTRL_WRITE_SESSION_PARAMETER:
					if(data[0] == STATUS_OK){
						ctn730_dev->status = detected;
						mutex_lock(&ctn730_dev->report_info_mutex);
						if(ctn730_dev->report_info_id ==1){
							ctn730_dev->report_info =1;
						}
						if(ctn730_dev->report_info_id ==0){
							ctn730_dev->report_info =0;
						}
						mutex_unlock(&ctn730_dev->report_info_mutex);

						ctn730_err("status HOST_CTRL_WRITE_SESSION_PARAMETER ok, treat as detected\n");
					}
					else{
						ctn730_err("status HOST_CTRL_WRITE_SESSION_PARAMETER failed\n");
					}
					break;

				case HOST_CTRL_DL_OPEN_SESSION:
					if(data[0] == STATUS_OK){
						ctn730_dev->status = dl_open_session_ok;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						switch(ctn730_dev->ext_cmd){
							case ext_cmd_rf_on:
								schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;

							case ext_cmd_update_firmware:
								schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;
							default:
								schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;
								
						}
					}
					else{
						ctn730_dev->status = update_firmware_err;
						ctn730_err("status HOST_CTRL_DL_OPEN_SESSION failed\n");
						switch(ctn730_dev->ext_cmd){
							case ext_cmd_rf_on:
								schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;

							case ext_cmd_update_firmware:
								schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;
							default:
								schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;
								
						}
					}
					break;

				case HOST_CTRL_DL_COMMIT_SESSION:
					if(data[0] == STATUS_OK){
						ctn730_dev->status = dl_commit_ok;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						switch(ctn730_dev->ext_cmd){
							case ext_cmd_rf_on:
								schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;

							case ext_cmd_update_firmware:
								schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;
							default:
								schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;
								
						}
					}
					else{
						ctn730_dev->status = update_firmware_err;
						ctn730_err("status HOST_CTRL_DL_COMMIT_SESSION failed\n");
						switch(ctn730_dev->ext_cmd){
							case ext_cmd_rf_on:
								schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;

							case ext_cmd_update_firmware:
								schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;
							default:
								schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;
								
						}
					}
					break;

				case HOST_CTRL_DL_WRITE_FLASH:
					if(data[0] == STATUS_OK){
						ctn730_dev->status = dl_flash_ok;
						ctn730_err("status %s\n",status_text[ctn730_dev->status]);
						switch(ctn730_dev->ext_cmd){
							case ext_cmd_rf_on:
								schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;

							case ext_cmd_update_firmware:
								schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;
							default:
								schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;
								
						}
					}
					else{
						ctn730_dev->status = update_firmware_err;
						ctn730_err("status HOST_CTRL_DL_WRITE_FLASH failed\n");
						switch(ctn730_dev->ext_cmd){
							case ext_cmd_rf_on:
								schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;

							case ext_cmd_update_firmware:
								schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(CMD_DELAY_TIME));
								break;
							default:
								schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
							break;
								
						}
					}
					break;
				default:
					ctn730_err("recieve %02x %02x\n", cmd ,count);
					ctn730_err("recieve %*ph\n", count, data);
					break;
			}
			break;
			
		default:	
			break;
	}
	return 0;
	
}

static void set_work(struct work_struct *work)
{
	struct ctn730_dev *ctn730_dev = container_of(work,
			struct ctn730_dev, set_work.work);

	ctn730_err("\n");
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	if(ctn730_dev->ext_cmd){
		ctn730_err("has ext_cmd, skip\n");
		goto exit;
	}
	switch(ctn730_dev->status){
		case  download_mode :
			ctn730_reset_init(ctn730_dev);
			break;
			
		case  initialized:
			ctn730_enable_chg(ctn730_dev);
			break;
			
		case  detected:
			if(!ctn730_dev->have_mac){
				ctn730_disable_chg(ctn730_dev);
				ctn730_dev->host_ctrl_flag = 1;
			}
			break;
			
		case host_request_dischg:
			ctn730_rf_on(ctn730_dev);
			break;
			
		case rf_on:
			if(!ctn730_dev->ext_cmd){
				if(!ctn730_dev->have_mac){
					ctn730_get_mac(ctn730_dev);
				}
			}
			break;
			
		case mac_getted:
			ctn730_rf_off(ctn730_dev);
			break;
			
		case rf_off:
			ctn730_enable_chg(ctn730_dev);
			ctn730_dev->host_ctrl_flag = 0;
			break;
			
		case need_to_app_chmn_enable:
			ctn730_enable_app_channel(ctn730_dev);
			break;
			
		case app_chn_connected:
			ctn730_app_set_param(ctn730_dev);
			break;
			
		case app_chn_reciced:
			ctn730_disable_app_channel(ctn730_dev);
			break;
			
		case need_to_host_ctrl_enable_send_charging_info:
			ctn730_enable_charging_info(ctn730_dev);
			break;

		case need_to_host_ctrl_disable_send_charging_info:
			ctn730_disable_charging_info(ctn730_dev);
			break;
			
		case need_to_rf_on:
			ctn730_disable_chg(ctn730_dev);
			break;
			
		case need_to_rf_off:
			ctn730_rf_off(ctn730_dev);
			break;

		case bist_err:
			ctn730_reset_init(ctn730_dev);
			break;
			
	default:
			break;
	}
exit:
	return;
}

static void check_static_charge_work(struct work_struct *work)
{
	struct ctn730_dev *ctn730_dev = container_of(work,
			struct ctn730_dev, check_static_charge_work.work);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_err("\n");
	ctn730_dev->pen_info.attached = 0;
	ctn730_dev->pen_info.level = -1;
	ctn730_dev->pen_info.charge_state = charge_unkown;
	ctn730_dev->have_mac = 0;
						
	memset(ctn730_dev->pen_info.mac, 0, 6);
	memset(ctn730_dev->pen_info.pen_uid, 0, 8);
	ctn730_notifier_call_chain(ctn730_dev, ctn730_dev->pen_info.attached, NULL);
	kobject_uevent(&ctn730_dev->ctn730_device.this_device->kobj, KOBJ_CHANGE);
}

static void rf_on_work(struct work_struct *work)
{
	struct ctn730_dev *ctn730_dev = container_of(work,
			struct ctn730_dev, rf_on_work.work);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	ctn730_err("\n");
	if(ctn730_dev->ext_cmd == 0){
		ctn730_hard_reset(ctn730_dev);		
	}
	if(ctn730_dev->ext_cmd == ext_cmd_rf_on){
		switch(ctn730_dev->status){
			case  download_mode :
				ctn730_reset_init(ctn730_dev);
				break;
			case initialized:
				ctn730_rf_on(ctn730_dev);
				break;
			default:
				ctn730_hard_reset(ctn730_dev);
				break;
		}
	}
}
static void update_firmware_work(struct work_struct *work)
{
	struct ctn730_dev *ctn730_dev = container_of(work,
			struct ctn730_dev, update_firmware_work.work);

	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return;
	}
	
	ctn730_dbg("\n");
	if(!ctn730_dev->enabled){
		ctn730_enable(ctn730_dev);
		ctn730_dev->firmware.index = 0;
		return;
	}
	switch(ctn730_dev->status){
		case download_mode:
			ctn730_dl_open(ctn730_dev);
			break;

		case dl_open_session_ok:
		case dl_flash_ok:	
			if(ctn730_dev->firmware.index < ctn730_dev->firmware.user_ee_size /128){
				if(ctn730_dl_flash(ctn730_dev, ctn730_dev->firmware.data+ctn730_dev->firmware.index * 128, \
											ctn730_dev->firmware.user_ee_addr+ ctn730_dev->firmware.index * 128)>=0){
					ctn730_dev->firmware.index ++;
				}
			}
			else if(ctn730_dev->firmware.index < ctn730_dev->firmware.size /128){
				if(ctn730_dl_flash(ctn730_dev, ctn730_dev->firmware.data+ctn730_dev->firmware.index * 128, \
											ctn730_dev->firmware.user_app_addr+ (ctn730_dev->firmware.index - ctn730_dev->firmware.user_ee_size /128) * 128)>=0){
					ctn730_dev->firmware.index ++;

				}
			}else if(ctn730_dev->firmware.index == ctn730_dev->firmware.size /128){
				if(ctn730_dl_commit(ctn730_dev, ctn730_dev->firmware.crc)>=0){
					ctn730_dev->firmware.index ++;

				}
			}
			break;
			
		case dl_commit_ok:
			ctn730_dev->ext_cmd = 0;
			ctn730_dev->status = download_mode;
			if(ctn730_dev->firmware.data != NULL)
				vfree(ctn730_dev->firmware.data);
			schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(100));

			break;
		case detecting:
		case lost:	
		case update_firmware_err:
		default:
			ctn730_disable(ctn730_dev);
	
			break;

	}

}

static int ctn730_parse_dt(struct device *dev,
							struct ctn730_dev *ctn730_dev)
{
	struct device_node *node = NULL;
	struct device_node *node_f = NULL;
	struct drm_panel *panel = NULL;
	u32 flags;
	int val;
	int i;
	int count;
	
	if(dev == NULL){
		ctn730_err("no dev\n");
		return -ENODEV;
	}
	node = dev->of_node;
	if (!node){
		ctn730_err("no node\n");
		return -ENODEV;
	}
	
	val = of_get_named_gpio_flags(node, "rst-gpios", 0, &flags);
	if (val >= 0) {
		ctn730_dev->rst_gpio = val;
	}
	else {
		dev_err(dev, "RST GPIO error getting from OF node\n");
		return val;
	}
	
	val = of_get_named_gpio_flags(node, "enable-gpios", 0, &flags);
	if (val >= 0) {
		ctn730_dev->ven_gpio = val;
	}
	else {
		dev_err(dev, "VEN GPIO error getting from OF node\n");
		return val;
	}

	val = of_get_named_gpio_flags(node, "interrupt-gpios", 0, &flags);
	if (val >= 0) {
		ctn730_dev->irq_gpio = val;
	}
	else {
		dev_err(dev, "IRQ GPIO error getting from OF node\n");
		return val;
	}

	count = of_count_phandle_with_args(node, "panel", NULL);
	
	ctn730_err("panel count:%d \n", count);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node_f = of_parse_phandle(node, "panel", i);
		panel = of_drm_find_panel(node_f);
		of_node_put(node_f);

		if (!IS_ERR(panel)) { 
			ctn730_err("find panel\n");
			ctn730_panel = panel;
			return 0; 
		}
	}

	return -EPROBE_DEFER; 
}

static ssize_t enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int enable;
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	enable = simple_strtoul(buf, NULL, 0);
	
	ctn730_dbg("enable:%d\n", enable);
	
	if(enable){
		ctn730_enable(ctn730_dev);
	}
	else{		
		ctn730_dev->ext_cmd = 0;
		ctn730_disable(ctn730_dev);
    }
	
	return size;
}

static ssize_t enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	return sprintf(buf, "%d\n",gpio_get_value(ctn730_dev->ven_gpio));	
}

static ssize_t pen_cmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);


	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	if(ctn730_dev->status == detected && ctn730_dev->have_mac){
		ctn730_dev->pen_param[0] = 0xaa;
		ctn730_dev->pen_param[1] = 0x55;
		ctn730_dev->pen_param[2] = 0xcc;
		ctn730_dev->pen_param[3] = 0x33;
		ctn730_dev->pen_param_count = 4;
		ctn730_dev->status = need_to_app_chmn_enable;
		schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(0));
	}
	else{
		ctn730_err("status: %s have_mac %d,no pen_cmd\n", status_text[ctn730_dev->status],ctn730_dev->have_mac );
	}
	return size;
}

static ssize_t reget_mac_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int enable;
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);

	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}

	enable = simple_strtoul(buf, NULL, 0);

	if(enable){
		ctn730_dev->status = detected;
		ctn730_dev->have_mac = 0;
		schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(0));
	}
	return size;
}

static ssize_t info_report_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int enable;
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);

	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}

	enable = simple_strtoul(buf, NULL, 0);

	if(enable){
		ctn730_dev->status = need_to_host_ctrl_enable_send_charging_info;
	}
	else{
		ctn730_dev->status = need_to_host_ctrl_disable_send_charging_info;
	}
	schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(100));

	return size;
}

static ssize_t rf_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int enable;
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);

	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	
	enable = simple_strtoul(buf, NULL, 0);

	if(enable){
		ctn730_dev->ext_cmd = ext_cmd_rf_on;
	}
	else{
		ctn730_dev->ext_cmd = 0;

	}
	schedule_delayed_work(&ctn730_dev->rf_on_work, msecs_to_jiffies(10));

	return size;
}

static ssize_t param_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}

	if(size >32){
		ctn730_err("too many input\n");
	}
	memcpy(ctn730_dev->pen_param, buf, size);
	ctn730_dev->pen_param_count = size;
	ctn730_dev->status = need_to_app_chmn_enable;
	schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(100));
	
	return size;
}

static ssize_t  __attribute__((__unused__)) update_firmware_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	int ret;
	const struct firmware *fw_user_ee = NULL;
	const struct firmware *fw_user_app = NULL;
	u32 user_ee_size =0;
	u32 user_app_size =0;
	u32 datasize =0;
	int version;
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	
	version = simple_strtoul(buf, NULL, 0);
	if(version){
		ctn730_dev->ext_cmd = ext_cmd_update_firmware;
	}
	else{
		ctn730_dev->ext_cmd = 0;
		return size;
	}
	ctn730_dev->firmware.version[1] = version;
	ctn730_dev->firmware.version[0] = version >> 8;
	ctn730_dev->firmware.index = 0;
	
	ret = request_firmware(&fw_user_ee, "user_ee_hostcontrolled.bin", ctn730_dev->ctn730_device.this_device);
	if(ret){
		ctn730_err("request_firmware user_ee_hostcontrolled.bin failed.ret= %d\n", ret);
		goto exit;
	}
	ctn730_dbg("user_ee_hostcontrolled size= %d\n", fw_user_ee->size);
	user_ee_size = (((long long)fw_user_ee->size % 128) == 0) ? (long long)fw_user_ee->size : (((long long)fw_user_ee->size >> 7) + 1) << 7;

	ctn730_err("user_ee_size = %d\n", user_ee_size);
	if(fw_user_ee->size > 3073){
		ctn730_err("user_ee_hostcontrolled.bin has wrong size,exit\n");
		goto exit;
	}
	if(fw_user_ee->size == 3073){
		ctn730_err("user_ee_hostcontrolled.bin has end char,size as 3072 \n");
		user_ee_size = 3072;
	}
	
	ret = request_firmware(&fw_user_app, "wlc_host_user_app.bin", ctn730_dev->ctn730_device.this_device);
	if(ret){
		ctn730_err("request_firmware wlc_host_user_app.bin failed.ret= %d\n", ret);
		goto exit;
	}
	ctn730_dbg("wlc_host_user_app size= %d\n", fw_user_app->size);
	user_app_size = (((long long)fw_user_app->size % 128) == 0) ? (long long)fw_user_app->size : (((long long)fw_user_app->size >> 7) + 1) << 7;

	datasize = user_app_size + user_ee_size;
	ctn730_err("user_app_size = %d\n", user_app_size);
	
	ctn730_err("size= %d\n", datasize);

	ctn730_dev->firmware.data= vmalloc(datasize);
	if(ctn730_dev->firmware.data == NULL){
		ctn730_err("vmalloc size %d failed\n", datasize);
		goto exit;
	}
	ctn730_dev->firmware.user_app_size = user_app_size;
	ctn730_dev->firmware.user_ee_size = user_ee_size;
	ctn730_dev->firmware.size = datasize;
	ctn730_dev->firmware.user_app_addr = 0x207000;
	ctn730_dev->firmware.user_ee_addr = 0x201200;

	memset(ctn730_dev->firmware.data, 0, datasize);
	memcpy(ctn730_dev->firmware.data, fw_user_ee->data, fw_user_ee->size);
	memcpy(ctn730_dev->firmware.data + user_ee_size, fw_user_app->data, fw_user_app->size);
	ctn730_dev->firmware.crc = get_crc32(ctn730_dev->firmware.data, datasize);
	ctn730_err("crc = 0x%x\n", ctn730_dev->firmware.crc );
	
	schedule_delayed_work(&ctn730_dev->update_firmware_work, msecs_to_jiffies(100));
	
	
exit:
	if(fw_user_app)
		release_firmware(fw_user_app);
	if(fw_user_ee)
		release_firmware(fw_user_ee);
	
	return size;
}
static ssize_t charge_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	return sprintf(buf, "%s\n", charge_state_text[ctn730_dev->pen_info.charge_state]);
}
static ssize_t mac_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	return sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X\n", ctn730_dev->pen_info.mac[5], ctn730_dev->pen_info.mac[4], ctn730_dev->pen_info.mac[3], ctn730_dev->pen_info.mac[2], ctn730_dev->pen_info.mac[1], ctn730_dev->pen_info.mac[0]);
}

static ssize_t version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
		
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	return sprintf(buf, "%02x%02x", ctn730_dev->version[0], ctn730_dev->version[1]);
}

static ssize_t level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	return sprintf(buf, "%d\n", ctn730_dev->pen_info.level);
}

static ssize_t attached_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	return sprintf(buf, "%d\n", ctn730_dev->pen_info.attached);
}

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	return sprintf(buf, "%s\n", status_text[ctn730_dev->status]);
}

static ssize_t cn_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);
	
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	ctn730_cn_test(ctn730_dev);
	return sprintf(buf, "%s\n", status_text[ctn730_dev->status]);
}

static DEVICE_ATTR_RW(enable);
static DEVICE_ATTR_WO(pen_cmd);
static DEVICE_ATTR_WO(reget_mac);
static DEVICE_ATTR_WO(info_report);
static DEVICE_ATTR_WO(rf_on);
static DEVICE_ATTR_WO(param);
//static DEVICE_ATTR_WO(update_firmware);
static DEVICE_ATTR_RO(charge_state);
static DEVICE_ATTR_RO(mac);
static DEVICE_ATTR_RO(version);
static DEVICE_ATTR_RO(level);
static DEVICE_ATTR_RO(attached);
static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_RO(cn_test);

static struct device_attribute * ctn730_attrs[] = {
	&dev_attr_enable,
	&dev_attr_pen_cmd,
	&dev_attr_reget_mac,
	&dev_attr_info_report,
	&dev_attr_rf_on,
	&dev_attr_param,
	//&dev_attr_update_firmware,
	&dev_attr_charge_state,
	&dev_attr_mac,
	&dev_attr_version,
	&dev_attr_level,
	&dev_attr_attached,
	&dev_attr_status,
	&dev_attr_cn_test,
	NULL
};

static int ctn730_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	int ret;
	int i;
	struct ctn730_dev *ctn730_dev = container_of(dev_get_drvdata(dev),
			struct ctn730_dev, ctn730_device);

	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	if(ctn730_dev->app_data_ok){
		if(ctn730_dev->app_data_ok ==1){
			ret = add_uevent_var(env, "WRITEPENDATA=0");
			ctn730_dev->app_data_ok = 0;
			if (ret)
				dev_err(dev, "failed to add uevent STATUS\n");
		}
		if(ctn730_dev->app_data_ok ==2){
			ret = add_uevent_var(env, "WRITEPENDATA=1");
			ctn730_dev->app_data_ok = 0;
			if (ret)
				dev_err(dev, "failed to add uevent STATUS\n");
		}
	}
	ret = add_uevent_var(env, "ATTACHED=%d", ctn730_dev->pen_info.attached);
	if (ret)
		dev_err(dev, "failed to add uevent STATUS\n");
	ret = add_uevent_var(env, "CHARGING_STATE=%s", charge_state_text[ctn730_dev->pen_info.charge_state]);
	if (ret)
		dev_err(dev, "failed to add uevent CHARGING_STATE\n");
	if(ctn730_dev->level_changed){
		if(ctn730_dev->pen_info.level != -1){
			ret = add_uevent_var(env, "LEVEL=%d", ctn730_dev->pen_info.level);
			if (ret)
				dev_err(dev, "failed to add uevent LEVEL\n");
			ctn730_dev->level_changed = false;
		}
	}
	ret = add_uevent_var(env, "MAC=%02X:%02X:%02X:%02X:%02X:%02X", ctn730_dev->pen_info.mac[5], ctn730_dev->pen_info.mac[4], ctn730_dev->pen_info.mac[3], ctn730_dev->pen_info.mac[2], ctn730_dev->pen_info.mac[1], ctn730_dev->pen_info.mac[0]);
	if (ret)
		dev_err(dev, "failed to add uevent LEVEL\n");
	
	for (i = 0; i < env->envp_idx; i++)
		ctn730_dbg("%s\n",env->envp[i]);
	return ret;
}

 const struct device_type ctn730_dev_type = {
	.name = "nfc_charger",
	.uevent = ctn730_uevent,
};

#if defined (CONFIG_DRM)

int ctn730_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct ctn730_dev *ctn730_dev = container_of(self,
			struct ctn730_dev, drm_notifier);

	struct drm_panel_notifier *notifier_data = data;

	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
		return -ENODEV;
	}
	if(data == NULL){
		ctn730_err("no data\n");
		return -ENODATA;
	}
	if(notifier_data){
		int *blank = notifier_data->data;
		//ctn730_err("event:%d,blank:%d\n",event,*blank);

		if (event == DRM_PANEL_EARLY_EVENT_BLANK) {
			if (*blank == DRM_PANEL_BLANK_POWERDOWN){
				mutex_lock(&ctn730_dev->report_info_mutex);
				ctn730_dev->need_report_info = 0;
				mutex_unlock(&ctn730_dev->report_info_mutex);
				ctn730_err("need_report_info:%d report_info:%d\n",ctn730_dev->need_report_info, ctn730_dev->report_info);
				
				if(ctn730_dev->pen_info.charge_state == charging && ctn730_dev->have_mac){
					ctn730_dev->status = need_to_host_ctrl_disable_send_charging_info;
					schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
				}
				else{
					ctn730_err("status: %s have_mac %d,no charging info cmd\n", status_text[ctn730_dev->status],ctn730_dev->have_mac );
				}
			}
		}
         	if (event == DRM_PANEL_EVENT_BLANK) {
			if (*blank == DRM_PANEL_BLANK_UNBLANK){
				mutex_lock(&ctn730_dev->report_info_mutex);
				ctn730_dev->need_report_info = 1;
				mutex_unlock(&ctn730_dev->report_info_mutex);
				ctn730_err("need_report_info:%d report_info:%d\n",ctn730_dev->need_report_info, ctn730_dev->report_info);
				if(ctn730_dev->pen_info.charge_state == charging && ctn730_dev->have_mac){
					ctn730_dev->status = need_to_host_ctrl_enable_send_charging_info;
					schedule_delayed_work(&ctn730_dev->set_work, msecs_to_jiffies(CMD_DELAY_TIME));
				}
				else{
					ctn730_err("status: %s have_mac %d,no charging info cmd\n", status_text[ctn730_dev->status],ctn730_dev->have_mac );
				}

			}
		}
	}
     return 0;
}
#endif

/*
 * ctn730_probe
 */

static int ctn730_probe(struct i2c_client *client,
		const struct i2c_device_id *id)

{
	int ret;
	int i;
	struct ctn730_dev *ctn730_dev; // internal device specific data
	ctn730_err("\n");
	
	ctn730_dev = kzalloc(sizeof(*ctn730_dev), GFP_KERNEL);
	if (ctn730_dev == NULL) {
		ctn730_err("failed to allocate memory for module data\n");
		return -ENOMEM;
	}
	ctn730 = ctn730_dev;
	ret= ctn730_parse_dt(&client->dev, ctn730_dev);
	if(ret < 0){
		ctn730_err("parse device tree fail,probe fail\n");
		goto exit1;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ctn730_err("need I2C_FUNC_I2C\n");
		ret=  -ENODEV;
		goto exit1;
	}

	ctn730_dbg("request irq_gpio %d\n", ctn730_dev->irq_gpio);
	
	ret = gpio_request(ctn730_dev->irq_gpio, "nfc_int");
	if (ret){
		ctn730_err("not able to get GPIO irq_gpio\n");
		ret=  -ENODEV;
		goto exit1;
	}
	ret = gpio_request(ctn730_dev->ven_gpio, "nfc_ven");
	if (ret){
		ctn730_err("not able to get GPIO ven_gpio\n");
		ret=  -ENODEV;
		goto exit2;
	}

	ret = gpio_to_irq(ctn730_dev->irq_gpio);
	if (ret < 0){
		ctn730_err("not able to map GPIO irq_gpio to an IRQ\n");
		ret=  -ENODEV;
		goto exit3;
	}
	else{
		ctn730_dev->irq = ret;
	}

	ctn730_dbg("request ven_gpio %d\n", ctn730_dev->ven_gpio);

	ctn730_dev->client = client;
	/* finish configuring the I/O */
	ret = gpio_direction_input(ctn730_dev->irq_gpio);
	if (ret < 0) {
		ctn730_err("not able to set irq_gpio as input\n");
		goto exit3;
	}

	ret = gpio_direction_output(ctn730_dev->ven_gpio, 0);
	if (ret < 0) {
		ctn730_err("not able to set ven_gpio as output\n");
		goto exit3;
	}


	/* init mutex and queues */
	spin_lock_init(&ctn730_dev->irq_enabled_lock);

	/* register as a misc device - character based with one entry point */
	ctn730_dev->ctn730_device.minor = MISC_DYNAMIC_MINOR;
	ctn730_dev->ctn730_device.name = "ctn730";
	ctn730_dev->ctn730_device.nodename = "ctn730";
	ret = misc_register(&ctn730_dev->ctn730_device);
	if (ret) {
		ctn730_err("misc_register failed\n");
		goto exit3;
	}
	ctn730_dev->ctn730_device.this_device->type = &ctn730_dev_type;
	for(i=0; i<sizeof(ctn730_attrs)/sizeof(struct device_attribute * ) -1; i++){
		device_create_file(ctn730_dev->ctn730_device.this_device,ctn730_attrs[i]);
	}
	ctn730_dev->dev = ctn730_dev->ctn730_device.this_device;
#ifdef CONFIG_DRM
	ctn730_dev->drm_notifier.notifier_call = ctn730_drm_notifier_callback;
	if(ctn730_panel == NULL)
		ctn730_err("drm panel is  NULL\n");
        else if(drm_panel_notifier_register(ctn730_panel, &ctn730_dev->drm_notifier)) 
		ctn730_err("Unable to register ctn730_drm_notifier\n");
#endif
	device_init_wakeup(ctn730_dev->ctn730_device.this_device, 1);
	device_wakeup_enable(ctn730_dev->ctn730_device.this_device);
	ctn730_dbg("requesting IRQ %d\n", ctn730_dev->irq);
	i2c_set_clientdata(client, ctn730_dev);
	INIT_DELAYED_WORK(&ctn730_dev->update_firmware_work, update_firmware_work);
	INIT_DELAYED_WORK(&ctn730_dev->rf_on_work, rf_on_work);
	INIT_DELAYED_WORK(&ctn730_dev->check_static_charge_work, check_static_charge_work);
	INIT_DELAYED_WORK(&ctn730_dev->set_work, set_work);
	ret = devm_request_threaded_irq(ctn730_dev->dev, ctn730_dev->irq,
			NULL, ctn730_dev_irq_handler, IRQF_TRIGGER_HIGH |IRQF_ONESHOT, ctn730_dev->ctn730_device.name, ctn730_dev);
	if (ret) {
		ctn730_err("request_irq failed\n");
		goto exit4;
	}
	disable_irq(ctn730_dev->irq);

	mutex_init(&ctn730_dev->bist_mutex);
	mutex_init(&ctn730_dev->report_info_mutex);
	enable_irq_wake(ctn730_dev->irq);
	ctn730_enable(ctn730_dev);
	ctn730_dev->report_info = 1;
	ctn730_err("success\n");
	
	return 0;

exit4:
	misc_deregister(&ctn730_dev->ctn730_device);
exit3:
	gpio_free(ctn730_dev->ven_gpio);
exit2:
	gpio_free(ctn730_dev->irq_gpio);
exit1:
	if(ctn730_dev != NULL){
		kfree(ctn730_dev);
		ctn730_dev = NULL;
	}
	return ret;
}


static int ctn730_remove(struct i2c_client *client)

{
	struct ctn730_dev *ctn730_dev;

	pr_info("ctn730_remove:%s\n", __func__);

	ctn730_dev = i2c_get_clientdata(client);
	free_irq(ctn730_dev->irq, ctn730_dev);
	misc_deregister(&ctn730_dev->ctn730_device);
	gpio_free(ctn730_dev->irq_gpio);
	gpio_free(ctn730_dev->ven_gpio);


	kfree(ctn730_dev);

	return 0;
}


static int ctn730_suspend(struct device *dev){
	struct ctn730_dev *ctn730_dev = dev_get_drvdata(dev);
	if(ctn730_dev == NULL){
		ctn730_err("no ctn730_dev\n");
	}
	ctn730_dev->suspend = 1;
	if(ctn730_dev->report_info && ctn730_dev->status == detected && ctn730_dev->have_mac){
		ctn730_disable_charging_info(ctn730_dev);
	}
	pr_err("%s\n",__func__);
	return 0;
}

static int ctn730_resume(struct device *dev){
	pr_err("%s\n",__func__);
	return 0;
}

static const struct dev_pm_ops ctn730_pm_ops = {
	.suspend	= ctn730_suspend,
	.resume	= ctn730_resume,
};

static struct of_device_id ctn730_dt_match[] = {
	{ 
		.compatible = "nxp,ctn730",
		.name = "ctn730",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ctn730_dt_match);

static struct i2c_driver ctn730_driver = {
	.probe		= ctn730_probe,
	.remove		= ctn730_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ctn730",
		.of_match_table = ctn730_dt_match,
		.pm		= &ctn730_pm_ops,
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

MODULE_DESCRIPTION("ctn730 driver");
MODULE_LICENSE("GPL");
