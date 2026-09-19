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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define CTN730_MAGIC	0xE9

/*
 * ctn730 power control via ioctl
 * ctn730_SET_PWR(0): power off
 * ctn730_SET_PWR(1): power on
 */

#define PWR_OFF 0
#define PWR_ON  1

#define CLK_OFF 0
#define CLK_ON  1

#define GPIO_UNUSED -1

#define ctn730_SET_PWR	_IOW(CTN730_MAGIC, 0x01, unsigned int)

struct ctn730_i2c_platform_data {
	unsigned int irq_gpio;
	unsigned int ven_gpio;
};
