#ifndef  __KTZ8866AB_H__
#define __KTZ8866AB_H__

#include <linux/leds.h>
/* KTZ8866 backlight I2C driver */
#define KTZ8866_backlight_EN_MASK         0x40
#define KTZ8866_backlight_EN_SHIFT        6
#define KTZ8866_backlight_DISABLE         0
#define KTZ8866_backlight_ENABLE          1
#define KTZ8866_LCD_BIAS_ENP              72       //GPIO for Enable pin for positive power (OUTP)
#define KTZ8866_LCD_BIAS_ENN              73       //GPIO for Enable pin for negative power (OUTN)
#define KTZ8866_LCD_DRV_HW_EN             82       //GPIO for Active high hardware enable pin
#define KTZ8866_LCD_DRV_I2C_SCL           61       //Clock of the I 2 C interface.
#define KTZ8866_LCD_DRV_I2C_SDA           60       //Bi-directional data pin of the I 2 C interface.

#define KTZ8866_DISP_ID                   0x01
#define KTZ8866_DISP_BC1                  0x02
#define KTZ8866_DISP_BC2                  0x03
#define KTZ8866_DISP_BB_LSB               0x04
#define KTZ8866_DISP_BB_MSB               0x05
#define KTZ8866_DISP_BL_ENABLE            0x08
#define KTZ8866_DISP_BIAS_CONF1           0x09
#define KTZ8866_DISP_BIAS_CONF2           0x0a
#define KTZ8866_DISP_BIAS_CONF3           0x0b
#define KTZ8866_DISP_BIAS_BOOST           0x0c
#define KTZ8866_DISP_BIAS_VPOS            0x0d
#define KTZ8866_DISP_BIAS_VNEG            0x0e
#define KTZ8866_DISP_FLAGS                0x0f
#define KTZ8866_DISP_OPTION1              0x10
#define KTZ8866_DISP_OPTION2              0x11
#define KTZ8866_DISP_PTD_LSB              0x12
#define KTZ8866_DISP_PTD_MSB              0x13
#define KTZ8866_DISP_DIMMING              0x14
#define KTZ8866_DISP_FULL_CURRENT         0x15
#define BL_LEVEL_MAX 2047

/*****************************************
* KTZ8866 STRUCT SETTINGS
******************************************/
struct ktz8866_data {
	struct i2c_client *client;
	struct input_dev *idev;
	struct led_classdev led_dev;
	atomic_t suspended;
};

struct ktz8866_reg {
	const char *name;
	uint8_t reg;
};

extern struct ktz8866_reg ktz8866a_regs[];

extern struct ktz8866_reg ktz8866b_regs[];

/**
   * struct ktz8866_led -
   * @lock - Lock for reading/writing the device
   * @level - setting backlight level
   * @level - setting backlight status
  **/
int backlight_ktz8866_init(void);
int lcd_bl_set_led_brightness(int value);
void lcd_bl_set_reg(void);
int lcd_bl_write_byte(unsigned char addr, unsigned char value);
int lcd_bl_bias_write_byte(unsigned char addr, unsigned char value);
int lcd_set_bias(bool enable);
 #endif 