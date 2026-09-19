#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/bootinfo.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <linux/suspend.h>

#define SYS_BOOT_INFO   "hwinfo"
#define	WS_DBG_INTERVAL_SECS	20
#define HARDWARE_MAX_ITEM_LONGTH 64
/* SYNC With ABL HWBoardID ID_Array */
#define BOARD_ID_ROW_PRC_BIT BIT(0)


struct boot_info {
	int sku;
	int hw;
	int boot_mode;
	bool hw_pm_debug;
	struct delayed_work ws_dbg_wk;
};

struct hardware {
	struct list_head node;
	char name[HARDWARE_MAX_ITEM_LONGTH];
	char model[HARDWARE_MAX_ITEM_LONGTH];
};


/* List of hardware handles (struct hardware) */
static LIST_HEAD(hardware_list);
static DEFINE_MUTEX(hardware_list_mutex);

static struct boot_info bootinfo;

const char *sku_info[] = {
	[HW_SKU_ROW_LTE] = "lte_row",
	[HW_SKU_PRC_LTE] = "lte_prc",
	[HW_SKU_WIFI] = "wifi",
	[HW_SKU_UNKNOWN] = "unknow",
};

const char *hw_info_str[] = {
	[HW_PLATFORM_EVB] = "evb",
	[HW_PLATFORM_EVT] = "evt",
	[HW_PLATFORM_DVT1] = "dvt1",
	[HW_PLATFORM_DVT1_2] = "dvt1_2",
	[HW_PLATFORM_DVT2] = "dvt2",
	[HW_PLATFORM_DVT2_2] = "dvt2_2",
	[HW_PLATFORM_DVT2_3] = "dvt2_3",
	[HW_PLATFORM_PVT1] = "pvt1",
	[HW_PLATFORM_PVT2] = "pvt2",
	[HW_PLATFORM_PVT] = "pvt",
	[HW_PLATFORM_UNKNOWN] = "unknow",
};

static int get_hw_board_id(char *str)
{
	int boardid_raw = 0;
	get_option(&str, &boardid_raw);

	bootinfo.sku = (boardid_raw >> 8) & 0xff;
	bootinfo.hw = boardid_raw & 0xff;
	pr_info("Board ID sku:%d hw:%d Raw:%d\n", bootinfo.sku, bootinfo.hw, boardid_raw);
	return 1;
}
__setup("androidboot.hwboardid=", get_hw_board_id);


bool hw_pm_debug_enable(void) {
	return bootinfo.hw_pm_debug;
}

static ssize_t hwid_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	int buf_size = 0;

	mutex_lock(&hardware_list_mutex);
	buf_size = sprintf(buf, "%s_%s\n", sku_info[bootinfo.sku], hw_info_str[bootinfo.hw]);
	mutex_unlock(&hardware_list_mutex);

	return buf_size;
}


static struct kobj_attribute hw_type_attr =
	__ATTR_RO(hwid);


/*ddr*/
static ssize_t ddr_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int buf_size = 0;
	struct sysinfo val;
        int temp_size;

	mutex_lock(&hardware_list_mutex);
	si_meminfo(&val);
        temp_size = ((val.totalram << (PAGE_SHIFT - 10)) / (1024*1024) + 1);
        buf_size = sprintf(buf, "%d", temp_size);
	mutex_unlock(&hardware_list_mutex);

	return buf_size;
}

static ssize_t hardwareinfo_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct hardware *h;

	mutex_lock(&hardware_list_mutex);
	list_for_each_entry(h, &hardware_list, node) {
		sprintf(buf+strlen(buf), "%s: %s\n", h->name, h->model);
	}
	mutex_unlock(&hardware_list_mutex);

	return strlen(buf);
}

extern char *socinfo_get_cpu_info(void);
static ssize_t cpu_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{

	return sprintf(buf, "%s", socinfo_get_cpu_info());
}

static struct kobj_attribute ddr_attr = __ATTR_RO(ddr);
static struct kobj_attribute hw_info_attr = __ATTR_RO(hardwareinfo);
static struct kobj_attribute cpu_attr = __ATTR_RO(cpu);

