#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/pm_runtime.h>
#include "mm8013c06_battery.h"
//#include <linux/hqsysfs.h>
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
#ifdef CONFIG_HQ_SYSFS_SUPPORT
#include <linux/hqsysfs.h>
#endif
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/

#define DRIVER_VERSION                      "1.0.0"

#define REG_CTRL_0                          0x00
#define REG_TEMPERATURE                     0x06
#define REG_VOLTAGE                         0x08
#define REG_CURRENT_AVG                    0x14
#define REG_CURRENT                         0x72
#define REG_RSOC                            0x2c
#define REG_CHG_VOLTAGE                    0x30
#define REG_BLOCKDATAOFFSET                0x3e
#define REG_BLOCKDATA                       0x40
#define REG_MONTH                           0x74
#define REG_DAY                             0x76
#define REG_HOUR                            0x78
#define REG_CYCLECOUNT                     0x2A
#define REG_REMAIN_CAPACITY                0x12


int last_temperature = 0;
int last_cycle = 1;
bool has_8013 = false;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
static int	fake_cycle_count;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/

/* add by yangdi 2021-1-25 start */
#define BATTERY_STRING_MAX      16
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
#ifdef CONFIG_HQ_SYSFS_SUPPORT
static char battery_manufacturer[HQ_BATTERY_UNKNOW + 1][BATTERY_STRING_MAX] = {
   "FMT",
   "SWD",
   "LINDE",
   "UNKNOW"
};
#endif
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/
/* add by yangdi 2021-1-25 end */

#define MM8013_DEFAULT_SOC              50
#define MM8013_DEFAULT_TEMP             250
#define MM8013_DEFAULT_VOLTAGE          3900
#define MM8013_DEFAULT_CHG_VOLTAGE     4430
#define MM8013_DEFAULT_CURRENT          500
#define MM8013_DEFAULT_REMAIN           7500
#define MM8013_DEFAULT_MONTH            10
#define MM8013_DEFAULT_DAY              12
#define MM8013_DEFAULT_HOUR             13
#define MM8013_DEFAULT_CYCLE            1

struct mm8013_chip {
    struct i2c_client *client;
    struct power_supply *mm8013_psy;
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    struct mutex suspend_lock;
    bool suspended;
    int bat_soc;
    int voltage;
    int bat_curr;
    int curr_avg;
    int remain_capacity;
    int month;
    int days;
    int hours;
    int chg_volt;
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
};
struct mm8013_chip *chip = NULL;

static int mm8013_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
    int ret = i2c_smbus_write_word_data(client, reg, value);

    if (ret < 0)
        dev_err(&client->dev, "%s: err %d\n", __func__, ret);

    msleep(4);
    return ret;
}

int mm8013_read_reg(struct i2c_client *client, u8 reg)
{
    int ret = 0;

    ret = i2c_smbus_read_word_data(client, reg);
    if (ret < 0) {
        /* MM8013 NACKs occasionally during its measurement cycle;
         * retry once after a short delay to avoid spurious failures. */
        usleep_range(1000, 2000);
        ret = i2c_smbus_read_word_data(client, reg);
    }
    if (ret < 0)
        dev_err(&client->dev, "%s: err %d\n", __func__, ret);

    return ret;
}

int mm8013_soc(int *val)
{
    int soc = 0;
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    int ret = 0;
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->bat_soc;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    if (has_8013) {
        soc = mm8013_read_reg(chip->client, REG_RSOC);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
        if (soc < 0) {
            *val = chip->bat_soc;
            ret = 0;
        }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
        else if (soc > 100)
            *val = 100;
        else
            *val = soc;
    } else {
        *val = MM8013_DEFAULT_SOC;
    }

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->bat_soc = *val;
done:
    mutex_unlock(&chip->suspend_lock);

    return ret;
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
}
EXPORT_SYMBOL(mm8013_soc);

