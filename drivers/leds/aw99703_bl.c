/*
* aw99703.c   aw99703 backlight module
*
* Version: v1.0.3
*
* Copyright (c) 2019 AWINIC Technology CO., LTD
*
*  Author: Joseph <zhangzetao@awinic.com.cn>
*
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
*/

#include "ktd3136.h"
#include "aw99703_bl.h"

#define aw99703_i2c_read(client, reg, val)    ktd3137_read_reg(client, reg, val)
#define aw99703_i2c_write(client, reg, val)   i2c_sgm_write(client, reg, val)

struct{
	struct i2c_client *client;
	bool enable;
	u8 pwm_cfg;
	int hwen_gpio;
	bool  pwm_mode;
	unsigned int full_scale_led;
	unsigned int ramp_on_time;
	unsigned int ramp_off_time;
	unsigned int pwm_trans_dim;
	unsigned int i2c_trans_dim;
	unsigned int channel;
	unsigned int max_brightness;
	unsigned int default_brightness;
}drvdata = {
	.client = NULL,
	.enable = true,
	.hwen_gpio = 0,
	.pwm_mode = false,      //pwm mode disable
	.full_scale_led = 0x1A, //25.6mA
	.ramp_on_time = 0x07,
	.ramp_off_time = 0x07,
	.pwm_trans_dim = 0,
	.i2c_trans_dim = 0x03,
	.channel = 2,
	.max_brightness = 2047,
	.default_brightness = 1638,
};

static int aw99703_i2c_write_bit(struct i2c_client *client,
	unsigned int reg_addr, unsigned int  mask, unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw99703_i2c_read(client, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw99703_i2c_write(client, reg_addr, reg_val);

	return 0;
}

static int aw99703_bl_enable_channel()
{
	int ret = 0;

	if (drvdata.channel == 3) {
		ret = aw99703_i2c_write_bit(drvdata.client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH3_ENABLE |
						AW99703_LEDCUR_CH2_ENABLE |
						AW99703_LEDCUR_CH1_ENABLE);
	} else if (drvdata.channel == 2) {
		ret = aw99703_i2c_write_bit(drvdata.client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH2_ENABLE |
						AW99703_LEDCUR_CH1_ENABLE);
	} else if (drvdata.channel == 1) {
		ret = aw99703_i2c_write_bit(drvdata.client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH1_ENABLE);
	} else {
		ret = aw99703_i2c_write_bit(drvdata.client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						0x98);
	}

	return ret;
}

static void aw99703_pwm_mode_enable()
{
	if (drvdata.pwm_mode) {
		aw99703_i2c_write_bit(drvdata.client,
					AW99703_REG_MODE,
					AW99703_MODE_PDIS_MASK,
					AW99703_MODE_PDIS_ENABLE);
	} else {
		aw99703_i2c_write_bit(drvdata.client,
					AW99703_REG_MODE,
					AW99703_MODE_PDIS_MASK,
					AW99703_MODE_PDIS_DISABLE);
	}
}

static void aw99703_ramp_setting()
{
	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_TURNCFG,
				AW99703_TURNCFG_ON_TIM_MASK,
				drvdata.ramp_on_time << 4);

	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_TURNCFG,
				AW99703_TURNCFG_OFF_TIM_MASK,
				drvdata.ramp_off_time);
}
static void aw99703_transition_ramp()
{
	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_TRANCFG,
				AW99703_TRANCFG_PWM_TIM_MASK,
				drvdata.pwm_trans_dim);

	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_TRANCFG,
				AW99703_TRANCFG_I2C_TIM_MASK,
				drvdata.i2c_trans_dim);
}

static int aw99703_backlight_init()
{
	aw99703_pwm_mode_enable();

	/*mode:map type*/
	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_MODE,
				AW99703_MODE_MAP_MASK,
				AW99703_MODE_MAP_LINEAR);

	/*default OVPSEL 38V*/
	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_OVPSEL_MASK,
				AW99703_BSTCTR1_OVPSEL_38V);

	/*switch frequency 1000kHz*/
	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_SF_MASK,
				AW99703_BSTCTR1_SF_1000KHZ);

	/*OCP SELECT*/
	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_OCPSEL_MASK,
				AW99703_BSTCTR1_OCPSEL_3P3A);

	/*BSTCRT2 IDCTSEL*/
	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_BSTCTR2,
				AW99703_BSTCTR2_IDCTSEL_MASK,
				AW99703_BSTCTR2_IDCTSEL_10UH);

	/*Backlight current full scale*/
	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_LEDCUR,
				AW99703_LEDCUR_BLFS_MASK,
				drvdata.full_scale_led << 3);

	aw99703_bl_enable_channel();
	aw99703_ramp_setting();
	aw99703_transition_ramp();

	return 0;
}

static int aw99703_backlight_enable()
{
	aw99703_i2c_write_bit(drvdata.client, AW99703_REG_MODE, AW99703_MODE_WORKMODE_MASK, AW99703_MODE_WORKMODE_BACKLIGHT);

	drvdata.enable = true;

	return 0;
}

static int  aw99703_brightness_set(int brt_val)
{
	if (drvdata.enable == false) {
		aw99703_backlight_init();
		aw99703_backlight_enable();
	}

	if (brt_val > 0) {
		/*enalbe bl mode*/
		/* real brightness = max_brighness*pwm_brightness */
		aw99703_i2c_write(drvdata.client, AW99703_REG_LEDLSB, brt_val&0x07);
		aw99703_i2c_write(drvdata.client, AW99703_REG_LEDMSB, (brt_val >> 3)&0xff);

		/* backlight enable */
		aw99703_i2c_write_bit(drvdata.client, AW99703_REG_MODE, AW99703_MODE_WORKMODE_MASK, AW99703_MODE_WORKMODE_BACKLIGHT);
	} else {
		/* standby mode*/
		aw99703_i2c_write_bit(drvdata.client, AW99703_REG_MODE, AW99703_MODE_WORKMODE_MASK, AW99703_MODE_WORKMODE_STANDBY);
	}

	return 0;
}

static int aw99703_hbm_set(int mode){
	int val = 0;
	
	switch (mode) {
	case HBM_MODE_DEFAULT:
		val = 0x00;
		break;
	case HBM_MODE_LEVEL1:
		val = 0x13;
		break;
	case HBM_MODE_LEVEL2:
		val = 0x17;
		break;
	case HBM_MODE_LEVEL3:
		val = 0x1A;
		break;
	default:
		break;
	 }

	aw99703_i2c_write_bit(drvdata.client,
				AW99703_REG_LEDCUR,
				AW99703_LEDCUR_BLFS_MASK,
				val << 3);

	return  0;
}

static void aw99703_init(void){
	if(!bkl_chip){return;}
	if(!bkl_chip->client){return;}

	drvdata.client = bkl_chip->client;
	aw99703_backlight_init();
	aw99703_backlight_enable();
	aw99703_brightness_set(drvdata.default_brightness);
}

const static uint8_t reg_list[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0xff};
/*,0x0b,0x0c,0x0d,0x0e,0x0f,0x11,0x21,0x22,
   0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x31,
   0x33,0x40,0x41,0x42,0x43,0x44,0x45,0x46,
   0x47,0x48,0x49,0x67,0x68,0x69};
*/

Backlight_I2C_Control lcdbl_aw99703 = {
	.id 			= 0x03,
	.ic 			= "aw99703",
	.reg_list       = (uint8_t*)reg_list,
	.init			= aw99703_init,
	.set_brightness = aw99703_brightness_set,
	.hbm_set		= aw99703_hbm_set
};
