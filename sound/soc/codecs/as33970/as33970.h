/*
 * as33970.h -- AS33970 Audio driver
 *
 * Copyright:   (C) 2020 Synaptics Systems, Inc.
 *
 * This is based on Alexander Sverdlin's CS4271 driver code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __AS33970_PRIV_H__
#define __AS33970_PRIV_H__

#define SND_SOC_COMPONENT
#define AS33970_BOOT_WITHOUT_FLASH 1  //enable this option if Tahiti has no dedicated flash
//#define AS33970_NEED_SET_I2C_SPEED 1  //enable this option if it need to set the I2C clock
//#define AS33970_ENABLE_SCREEN_STATUS //enable this option to notify screen status so that as33970 can enter into different power state
//#define AS33970_ENABLE_PHONE_CALL_STATUS
//#define AS33970_ENABLE_PIN           //enable this option when there is power enable pin
#define AS33970_I2C_SPEED_IN_HZ (100000)
#define AS33970_I2C_ADDRESS (0x41)
//#define AS33970_USE_WORK_QUEUE 1

#define AS33970_REG_MAX 0x2000

typedef enum {
	FWID_SYS = 0,
	FWID_SW  = 2,
	FWID_EXT = 3,
} FwModuleId;

typedef enum {
	SYS_CMD_VERSION                       = 1,
	SYS_CMD_SOFTRESET                     = 3,
	SYS_CMD_EXEC_FILE                     = 9,
	SYS_CMD_PARAMETER_VALUE               = 14,
	SYS_CMD_EVENT_PARAM                   = 15,
	SYS_CMD_MODE_ID                       = 19,
	SYS_CMD_SCRIPT_DATA                   = 20,
	SYS_CMD_BL_VERSION                    = 26,
	SYS_CMD_CURRENT_SYS_PARTITION         = 28,
	SYS_CMD_PM_CMD                        = 29,
	SYS_CMD_IPC_CMD                       = 37,
	SYS_CMD_IPC_MSG                       = 38,
	SYS_CMD_MEM_BULK_ACS_CTL              = 44,
	SYS_CMD_MEM_BULK_ACS_DAT			  = 45, 
} SysCmdCode;

typedef enum {
	SW_CADENCE_CMD_START                  = 15,
} SwCmdCode;

#define SYS_CADENCE_LP_TRIGGER_DET (1)
#define CHANNEL_MIXER_CMD_CONFIG  (64)
#define CPTR_MIXER_ID             (44)
#define EVENT_PAR_LP_TRIG_GOOG_EN  (60)
#define EVENT_PAR_LP_TRIG_SENS_EN  (61)

typedef enum {
	EVENT_PAR_I2S_IDX            = 29,
	PAR_INDEX_EVENT_ARG0         = 32
} ParProcessIndex;

typedef enum {
	EVENT_PAR_RATE_MAIN_INPUT             = PAR_INDEX_EVENT_ARG0, 
	EVENT_PAR_RATE_EC_REF                 = PAR_INDEX_EVENT_ARG0 + 5,
	EVENT_PAR_RATE_HOST_RECORD            = PAR_INDEX_EVENT_ARG0 + 6, //Used by Freeman3 on tahiti, careful when modify its value.
	EVENT_PAR_RATE_INDX_EC_REF            = PAR_INDEX_EVENT_ARG0 + 7,
	EVENT_PAR_RATE_INDX_HOST_RECORD       = PAR_INDEX_EVENT_ARG0 + 8,
	EVENT_PAR_RATE_INDX_USB_HOST_RECORD   = PAR_INDEX_EVENT_ARG0 + 9,
	EVENT_PAR_RATE_INDX_USB_HOST_PLAYBACK = PAR_INDEX_EVENT_ARG0 + 10,
	EVENT_PAR_USB_RECORD_STATE            = PAR_INDEX_EVENT_ARG0 + 11,
	EVENT_PAR_VOL_USB_RECORD_CH0          = PAR_INDEX_EVENT_ARG0 + 12,
	EVENT_PAR_VOL_USB_RECORD_CH1          = PAR_INDEX_EVENT_ARG0 + 13,
	EVENT_PAR_LP_TRIG_STATE               = PAR_INDEX_EVENT_ARG0 + 16,
	EVENT_PAR_USB_PLAYBACK_STATE          = PAR_INDEX_EVENT_ARG0 + 18,
	EVENT_PAR_VOL_USB_PLAYBACK_CH0        = PAR_INDEX_EVENT_ARG0 + 19,
	EVENT_PAR_VOL_USB_PLAYBACK_CH1        = PAR_INDEX_EVENT_ARG0 + 20,
	EVENT_PAR_ANC_FF_CALIB_GAIN           = PAR_INDEX_EVENT_ARG0 + 21,
	EVENT_PAR_ANC_FB_CALIB_GAIN           = PAR_INDEX_EVENT_ARG0 + 22,
	EVENT_PAR_ANC_PLBK_CANC_CALIB_GAIN    = PAR_INDEX_EVENT_ARG0 + 23,
	EVENT_PAR_AMB_INCL_CALIB_GAIN         = PAR_INDEX_EVENT_ARG0 + 24,
	EVENT_PAR_PLBK_CALIB_GAIN             = PAR_INDEX_EVENT_ARG0 + 25,
	EVENT_PAR_PSAP_CALIB_GAIN             = PAR_INDEX_EVENT_ARG0 + 26,
	EVENT_PAR_TRIG_DETECTED_INDEX         = PAR_INDEX_EVENT_ARG0 + 40,
	EVENT_PAR_ANC_PROFILE_LEFT            = PAR_INDEX_EVENT_ARG0 + 48, //
	EVENT_PAR_ANC_PROFILE_RIGHT           = PAR_INDEX_EVENT_ARG0 + 49, //
	EVENT_PAR_EAR_ROLE                    = PAR_INDEX_EVENT_ARG0 + 50, //
	EVENT_PAR_USER_FUNC_MODE_SEL1         = PAR_INDEX_EVENT_ARG0 + 68,
	EVENT_PAR_HPM_STATE                   = PAR_INDEX_EVENT_ARG0 + 73,
} EventParIndex;

typedef enum {
	EVENT_SAMPLE_RATE_CHANGE              = 1 << 0,  //used by software caf 
	EVENT_USB_RECORD_STARTSTOP            = 1 << 1,
	EVENT_CADENCE_END                     = 1 << 2,
	EVENT_VOLUME_CHANGE_MUTE_USB_RECORD   = 1 << 3,
	EVENT_VOLUME_CHANGE_USB_RECORD        = 1 << 4,
	EVENT_USB_PLAYBACK_STARTSTOP          = 1 << 5,
	EVENT_FAST_EXEC_END                   = 1 << 6,
	EVENT_VOLUME_CHANGE_USB_PLAYBACK      = 1 << 7,
	EVENT_TRIG_DETECTED                   = 1 << 8,
	EVENT_CMND_DETECTED                   = 1 << 9,
	EVENT_LP_TRIG_DETECTED                = 1 << 10,
	EVENT_TRIG_VAD_HANGOVER               = 1 << 11,
	EVENT_AVAD_CONTROL                    = 1 << 12,
	EVENT_CUSTOM_0                        = 1 << 13,
	EVENT_CUSTOM_1                        = 1 << 14,
	EVENT_CUSTOM_2                        = 1 << 15,
	EVENT_CUSTOM_3                        = 1 << 16,
	EVENT_UART_TX_RECD_STARTSTOP          = 1 << 17,
	EVENT_SPIM_TX_RECD_STARTSTOP          = 1 << 18,
	EVENT_ANC_CONTROL                     = 1 << 19,
	EVENT_ENC_CONTROL                     = 1 << 20,
	EVENT_ANC_PROFILE                     = 1 << 21,
	EVENT_AF_CONTROL                      = 1 << 22,
	EVENT_LP_TRIG_CONTROL                 = 1 << 23,
	EVENT_KEY_DETECTED                    = 1 << 24,

	EVENT_HPM_CONTROL                     = 1 << 27,
} EventId;

enum {
	PAR_INDEX_I2S_RX_WIDTH = 14,
	PAR_INDEX_I2S_RX_DELAY = 15,
	PAR_INDEX_I2S_RX_JUSTIFY = 16,
	PAR_INDEX_I2S_RX_LR_POLARITY = 17,
	PAR_INDEX_I2S_RX_NUM_OF_BITS = 18,
	PAR_INDEX_I2S_TX_WIDTH = 19,
	PAR_INDEX_I2S_TX_DELAY = 20,
	PAR_INDEX_I2S_TX_JUSTIFY = 21,
	PAR_INDEX_I2S_TX_LR_POLARITY = 22,
	PAR_INDEX_I2S_TX_NUM_OF_BITS = 23
} i2s_fmt;

enum MemBulkAcsCtrl_E{
	MEM_BULK_CTL_VALID_ADDR = 1,
	MEM_BULK_CTL_INF,
	MEM_BULK_CTL_TYPE,  
}; 

enum MemBulkAcsCtrlType_E{
	MEM_TYPE_CAPE_A_X = 1,
	MEM_TYPE_CAPE_A_Y,
	MEM_TYPE_CAPE_B_X,
	MEM_TYPE_CAPE_B_Y,
};

enum LOAD_MODEL_STEP{
	MODEL_START = 0,
	MODEL_WRITE,
	MODEL_END,  
}; 


#define EVENT_PAR_RATE_INDX_EXTRA_INPUT (61)
#define EVENT_PAR_RATE_INDX_MAIN_OUTPUT (62)

#endif