int mm8013_voltage(int *val)
{
    int volt = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->voltage;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    if (has_8013) {
       volt = mm8013_read_reg(chip->client, REG_VOLTAGE);
       if (volt < 0)
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
           *val = chip->voltage;
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
       else
           *val = volt*1000;
    } else {
        /*Linden code for JLINDEN-226 by zhoujj21 at 20221110 start*/
        *val = MM8013_DEFAULT_VOLTAGE*1000;
        /*Linden code for JLINDEN-226 by zhoujj21 at 20221110 end*/
    }

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->voltage = *val;
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_voltage);

int mm8013_current(int *val)
{
    int curr = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->bat_curr;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    /*Linden code for JLINDEN-226 by zhoujj21 at 20221110 start*/
    if (has_8013) {
        curr = mm8013_read_reg(chip->client, REG_CURRENT);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
        if (curr < 0) {
            *val = chip->bat_curr;
        }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
        if (curr >= 0) {
            if (curr > 32767) {
                curr -= 65536;
            }
            *val = curr*1000;
        }
    } else {
        *val = MM8013_DEFAULT_CURRENT*1000;
    }
    /*Linden code for JLINDEN-226 by zhoujj21 at 20221110 end*/

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->bat_curr = *val;
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_current);

/*Linden code for JLINDEN-692 by zhoujj21 at 20230106 start*/
int mm8013_current_avg(int *val)
{
    int curr = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->curr_avg;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    if (has_8013) {
        curr = mm8013_read_reg(chip->client, REG_CURRENT_AVG);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
        if (curr < 0) {
            *val = chip->curr_avg;
        }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
        if (curr >= 0) {
            if (curr > 32767) {
                curr -= 65536;
            }
            *val = curr*1000;
        }
    } else {
        *val = MM8013_DEFAULT_CURRENT*1000;
    }

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->curr_avg = *val;
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_current_avg);
/*Linden code for JLINDEN-692 by zhoujj21 at 20230106 end*/

int mm8013_remain(int *val)
{
    int remain = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->remain_capacity;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    if (has_8013) {
        remain = mm8013_read_reg(chip->client, REG_REMAIN_CAPACITY);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
        if (remain < 0) {
            *val = chip->remain_capacity;
        }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
        if (remain > 7500) {
           remain -= 7500;
       }
       *val = remain;
    } else {
        *val = MM8013_DEFAULT_REMAIN;
    }

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->remain_capacity = *val;
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_remain);

int mm8013_temperature(int *val)
{
    int temp = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = last_temperature;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    if (has_8013) {
        temp = mm8013_read_reg(chip->client, REG_TEMPERATURE);
        *val = temp - 2731;
        if (temp < 0) {
            *val = last_temperature;
        }
    } else {
        *val = MM8013_DEFAULT_TEMP;
    }

    last_temperature = *val;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_temperature);

int mm8013_month(int *val)
{
    int mon = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->month;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    if (has_8013) {
        mon =  mm8013_read_reg(chip->client, REG_MONTH);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
        if (mon < 0) {
            *val = chip->month;
        }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
        if (mon > 32767) {
            mon -= 65536;
        }
        *val = mon;
    } else {
        *val = MM8013_DEFAULT_MONTH;
    }

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->month = *val;
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_month);

int mm8013_day(int *val)
{
    int day = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->days;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    if (has_8013) {
        day =  mm8013_read_reg(chip->client, REG_DAY);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
        if (day < 0) {
            *val = chip->days;
        }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
        if (day > 32767) {
            day -= 65536;
        }
        *val = day;
    } else {
        *val = MM8013_DEFAULT_DAY;
    }

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->days = *val;
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_day);

int mm8013_hour(int *val)
{
    int hour = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->hours;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    if (has_8013) {
        hour =  mm8013_read_reg(chip->client, REG_HOUR);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
        if (hour < 0) {
            *val = chip->hours;
        }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
        if (hour > 32767) {
            hour -= 65536;
        }
        *val = hour;
    } else {
        *val = MM8013_DEFAULT_HOUR;
    }

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->hours = *val;
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_hour);

int mm8013_cycle(int *val)
{
    int cycle = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = last_cycle;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    if (has_8013) {
        cycle = mm8013_read_reg(chip->client, REG_CYCLECOUNT);
        if (cycle > 32767) {
            cycle -= 65536;
        }
        *val = cycle;

        if (cycle < 0)
            *val = last_cycle;
    } else {
        *val = MM8013_DEFAULT_CYCLE;
    }

    last_cycle = *val;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_cycle);