static ssize_t hw_pm_debug_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	return sprintf(buf, "hw pm debug flag %d\n", bootinfo.hw_pm_debug);
}

static ssize_t  hw_pm_debug_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	int ret = 0;
	bool enabled = 0;
	ret = kstrtobool(buf, &enabled);
	if (ret)
		return ret;
	pr_info("HW PM Debug %s\n", enabled ? "Enable":"Disable");
	bootinfo.hw_pm_debug = enabled;
	cancel_delayed_work_sync(&bootinfo.ws_dbg_wk);
	if (enabled)
		schedule_delayed_work(&bootinfo.ws_dbg_wk, HZ);
	return count;
}


static struct kobj_attribute hw_pm_debug_attr =
	__ATTR_RW(hw_pm_debug);

extern u8 *cnss_utils_get_wlan_mac_address(struct device *dev, uint32_t *num);

static ssize_t wifi_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	u8 *mac;
        u32 num = 0;

	mac = cnss_utils_get_wlan_mac_address(NULL, &num);
	if (mac == NULL)
            return sprintf(buf, "error");

	return sprintf(buf, "%2x:%2x:%2x:%2x:%2x:%2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

extern int cnss_utils_set_wlan_mac_address(const u8 *mac_list, const uint32_t len);
static ssize_t  wifi_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	int ret = 0;
        u8 mac[6];

        ret = sscanf(buf, "%2x:%2x:%2x:%2x:%2x:%2x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

        if (ret != 6) {
                pr_err("buf rule fail");
                return -1;
        }

        pr_info("--store %2x:%2x:%2x:%2x:%2x:%2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        ret = cnss_utils_set_wlan_mac_address(mac, 6);
        if (ret < 0)
                pr_err("set wlan mac address fail");

	return count;
}


static struct kobj_attribute wifi_attr =
	__ATTR_RW(wifi);
static struct attribute *boot_attrs[] = {
	&hw_type_attr.attr,
	&hw_pm_debug_attr.attr,
	&ddr_attr.attr,
	&hw_info_attr.attr,
	&wifi_attr.attr,
	&cpu_attr.attr,
	NULL
};

static const struct attribute_group boot_attr_group = {
	.attrs = boot_attrs,
};

int register_hardware_info(const char *name, const char *model)
{
	struct hardware *h;
	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (h == NULL) {
		pr_info("failed to alloc struct hardware\n");
		return -1;
	}

	if (strlen(name) > HARDWARE_MAX_ITEM_LONGTH || strlen(model) > HARDWARE_MAX_ITEM_LONGTH) {
		pr_info("name or model length is high %d\n", HARDWARE_MAX_ITEM_LONGTH);
		return -1;
	}
	strcpy(h->name, name);
        strcpy(h->model, model);

	mutex_lock(&hardware_list_mutex);
	list_add_tail(&h->node, &hardware_list);
	mutex_unlock(&hardware_list_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(register_hardware_info);

int unregister_hardware_info(const char *name)
{
	struct hardware *h, *tmp;

	mutex_lock(&hardware_list_mutex);
	list_for_each_entry_safe(h, tmp, &hardware_list, node) {
		if (strcmp(h->name, name) == 0) {
			list_del(&h->node);
			kfree(h);
		}
	}
	mutex_unlock(&hardware_list_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_hardware_info);

static void wakeup_source_print_work(struct work_struct *work)
{
	pm_print_active_wakeup_sources();
	schedule_delayed_work(&bootinfo.ws_dbg_wk, WS_DBG_INTERVAL_SECS * HZ);
}

int bootinfo_init(void)
{
	int ret = 0;
 	struct kobject *bootinfo_kobj = NULL;
	bootinfo_kobj = kobject_create_and_add(SYS_BOOT_INFO, NULL);
	if (!bootinfo_kobj)
		return -ENOMEM;
	INIT_DELAYED_WORK(&bootinfo.ws_dbg_wk, wakeup_source_print_work);
	ret = sysfs_create_group(bootinfo_kobj, &boot_attr_group);
	pr_info("%s(): ret is %d\n", __func__, ret);
	return ret;
}
subsys_initcall(bootinfo_init);
