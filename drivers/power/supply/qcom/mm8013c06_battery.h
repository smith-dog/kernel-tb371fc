#ifndef __MM8013C16_BATTERY_H__
#define __MM8013C16_BATTERY_H__

enum hq_batteryID
{
	HQ_BATTERY_FMT,
	HQ_BATTERY_SWD,
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 start*/
	HQ_BATTERY_LINDE,
/*Linden code for JLINDEN-17 by kangkai4 at 20221128 end*/
	HQ_BATTERY_UNKNOW
};

int mm8013_temperature(int *val);
int mm8013_current(int *val);
int mm8013_voltage(int *val);
int mm8013_soc(int *val);
int mm8013_cycle(int *val);
int mm8013_remain(int *val);
int mm8013_month(int *val);
int mm8013_day(int *val);
int mm8013_hour(int *val);
int mm8013_set_chg_voltage(int val);
int mm8013_get_chg_voltage(int *val);

/* add by yangdi 2020-12-10 start */
bool mm8013_get_present(void);
enum hq_batteryID hq_batt_id_get(void);
/* add by yangdi 2020-12-10 end */

#endif