/*Set Battery FV uV*/
int mm8013_set_chg_voltage(int val)
{
    int ret = 0;
    int val_mv = val/1000;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        mutex_unlock(&chip->suspend_lock);
        return -EBUSY;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    ret = mm8013_write_reg(chip->client, REG_CHG_VOLTAGE, val_mv);
    if (ret < 0)
        pr_err("[%s] write %d error!\n", __func__, REG_CHG_VOLTAGE);

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return ret;
}
EXPORT_SYMBOL(mm8013_set_chg_voltage);

/*Get Battery FV uV*/
int mm8013_get_chg_voltage(int *val)
{
    int volt = 0;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    mutex_lock(&chip->suspend_lock);
    if (chip->suspended) {
        *val = chip->chg_volt;
        goto done;
    }
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/

    volt = mm8013_read_reg(chip->client, REG_CHG_VOLTAGE);
    if (volt < 0) {
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
        *val = chip->chg_volt;
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    } else {
        *val = volt*1000;
    }

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->chg_volt = *val;
done:
    mutex_unlock(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    return 0;
}
EXPORT_SYMBOL(mm8013_get_chg_voltage);

static enum hq_batteryID mm8013_get_bat_id(struct mm8013_chip *chip)
{
    int ret = 0;
    enum hq_batteryID id;

    if (has_8013) {
        ret = mm8013_write_reg(chip->client, REG_CTRL_0, 0x08);
        if (ret < 0)
            pr_err("[%s] write %d error!\n", __func__, REG_CTRL_0);
        ret = mm8013_read_reg(chip->client, REG_CTRL_0);
        pr_err("[%s] read %d reg:%x!\n", __func__, REG_CTRL_0, ret);
        switch(ret)
        {
            case 0x0101:
                id = HQ_BATTERY_SWD;break;
            case 0x0102:
                id = HQ_BATTERY_FMT;break;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
            case 0x0103:
                id = HQ_BATTERY_LINDE; break;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/
            default:
                id = HQ_BATTERY_UNKNOW;
        }
    } else {
        id = HQ_BATTERY_UNKNOW;
    }

    //hq_register_hw_info(HWID_BATTERY, battery_manufacturer[id]);
    return id;
}

enum hq_batteryID hq_batt_id_get(void)
{
    return mm8013_get_bat_id(chip);
}
EXPORT_SYMBOL(hq_batt_id_get);

bool mm8013_get_present(void)
{
    return has_8013;
}
EXPORT_SYMBOL(mm8013_get_present);

static int mm8013_checkdevice(struct mm8013_chip *chip)
{
    int ret;
    int count = 3;
    while (count--) {
        ret = mm8013_write_reg(chip->client, REG_CTRL_0, 0x08);
        if (ret < 0)
            pr_err("[%s] write %d error!\n", __func__, REG_CTRL_0);

        ret = mm8013_read_reg(chip->client, REG_CTRL_0);
        if (ret > 0)
            break;
    }
    pr_err("mm8013_checkdevice, count=%d, ret=0x%02X\n", count, ret);

    return ret;
#if 0
    enum hq_batteryID id = HQ_BATTERY_UNKNOW;
    id = mm8013_get_bat_id(chip);
    //hq_register_hw_info(HWID_BATTERY, battery_manufacturer[id]);

    int ret = 0;
    /*uint prev;
    uint id1;
    uint id2;*/

    // Parameter Rev.
    ret = mm8013_write_reg(chip->client, REG_BLOCKDATAOFFSET, 0x41f2);
    if (ret < 0) return ret;
    ret = mm8013_read_reg(chip->client, REG_BLOCKDATAOFFSET);
    if (ret != 0x41f2) return ret;
    ret = mm8013_read_reg(chip->client, REG_BLOCKDATA);
    if (ret < 0) return ret;
    prev = (uint)(ret & 0xffff);

    // ID information 1
    ret = mm8013_write_reg(chip->client, REG_BLOCKDATAOFFSET, 0x41f8);
    if (ret < 0) return ret;
    ret = mm8013_read_reg(chip->client, REG_BLOCKDATAOFFSET);
    if (ret != 0x41f8) return ret;
    ret = mm8013_read_reg(chip->client, REG_BLOCKDATA);
    if (ret < 0) return ret;
    id1 = (uint)((ret & 0x00ff) << 8) | (uint)((ret & 0xff00) >> 8);
    id1 = id1 << 16;
    ret = mm8013_read_reg(chip->client, REG_BLOCKDATA + 2);
    if (ret < 0) return ret;
    id1 |= (uint)((ret & 0x00ff) << 8) | (uint)((ret & 0xff00) >> 8);

    // ID information 2
    ret = mm8013_write_reg(chip->client, REG_BLOCKDATAOFFSET, 0x41fc);
    if (ret < 0) return ret;
    ret = mm8013_read_reg(chip->client, REG_BLOCKDATAOFFSET);
    if (ret != 0x41fc) return ret;
    ret = mm8013_read_reg(chip->client, REG_BLOCKDATA);
    if (ret < 0) return ret;
    id2 = (uint)((ret & 0x00ff) << 8) | (uint)((ret & 0xff00) >> 8);
    id2 = id2 << 16;
    ret = mm8013_read_reg(chip->client, REG_BLOCKDATA + 2);
    if (ret < 0) return ret;
    id2 |= (uint)((ret & 0x00ff) << 8) | (uint)((ret & 0xff00) >> 8);
#endif
    return 0;
}

static enum power_supply_property mm8013_battery_props[] = {
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CURRENT_AVG,
    POWER_SUPPLY_PROP_TEMP,
    POWER_SUPPLY_PROP_CYCLE_COUNT,
    POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int mm8013_get_property(struct power_supply *psy,
                    enum power_supply_property psp,
                    union power_supply_propval *val)
{
    int ret  = 0;

    switch (psp) {
    case POWER_SUPPLY_PROP_CAPACITY:
        ret = mm8013_soc(&val->intval);
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        ret = mm8013_voltage(&val->intval);
        break;
    case POWER_SUPPLY_PROP_CURRENT_NOW:
        ret = mm8013_current(&val->intval);
        break;
/*Linden code for JLINDEN-692 by zhoujj21 at 20230106 start*/
    case POWER_SUPPLY_PROP_CURRENT_AVG:
        ret = mm8013_current_avg(&val->intval);
        break;
/*Linden code for JLINDEN-692 by zhoujj21 at 20230106 end*/
    case POWER_SUPPLY_PROP_TEMP:
        ret = mm8013_temperature(&val->intval);
        break;
    case POWER_SUPPLY_PROP_CYCLE_COUNT:
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
        if (fake_cycle_count >= 0) {
            val->intval = fake_cycle_count;
        } else {
            ret = mm8013_cycle(&val->intval);
        }
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_MAX:
        ret = mm8013_get_chg_voltage(&val->intval);
        break;
    default:
        return -EINVAL;
    }

	if (ret < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, ret);
		return -ENODATA;
	}

    return ret;
}

static int mm8013_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
    int ret  = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
        mm8013_set_chg_voltage(val->intval);
		break;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		fake_cycle_count = val->intval;
        break;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int mm8013_property_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc mm8013_psy_desc = {
	.name = "bms",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = mm8013_battery_props,
	.num_properties = ARRAY_SIZE(mm8013_battery_props),
	.get_property = mm8013_get_property,
	.set_property = mm8013_set_property,
	.property_is_writeable = mm8013_property_is_writeable,
};

static const struct of_device_id match_table[] = {
	{
		.compatible	= "mitsumi,mm8013c06",
	},
	{ },
};

static int mm8013_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct power_supply_config psy_cfg = {};
    int ret = 0;

    has_8013 = false;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
    fake_cycle_count = -EPERM;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/

    pr_err("mm8013_probe start!\n");
    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
        return -EIO;

    chip = kzalloc(sizeof(*chip), GFP_KERNEL);
    if (!chip)
        return -ENOMEM;
    chip->client = client;

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
    chip->bat_soc = MM8013_DEFAULT_SOC;
    chip->voltage = MM8013_DEFAULT_VOLTAGE * 1000;
    chip->bat_curr = MM8013_DEFAULT_CURRENT * 1000;
    chip->curr_avg = MM8013_DEFAULT_CURRENT * 1000;
    chip->remain_capacity = MM8013_DEFAULT_REMAIN;
    chip->month = MM8013_DEFAULT_MONTH;
    chip->days = MM8013_DEFAULT_DAY;
    chip->hours = MM8013_DEFAULT_HOUR;
    chip->chg_volt = MM8013_DEFAULT_CHG_VOLTAGE * 1000;
    last_temperature = MM8013_DEFAULT_TEMP;
    mutex_init(&chip->suspend_lock);
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    i2c_set_clientdata(client, chip);

    ret = mm8013_checkdevice(chip);
    if (ret < 0) {
        dev_err(&client->dev, "failed to access ret = %d\n", ret);
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
        #ifdef CONFIG_HQ_SYSFS_SUPPORT
        hq_register_hw_info(HWID_BATTERY, "fail");
        #endif
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/
    } else {
        has_8013 = true;
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
        #ifdef CONFIG_HQ_SYSFS_SUPPORT
        ret = mm8013_get_bat_id(chip);
        hq_register_hw_info(HWID_BATTERY, battery_manufacturer[ret]);
        #endif
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/
    }

    psy_cfg.drv_data = chip;
    chip->mm8013_psy = devm_power_supply_register(&client->dev,
					   &mm8013_psy_desc,
					   &psy_cfg);
    if (IS_ERR(chip->mm8013_psy)) {
        ret = PTR_ERR(chip->mm8013_psy);
        dev_err(&client->dev, "failed:mm8013 power supply register\n");
        i2c_set_clientdata(client, NULL);
        kfree(chip);
        return ret;
    }

    pr_err("mm8013_probe success!\n");

    return 0;
}

