#ifndef __LINUX_BOOT_INFO_H_
#define __LINUX_BOOT_INFO_H_
#include <linux/types.h>

enum hw_platform_type {
	HW_PLATFORM_EVB = 0,
	HW_PLATFORM_EVT = 2,
	HW_PLATFORM_DVT1 = 4,
	HW_PLATFORM_DVT1_2 = 6,
	HW_PLATFORM_DVT2 = 8,
	HW_PLATFORM_DVT2_2 = 9,
	HW_PLATFORM_PVT = 10,
	HW_PLATFORM_DVT2_3 = 11,
	HW_PLATFORM_PVT1 = 13,
	HW_PLATFORM_PVT2 = 15,
	HW_PLATFORM_UNKNOWN,
};

typedef enum
{
  HW_SKU_ROW_LTE      = 0x00,
  HW_SKU_PRC_LTE      = 0x01,
  HW_SKU_WIFI         = 0x03,
  HW_SKU_UNKNOWN,
} HWSkuInfo;

typedef enum
{
  HW_PALADIN      = 0x00,
  HW_BLADEX      = 0x01,
} HWProject;


extern bool hw_pm_debug_enable(void);
extern enum hw_platform_type hw_get_platform_type(void);
/*
enum boot_mode_type {
	BOOT_NORMAL,
	BOOT_CHARGING,
	BOOT_RECOVERY,
	BOOT_FASTBOOTD,
        BOOT_FACTORY,
	BOOT_FFBM,
	BOOT_ALARM
}*/

int register_hardware_info(const char *name, const char *model);
int unregister_hardware_info(const char *name);

#endif /* __LINUX_BOOT_INFO_H_ */