static int mm8013_remove(struct i2c_client *client)
{
    chip = i2c_get_clientdata(client);

    power_supply_unregister(chip->mm8013_psy);
    i2c_set_clientdata(client, NULL);
    kfree(chip);

    return 0;
}

static const struct i2c_device_id mm8013_id[] = {
    { "mm8013", 0 },
    {},
};
MODULE_DEVICE_TABLE(i2c, mm8013_id);

/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
#ifdef CONFIG_PM_SLEEP
static int mm8013_suspend(struct device *dev)
{
    struct mm8013_chip *chip = dev_get_drvdata(dev);

    mutex_lock(&chip->suspend_lock);
    chip->suspended = true;
    mutex_unlock(&chip->suspend_lock);

    return 0;
}

static int mm8013_resume(struct device *dev)
{
    struct mm8013_chip *chip = dev_get_drvdata(dev);

    mutex_lock(&chip->suspend_lock);
    chip->suspended = false;
    mutex_unlock(&chip->suspend_lock);

    return 0;
}

static SIMPLE_DEV_PM_OPS(mm8013_pm_ops, mm8013_suspend, mm8013_resume);
#endif
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
static struct i2c_driver mm8013_i2c_driver = {
    .driver    = {
        .name  = "mm8013",
        .of_match_table	= match_table,
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 start*/
#ifdef CONFIG_PM_SLEEP
        .pm = &mm8013_pm_ops,
#endif
/*Linden code for JLINDEN-11308 by huyh10 at 20230711 end*/
    },
    .probe     = mm8013_probe,
    .remove    = mm8013_remove,
    .id_table  = mm8013_id,
};
static inline int mm8013_i2c_init(void)
{
    return i2c_add_driver(&mm8013_i2c_driver);
}
static int __init mm8013_init(void)
{
    return mm8013_i2c_init();
}
module_init(mm8013_init);

static void __exit mm8013_exit(void)
{
    i2c_del_driver(&mm8013_i2c_driver);
}
module_exit(mm8013_exit);
MODULE_DESCRIPTION("MM8013 battery Driver");
MODULE_LICENSE("GPL v2");